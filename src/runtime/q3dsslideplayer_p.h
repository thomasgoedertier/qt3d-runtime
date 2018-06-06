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

#ifndef Q3DSSLIDEPLAYER_P_H
#define Q3DSSLIDEPLAYER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <QObject>

#include "q3dsuippresentation_p.h"
#include "q3dsscenemanager_p.h"
#include <QtCore/qvector.h>

QT_BEGIN_NAMESPACE

class Q3DSAnimationManager;
class Q3DSSlideDeck;

namespace Q3DSSlideUtils
{
    Q3DSV_PRIVATE_EXPORT void getStartAndEndTime(Q3DSSlide *slide, qint32 *start, qint32 *end);
};

class Q3DSV_PRIVATE_EXPORT Q3DSSlidePlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Q3DSSlideDeck *slideDeck READ slideDeck WRITE setSlideDeck NOTIFY slideDeckChanged)
    Q_PROPERTY(float duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(float position READ position NOTIFY positionChanged)

public:
    enum class PlayerState
    {
        Idle,
        Ready,
        Stopped,
        Playing,
        Paused
    };
    Q_ENUM(PlayerState)

    enum class PlayerMode
    {
        Viewer,
        Editor
    };
    Q_ENUM(PlayerMode)

    enum class PlayerType
    {
        Slide,
        ComponentSlide
    };
    Q_ENUM(PlayerType)

    Q3DSSlidePlayer(Q3DSSceneManager *sceneManager,
                    QObject *parent = nullptr);
    ~Q3DSSlidePlayer();

    Q3DSSlideDeck *slideDeck() const;
    void advanceFrame();
    void sceneReady();

    float duration() const;
    float position() const;
    PlayerState state() const { return m_data.state; }

    void setMode(PlayerMode mode);
    PlayerMode mode() const { return m_mode; }

    void objectAboutToBeRemovedFromScene(Q3DSGraphObject *obj);

public Q_SLOTS:
    void play();
    void stop();
    void pause();
    void seek(float);
    void setSlideDeck(Q3DSSlideDeck *slideDeck);
    void setPlaybackRate(float rate);
    void nextSlide();
    void previousSlide();
    void precedingSlide();
    void reload();

Q_SIGNALS:
    void slideDeckChanged(Q3DSSlideDeck *slideDeck);
    void durationChanged(float duration);
    void positionChanged(float position);
    void stateChanged(PlayerState state);
    void slideChanged(Q3DSSlide *);

private:
    Q3DSSlidePlayer(QSharedPointer<Q3DSAnimationManager> animationManager,
                    Q3DSSceneManager *sceneManager,
                    Q3DSComponentNode *component,
                    Q3DSSlidePlayer *parent = nullptr);

    void init();
    void reset();
    void setInternalState(PlayerState state);
    void onDurationChanged(float duration);
    void onSlideFinished(Q3DSSlide *slide);
    void setSlideTime(Q3DSSlide *slide, float time);

    void handleCurrentSlideChanged(Q3DSSlide *slide,
                                   Q3DSSlide *previousSlide,
                                   bool forceUpdate = false);

    // TODO: Move out to a "slide manager"?
    void sendPositionChanged(Q3DSSlide *slide, float pos);
    void updateObjectVisibility(Q3DSGraphObject *obj, bool visible);
    bool isSlideVisible(Q3DSSlide *slide);
    void processPropertyChanges(Q3DSSlide *currentSlide);

    struct Data {
        Q3DSSlideDeck *slideDeck = nullptr;
        PlayerState state = PlayerState::Idle;
        PlayerState pendingState = PlayerState::Idle;
        float position = 0.0f;
        float duration = 0.0f;
        float playbackRate = 1.0f;
        int loopCount = 0;
    } m_data;

    Q3DSSceneManager *m_sceneManager;
    Q3DSComponentNode *m_component = nullptr;
    QSharedPointer<Q3DSAnimationManager> m_animationManager;
    PlayerMode m_mode = PlayerMode::Viewer;
    PlayerType m_type = PlayerType::Slide;

    // This class handles animation callback from animationmanager and calls setSlideTime
    friend class Q3DSSlidePositionCallback;
    friend class Q3DSSceneManager;
};

class Q3DSV_PRIVATE_EXPORT Q3DSSlideDeck
{
public:
    Q3DSSlideDeck(Q3DSSlide *masterSlide, Q3DSSlide *parent = nullptr)
        : m_masterSlide(masterSlide)
        , m_parent(parent)
    {
        Q_ASSERT(masterSlide);
        Q_ASSERT_X(!masterSlide->parent(), Q_FUNC_INFO, "Slide is not a master slide!");
        if (!masterSlide->firstChild()) {
            qWarning("No slides?");
            return;
        }

        m_index = 0;
    }

