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

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QLayer>

QT_BEGIN_NAMESPACE

// NOTE: We assume that the properties for the layer has been applied before this is called
// if it hasn't it will either return the previous value or the odl value
void Q3DSSlideUtils::getStartAndEndTime(Q3DSSlide *slide, qint32 *startTime, qint32 *endTime)
{
    Q_ASSERT(startTime != nullptr || endTime != nullptr);

    qint32 eTime = -1;

    // First go through the slide's parent (master slide) and look for layers which have
    // endtime explicitly set by this slide's property changes
    if (Q3DSSlide *p = static_cast<Q3DSSlide *>(slide->parent())) {
        for (auto *obj : p->objects()) {
            if (obj->type() != Q3DSGraphObject::Layer)
                continue;

            const QHash<Q3DSGraphObject *, Q3DSPropertyChangeList *> props =
                slide->propertyChanges();
            const QHash<Q3DSGraphObject *, Q3DSPropertyChangeList *>::const_iterator it =
                props.find(obj);
            if (it == props.constEnd())
                continue;
            if ((*it)->keys().contains(QStringLiteral("endtime")) && obj->endTime() > eTime)
                eTime = obj->endTime();
        }
    }

    // Now look for the endtime from the slides explicit layer objects
    for (const auto obj : slide->objects()) {
        if (obj->type() != Q3DSGraphObject::Layer)
            continue;

        if (obj->endTime() > eTime)
            eTime = obj->endTime();
    }

    if (startTime)
        *startTime = slide->startTime();
    if (endTime)
        *endTime = eTime != -1 ? eTime : slide->endTime();
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

static void updatePosition(Q3DSSlide *slide, float pos, float duration)
{
    const auto updateAnimator = [pos](Qt3DAnimation::QClipAnimator *animator, float duration) {
        if (!animator)
            return;

        const float nt = qBound(0.0f, pos / duration, 1.0f);
        // NOTE!!!: This is a bit funky, but it means we can avoid tracking the normalized values in the
        // frontend node. This of course assumes that the limit in the fuzzy compare doesn't change!
        animator->setNormalizedTime(qFuzzyCompare(nt, animator->normalizedTime()) ? qAbs(nt - (1.0f / 100000.0f)) : nt);
    };
    const auto updateAnimators = [&updateAnimator](const QVector<Qt3DAnimation::QClipAnimator *> &animators) {
        for (auto animator : animators) {
            const float duration = animator->clip()->duration() * 1000.0f;
            updateAnimator(animator, duration);
        }
    };

    const auto updateAllComponentPlayers = [pos](Q3DSSlide *slide) {
        for (auto obj : slide->objects()) {
            if (obj->type() == Q3DSGraphObject::Component) {
                Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
                const float slidePos = pos - obj->startTime();
                Q3DSSlidePlayer *player = comp->masterSlide()->attached<Q3DSSlideAttached>()->slidePlayer;
                player->seek(slidePos);
            }
        }
    };

    Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
    Q_ASSERT(data);

    updateAllComponentPlayers(slide);
    updateAnimator(data->animator, duration);
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
            // NOTE: The running value might not have been updated in the frontend node yet,
            // so force update this one as well!
            if (animator->isRunning())
                animator->setRunning(false);
        }
        animator->clock()->setPlaybackRate(double(rate));
        animator->setRunning(running);
    };
    const auto updateAnimators = [&updateAnimator](const QVector<Qt3DAnimation::QClipAnimator *> &animators) {
        for (auto animator : animators)
            updateAnimator(animator);
    };

    const auto updateAllComponentPlayers = [running](Q3DSSlide *slide) {
        for (auto obj : slide->objects()) {
            if (obj->type() == Q3DSGraphObject::Component) {
                Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
                Q3DSSlide *compSlide = comp->currentSlide();
                Q3DSSlidePlayer *player = comp->masterSlide()->attached<Q3DSSlideAttached>()->slidePlayer;
                if (running && compSlide->initialPlayState() == Q3DSSlide::Play)
                    player->play();
                else
                    player->stop();
            }
        }
    };

    Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
    Q_ASSERT(data);

    updateAllComponentPlayers(slide);
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

    const auto updateAllComponentPlayers = [rate](Q3DSSlide *slide) {
        for (auto obj : slide->objects()) {
            if (obj->type() == Q3DSGraphObject::Component) {
                Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
                Q3DSSlidePlayer *player = comp->masterSlide()->attached<Q3DSSlideAttached>()->slidePlayer;
                player->setPlaybackRate(rate);
            }
        }
    };

    Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
    Q_ASSERT(data);

    updateAllComponentPlayers(slide);
    updateAnimator(data->animator);
    updateAnimators(data->animators);
}

