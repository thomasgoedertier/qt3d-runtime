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

#include <Qt3DRender/QRenderTargetSelector>
#include <Qt3DRender/QRenderTargetOutput>
#include <Qt3DRender/QTexture>

#include <Qt3DInput/QInputSettings>

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
{
    setSurfaceType(QSurface::OpenGLSurface);
    createAspectEngine();
}

Q3DStudioWindow::~Q3DStudioWindow()
{
    for (Presentation &pres : m_presentations) {
        delete pres.sceneManager;
        delete pres.uipDocument;
    }
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
    if (!m_presentations.isEmpty()) {
        for (Presentation &pres : m_presentations)
            pres.sceneManager->prepareEngineReset();

        Q3DSSceneManager::prepareEngineResetGlobal();
        Qt3DCore::QAspectEnginePrivate::get(m_aspectEngine.data())->exitSimulationLoop();
        createAspectEngine();

        for (Presentation &pres : m_presentations) {
            delete pres.sceneManager;
            delete pres.uipDocument;
        }
        m_presentations.clear();
    }

    Presentation pres;
    pres.uipFileName = filename;

    // Parse.
    QScopedPointer<Q3DSUipDocument> uipDocument(new Q3DSUipDocument);
    if (!uipDocument->loadUip(pres.uipFileName)) {
        Q3DSUtils::showMessage(QObject::tr("Failed to build Qt3D scene"));
        return false;
    }
    pres.uipDocument = uipDocument.take();

    // Presentation is ready. Build the Qt3D scene. This will also activate the first sub-slide.
    Q3DSSceneManager::SceneBuilderParams params;
    params.flags = 0;
    if (initFlags.testFlag(MSAA4x))
        params.flags |= Q3DSSceneManager::LayerMSAA4x;

    params.outputSize = size();
    params.outputDpr = devicePixelRatio();
    params.window = this;

    QScopedPointer<Q3DSSceneManager> sceneManager(new Q3DSSceneManager(gfxLimits));
    pres.q3dscene = sceneManager->buildScene(pres.uipDocument->presentation(), params);
    if (!pres.q3dscene.rootEntity) {
        Q3DSUtils::showMessage(QObject::tr("Failed to build Qt3D scene"));
        return false;
    }
    pres.sceneManager = sceneManager.take();

    m_presentations.append(pres);

    // Input.
    Qt3DInput::QInputSettings *inputSettings = new Qt3DInput::QInputSettings;
    inputSettings->setEventSource(this);
    pres.q3dscene.rootEntity->addComponent(inputSettings);

    // Try sizing the window to the presentation.
    Q3DSPresentation *pres3DS = pres.uipDocument->presentation();
    QSize winSize(pres3DS->presentationWidth(), pres3DS->presentationHeight());
    if (winSize.isEmpty())
        winSize = QSize(800, 480);
    resize(winSize);

    // Set new root entity if the window was already up and running.
    if (isExposed())
        m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(pres.q3dscene.rootEntity));

    return true;
}

bool Q3DStudioWindow::addSubPresentation(const QString &filename)
{
    Q_ASSERT(!m_presentations.isEmpty());

    Presentation pres;
    pres.uipFileName = filename;

    // Parse.
    QScopedPointer<Q3DSUipDocument> uipDocument(new Q3DSUipDocument);
    if (!uipDocument->loadUip(pres.uipFileName)) {
        Q3DSUtils::showMessage(QObject::tr("Failed to build Qt3D scene"));
        return false;
    }
    pres.uipDocument = uipDocument.take();

    Qt3DRender::QFrameGraphNode *fgParent = m_presentations[0].q3dscene.subPresFrameGraphRoot;
    Qt3DCore::QNode *entityParent = m_presentations[0].q3dscene.rootEntity;

    Q3DSSceneManager::SceneBuilderParams params;
    params.flags = Q3DSSceneManager::SubPresentation;

    Q3DSPresentation *pres3DS = pres.uipDocument->presentation();
    params.outputSize = QSize(pres3DS->presentationWidth(), pres3DS->presentationHeight());
    params.outputDpr = 1;

    Qt3DRender::QRenderTargetSelector *rtSel = new Qt3DRender::QRenderTargetSelector(fgParent);
    Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
    Qt3DRender::QRenderTargetOutput *color = new Qt3DRender::QRenderTargetOutput;

    color->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
    // no MSAA for subpresentations
    pres.subPres.tex = new Qt3DRender::QTexture2D;
    pres.subPres.tex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
    pres.subPres.tex->setWidth(pres3DS->presentationWidth());
    pres.subPres.tex->setHeight(pres3DS->presentationHeight());
    pres.subPres.tex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    pres.subPres.tex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    color->setTexture(pres.subPres.tex);

    Qt3DRender::QRenderTargetOutput *ds = new Qt3DRender::QRenderTargetOutput;
    ds->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::DepthStencil);
    Qt3DRender::QAbstractTexture *dsTexOrRb = new Qt3DRender::QTexture2D;
    dsTexOrRb->setFormat(Qt3DRender::QAbstractTexture::D24S8);
    dsTexOrRb->setWidth(pres3DS->presentationWidth());
    dsTexOrRb->setHeight(pres3DS->presentationHeight());
    dsTexOrRb->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    dsTexOrRb->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    ds->setTexture(dsTexOrRb);

    rt->addOutput(color);
    rt->addOutput(ds);

    rtSel->setTarget(rt);
    params.frameGraphRoot = rtSel;

    QScopedPointer<Q3DSSceneManager> sceneManager(new Q3DSSceneManager(gfxLimits));
    pres.q3dscene = sceneManager->buildScene(pres.uipDocument->presentation(), params);
    if (!pres.q3dscene.rootEntity) {
        Q3DSUtils::showMessage(QObject::tr("Failed to build Qt3D scene"));
        return false;
    }
    pres.sceneManager = sceneManager.take();

    pres.q3dscene.rootEntity->setParent(entityParent);

    m_presentations.append(pres);
    return true;
}

void Q3DStudioWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed()
            && !m_presentations.isEmpty()
            && m_aspectEngine->rootEntity() != m_presentations[0].q3dscene.rootEntity)
    {
        m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(m_presentations[0].q3dscene.rootEntity));
    }
}

void Q3DStudioWindow::resizeEvent(QResizeEvent *)
{
    if (!m_presentations.isEmpty())
        m_presentations[0].sceneManager->updateSizes(size(), devicePixelRatio());
}

void Q3DStudioWindow::setOnDemandRendering(bool enabled)
{
    m_presentations[0].q3dscene.renderSettings->setRenderPolicy(enabled ? Qt3DRender::QRenderSettings::OnDemand
                                                                        : Qt3DRender::QRenderSettings::Always);
}

QT_END_NAMESPACE
