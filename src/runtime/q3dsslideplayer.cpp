/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of Qt 3D Studio.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "q3dsslideplayer_p.h"

#include <QtCore/qglobal.h>
#include <QtCore/qloggingcategory.h>
#include "q3dsscenemanager_p.h"
#include "q3dsanimationmanager_p.h"
#include "q3dslogging_p.h"

#include <Qt3DAnimation/qclipanimator.h>
#include <Qt3DAnimation/qclock.h>
#include <Qt3DAnimation/qanimationclip.h>
#include <Qt3DAnimation/qanimationcallback.h>
#include <Qt3DAnimation/qchannel.h>
#include <Qt3DAnimation/qchannelmapper.h>
#include <Qt3DAnimation/qcallbackmapping.h>

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QLayer>

QT_BEGIN_NAMESPACE

class Q3DSSlidePositionCallback : public Qt3DAnimation::QAnimationCallback
{
public:
    Q3DSSlidePositionCallback(Q3DSSlide *slide)
        : m_slide(slide) {}

    void valueChanged(const QVariant &value) override {
        Q_ASSERT(m_slide);

        const float newValue = value.toFloat();
        if (qFuzzyCompare(m_previousValue, newValue))
            return;

        Q3DSSlidePlayer *slidePlayer = m_slide->attached<Q3DSSlideAttached>()->slidePlayer;
        // TODO: See QT3DS-1302
        if (!slidePlayer)
            return;

        slidePlayer->setSlideTime(m_slide, newValue * 1000.0f);
        m_previousValue = newValue;
    }

private:
    Q3DSSlide *m_slide;
    float m_previousValue = -1.0f;
};

// Dummy animator for keeping track of the time line for the current slide
static void attatchPositionCallback(Q3DSSlide *slide)
{
    using namespace Qt3DAnimation;

    Q_ASSERT(slide);
    qint32 startTime = 0.0f; // We always start from 0.0
    qint32 endTime = 0.0f;
    Q3DSSlideUtils::getStartAndEndTime(slide, nullptr, &endTime);

    Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
    QClipAnimator *animator = data->animator ? data->animator : (data->animator = new QClipAnimator);
    if (!animator->clock())
        animator->setClock(new QClock);

    QAnimationClip *clip = animator->clip() ? static_cast<QAnimationClip *>(animator->clip()) : new QAnimationClip;

    const QString channelName = slide->name() + QLatin1String("_timeDummy");
    QKeyFrame keyFrameStart(QVector2D(startTime / 1000.0f, 0.0f));
    QKeyFrame keyFrameEnd(QVector2D(endTime / 1000.0f, endTime / 1000.0f));

    // New clip data
    QAnimationClipData clipData;
    QChannel channel(channelName);
    QChannelComponent component;
    component.appendKeyFrame(keyFrameStart);
    component.appendKeyFrame(keyFrameEnd);
    channel.appendChannelComponent(component);
    clipData.appendChannel(channel);

    if (!animator->channelMapper()) {
        QChannelMapper *mapper = new QChannelMapper;
        QCallbackMapping *mapping = new QCallbackMapping;
        mapping->setChannelName(channelName);
        mapping->setCallback(QMetaType::Float, new Q3DSSlidePositionCallback(slide));
        mapper->addMapping(mapping);
        animator->setChannelMapper(mapper);
    }

    clip->setClipData(clipData);
    animator->setClip(clip);

    data->entity->addComponent(animator);
}

static void detatchPositionCallback(Q3DSSlide *slide)
{
    if (!slide)
        return;

    Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
    auto animator = data->animator;
    if (animator) {
        Q_ASSERT(!animator->isRunning());
        slide->attached()->entity->removeComponent(animator);
        // TODO: Figure out why this is needed.
        data->animator = nullptr;
        delete animator;
    }
}

void Q3DSSlideUtils::getStartAndEndTime(Q3DSSlide *slide, qint32 *startTime, qint32 *endTime)
{
    Q_ASSERT(startTime != nullptr || endTime != nullptr);

    qint32 layerEndTime = -1;
    qint32 nodesEndtime = -1;

    // Check if there are nodes  from the parent slide that has property changes on this slide.
    if (Q3DSSlide *p = static_cast<Q3DSSlide *>(slide->parent())) {
        for (auto *obj : p->objects()) {
            if (!obj->isNode() || obj->state() != Q3DSGraphObject::Enabled)
                continue;

            // Look for property updates on "this" slide.
            const auto &props = slide->propertyChanges();
            const auto foundIt = props.constFind(obj);
            if (foundIt == props.constEnd())
                continue;

            // If there are property changes for the object, check if it has a new endtime.
            std::find_if(foundIt.value()->cbegin(), foundIt.value()->cend(), [&layerEndTime, &nodesEndtime, obj](const Q3DSPropertyChange &propChange) {
                if (propChange.name() == QLatin1String("endtime")) {
                    bool ok = false;
                    const qint32 value = propChange.value().toInt(&ok);
                    if (ok) {
                        if (obj->type() == Q3DSGraphObject::Layer && (value > layerEndTime))
                            layerEndTime = value;
                        else if (value > nodesEndtime)
                            nodesEndtime = value;
                    }
                }
                return false;
            });
        }
    }

    // Now look for the endtime on nodes on this slide.
    for (const auto obj : slide->objects()) {
        // Skip non-node types.
        if (!obj->isNode() || obj->state() != Q3DSGraphObject::Enabled)
            continue;

        // We collect both layer endtimes (if any) and object endtimes in one go.
        if (obj->type() == Q3DSGraphObject::Layer && (obj->endTime() > layerEndTime))
            layerEndTime = obj->endTime();
        else if (obj->endTime() > nodesEndtime)
            nodesEndtime = obj->endTime();
    }

    // Final fallback, if neither was found use the value set by the slide.
    if (layerEndTime == -1 && nodesEndtime == -1)
        nodesEndtime = slide->endTime();

    if (startTime)
        *startTime = slide->startTime();
    if (endTime)
        *endTime = layerEndTime != -1 ? layerEndTime : nodesEndtime;
}