Q3DSSlidePlayer::Q3DSSlidePlayer(Q3DSSceneManager *sceneManager,
                                 QObject *parent)
    : QObject(parent)
    , m_sceneManager(sceneManager)
    , m_animationManager(new Q3DSAnimationManager(this))
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
    if (m_data.state != PlayerState::Playing)
        return;

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

        Q_ASSERT(compMasterData->slidePlayer->state() == PlayerState::Ready);
    };

    const auto forAllComponentsOnSlide = [this, prepareComponentsOnSlide](Q3DSSlide *slide) {
        Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
        if (!data) {
            data = new Q3DSSlideAttached;
            data->entity = m_sceneManager->getRootEntity();
            slide->setAttached(data);
        }

        for (auto object : slide->objects()) {
            if (object->type() == Q3DSGraphObject::Component)
                prepareComponentsOnSlide(slide, object);
        }
    };

    const auto forAllSlides = [&forAllComponentsOnSlide, this](Q3DSSlideDeck *slideDeck) {
        // Process the master slide first
        Q3DSSlide *masterSlide = slideDeck->masterSlide();
        masterSlide->attached<Q3DSSlideAttached>()->slidePlayer = this;
        forAllComponentsOnSlide(masterSlide);
        Q3DSSlide *currentSlide = slideDeck->currentSlide();
        Q3DSSlide *slide = static_cast<Q3DSSlide *>(masterSlide->firstChild());
        while (slide) {
            slide->attached<Q3DSSlideAttached>()->slidePlayer = this;
            forAllComponentsOnSlide(slide);
            if (slide != currentSlide)
                setSlideTime(slide, -1.0f);
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
                                 QObject *parent)
    : QObject(parent),
      m_sceneManager(sceneManager),
      m_component(component),
      m_animationManager(animationManager),
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

    if (previousSlide && slideDidChange) {
        if (parentChanged)
            setSlideTime(static_cast<Q3DSSlide *>(previousSlide->parent()), -1.0f);
        setSlideTime(previousSlide, -1.0f);
        Q3DSSlideAttached *data = previousSlide->attached<Q3DSSlideAttached>();
        if (data && data->animator) {
            // TODO: We probably want to be a bit less brute.
            if (slide) Q_ASSERT(previousSlide->parent() == slide->parent());
            updateAnimators(previousSlide, false, false, 1.0f);
            m_animationManager->clearAnimations(previousSlide);
        }
    }

    if (slide && slideDidChange && isSlideVisible(slide)) {
        m_sceneManager->handleSlideChange(previousSlide, slide);
        m_animationManager->updateAnimations(slide, (m_mode == PlayerMode::Editor));

        if (parentChanged)
            setSlideTime(static_cast<Q3DSSlide *>(slide->parent()), 0.0f);
        setSlideTime(slide, 0.0f);

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
            m_sceneManager->setCurrentSlide(slide, true);
        else
            m_component->setCurrentSlide(slide);

        m_sceneManager->updateSubTree(m_sceneManager->m_scene);

        qint32 startTime = 0;
        qint32 endTime = 0;
        Q3DSSlideUtils::getStartAndEndTime(slide, &startTime, &endTime);
        onDurationChanged(endTime - startTime);
    }

    if (previousSlide != slide)
        Q_EMIT slideChanged(slide);
}

