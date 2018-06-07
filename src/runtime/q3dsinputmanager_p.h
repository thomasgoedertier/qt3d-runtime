/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef Q3DSINPUTMANAGER_H
#define Q3DSINPUTMANAGER_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//


#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QQueue>
#include <Qt3DRender/QRayCaster>

QT_BEGIN_NAMESPACE

class Q3DSSceneManager;
class Q3DSGraphObject;
class Q3DSLayerNode;
class QMouseEvent;

namespace Qt3DCore {
class QEntity;
}

namespace Qt3DRender {
class QCamera;
}

class Q3DSInputManager : public QObject
{
    Q_OBJECT
public:
    explicit Q3DSInputManager(Q3DSSceneManager *sceneManager, QObject *parent = nullptr);

    void handleMousePressEvent(QMouseEvent *e);
    void handleMouseReleaseEvent(QMouseEvent *e);
    void handleMouseMoveEvent(QMouseEvent *e);

    void runPicks();

    struct InputState {
        bool mousePressed = false;
    };

private slots:
    void castNextRay(Q3DSLayerNode *layer);

private:
    void pick(const QPoint &point, const InputState &inputState);
    void castRayIntoLayer(Q3DSLayerNode *layer, const QPointF &pos, const InputState &inputState, int eventId);
    void sendMouseEvent(Q3DSGraphObject *target, const Qt3DRender::QRayCasterHit &hit, const InputState &inputState);
    Q3DSGraphObject *getNodeForEntity(Q3DSLayerNode *layer, Qt3DCore::QEntity *entity);
    QPoint convertToViewportSpace(const QPoint &point) const;

    Q3DSSceneManager *m_sceneManager = nullptr;
    bool m_isHoverEnabled = false;
    QHash <int, QMetaObject::Connection> m_connectionMap;
    int m_eventId = 0;

    struct PickRequest {
        PickRequest() = default;
        PickRequest(const QPoint& pos_, const InputState &inputState_)
            : pos(pos_), inputState(inputState_)
        { }
        QPoint pos;
        InputState inputState;
    };
    QQueue<PickRequest> m_pickRequests;

    InputState m_currentState;
};

QT_END_NAMESPACE

#endif // Q3DSINPUTMANAGER_H
