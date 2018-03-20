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

#include "q3dsinputmanager_p.h"
#include <Qt3DRender/QRayCaster>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QLayer>
#include <QtGui/QMouseEvent>

#include "q3dsscenemanager_p.h"

QT_BEGIN_NAMESPACE

Q3DSInputManager::Q3DSInputManager(Q3DSSceneManager *sceneManager, QObject *parent)
    : QObject(parent)
    , m_sceneManager(sceneManager)
{
    qRegisterMetaType<Q3DSLayerNode *>("Q3DSLayerNode*");
}

void Q3DSInputManager::handleMousePressEvent(QMouseEvent *e)
{
    m_currentState.mousePressed = true;
    PickRequest req(e->pos(), m_currentState);
    m_pickRequests.append(req);
}

void Q3DSInputManager::handleMouseReleaseEvent(QMouseEvent *e)
{
    m_currentState.mousePressed = false;
    PickRequest req(e->pos(), m_currentState);
    m_pickRequests.append(req);
}

void Q3DSInputManager::handleMouseMoveEvent(QMouseEvent *e)
{
    if (!m_isHoverEnabled && !m_currentState.mousePressed)
        return;

    PickRequest req(e->pos(), m_currentState);
    m_pickRequests.append(req);
}

void Q3DSInputManager::runPicks()
{
    for (const auto &p : m_pickRequests)
        pick(p.pos, p.inputState);

    m_pickRequests.clear();
}

void Q3DSInputManager::sendMouseEvent(Q3DSGraphObject *target,
                                      const Qt3DRender::QRayCasterHit &hit,
                                      const InputState &inputState)
{
    Q_UNUSED(hit);
    if (!target->attached())
        return;

    const bool isPress = inputState.mousePressed && !m_lastSentState.mousePressed;
    const bool isRelease = !inputState.mousePressed && m_lastSentState.mousePressed;

    if (isPress)
        m_sceneManager->queueEvent(Q3DSGraphObject::Event(target, Q3DSGraphObjectEvents::pressureDownEvent()));
    if (isRelease)
        m_sceneManager->queueEvent(Q3DSGraphObject::Event(target, Q3DSGraphObjectEvents::pressureUpEvent()));

    m_lastSentState.mousePressed = m_currentState.mousePressed;
}

namespace {
QMatrix4x4 calculateCameraViewMatrix(const QMatrix4x4 &cameraWorldTransform)
{
    const QVector4D position = cameraWorldTransform * QVector4D(0.0f, 0.0f, 0.0f, 1.0f);
    const QVector4D viewDirection = cameraWorldTransform * QVector4D(0.0f, 0.0f, -1.0f, 0.0f);
    const QVector4D upVector = cameraWorldTransform * QVector4D(0.0f, 1.0f, 0.0f, 0.0f);

    QMatrix4x4 m;
    m.lookAt(QVector3D(position),
             QVector3D(position + viewDirection),
             QVector3D(upVector));
    return QMatrix4x4(m);
}
}

void Q3DSInputManager::castRayIntoLayer(Q3DSLayerNode *layer, const QPointF &pos, const InputState &inputState, int eventId)
{
    // Create the ray to cast into the layer's scene
    auto camera = m_sceneManager->findFirstCamera(layer);
    if (!camera) {
        // After a slide change the camera for a layer may be null if the
        // camera object is not on the current (or master) slide.
        return;
    }

    auto cameraNodeAttached = static_cast<Q3DSNodeAttached*>(camera->attached());
    auto cameraData = static_cast<Q3DSCameraAttached*>(camera->attached());
    // Get Camera ViewMatrix
    auto viewMatrix = calculateCameraViewMatrix(cameraNodeAttached->globalTransform);
    auto projectionMatrix = cameraData->camera->lens()->projectionMatrix();
    QRect viewport(-1, -1, 2, 2);

    QVector3D nearPos(float(pos.x()), float(pos.y()), 0.0f);
    nearPos = nearPos.unproject(viewMatrix, projectionMatrix, viewport);
    QVector3D farPos(float(pos.x()), float(pos.y()), 1.0f);
    farPos = farPos.unproject(viewMatrix, projectionMatrix, viewport);

    QVector3D origin(nearPos);
    QVector3D direction((farPos - nearPos).normalized());
    float length = (farPos - nearPos).length();

    // Queue a ray cast request. The QRayCaster can only handle one request at a time.
    auto layerData = static_cast<Q3DSLayerAttached *>(layer->attached());
    Q3DSLayerAttached::RayCastQueueEntry e;
    e.direction = direction;
    e.origin = origin;
    e.length = length;
    e.inputState = inputState;
    e.eventId = eventId;
    layerData->rayCastQueue.enqueue(e);

    castNextRay(layer);
}