static QString getSlideName(Q3DSSlide *slide)
{
    return slide ? slide->name() : QStringLiteral("nullptr");
}

static Q3DSSlidePlayer::PlayerState getInitialSlideState(Q3DSSlide *slide)
{
    return (slide->initialPlayState() == Q3DSSlide::Play) ? Q3DSSlidePlayer::PlayerState::Playing
                                                          : Q3DSSlidePlayer::PlayerState::Paused;

}

static void updatePosition(Q3DSSlide *slide, float newTimeMs, float durationMs)
{
    const auto updateAnimator = [newTimeMs](Qt3DAnimation::QClipAnimator *animator, float durationMs) {
        if (!animator)
            return;

        const float nt = qBound(0.0f, newTimeMs / durationMs, 1.0f);
        // NOTE!!!: This is a bit funky, but it means we can avoid tracking the normalized values in the
        // frontend node. This of course assumes that the limit in the fuzzy compare doesn't change!
        animator->setNormalizedTime(qFuzzyCompare(nt, animator->normalizedTime()) ? qAbs(nt - (1.0f / 100000.0f)) : nt);
    };
    const auto updateAnimators = [&updateAnimator](const QVector<Qt3DAnimation::QClipAnimator *> &animators) {
        for (auto animator : animators) {
            const float durationMs = animator->clip()->duration() * 1000.0f;
            updateAnimator(animator, durationMs);
        }
    };

    Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
    Q_ASSERT(data);

    updateAnimator(data->animator, durationMs);
    updateAnimators(data->animators);
};

static void updateAnimators(Q3DSSlide *slide, bool running, bool restart, float rate)
{
    const auto updateAnimator = [running, restart, rate](Qt3DAnimation::QClipAnimator *animator) {
        if (!animator)
            return;

        if (restart) {
            // NOTE!!!: This is a bit funky, but it means we can avoid tracking the normalized values in the
            // frontend node. This of course assumes that the limit in the fuzzy compare doesn't change!
            animator->setNormalizedTime(qFuzzyCompare(animator->normalizedTime(), 0.0f) ? (0.0f + (1.0f / 100000.0f)) : 0.0f);
        }
        animator->clock()->setPlaybackRate(double(rate));
        // NOTE: The running value might not have been updated in the frontend node yet,
        // so force update if the values are the same...
        animator->setRunning(!running);
        animator->setRunning(running);
    };
    const auto updateAnimators = [&updateAnimator](const QVector<Qt3DAnimation::QClipAnimator *> &animators) {
        for (auto animator : animators)
            updateAnimator(animator);
    };

    Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
    Q_ASSERT(data);

    updateAnimator(data->animator);
    updateAnimators(data->animators);
}

static void updatePlaybackRate(Q3DSSlide *slide, float rate)
{
    const auto updateAnimator = [rate](Qt3DAnimation::QClipAnimator *animator) {
        if (!animator)
            return;

        animator->clock()->setPlaybackRate(double(rate));
    };
    const auto updateAnimators = [&updateAnimator](const QVector<Qt3DAnimation::QClipAnimator *> &animators) {
        for (auto animator : animators)
            updateAnimator(animator);
    };

    Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
    Q_ASSERT(data);

    updateAnimator(data->animator);
    updateAnimators(data->animators);
}

Q3DSSlidePlayer::Q3DSSlidePlayer(Q3DSSceneManager *sceneManager,
                                 QObject *parent)
    : QObject(parent)
    , m_sceneManager(sceneManager)
    , m_animationManager(new Q3DSAnimationManager)
{
}

Q3DSSlidePlayer::~Q3DSSlidePlayer()
{
    if (!m_component)
        m_animationManager->clearPendingChanges();

    reset();
}

Q3DSSlideDeck *Q3DSSlidePlayer::slideDeck() const
{
    return m_data.slideDeck;
}

void Q3DSSlidePlayer::advanceFrame()
{
    m_animationManager->applyChanges();
}