    int slideCount() const {  return m_masterSlide->childCount(); }
    bool isEmpty() const { return (slideCount() == 0); }
    Q3DSSlide *currentSlide() const
    {
        if (!m_masterSlide)
            return nullptr;

        if (isEmpty())
            return nullptr;

        Q_ASSERT(m_index < slideCount());

        return static_cast<Q3DSSlide *>(m_masterSlide->childAtIndex(m_index));
    }

    Q3DSSlide *slideAtIndex(int index) const
    {
        const int count = slideCount();
        if (index < 0 || index > count - 1)
            return nullptr;

        return static_cast<Q3DSSlide *>(m_masterSlide->childAtIndex(index));
    }

    void setCurrentSlide(int index)
    {
        Q_ASSERT(m_masterSlide);
        const int count = m_masterSlide->childCount();
        if ((index < 0) || (index > count - 1)) {
            qWarning("Invalid index!");
            return;
        }

        if (index == m_index)
            return;

        if (count < 1)
            return;

        m_previouslyActiveIndex = m_index;
        m_index = index;

        if (m_player)
            m_player->reload();
    }

    Q3DSSlide *setCurrentIndex(int index)
    {
        Q_ASSERT(m_masterSlide);
        const int count = m_masterSlide->childCount();
        if ((index < 0) || (index > count - 1)) {
            qWarning("Invalid index!");
            return nullptr;
        }

        if (index != m_index) {
            m_previouslyActiveIndex = m_index;
            m_index = index;
        }

        return static_cast<Q3DSSlide *>(m_masterSlide->childAtIndex(m_index));
    }

    Q3DSSlide *nextSlide(bool wrap = false)
    {
        if (!m_masterSlide)
            return nullptr;

        if (isEmpty() || m_index == -1)
            return nullptr;

        if (m_index < slideCount() - 1) {
            m_previouslyActiveIndex = m_index;
            return static_cast<Q3DSSlide *>(m_masterSlide->childAtIndex(++m_index));
        } else if (wrap && m_index == slideCount() - 1) {
            m_previouslyActiveIndex = m_index;
            m_index = 0;
            return static_cast<Q3DSSlide *>(m_masterSlide->childAtIndex(m_index));
        } else {
            return nullptr;
        }
    }

    Q3DSSlide *previousSlide(bool wrap = false)
    {
        if (!m_masterSlide)
            return nullptr;

        if (isEmpty() || m_index == -1)
            return nullptr;

        if ((m_index > 0) && (m_index < slideCount())) {
            m_previouslyActiveIndex = m_index;
            return static_cast<Q3DSSlide *>(m_masterSlide->childAtIndex(--m_index));
        } else if (wrap && m_index == 0) {
            m_previouslyActiveIndex = m_index;
            m_index = slideCount() - 1;
            return static_cast<Q3DSSlide *>(m_masterSlide->childAtIndex(m_index));
        } else {
            return nullptr;
        }
    }

    Q3DSSlide *precedingSlide()
    {
        // This is effectively a go-back-in-history with a 1 level history,
        // i.e. go to the slide that was current before the current one - but
        // going "back" from there would bring us back to what is current now.

        if (m_previouslyActiveIndex == -1)
            return nullptr;

        return setCurrentIndex(m_previouslyActiveIndex);
    }

    int indexOfSlide(Q3DSSlide *slide)
    {
        Q_ASSERT(m_masterSlide);

        Q3DSSlide *ns = static_cast<Q3DSSlide *>(m_masterSlide->firstChild());
        int index = 0;
        bool found = false;
        while (ns) {
            if (ns == slide) {
                found = true;
                break;
            }
            ns = static_cast<Q3DSSlide *>(ns->nextSibling());
            ++index;
        }

        return found ? index : -1;
    }

    int indexOfSlide(const QByteArray &slideId)
    {
        Q_ASSERT(m_masterSlide);

        Q3DSSlide *ns = static_cast<Q3DSSlide *>(m_masterSlide->firstChild());
        int index = 0;
        bool found = false;
        while (ns) {
            if (ns->id() == slideId) {
                found = true;
                break;
            }
            ns = static_cast<Q3DSSlide *>(ns->nextSibling());
            ++index;
        }

        return found ? index : -1;
    }

    Q3DSSlide *parentSlide() const { return m_parent; }
    Q3DSSlide *masterSlide() const { return m_masterSlide; }

    void bind(Q3DSSlidePlayer *player)
    {
        if (player == m_player)
            return;

        if (m_player)
            m_player->setSlideDeck(nullptr);

        m_player = player;
    }

private:
    Q3DSSlidePlayer *m_player = nullptr;
    Q3DSSlide *m_masterSlide = nullptr;
    Q3DSSlide *m_parent = nullptr;
    int m_index = -1;
    int m_previouslyActiveIndex = -1;
};

QT_END_NAMESPACE

#endif // Q3DSSLIDEPLAYER_P_H
