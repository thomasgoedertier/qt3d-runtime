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

#include "q3dswindow.h"
#include <Qt3DStudioRuntime2/q3dsutils.h>

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOffscreenSurface>
#include <QOpenGLTexture>

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QRenderAspect>
#include <Qt3DRender/QRenderSettings>
#include <Qt3DInput/QInputAspect>
#include <Qt3DAnimation/QAnimationAspect>
#include <Qt3DLogic/QLogicAspect>

#include <Qt3DCore/private/qaspectengine_p.h>

#ifndef GL_MAX_DRAW_BUFFERS
#define GL_MAX_DRAW_BUFFERS               0x8824
#endif

static void initResources()
{
    Q_INIT_RESOURCE(q3dsres);
}

QT_BEGIN_NAMESPACE

static Q3DSGraphicsLimits gfxLimits;
static Q3DStudioWindow::InitFlags initFlags;

Q3DStudioWindow::Q3DStudioWindow()
    : m_sceneBuilder(gfxLimits) // gfxLimits was initialized from initStaticPostApp()
{
    setSurfaceType(QSurface::OpenGLSurface);
    createAspectEngine();
}

static void initGraphicsLimits(QOpenGLContext *ctx)
{
    QOffscreenSurface s;
    s.setFormat(ctx->format());
    s.create();
    if (!ctx->makeCurrent(&s)) {
        qWarning("Failed to make temporary context current");
        return;
    }

    QOpenGLFunctions *f = ctx->functions();
    GLint n;

    // Max number of MRT outputs is typically 8 or so, but may be 4 on some implementations.
    n = 0;
    f->glGetIntegerv(GL_MAX_DRAW_BUFFERS, &n);
    qDebug("  GL_MAX_DRAW_BUFFERS: %d", n);
    gfxLimits.maxDrawBuffers = n;

    gfxLimits.multisampleTextureSupported = QOpenGLTexture::hasFeature(QOpenGLTexture::TextureMultisample);
    qDebug("  multisample textures: %s", gfxLimits.multisampleTextureSupported ? "true" : "false");

    ctx->doneCurrent();
}

static QSurfaceFormat findIdealGLVersion()
{
    QSurfaceFormat fmt;
    fmt.setProfile(QSurfaceFormat::CoreProfile);

    // Advanced: Try 4.3 core (so we get compute shaders for instance)
    fmt.setVersion(4, 3);
    QOpenGLContext ctx;
    ctx.setFormat(fmt);
    if (ctx.create() && ctx.format().version() >= qMakePair(4, 3)) {
        qDebug("Requesting OpenGL 4.3 core context succeeded");
        initGraphicsLimits(&ctx);
        return fmt;
    }

    // Basic: Stick with 3.3 for now to keep less fortunate, Mesa-based systems happy
    fmt.setVersion(3, 3);
    ctx.setFormat(fmt);
    if (ctx.create()) {
        qDebug("Requesting OpenGL 3.3 core context succeeded");
        initGraphicsLimits(&ctx);
        return fmt;
    }

    qDebug("Impending doom");
    return fmt;
}

static QSurfaceFormat findIdealGLESVersion()
{
    QSurfaceFormat fmt;

    // Advanced: Try 3.1 (so we get compute shaders for instance)
    fmt.setVersion(3, 1);
    QOpenGLContext ctx;
    ctx.setFormat(fmt);
    if (ctx.create()) {
        qDebug("Requesting OpenGL ES 3.1 context succeeded");
        initGraphicsLimits(&ctx);
        return fmt;
    }

    // Basic: OpenGL ES 3.0 is a hard requirement at the moment since we can
    // only generate 300 es shaders, uniform buffers are mandatory.
    fmt.setVersion(3, 0);
    ctx.setFormat(fmt);
    if (ctx.create()) {
        qDebug("Requesting OpenGL ES 3.0 context succeeded");
        initGraphicsLimits(&ctx);
        return fmt;
    }

    qDebug("Impending doom");
    return fmt;
}

void Q3DStudioWindow::initStaticPreApp()
{
    initResources();
}

void Q3DStudioWindow::initStaticPostApp(InitFlags flags)
{
    initFlags = flags;

    QSurfaceFormat fmt;
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) { // works in dynamic gl builds too because there's a qguiapp already
        fmt = findIdealGLVersion();
    } else {
        fmt = findIdealGLESVersion();
    }
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    if (flags.testFlag(Q3DStudioWindow::MSAA4x))
        fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);
}

void Q3DStudioWindow::createAspectEngine()
{
    m_aspectEngine.reset(new Qt3DCore::QAspectEngine);
    m_aspectEngine->registerAspect(new Qt3DRender::QRenderAspect);
    m_aspectEngine->registerAspect(new Qt3DInput::QInputAspect);
    m_aspectEngine->registerAspect(new Qt3DAnimation::QAnimationAspect);
    m_aspectEngine->registerAspect(new Qt3DLogic::QLogicAspect);
}

bool Q3DStudioWindow::setUipSource(const QString &filename)
{
    if (m_q3dscene.rootEntity) {
        m_sceneBuilder.prepareSceneChange();
        Qt3DCore::QAspectEnginePrivate::get(m_aspectEngine.data())->exitSimulationLoop();
        createAspectEngine();
        m_q3dscene = Q3DSSceneBuilder::Scene();
    }

    m_uipFileName = filename;
    if (m_uipParser.parse(m_uipFileName)) {
        Q3DSPresentation *pres = m_uipParser.presentation();
        // Presentation is ready. Build the Qt3D scene. This will also activate the first sub-slide.
        Q3DSSceneBuilder::SceneBuilderFlags flags = 0;
        if (initFlags.testFlag(MSAA4x))
            flags |= Q3DSSceneBuilder::LayerMSAA4x;
        m_q3dscene = m_sceneBuilder.buildScene(pres, this, flags);
        if (m_q3dscene.rootEntity) {
            // Ready to go.
            QSize winSize(pres->presentationWidth(), pres->presentationHeight());
            if (winSize.isEmpty())
                winSize = QSize(800, 480);
            resize(winSize);
            // Set new root entity if the window was already up and running.
            if (isExposed())
                m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(m_q3dscene.rootEntity));
            return true;
        }

        Q3DSUtils::showMessage(QObject::tr("Failed to build Qt3D scene"));
    }
    return false;
}

void Q3DStudioWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed() && m_q3dscene.rootEntity && m_aspectEngine->rootEntity() != m_q3dscene.rootEntity)
        m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(m_q3dscene.rootEntity));
}

void Q3DStudioWindow::resizeEvent(QResizeEvent *)
{
    if (m_q3dscene.rootEntity)
        m_sceneBuilder.updateSizes(this);
}

void Q3DStudioWindow::setOnDemandRendering(bool enabled)
{
    m_q3dscene.renderSettings->setRenderPolicy(enabled ? Qt3DRender::QRenderSettings::OnDemand
                                                       : Qt3DRender::QRenderSettings::Always);
}

QT_END_NAMESPACE