void Q3DSSlidePlayer::sceneReady()
{
    Q3DSSlideDeck *slideDeck = m_data.slideDeck;
    if (!slideDeck)
        return;

    Q3DSSlide *currentSlide = slideDeck->currentSlide();
    Q_ASSERT(currentSlide);

    const bool viewerMode = (m_mode == PlayerMode::Viewer);
    if (viewerMode && (currentSlide->initialPlayState() == Q3DSSlide::Play))
        play();
    else
        pause();

    // In viewer-mode we need to go through all components players as well
    if (viewerMode) {
        static const auto notifyComponentPlayers = [](Q3DSSlide *slide) {
            if (!slide)
                return;

            const auto &objects = slide->objects();
            std::find_if(objects.constBegin(), objects.constEnd(), [](Q3DSGraphObject *obj) {
                if (obj->type() == Q3DSGraphObject::Component && obj->state() == Q3DSGraphObject::Enabled) {
                    Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
                    Q3DSSlide *compSlide = comp->masterSlide();
                    Q3DSSlidePlayer *player = compSlide->attached<Q3DSSlideAttached>()->slidePlayer;
                    player->sceneReady();
                }
                return false;
            });
        };

        Q3DSSlide *slide = m_data.slideDeck->masterSlide();
        notifyComponentPlayers(slide);
        slide = static_cast<Q3DSSlide *>(slide->firstChild());
        while (slide) {
            notifyComponentPlayers(slide);
            slide = static_cast<Q3DSSlide *>(slide->nextSibling());
        }
    }
}

float Q3DSSlidePlayer::duration() const
{
    if (m_data.state == PlayerState::Idle)
        return 0;

    return m_data.duration;
}

float Q3DSSlidePlayer::position() const
{
    return m_data.position;
}

void Q3DSSlidePlayer::setMode(Q3DSSlidePlayer::PlayerMode mode)
{
    m_mode = mode;
    if (!m_data.slideDeck)
        return;

    const auto notifyComponentPlayers = [mode](Q3DSSlide *slide) {
        if (!slide)
            return;

        const auto &objects = slide->objects();
        std::find_if(objects.constBegin(), objects.constEnd(), [mode](Q3DSGraphObject *obj) {
            if (obj->type() == Q3DSGraphObject::Component && obj->state() == Q3DSGraphObject::Enabled) {
                Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
                Q3DSSlide *compSlide = comp->masterSlide();
                Q3DSSlidePlayer *player = compSlide->attached<Q3DSSlideAttached>()->slidePlayer;
                player->setMode(mode);
            }
            return false;
        });
    };

    Q3DSSlide *slide = m_data.slideDeck->masterSlide();
    notifyComponentPlayers(slide);
    slide = static_cast<Q3DSSlide *>(slide->firstChild());
    while (slide) {
        notifyComponentPlayers(slide);
        slide = static_cast<Q3DSSlide *>(slide->nextSibling());
    }

    reload();
}

void Q3DSSlidePlayer::play()
{
    if (m_data.state == PlayerState::Playing)
        return;

    if (m_data.state == PlayerState::Idle) {
        qCWarning(lcSlidePlayer) << "Play called in Idle state (no content)";
        return;
    }

    Q3DSSlideDeck *slideDeck = m_data.slideDeck;
    Q_ASSERT(slideDeck);
    Q_ASSERT(!slideDeck->isEmpty());

    Q3DSSlide *currentSlide = slideDeck->currentSlide();
    if (!currentSlide) {
        qCWarning(lcSlidePlayer, "No slide selected!");
        return;
    }

    setInternalState(PlayerState::Playing);
}

void Q3DSSlidePlayer::stop()
{
    if (m_data.state == PlayerState::Idle) {
        qCWarning(lcSlidePlayer) << "Stop called in Idle state (no content)";
        return;
    }

    Q3DSSlideDeck *slideDeck = m_data.slideDeck;
    Q_ASSERT(slideDeck);
    Q_ASSERT(!slideDeck->isEmpty());

    Q3DSSlide *currentSlide = slideDeck->currentSlide();
    if (!currentSlide) {
        qCWarning(lcSlidePlayer, "No slide selected!");
        return;
    }

    setInternalState(PlayerState::Stopped);
    // NOTE: We force an update here to make sure we don't get stale updates from
    // from Qt3D, this way we can properly rollback the animatables to their initial values.
    updatePosition(currentSlide, 0.0f, duration());
    handleCurrentSlideChanged(currentSlide, currentSlide, true);
}

void Q3DSSlidePlayer::pause()
{
    if (m_data.state == PlayerState::Paused)
        return;

    if (m_data.state == PlayerState::Idle) {
        qCWarning(lcSlidePlayer) << "Pause called in Idle state (no content)";
        return;
    }

    setInternalState(PlayerState::Paused);
}

void Q3DSSlidePlayer::seek(float pos)
{
    if (qFuzzyCompare(m_data.position, pos))
        return;

    Q3DSSlideDeck *slideDeck = m_data.slideDeck;
    if (!slideDeck)
        return;

    Q3DSSlide *slide = slideDeck->currentSlide();
    if (!slide)
        return;

    updatePosition(slide, pos, duration());
}

