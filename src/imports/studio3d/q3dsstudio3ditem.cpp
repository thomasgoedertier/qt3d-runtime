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

#include "q3dsstudio3ditem_p.h"
#include "q3dsstudio3drenderer_p.h"
#include "q3dsstudio3dnode_p.h"
#include <QSGNode>
#include <QLoggingCategory>
#include <QRunnable>
#include <QQuickWindow>
#include <QQuickRenderControl>
#include <QOffscreenSurface>
#include <QQmlFile>
#include <QGuiApplication>
#include <private/q3dsengine_p.h>
#include <private/q3dsutils_p.h>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QRenderSurfaceSelector>
#include <Qt3DRender/private/qrendersurfaceselector_p.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcStudio3D, "q3ds.studio3d")

Q3DSStudio3DItem::Q3DSStudio3DItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
    setAcceptedMouseButtons(Qt::MouseButtonMask);
    setAcceptHoverEvents(true);
    Q3DSUtils::setDialogsEnabled(false);
}

Q3DSStudio3DItem::~Q3DSStudio3DItem()
{
}

QUrl Q3DSStudio3DItem::source() const
{
    return m_source;
}

void Q3DSStudio3DItem::setSource(const QUrl &newSource)
{
    if (m_source == newSource)
        return;

    m_source = newSource;
    emit sourceChanged();

    if (window()) // else defer to itemChange()
        createEngine();
}

void Q3DSStudio3DItem::createEngine()
{
    // note: cannot have an engine without the source set
    // (since once we assume m_engine!=nullptr, m_engine->renderAspect() must be valid as well)

    if (!m_engine) {
        qCDebug(lcStudio3D, "creating engine");
        QQuickWindow *w = window();
        Q_ASSERT(w);

        m_engine = new Q3DSEngine;
        // Rendering will be driven manually from the Quick render thread via the QRenderAspect.
        // We create the render aspect ourselves on the Quick render thread.
        m_engine->setFlags(Q3DSEngine::WithoutRenderAspect);

        if (QWindow *rw = QQuickRenderControl::renderWindowFor(w)) {
            // rw is the top-level window that is backed by a native window. Do
            // not use that though since we must not clash with e.g. the widget
            // backingstore compositor in the gui thread.
            QOffscreenSurface *dummySurface = new QOffscreenSurface;
            dummySurface->setParent(qGuiApp); // parent to something suitably long-living
            dummySurface->setFormat(rw->format());
            dummySurface->create();
            m_engine->setSurface(dummySurface);
        } else {
            m_engine->setSurface(w);
        }
    }

    const QString fn = QQmlFile::urlToLocalFileOrQrc(m_source);
    qCDebug(lcStudio3D) << "source is now" << fn;
    const QSize sz(width(), height());
    if (!sz.isEmpty())
        sendResize(sz);

    m_engine->setSource(fn);

    // cannot start() here, that must be deferred

    update();
}

QSGNode *Q3DSStudio3DItem::updatePaintNode(QSGNode *node, QQuickItem::UpdatePaintNodeData *)
{
    // this on the render thread; the engine lives on the gui thread and should
    // be ready at this point - unless there's no source set yet

    if (!m_engine) {
        delete node;
        return nullptr;
    }

    Q3DSStudio3DNode *n = static_cast<Q3DSStudio3DNode *>(node);
    if (!n)
        n = new Q3DSStudio3DNode;

    n->setRect(boundingRect());

    if (!m_renderer)
        m_renderer = new Q3DSStudio3DRenderer(this, n, m_engine->aspectEngine());

    return n;
}

// Let's do resource handling correctly, which means handling releaseResources
// on the item and connecting to the sceneGraphInvalidated signal. Care must be
// taken to support the case of moving the item from window to another as well.

void Q3DSStudio3DItem::itemChange(QQuickItem::ItemChange change,
                                  const QQuickItem::ItemChangeData &changeData)
{
    if (change == QQuickItem::ItemSceneChange) {
        if (changeData.window) {
            connect(changeData.window, &QQuickWindow::sceneGraphInvalidated, this, [this]() {
                // render thread
                qCDebug(lcStudio3D, "[R] sceneGraphInvalidated");
                delete m_renderer;
                m_renderer = nullptr;
                QMetaObject::invokeMethod(this, "destroyEngine");
            }, Qt::DirectConnection);

            if (!m_source.isEmpty() && !m_engine)
                createEngine();
        }
    }
}

