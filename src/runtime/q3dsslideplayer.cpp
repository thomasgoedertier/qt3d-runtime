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

#include <Qt3DAnimation/qclipanimator.h>
#include <Qt3DAnimation/qclock.h>
#include <Qt3DAnimation/qanimationclip.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcSlidePlayer)
Q_LOGGING_CATEGORY(lcSlidePlayer, "q3ds.slideplayer")

static QString getSlideName(Q3DSSlide *slide)
{
    return slide ? slide->name() : QStringLiteral("nullptr");
}

static Q3DSSlidePlayer::PlayerState getInitialSlideState(Q3DSSlide *slide)
{
    return (slide->initialPlayState() == Q3DSSlide::Play) ? Q3DSSlidePlayer::PlayerState::Playing
                                                          : Q3DSSlidePlayer::PlayerState::Paused;

}

static void updatePosition(Q3DSSlide *slide, float pos)
{
    const auto updateAnimator = [pos](Qt3DAnimation::QClipAnimator *animator) {
        if (!animator)
            return;

        animator->setNormalizedTime(pos);
    };
    const auto updateAnimators = [&updateAnimator](const QVector<Qt3DAnimation::QClipAnimator *> &animators) {
        for (auto animator : animators)
            updateAnimator(animator);
    };

    const auto updateAllComponentPlayers = [pos](Q3DSSlide *slide) {
        for (auto obj : slide->objects()) {
            if (obj->type() == Q3DSGraphObject::Component) {
                Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
                Q3DSSlidePlayer *player = comp->masterSlide()->attached<Q3DSSlideAttached>()->slidePlayer;
                player->seek(pos);
            }
        }
    };

    Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
    Q_ASSERT(data);

    updateAllComponentPlayers(slide);
    updateAnimator(data->animator);
    updateAnimators(data->animators);
};