void Q3DSSlidePlayer::setSlideDeck(Q3DSSlideDeck *slideDeck)
{
    if (m_data.slideDeck == slideDeck) {
        qCWarning(lcSlidePlayer, "Setting same slide deck (nop)");
        return;
    }

    const bool forceReset = (m_data.state != PlayerState::Idle || slideDeck == nullptr);
    if (forceReset)
        reset();

    if (slideDeck == nullptr || slideDeck->isEmpty()) {
        qCWarning(lcSlidePlayer, "Got an empty slide deck!");
        return;
    }

    m_data.slideDeck = slideDeck;
    m_data.slideDeck->bind(this);

    // The master slide should always have an entity, so if it hasn't one yet set it to
    // be the root entity.
    if (!slideDeck->currentSlide()->parent()->attached()->entity)
        slideDeck->currentSlide()->parent()->attached()->entity = m_sceneManager->getRootEntity();

    qCDebug(lcSlidePlayer, "Setting slide deck with %d slides", slideDeck->slideCount());

    // This will recurse down all the slides and components slides
    const auto prepareComponentsOnSlide = [this](Q3DSSlide *slide, Q3DSGraphObject *obj) {
        // Check if the owning slide already has a player for this component
        Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
        Q_ASSERT(comp->attached()->entity);

        Q3DSSlide *compMasterSlide = comp->masterSlide();
        Q3DSSlideAttached *compMasterData = compMasterSlide->attached<Q3DSSlideAttached>();
        Q_ASSERT(compMasterData);
        qCDebug(lcSlidePlayer, "Processing component \"%s\", on slide \"%s\"",
                qPrintable(comp->name()), qPrintable(getSlideName(slide)));
        if (!compMasterData->slidePlayer) {
            qCDebug(lcSlidePlayer, "No player found for Component \"%s\", adding one", qPrintable(comp->name()));
            compMasterData->slidePlayer = new Q3DSSlidePlayer(m_animationManager, m_sceneManager, comp, this);
            compMasterSlide->attached<Q3DSSlideAttached>()->entity = comp->attached()->entity;
            Q3DSSlide *s = static_cast<Q3DSSlide *>(compMasterSlide->firstChild());
            while (s) {
                if (!s->attached())
                    s->setAttached(new Q3DSSlideAttached);
                s->attached<Q3DSSlideAttached>()->entity = comp->attached()->entity;
                s = static_cast<Q3DSSlide *>(s->nextSibling());
            }

            // Create a slide deck for this component
            compMasterData->slidePlayer->setSlideDeck(new Q3DSSlideDeck(compMasterSlide, slide));
        }
    };

    const auto forAllComponentsOnSlide = [this, prepareComponentsOnSlide](Q3DSSlide *slide) {
        Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
        if (!data) {
            data = new Q3DSSlideAttached;
            data->entity = m_sceneManager->getRootEntity();
            slide->setAttached(data);
        }

        for (auto object : slide->objects()) {
            if (object->type() == Q3DSGraphObject::Component && object->state() == Q3DSGraphObject::Enabled)
                prepareComponentsOnSlide(slide, object);
        }
    };

    const auto forAllSlides = [&forAllComponentsOnSlide, this](Q3DSSlideDeck *slideDeck) {
        // Process the master slide first
        Q3DSSlide *masterSlide = slideDeck->masterSlide();
        masterSlide->attached<Q3DSSlideAttached>()->slidePlayer = this;
        forAllComponentsOnSlide(masterSlide);
        Q3DSSlide *slide = static_cast<Q3DSSlide *>(masterSlide->firstChild());
        while (slide) {
            slide->attached<Q3DSSlideAttached>()->slidePlayer = this;
            forAllComponentsOnSlide(slide);
            slide = static_cast<Q3DSSlide *>(slide->nextSibling());
        }
    };

    forAllSlides(slideDeck);

    setInternalState(PlayerState::Ready);
    Q_EMIT slideDeckChanged(m_data.slideDeck);
}

void Q3DSSlidePlayer::setPlaybackRate(float rate)
{
    Q3DSSlideDeck *slideDeck = m_data.slideDeck;
    if (!slideDeck)
        return;

    Q3DSSlide *slide = slideDeck->currentSlide();
    if (!slide)
        return;

    m_data.playbackRate = rate;
    updatePlaybackRate(slide, rate);
}

void Q3DSSlidePlayer::nextSlide()
{
    if (m_data.state == PlayerState::Idle)
        return;

    m_data.slideDeck->nextSlide();
    reload();
}

void Q3DSSlidePlayer::previousSlide()
{
    if (m_data.state == PlayerState::Idle)
        return;

    m_data.slideDeck->previousSlide();
    reload();
}

void Q3DSSlidePlayer::precedingSlide()
{
    if (m_data.state == PlayerState::Idle)
        return;

    m_data.slideDeck->precedingSlide();
    reload();
}

void Q3DSSlidePlayer::reload()
{
    setInternalState(PlayerState::Stopped);
    if (m_mode == PlayerMode::Viewer)
        setInternalState(getInitialSlideState(m_data.slideDeck->currentSlide()));
}

Q3DSSlidePlayer::Q3DSSlidePlayer(QSharedPointer<Q3DSAnimationManager> animationManager,
                                 Q3DSSceneManager *sceneManager,
                                 Q3DSComponentNode *component,
                                 Q3DSSlidePlayer *parent)
    : QObject(parent),
      m_sceneManager(sceneManager),
      m_component(component),
      m_animationManager(animationManager),
      m_mode(parent->mode()),
      m_type(PlayerType::ComponentSlide)
{
}