void Q3DSInputManager::castNextRay(Q3DSLayerNode *layer)
{
    auto layerData = static_cast<Q3DSLayerAttached *>(layer->attached());
    if (layerData->rayCasterBusy || layerData->rayCastQueue.isEmpty())
        return;

    Q3DSLayerAttached::RayCastQueueEntry e = layerData->rayCastQueue.dequeue();
    auto rayCaster = layerData->layerRayCaster;
    rayCaster->setDirection(e.direction);
    rayCaster->setOrigin(e.origin);
    rayCaster->setLength(e.length);

    if (!m_connectionMap.value(e.eventId)) {
        QMetaObject::Connection connection = connect(rayCaster, &Qt3DRender::QAbstractRayCaster::hitsChanged, rayCaster,
                                                     [=](const Qt3DRender::QAbstractRayCaster::Hits &hits)
        {
            for (auto hit : hits) {
                auto node = getNodeForEntity(layer, hit.entity());
                sendMouseEvent(node, hit, e.inputState);
            }
            disconnect(m_connectionMap.value(e.eventId));
            m_connectionMap.remove(e.eventId);
            layerData->rayCasterBusy = false;
            if (!layerData->rayCastQueue.isEmpty()) {
                // the stupid thing is blocking property notifications so issue the
                // next raycast after the emit returns
                QMetaObject::invokeMethod(this, "castNextRay", Qt::QueuedConnection, Q_ARG(Q3DSLayerNode*, layer));
            }
        });
        m_connectionMap.insert(e.eventId, connection);
    }

    layerData->rayCasterBusy = true;
    rayCaster->trigger();
}

Q3DSGraphObject *Q3DSInputManager::getNodeForEntity(Q3DSLayerNode *layer, Qt3DCore::QEntity *entity)
{
    QVector<Q3DSNode*> nodes;
    Q3DSUipPresentation::forAllNodes(layer, [&nodes](Q3DSNode *node){
        nodes.append(node);
    });

    for (auto node : nodes) {
        if (node->type() == Q3DSNode::Model) {
            auto model = static_cast<Q3DSModelNode*>(node);
            auto modelData = static_cast<Q3DSModelAttached*>(node->attached());
            for (auto subMesh : modelData->subMeshes) {
                if (subMesh.entity == entity) {
                    return model;
                }
            }
        } else {
            // other entities with mesh data
            Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(node->attached());
            if (data->entity == entity)
                return node;
        }
    }

    return nullptr;
}

void Q3DSInputManager::pick(const QPoint &point, const InputState &inputState)
{
    // Get a list of layers in this scene (in order)
    QVarLengthArray<Q3DSLayerNode *, 16> layers;
    Q3DSUipPresentation::forAllLayers(m_sceneManager->m_scene, [&layers](Q3DSLayerNode *layer3DS) {
        layers.append(layer3DS);
    }, false); // process layers in order

    // Figure if we hit a layer, and where
    for (auto layer : layers) {
        Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer->attached());
        if (!layerData)
            continue;
        QRectF layerRect(layerData->layerPos, layerData->layerSize);
        if (layerRect.contains(point)) {
            // We interset with this layer so figure out where then raycast into that layer
            qreal x = qreal(point.x() - layerRect.x());
            qreal y = qreal(point.y() - layerRect.y());
            // Layers have coordinates from -1, 1 so normalize for that
            x = x / layerRect.width() * 2 - 1;
            y = y / layerRect.height() * 2 - 1;
            // OpenGL has inverted Y
            y = -y;

            // Cast a ray into the layer and get hits
            castRayIntoLayer(layer, QPointF(x, y), inputState, m_eventId);
            m_eventId++;
        }
    }
}

QT_END_NAMESPACE