static void updateAnimators(Q3DSSlide *slide, bool running, bool restart, float rate)
{
    const auto updateAnimator = [running, restart, rate](Qt3DAnimation::QClipAnimator *animator) {
        if (!animator)
            return;

        if (restart)
            animator->setNormalizedTime(0.0f);
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

Q3DSSlidePlayer::Q3DSSlidePlayer(Q3DSAnimationManager *animationManager,
                                 Q3DSSceneManager *sceneManager,
                                 QObject *parent)
    : QObject(parent)
    , m_sceneManager(sceneManager)
    , m_animationManager(animationManager)
{
    Q_ASSERT(animationManager);
}

Q3DSSlidePlayer::~Q3DSSlidePlayer()
{
    reset();
}

Q3DSSlideDeck *Q3DSSlidePlayer::slideDeck() const
{
    return m_data.slideDeck;
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
    if (m_data.state != PlayerState::Playing
            && m_data.state != PlayerState::Paused
            && m_data.state != PlayerState::Stopped)
        return;

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
    updatePosition(currentSlide, 0.0f);
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
    if ((pos > 1.0f) || (pos < 0.0f)) {
        qCWarning(lcSlidePlayer) << "Seek position must be between 0 and 1!";
        return;
    }

    m_data.position = pos;

    Q3DSSlideDeck *slideDeck = m_data.slideDeck;
    if (!slideDeck)
        return;

    Q3DSSlide *slide = slideDeck->currentSlide();
    if (!slide)
        return;

    updatePosition(slide, pos);
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

    Q3DSSlideAttached *masterSlideData = slideDeck->m_masterSlide->attached<Q3DSSlideAttached>();
    if (!masterSlideData->slidePlayer)
        masterSlideData->slidePlayer = this;

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
        Q3DSSlidePlayer *player = compMasterData->slidePlayer;
        qCDebug(lcSlidePlayer, "Processing component \"%s\", on slide \"%s\"",
                qPrintable(comp->name()), qPrintable(getSlideName(slide)));
        if (!player) {
            qCDebug(lcSlidePlayer, "No player found for Component \"%s\", adding one", qPrintable(comp->name()));
            // No player found, create on.
            player = aquireComponentPlayer(comp, slide);
            compMasterData->slidePlayer = player;

            compMasterSlide->attached<Q3DSSlideAttached>()->entity = comp->attached()->entity;
            Q3DSSlide *s = static_cast<Q3DSSlide *>(compMasterSlide->firstChild());
            while (s) {
                if (!s->attached())
                    s->setAttached(new Q3DSSlideAttached);
                s->attached<Q3DSSlideAttached>()->entity = comp->attached()->entity;
                s = static_cast<Q3DSSlide *>(s->nextSibling());
            }

            // Create a slide deck for this component
            player->setSlideDeck(new Q3DSSlideDeck(compMasterSlide));
        }

        Q_ASSERT(player->state() == PlayerState::Ready);
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
        forAllComponentsOnSlide(slideDeck->m_masterSlide);
        Q3DSSlide *currentSlide = slideDeck->currentSlide();
        Q3DSSlide *slide = static_cast<Q3DSSlide *>(slideDeck->m_masterSlide->firstChild());
        while (slide) {
            if (slide != currentSlide)
                updateSlideVisibility(slide, false);
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

void Q3DSSlidePlayer::reload()
{
    setInternalState(PlayerState::Stopped);
    if (m_mode == PlayerMode::Viewer)
        setInternalState(getInitialSlideState(m_data.slideDeck->currentSlide()));
}

Q3DSSlidePlayer::Q3DSSlidePlayer(Q3DSAnimationManager *animationManager,
                                 Q3DSSceneManager *sceneManager,
                                 Q3DSComponentNode *component,
                                 Q3DSSlide *parentSlide,
                                 QObject *parent)
    : QObject(parent),
      m_sceneManager(sceneManager),
      m_component(component),
      m_parent(parentSlide),
      m_animationManager(animationManager),
      m_type(PlayerType::ComponentSlide)
{
}

Q3DSSlidePlayer *Q3DSSlidePlayer::aquireComponentPlayer(Q3DSComponentNode *component, Q3DSSlide *parent)
{
    return new Q3DSSlidePlayer(m_animationManager, m_sceneManager, component, parent, this);
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
    Q3DSSlide *currentSlide = m_data.slideDeck->currentSlide();

    qCDebug(lcSlidePlayer, "Setting internal state from %d to %d", int(m_data.state), int(state));

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
                             && (currentSlide->playMode() == Q3DSSlide::Looping);
        updateAnimators(currentSlide, true, restart, m_data.playbackRate);
    } else if (state == PlayerState::Stopped) {
        updateAnimators(currentSlide, false, false, m_data.playbackRate);
    } else if (state == PlayerState::Paused) {
        updateAnimators(currentSlide, false, false, m_data.playbackRate);
    }

    if (m_data.state != state) {
        m_data.state = state;
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

    // Disconnect monitors from the old slide
    if (previousSlide && slideDidChange) {
        if (parentChanged)
            updateSlideVisibility(static_cast<Q3DSSlide *>(previousSlide->parent()), false);
        updateSlideVisibility(previousSlide, false);
        Q3DSSlideAttached *data = previousSlide->attached<Q3DSSlideAttached>();
        if (data && data->animator) {
            Qt3DAnimation::QClipAnimator *animator = data->animator;
            // TODO: We probably want to be a bit less brute.
            if (slide) Q_ASSERT(previousSlide->parent() == slide->parent());
            animator->clearPropertyTrackings();
            animator->disconnect();
            updateAnimators(previousSlide, false, true, 1.0f);
            m_animationManager->clearAnimations(previousSlide);
        }
    }

    // Connect to monitors to the new slide
    if (slide && slideDidChange && isSlideVisible(slide)) {
        m_sceneManager->handleSlideChange(previousSlide, slide);
        m_animationManager->updateAnimations(slide);

        if (parentChanged)
            updateSlideVisibility(static_cast<Q3DSSlide *>(slide->parent()), true);
        updateSlideVisibility(slide, true);

        // A bit crude, but whatever
        if (m_type == PlayerType::Slide)
            m_sceneManager->setCurrentSlide(slide, true);
        else
            m_component->setCurrentSlide(slide);

        m_sceneManager->updateSubTree(m_sceneManager->m_scene);

        Q3DSSlideAttached *data = static_cast<Q3DSSlideAttached *>(slide->attached());
        if (data->animator) {
            Qt3DAnimation::QClipAnimator *animator = data->animator;
            QObject::connect(animator, &Qt3DAnimation::QClipAnimator::normalizedTimeChanged, [this, slide](float t) {
                const float oldPosition = m_data.position;
                m_data.position = t;
                if (!qFuzzyCompare(oldPosition, t))
                    Q_EMIT positionChanged(t);

                if ((m_data.position == 0.0f && m_data.playbackRate < 0.0f) || (m_data.position == 1.0f && m_data.playbackRate > 0.0f))
                    QMetaObject::invokeMethod(this, "onSlideFinished", Qt::QueuedConnection, Q_ARG(void *, slide));
            });
            onDurationChanged(slide->endTime() - slide->startTime());

            // TODO: Workaround the fact that Qt 3D doesn't send the final value when there's a negative
            // playback rate (so we track all values regardless of the mode).
            const auto trackingMode = (m_mode == PlayerMode::Editor) ? Qt3DCore::QNode::TrackAllValues
                                                                     : Qt3DCore::QNode::TrackAllValues;
            animator->setPropertyTracking(QStringLiteral("normalizedTime"), trackingMode);
        }
    }

    if (previousSlide != slide)
        Q_EMIT slideChanged(slide);
}

void Q3DSSlidePlayer::updateSlideVisibility(Q3DSSlide *slide, bool visible)
{
    for (Q3DSGraphObject *obj : slide->objects()) {
        if (obj->type() == Q3DSGraphObject::Component) {
            Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
            Q3DSSlide *compMasterSlide = comp->masterSlide();
            Q_ASSERT(compMasterSlide);
            updateSlideVisibility(compMasterSlide, visible);

            if (!visible) {
                Q3DSGraphObject *n = compMasterSlide->firstChild();
                while (n) {
                    updateSlideVisibility(static_cast<Q3DSSlide *>(n), visible);
                    n = n->nextSibling();
                }
            } else {
                updateSlideVisibility(comp->currentSlide(), visible);
            }
        }

        if (obj->isNode() && obj->type() != Q3DSGraphObject::Camera) {
            Q3DSNode *node = static_cast<Q3DSNode *>(obj);
            Q3DSNodeAttached *ndata = static_cast<Q3DSNodeAttached *>(node->attached());
            if (ndata) {
                if (visible && ndata->globalVisibility && node->flags().testFlag(Q3DSNode::Active)) {
                    qCDebug(lcSlidePlayer, "Scheduling node \"%s\" to be shown", node->id().constData());
                    m_sceneManager->m_pendingNodeShow.insert(node);
                } else {
                    qCDebug(lcSlidePlayer, "Scheduling node \"%s\" to be hidden", node->id().constData());
                    m_sceneManager->m_pendingNodeHide.insert(node);
                }
            }
        }
    }
}

bool Q3DSSlidePlayer::isSlideVisible(Q3DSSlide *slide)
{
    qCDebug(lcSlidePlayer, "Checking visibility for \"%s\"", qPrintable(getSlideName(slide)));
    // If we the slide is not null, we assume it's visible until proven otherwise.
    bool visible = (slide != nullptr);
    if (slide && slide->parent()) {
        // The slide has a parent, check if we're current.
        Q3DSSlide *master = static_cast<Q3DSSlide *>(slide->parent());
        Q_ASSERT(master->attached());
        Q3DSSlidePlayer *player = master->attached<Q3DSSlideAttached>()->slidePlayer;
        Q3DSSlideDeck *slideDeck = player->slideDeck();
        if (slideDeck->currentSlide() == slide) {
            if (player->m_type == PlayerType::ComponentSlide) {
                // We're a component and current, continue up the ladder...
                Q3DSSlide *parent = player->m_parent;
                Q_ASSERT(parent);
                visible = isSlideVisible(parent);
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

void Q3DSSlidePlayer::onSlideFinished(void *slide)
{
    Q_ASSERT(m_data.slideDeck);

    // The call to onSlideFinished is queued to avoid re-entrance issues, so make sure
    // we're still operating and that the slide that finish is still current!
    if (m_data.state != PlayerState::Playing)
        return;

    Q3DSSlide *currentSlide = m_data.slideDeck->currentSlide();
    if (currentSlide != slide) {
        qCDebug(lcSlidePlayer, "onSlideFinished called for \"%s\", but slide has already changed to \"%s!",
                qPrintable(getSlideName(static_cast<Q3DSSlide *>(slide))), qPrintable(getSlideName(currentSlide)));
        return;
    }


    // We don't change slides automatically in Editor mode
    if (m_mode == PlayerMode::Editor) {
        setInternalState(PlayerState::Stopped);
        return;
    }

    // Get the slide's play mode
    const auto playMode = currentSlide->playMode();

    PlayerState state = PlayerState::Stopped;

    switch (playMode) {
    case Q3DSSlide::Looping:
        state = PlayerState::Playing;
        break;
    case Q3DSSlide::PlayThroughTo:
    {
        if (currentSlide->playThrough() == Q3DSSlide::Next) {
            m_data.slideDeck->nextSlide();
        } else {
            m_data.slideDeck->previousSlide();
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

QT_END_NAMESPACE