void Q3DSSlidePlayer::init()
{

}

void Q3DSSlidePlayer::reset()
{
    setInternalState(PlayerState::Idle);
}

void Q3DSSlidePlayer::setInternalState(Q3DSSlidePlayer::PlayerState state)
{
    m_data.pendingState = state;
    qCDebug(lcSlidePlayer, "Setting internal state from %d to %d", int(m_data.state), int(m_data.pendingState));

    Q3DSSlide *currentSlide = m_data.slideDeck->currentSlide();

    // The current slide is stored in the scene manager or in the component, depending
    // on which type of player we are.
    Q3DSSlide *previousSlide = (m_type == PlayerType::Slide) ? m_sceneManager->currentSlide()
                                                             : m_component->currentSlide();

    if (state == PlayerState::Idle) {
        handleCurrentSlideChanged(nullptr, previousSlide);
        if (m_data.slideDeck)
            delete m_data.slideDeck;
        m_data = Data();
        return;
    }

    const bool slideChanged = (previousSlide != currentSlide);
    if (slideChanged || (state == PlayerState::Ready))
        handleCurrentSlideChanged(currentSlide, previousSlide, true);

    if (state == PlayerState::Playing) {
        const bool restart = (m_mode == PlayerMode::Viewer)
                             && (currentSlide->playMode() == Q3DSSlide::Looping)
                             && (m_data.state != PlayerState::Paused);
        updateAnimators(currentSlide, true, restart, m_data.playbackRate);
    } else if (state == PlayerState::Stopped) {
        updateAnimators(currentSlide, false, false, m_data.playbackRate);
    } else if (state == PlayerState::Paused) {
        updateAnimators(currentSlide, false, false, m_data.playbackRate);
    }

    if (m_data.state != m_data.pendingState) {
        m_data.state = m_data.pendingState;
        Q_EMIT stateChanged(m_data.state);
    }
}

void Q3DSSlidePlayer::handleCurrentSlideChanged(Q3DSSlide *slide,
                                                Q3DSSlide *previousSlide,
                                                bool forceUpdate)
{
    const bool slideDidChange = (previousSlide != slide) || forceUpdate;
    const bool parentChanged = [previousSlide, slide, forceUpdate]() -> bool {
        if (forceUpdate)
            return true;

        auto parent = slide ? slide->parent() : nullptr;
        auto pparent = previousSlide ? previousSlide->parent() : nullptr;
        return (parent != pparent);
    }();

    qCDebug(lcSlidePlayer, "Handling current slide change: from slide \"%s\", to slide \"%s\"",
            qPrintable(getSlideName(previousSlide)), qPrintable(getSlideName(slide)));

    static const auto cleanUpComponentPlayers = [](Q3DSSlide *slide) {
        if (!slide)
            return;

        const auto &objects = slide->objects();
        std::find_if(objects.constBegin(), objects.constEnd(), [](Q3DSGraphObject *obj) {
            if (obj->type() == Q3DSGraphObject::Component && obj->state() == Q3DSGraphObject::Enabled) {
                Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
                Q3DSSlide *compSlide = comp->currentSlide();
                Q3DSSlideAttached *data = compSlide->attached<Q3DSSlideAttached>();
                data->slidePlayer->handleCurrentSlideChanged(nullptr, compSlide);
            }
            return false;
        });
    };
    if (previousSlide && slideDidChange) {
        cleanUpComponentPlayers(previousSlide);
        Q3DSSlideAttached *data = previousSlide->attached<Q3DSSlideAttached>();
        if (data && data->animator) {
            // TODO: We probably want to be a bit less brute.
            if (slide) Q_ASSERT(previousSlide->parent() == slide->parent());
            updateAnimators(previousSlide, false, false, 1.0f);
            detatchPositionCallback(previousSlide);
            m_animationManager->clearAnimations(previousSlide);
        }
        if (parentChanged)
            cleanUpComponentPlayers(static_cast<Q3DSSlide *>(previousSlide->parent()));

        setSlideTime(previousSlide, -1.0f);
    }

    if (slide && slideDidChange && isSlideVisible(slide)) {
        setSlideTime(slide, 0.0f);
        processPropertyChanges(slide);
        attatchPositionCallback(slide);
        m_animationManager->updateAnimations(slide, (m_mode == PlayerMode::Editor));

        Q3DSGraphObject *eventTarget = m_sceneManager->m_scene;
        if (m_type != PlayerType::Slide)
            eventTarget = m_component;

        if (previousSlide) {
            const QVariantList args {
                QVariant::fromValue(previousSlide),
                m_data.slideDeck->indexOfSlide(previousSlide->id())
            };
            m_sceneManager->queueEvent(Q3DSGraphObject::Event(eventTarget,
                                                              Q3DSGraphObjectEvents::slideExitEvent(),
                                                              args));
        }

        if (slide) {
            const QVariantList args {
                QVariant::fromValue(slide),
                m_data.slideDeck->indexOfSlide(slide->id())
            };
            m_sceneManager->queueEvent(Q3DSGraphObject::Event(eventTarget,
                                                              Q3DSGraphObjectEvents::slideEnterEvent(),
                                                              args));
        }

        // A bit crude, but whatever
        if (m_type == PlayerType::Slide)
            m_sceneManager->m_currentSlide = slide;
        else
            m_component->setCurrentSlide(slide);

        m_sceneManager->syncScene();

        qint32 startTime = 0;
        qint32 endTime = 0;
        Q3DSSlideUtils::getStartAndEndTime(slide, &startTime, &endTime);
        onDurationChanged(endTime - startTime);

        static const auto updateComponentSlides = [](Q3DSSlide *slide) {
            if (!slide)
                return;
            const auto &objects = slide->objects();
            std::find_if(objects.constBegin(), objects.constEnd(), [](Q3DSGraphObject *obj) {
                if (obj->type() == Q3DSGraphObject::Component && obj->state() == Q3DSGraphObject::Enabled) {
                    Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
                    Q3DSSlide *compSlide = comp->currentSlide();
                    Q3DSSlideAttached *data = compSlide->attached<Q3DSSlideAttached>();
                    data->slidePlayer->handleCurrentSlideChanged(compSlide, nullptr);
                    if (data->slidePlayer->m_mode == PlayerMode::Viewer)
                        data->slidePlayer->setInternalState(getInitialSlideState(compSlide));
                }
                return false;
            });
        };
        if (parentChanged)
            updateComponentSlides(static_cast<Q3DSSlide *>(slide->parent()));
        updateComponentSlides(slide);
    }

    if (previousSlide != slide)
        Q_EMIT slideChanged(slide);
}