void Q3DSStudio3DItem::startEngine()
{
    // set root entity
    m_engine->start();
}

void Q3DSStudio3DItem::destroyEngine()
{
    if (m_engine) {
        qCDebug(lcStudio3D, "destroying engine");
        delete m_engine; // recreate on next window change - if we are still around, that is
        m_engine = nullptr;
    }
}

class Releaser : public QRunnable
{
public:
    Releaser(Q3DSStudio3DRenderer *r) : m_renderer(r) { }
    void run() override {
        delete m_renderer;
    }
private:
    Q3DSStudio3DRenderer *m_renderer;
};

void Q3DSStudio3DItem::releaseResources()
{
    qCDebug(lcStudio3D, "releaseResources");

    // by the time the runnable runs we (the item) may already be gone; that's fine, just pass the renderer ref
    if (window()) {
        destroyEngine();
        if (m_renderer) {
            window()->scheduleRenderJob(new Releaser(m_renderer), QQuickWindow::BeforeSynchronizingStage);
            // this may not be an application exit; if this is just a window change then allow continuing
            // by eventually creating a new renderer object
            m_renderer = nullptr;
        }
    }
}

void Q3DSStudio3DItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    if (!newGeometry.isEmpty() && m_engine)
        sendResize(newGeometry.size().toSize());
}

void Q3DSStudio3DItem::sendResize(const QSize &size)
{
    Q_ASSERT(m_engine);
    m_engine->resize(size);

    Qt3DCore::QEntity *rootEntity = m_engine->rootEntity();
    Qt3DRender::QRenderSurfaceSelector *surfaceSelector = Qt3DRender::QRenderSurfaceSelectorPrivate::find(rootEntity);
    qCDebug(lcStudio3D, "Setting external render target size on surface selector %p", surfaceSelector);
    if (surfaceSelector) {
        surfaceSelector->setExternalRenderTargetSize(size);
        surfaceSelector->setSurfacePixelRatio(window()->effectiveDevicePixelRatio());
    }
}

void Q3DSStudio3DItem::keyPressEvent(QKeyEvent *event)
{
    m_engine->handleKeyPressEvent(event);
}

void Q3DSStudio3DItem::keyReleaseEvent(QKeyEvent *event)
{
    m_engine->handleKeyReleaseEvent(event);
}

void Q3DSStudio3DItem::mousePressEvent(QMouseEvent *event)
{
    m_engine->handleMousePressEvent(event);
}

void Q3DSStudio3DItem::mouseMoveEvent(QMouseEvent *event)
{
    m_engine->handleMouseMoveEvent(event);
}

void Q3DSStudio3DItem::mouseReleaseEvent(QMouseEvent *event)
{
    m_engine->handleMouseReleaseEvent(event);
}

void Q3DSStudio3DItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    m_engine->handleMouseDoubleClickEvent(event);
}

#if QT_CONFIG(wheelevent)
void Q3DSStudio3DItem::wheelEvent(QWheelEvent *event)
{
    m_engine->handleWheelEvent(event);
}
#endif

void Q3DSStudio3DItem::hoverMoveEvent(QHoverEvent *event)
{
    // Simulate the QWindow behavior, which means sending moves even when no
    // button is down. The profile ui for example depends on this.

    if (QGuiApplication::mouseButtons() != Qt::NoButton)
        return;

    const QPointF sceneOffset = mapToScene(event->pos());
    const QPointF globalOffset = mapToGlobal(event->pos());
    QMouseEvent e(QEvent::MouseMove, event->pos(), event->pos() + sceneOffset, event->pos() + globalOffset,
                  Qt::NoButton, Qt::NoButton, QGuiApplication::keyboardModifiers());
    m_engine->handleMouseMoveEvent(&e);
}

QT_END_NAMESPACE
