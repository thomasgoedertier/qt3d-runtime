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

#include "q3dsstudio3drenderer_p.h"
#include "q3dsstudio3ditem_p.h"
#include "q3dsstudio3dnode_p.h"
#include <QQuickWindow>
#include <QOpenGLContext>
#include <QLoggingCategory>
#include <Qt3DCore/QAspectEngine>
#include <Qt3DRender/private/qrenderaspect_p.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcStudio3D)

class ContextSaver
{
public:
    ContextSaver()
    {
        m_context = QOpenGLContext::currentContext();
        m_surface = m_context ? m_context->surface() : nullptr;
    }

    ~ContextSaver()
    {
        if (m_context && m_context->surface() != m_surface)
            m_context->makeCurrent(m_surface);
    }

    QOpenGLContext *context() const { return m_context; }
    QSurface *surface() const { return m_surface; }

private:
    QOpenGLContext *m_context;
    QSurface *m_surface;
};

// the renderer object lives on the Qt Quick render thread

Q3DSStudio3DRenderer::Q3DSStudio3DRenderer(Q3DSStudio3DItem *item, Q3DSStudio3DNode *node, Qt3DCore::QAspectEngine *aspectEngine)
    : m_item(item),
      m_node(node),
      m_aspectEngine(aspectEngine)
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    qCDebug(lcStudio3D, "[R] new renderer %p, window is %p, context is %p, aspect engine %p",
            this, m_item->window(), ctx, m_aspectEngine);

    connect(m_item->window(), &QQuickWindow::beforeRendering,
            this, &Q3DSStudio3DRenderer::renderOffscreen, Qt::DirectConnection);

    ContextSaver saver;
    m_renderAspect = new Qt3DRender::QRenderAspect(Qt3DRender::QRenderAspect::Synchronous);
    m_aspectEngine->registerAspect(m_renderAspect);
    m_renderAspectD = static_cast<Qt3DRender::QRenderAspectPrivate *>(Qt3DRender::QRenderAspectPrivate::get(m_renderAspect));
    m_renderAspectD->renderInitialize(ctx);

    QMetaObject::invokeMethod(m_item, "startEngine");
}

Q3DSStudio3DRenderer::~Q3DSStudio3DRenderer()
{
    qCDebug(lcStudio3D, "[R] renderer %p dtor", this);
    ContextSaver saver;
    m_renderAspectD->renderShutdown();
}

void Q3DSStudio3DRenderer::renderOffscreen()
{
    QQuickWindow *w = m_item->window();

    const QSize size = m_item->boundingRect().size().toSize() * w->effectiveDevicePixelRatio();
    if (m_fbo.isNull() || m_fbo->size() != size) {
        m_fbo.reset(new QOpenGLFramebufferObject(size, QOpenGLFramebufferObject::CombinedDepthStencil));
        m_texture.reset(w->createTextureFromId(m_fbo->texture(), m_fbo->size(), QQuickWindow::TextureHasAlphaChannel));
        m_node->setTexture(m_texture.data());
    }

    ContextSaver saver;
    w->resetOpenGLState();
    m_fbo->bind();
    m_renderAspectD->renderSynchronous();
    if (saver.context()->surface() != saver.surface())
        saver.context()->makeCurrent(saver.surface());
    w->resetOpenGLState();

    m_node->markDirty(QSGNode::DirtyMaterial);

    // ### hmm...
    w->update();
}

QT_END_NAMESPACE