static bool objectHasVisibilityTag(Q3DSGraphObject *object)
{
    Q_ASSERT(object);
    Q_ASSERT(object->attached());
    Q_ASSERT(object->isNode() || object->type() == Q3DSGraphObject::Effect);

    return (object->attached()->visibilityTag == Q3DSGraphObjectAttached::Visible);
}

void Q3DSSlidePlayer::setSlideTime(Q3DSSlide *slide, float time)
{
    if (!slide)
        return;

    bool parentVisible = true;
    // If this is a component player, then check if the component is visible.
    if (m_type == Q3DSSlidePlayer::PlayerType::ComponentSlide) {
        // Since we can be called from another slide, check if there's a pending visibility
        // update for us.
        const auto foundIt = m_sceneManager->m_pendingObjectVisibility.constFind(m_component);
        if (foundIt != m_sceneManager->m_pendingObjectVisibility.cend())
            parentVisible = foundIt.value();
        else
            parentVisible = (m_component->attached()->visibilityTag == Q3DSGraphObjectAttached::Visible);
    }

    // We force an update if we are at the beginning (0.0f) or the end (-1.0f)
    // to ensure the scenemanager has correct global values for visibility during
    // slide changes
    const bool forceUpdate = (qFuzzyCompare(time, 0.0f) || qFuzzyCompare(time, -1.0f));
    const auto updateObjects = [=](Q3DSSlide *s) {
        if (!s)
            return;

        for (Q3DSGraphObject *obj : s->objects())
            setObjectVisibility(obj, parentVisible, forceUpdate, time);
    };

    updateObjects(static_cast<Q3DSSlide *>(slide->parent()));
    updateObjects(slide);

    // This method is called for all slides, but we only want to update the
    // position for the associated slide player once
    if (m_data.slideDeck->currentSlide() != slide)
        return;

    sendPositionChanged(slide, time);
}

void Q3DSSlidePlayer::setObjectVisibility(Q3DSGraphObject *obj, bool parentVisible, bool forceUpdate, float time)
{
    if (obj->state() != Q3DSGraphObject::Enabled)
        return;

    const bool isEffect = (obj->type() == Q3DSGraphObject::Effect);
    if ((!obj->isNode() && !isEffect))
        return;

    bool nodeActive = false;
    if (obj->isNode()) {
        Q3DSNode *node = static_cast<Q3DSNode *>(obj);
        if (node && !node->attached())
            return;

        if (node) {
            nodeActive = node->flags().testFlag(Q3DSNode::Active);
            if (node->parent() && node->parent()->isNode())
                nodeActive &= node->parent()->attached<Q3DSNodeAttached>()->globalLogicalVisibility;
        }
    }

    const bool effectActive = (isEffect && static_cast<Q3DSEffectInstance *>(obj)->eyeballEnabled());
    const bool shouldBeVisible = parentVisible
            && time >= obj->startTime() && time <= obj->endTime()
            && (nodeActive || effectActive);

    if (forceUpdate || shouldBeVisible != objectHasVisibilityTag(obj))
        updateObjectVisibility(obj, shouldBeVisible);

    if (obj->type() == Q3DSGraphObject::Component) {
        Q3DSComponentNode *component = static_cast<Q3DSComponentNode *>(obj);
        Q3DSSlide *componentCurrentSlide = component->currentSlide();
        Q3DSSlidePlayer *player = componentCurrentSlide->attached<Q3DSSlideAttached>()->slidePlayer;
        // If the component is not playing it won't get time updates, so to ensure it correctly
        // updates its visibility we tell it to update with time from its last time update.
        if (player->state() != PlayerState::Idle && player->state() != PlayerState::Playing) {
            const float lastTime = player->position();
            player->setSlideTime(componentCurrentSlide, lastTime);
        }
    }
}