static bool nodeHasVisibilityTag(Q3DSNode *node)
{
    Q_ASSERT(node);
    Q_ASSERT(node->attached());

    auto nodeAttached = static_cast<Q3DSNodeAttached *>(node->attached());
    auto entity = nodeAttached->entity;
    if (!entity)
        return false;

    auto layerAttached = static_cast<Q3DSLayerAttached *>(nodeAttached->layer3DS->attached());
    if (entity->components().contains(layerAttached->opaqueTag) || entity->components().contains(layerAttached->transparentTag))
        return true;

    return false;
}

void Q3DSSlidePlayer::setSlideTime(Q3DSSlide *slide, float time, bool parentVisible)
{
    // We force an update if we are at the beginning (0.0f) or the end (-1.0f)
    // to ensure the scenemanager has correct global values for visibility during
    // slide changes
    const bool forceUpdate = parentVisible &&
        (qFuzzyCompare(time, 0.0f) || qFuzzyCompare(time, -1.0f));
    for (Q3DSGraphObject *obj : slide->objects()) {
        if (!obj->isNode() || obj->type() == Q3DSGraphObject::Camera || obj->type() == Q3DSGraphObject::Layer)
            continue;

        Q3DSNode *node = static_cast<Q3DSNode *>(obj);
        if (!node->attached())
            continue;

        const bool shouldBeVisible = parentVisible
            && time >= obj->startTime() && time <= obj->endTime()
            && node->flags().testFlag(Q3DSNode::Active)
            && static_cast<Q3DSNodeAttached *>(node->attached())->globalVisibility;

        if (obj->type() == Q3DSGraphObject::Component) {
            Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
            Q3DSSlide *compMasterSlide = comp->masterSlide();
            Q_ASSERT(compMasterSlide);
            Q3DSSlidePlayer *compPlayer = compMasterSlide->attached<Q3DSSlideAttached>()->slidePlayer;
            Q_ASSERT(compPlayer);

            const float slideTime = time - obj->startTime();
            compPlayer->setSlideTime(compMasterSlide, slideTime, shouldBeVisible);

            if (!shouldBeVisible) {
                Q3DSGraphObject *n = compMasterSlide->firstChild();
                while (n) {
                    compPlayer->setSlideTime(static_cast<Q3DSSlide *>(n), slideTime, shouldBeVisible);
                    n = n->nextSibling();
                }
            } else {
                compPlayer->setSlideTime(comp->currentSlide(), slideTime);
            }
        }

        if (forceUpdate || shouldBeVisible != nodeHasVisibilityTag(node))
            updateNodeVisibility(node, shouldBeVisible);
    }

    // This method is called for all slides, but we only want to update the
    // position for the associated slide player once
    if (m_data.slideDeck->currentSlide() != slide)
        return;

    sendPositionChanged(slide, time);
}

void Q3DSSlidePlayer::sendPositionChanged(Q3DSSlide *slide, float pos)
{
    const float oldPosition = m_data.position;
    m_data.position = pos;
    if (!qFuzzyCompare(oldPosition, pos))
        Q_EMIT positionChanged(pos);

    const bool onEOS = (m_data.state == PlayerState::Playing)
            && ((m_data.position == 0.0f && m_data.playbackRate < 0.0f)
                || (m_data.position == duration() && m_data.playbackRate > 0.0f));

    if (onEOS)
        onSlideFinished(slide);
}

void Q3DSSlidePlayer::updateNodeVisibility(Q3DSNode *node, bool shouldBeVisible)
{
    if (shouldBeVisible) {
        qCDebug(lcSlidePlayer, "Scheduling node \"%s\" to be shown", node->id().constData());
        m_sceneManager->m_pendingNodeShow.insert(node);
    } else {
        qCDebug(lcSlidePlayer, "Scheduling node \"%s\" to be hidden", node->id().constData());
        m_sceneManager->m_pendingNodeHide.insert(node);
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
        if (slideDeck->currentSlide() == slide) {
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

void Q3DSSlidePlayer::objectAboutToBeRemovedFromScene(Q3DSGraphObject *obj)
{
    m_animationManager->objectAboutToBeRemovedFromScene(obj);
}

QT_END_NAMESPACE