void Q3DSSlidePlayer::sendPositionChanged(Q3DSSlide *slide, float pos)
{
    const float oldPosition = m_data.position;
    m_data.position = pos;
    if (!qFuzzyCompare(oldPosition, pos))
        Q_EMIT positionChanged(pos);

    // Check if we are close enough to the end of playback that we
    // are unlikely to get another position change
    float timeLeft = 0.0f;
    if (m_data.playbackRate < 0.0f)
        timeLeft = m_data.position;
    else
        timeLeft = qMax(0.0f, duration() - m_data.position);

    const bool onEOS = m_data.state == PlayerState::Playing
            && m_data.pendingState == PlayerState::Playing
            && timeLeft < 0.1f;

    if (onEOS)
        onSlideFinished(slide);
}

void Q3DSSlidePlayer::updateObjectVisibility(Q3DSGraphObject *obj, bool shouldBeVisible)
{
    Q_ASSERT(obj->isNode() || obj->type() == Q3DSGraphObject::Effect);
    auto foundIt = m_sceneManager->m_pendingObjectVisibility.find(obj);
    const bool insertValue = (foundIt == m_sceneManager->m_pendingObjectVisibility.end());
    const bool updateValue = (!insertValue && foundIt.value() != shouldBeVisible);

    if (insertValue || updateValue) {
        qCDebug(lcSlidePlayer, "Scheduling object \"%s\" to be %s", obj->id().constData(), shouldBeVisible ? "shown" : "hidden");
        if (updateValue)
            *foundIt = shouldBeVisible;
        else if (insertValue)
            m_sceneManager->m_pendingObjectVisibility.insert(obj, shouldBeVisible);
    }
}

bool Q3DSSlidePlayer::isSlideVisible(Q3DSSlide *slide)
{
    qCDebug(lcSlidePlayer, "Checking visibility for \"%s\"", qPrintable(getSlideName(slide)));
    // If we the slide is not null, we assume it's visible until proven otherwise.
    bool visible = (slide != nullptr);
    if (slide) {
        Q3DSSlidePlayer *player = slide->attached<Q3DSSlideAttached>()->slidePlayer;
        Q3DSSlideDeck *slideDeck = player->slideDeck();
        const bool isMasterSlide = (slideDeck->masterSlide() == slide);
        const bool isCurrentSlide = (slideDeck->currentSlide() == slide);
        if (isCurrentSlide || isMasterSlide) {
            Q3DSSlide *parentSlide = slideDeck->parentSlide();
            if (parentSlide) {
                // We're a component and current, continue up the ladder...
                visible = isSlideVisible(parentSlide);
            } else {
                visible = true;
            }
        } else {
            visible = false;
        }
    }

    qCDebug(lcSlidePlayer, "The slides's (\"%s\") visibility is %d", qPrintable(getSlideName(slide)), visible);

    return  visible;
}

void Q3DSSlidePlayer::processPropertyChanges(Q3DSSlide *currentSlide)
{
    Q_ASSERT(currentSlide->attached());

    // Find properties on targets that has dynamic properties.
    // TODO: Find a better solution (e.g., there can be duplicate updates for e.g., xyz here).
    QHash<Q3DSGraphObject *, Q3DSPropertyChangeList> dynamicPropertyChanges;
    const auto &tracks = currentSlide->animations();
    std::find_if(tracks.cbegin(), tracks.cend(), [&dynamicPropertyChanges](const Q3DSAnimationTrack &track) {
        if (track.isDynamic() && track.target()->state() == Q3DSGraphObject::Enabled) {
            const auto foundIt = dynamicPropertyChanges.constFind(track.target());
            Q3DSPropertyChangeList changeList;
            if (foundIt != dynamicPropertyChanges.constEnd())
                changeList = *foundIt;
            const QString property = track.property().split('.')[0];
            const auto value = track.target()->property(property.toLatin1());
            changeList.append(Q3DSPropertyChange::fromVariant(property, value));
            dynamicPropertyChanges[track.target()] = changeList;
        }
        return false;
    });

    // Rollback master slide properties
    if (currentSlide->parent()) {
        Q3DSSlide *parent = static_cast<Q3DSSlide *>(currentSlide->parent());
        const auto &objects = parent->objects();
        std::find_if(objects.constBegin(), objects.constEnd(), [](Q3DSGraphObject *object){
            if (object->state() != Q3DSGraphObject::Enabled)
                return false;

            if (!object->isNode() && object->type() != Q3DSGraphObject::Effect)
                return false;

            const Q3DSPropertyChangeList &masterRollbackList = object->isNode() ? static_cast<Q3DSNode *>(object)->masterRollbackList()
                                                                                : static_cast<Q3DSEffectInstance *>(object)->masterRollbackList();
            if (masterRollbackList.isEmpty())
                return false;

            object->applyPropertyChanges(masterRollbackList);
            return false;
        });
    }

    // Set the property values
    const auto &propertyChanges = currentSlide->propertyChanges();
    for (auto it = propertyChanges.cbegin(); it != propertyChanges.cend(); ++it)
        it.key()->applyPropertyChanges(*it.value());

    // Now update the properties from dynamic property values
    for (auto it = dynamicPropertyChanges.cbegin(), ite = dynamicPropertyChanges.cend(); it != ite; ++it)
        it.key()->applyPropertyChanges(it.value());
}

void Q3DSSlidePlayer::onDurationChanged(float duration)
{
    if (qFuzzyCompare(duration, m_data.duration))
        return;

    m_data.duration = duration;
    Q_EMIT durationChanged(duration);
}

void Q3DSSlidePlayer::onSlideFinished(Q3DSSlide *slide)
{
    Q_ASSERT(m_data.slideDeck);
    Q_ASSERT(m_data.state == PlayerState::Playing);
    Q_ASSERT(slide == m_data.slideDeck->currentSlide());

    // We don't change slides automatically in Editor mode
    if (m_mode == PlayerMode::Editor) {
        setInternalState(PlayerState::Stopped);
        return;
    }

    // Get the slide's play mode
    const auto playMode = slide->playMode();

    PlayerState state = PlayerState::Stopped;

    switch (playMode) {
    case Q3DSSlide::Looping:
        state = PlayerState::Playing;
        break;
    case Q3DSSlide::PlayThroughTo:
    {
        if (slide->playThrough() == Q3DSSlide::Next) {
            m_data.slideDeck->nextSlide();
        } else if (slide->playThrough() == Q3DSSlide::Previous) {
            m_data.slideDeck->previousSlide();
        } else if (slide->playThrough() == Q3DSSlide::Value) {
            const auto value = slide->playThroughValue();
            if (value.type() == QVariant::Int) { // Assume this is a fixed index value
                m_data.slideDeck->setCurrentIndex(value.toInt());
            } else if (value.type() == QVariant::String) { // Reference to a slide?
                const QString &slideId = value.toString().mid(1);
                const int index = m_data.slideDeck->indexOfSlide(slideId.toLocal8Bit());
                if (!m_data.slideDeck->setCurrentIndex(index))
                    qCWarning(lcSlidePlayer, "Unable to make slide \"%s\" current", qPrintable(slideId));
            }
        }
        // Since we're jumping to a new slide, make sure we take the initial play-state into account.
        state = getInitialSlideState(m_data.slideDeck->currentSlide());
    }
        break;
    case Q3DSSlide::Ping:
    {
        const float rate = qAbs(m_data.playbackRate);
        if (++m_data.loopCount == 1) {
            m_data.playbackRate = -rate;
            state = PlayerState::Playing;
        } else {
            m_data.loopCount = 0;
            m_data.playbackRate = rate;
            state = PlayerState::Stopped;
        }
    }
        break;
    case Q3DSSlide::PingPong:
    {
        const float rate = qAbs(m_data.playbackRate);
        m_data.playbackRate = (m_data.playbackRate < 0.0f) ? rate : -rate;
        state = PlayerState::Playing;
    }
        break;
    case Q3DSSlide::StopAtEnd:
        state = PlayerState::Stopped;
        break;
    }

    setInternalState(state);
}

void Q3DSSlidePlayer::evaluateDynamicObjectVisibility(Q3DSGraphObject *obj)
{
    bool parentVisible = true;
    // If this is a component player, then check if the component is visible.
    if (m_type == Q3DSSlidePlayer::PlayerType::ComponentSlide) {
        // Since we can be called from another slide, check if there's a pending visibility
        // update for us.
        const auto foundIt = m_sceneManager->m_pendingObjectVisibility.constFind(m_component);
        if (foundIt != m_sceneManager->m_pendingObjectVisibility.cend())
            parentVisible = foundIt.value();
        else
            parentVisible = (m_component->attached()->visibilityTag == Q3DSGraphObjectAttached::Visible);
    }

    float time = -1.0f;
    Q3DSSlide *slide = m_data.slideDeck->currentSlide();
    if (slide && (slide->objects().contains(obj) || static_cast<Q3DSSlide *>(slide->parent())->objects().contains(obj)))
        time = position();

    setObjectVisibility(obj, parentVisible, true, time);
}

void Q3DSSlidePlayer::objectAboutToBeAddedToScene(Q3DSGraphObject *obj)
{
    evaluateDynamicObjectVisibility(obj);
}

void Q3DSSlidePlayer::objectAboutToBeRemovedFromScene(Q3DSGraphObject *obj)
{
    m_animationManager->objectAboutToBeRemovedFromScene(obj);
}

void Q3DSSlidePlayer::objectAddedToSlide(Q3DSGraphObject *obj, Q3DSSlide *slide)
{
    qDebug(lcSlidePlayer) << "Dyn.added object" << obj->id() << "to slide" << slide->id();
    evaluateDynamicObjectVisibility(obj);
}

void Q3DSSlidePlayer::objectRemovedFromSlide(Q3DSGraphObject *obj, Q3DSSlide *slide)
{
    qDebug(lcSlidePlayer) << "Dyn.removed object" << obj->id() << "from slide" << slide->id();
    evaluateDynamicObjectVisibility(obj);
}

QT_END_NAMESPACE
