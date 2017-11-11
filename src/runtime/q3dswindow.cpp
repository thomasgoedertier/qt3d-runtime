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
#include <Qt3DStudioRuntime2/q3dsuiaparser.h>
#include <Qt3DStudioRuntime2/q3dsutils.h>

#include <QLoggingCategory>
#include <QKeyEvent>
#include <QMouseEvent>

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QOffscreenSurface>

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QRenderAspect>
#include <Qt3DRender/QRenderSettings>
#include <Qt3DInput/QInputAspect>
#include <Qt3DAnimation/QAnimationAspect>
#include <Qt3DLogic/QLogicAspect>
#include <Qt3DLogic/QFrameAction>

#include <Qt3DRender/QRenderTargetSelector>
#include <Qt3DRender/QRenderTargetOutput>
#include <Qt3DRender/QRenderTarget>
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

Q_DECLARE_LOGGING_CATEGORY(lcUip)

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

    // version string bonanza for the profiler
    const char *rendererStr = reinterpret_cast<const char *>(f->glGetString(GL_RENDERER));
    if (rendererStr) {
        gfxLimits.renderer = rendererStr;
        qDebug("  renderer: %s", rendererStr);
    }
    const char *vendorStr = reinterpret_cast<const char *>(f->glGetString(GL_VENDOR));
    if (vendorStr) {
        gfxLimits.vendor = vendorStr;
        qDebug("  vendor: %s", vendorStr);
    }
    const char *versionStr = reinterpret_cast<const char *>(f->glGetString(GL_VERSION));
    if (versionStr) {
        gfxLimits.version = versionStr;
        qDebug("  version: %s", versionStr);
    }

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
    // Ignore MSAA here as that is a per-layer setting.
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

QString Q3DStudioWindow::source() const
{
    return m_source;
}

bool Q3DStudioWindow::setSource(const QString &uipOrUiaFileName)
{
    // no check for m_source being the same - must reload no matter what

    if (!m_presentations.isEmpty()) {
        for (Presentation &pres : m_presentations) {
            if (pres.sceneManager)
                pres.sceneManager->prepareEngineReset();
        }

        Q3DSSceneManager::prepareEngineResetGlobal();
        Qt3DCore::QAspectEnginePrivate::get(m_aspectEngine.data())->exitSimulationLoop();
        createAspectEngine();

        for (Presentation &pres : m_presentations) {
            delete pres.sceneManager;
            delete pres.uipDocument;
        }
        m_presentations.clear();
    }

    m_source = uipOrUiaFileName;

    // There are two cases:
    //   m_source is a .uip file:
    //     - check if there is a .uia with the same complete-basename
    //     - if there isn't, go with m_source as the sole .uip
    //     - if there is, parse the .uia instead and use the initial .uip as the main presentation. Add all others as subpresentations.
    //  m_source is a .uia file:
    //     - like the .uia path above

    QFileInfo fi(m_source);
    const QString sourcePrefix = fi.canonicalPath() + QLatin1Char('/');
    QString uia;
    if (fi.suffix() == QStringLiteral("uia")) {
        uia = m_source;
    } else {
        const QString maybeUia = sourcePrefix + fi.completeBaseName() + QLatin1String(".uia");
        if (QFile::exists(maybeUia)) {
            qCDebug(lcUip, "Switching to .uia file %s", qPrintable(maybeUia));
            uia = maybeUia;
        }
    }
    if (uia.isEmpty()) {
        Presentation pres;
        pres.uipFileName = m_source;
        m_presentations.append(pres);
    } else {
        Q3DSUiaParser uiaParser;
        Q3DSUiaParser::Uia uiaDoc = uiaParser.parse(uia);
        if (!uiaDoc.isValid()) {
            Q3DSUtils::showMessage(QObject::tr("Failed to parse application file"));
            return false;
        }
        for (const Q3DSUiaParser::Uia::Presentation &p : uiaDoc.presentations) {
            if (p.type != Q3DSUiaParser::Uia::Presentation::Uip)
                continue;
            Presentation pres;
            pres.subPres.id = p.id;
            // assume the .uip name in the .uia is relative to the .uia's location
            pres.uipFileName = sourcePrefix + p.source;
            qCDebug(lcUip, "Registered subpresentation %s as %s", qPrintable(pres.uipFileName), qPrintable(p.id));
            if (p.id == uiaDoc.initialPresentationId) // initial (main) presentation must be m_presentations[0]
                m_presentations.prepend(pres);
            else
                m_presentations.append(pres);
        }
        if (m_presentations.isEmpty())
            return false;
    }

    if (!loadPresentation(&m_presentations[0])) {
        m_presentations.clear();
        return false;
    }

    for (int i = 1; i < m_presentations.count(); ++i)
        loadSubPresentation(&m_presentations[i]);

    QVector<Q3DSSubPresentation> subPresentations;
    for (const Presentation &pres : m_presentations) {
        if (!pres.subPres.id.isEmpty() && pres.subPres.colorTex)
            subPresentations.append(pres.subPres);
    }
    m_presentations[0].sceneManager->finalizeMainScene(subPresentations);

    return true;
}

bool Q3DStudioWindow::loadPresentation(Presentation *pres)
{
    // Parse.
    QScopedPointer<Q3DSUipDocument> uipDocument(new Q3DSUipDocument);
    if (!uipDocument->loadUip(pres->uipFileName)) {
        Q3DSUtils::showMessage(QObject::tr("Failed to parse main presentation"));
        return false;
    }
    pres->uipDocument = uipDocument.take();

    // Presentation is ready. Build the Qt3D scene. This will also activate the first sub-slide.
    Q3DSSceneManager::SceneBuilderParams params;
    params.flags = 0;
    if (initFlags.testFlag(Force4xMSAA))
        params.flags |= Q3DSSceneManager::Force4xMSAA;

    params.outputSize = size();
    params.outputDpr = devicePixelRatio();
    params.window = this;

    QScopedPointer<Q3DSSceneManager> sceneManager(new Q3DSSceneManager(gfxLimits));
    pres->q3dscene = sceneManager->buildScene(pres->uipDocument->presentation(), params);
    if (!pres->q3dscene.rootEntity) {
        Q3DSUtils::showMessage(QObject::tr("Failed to build Qt3D scene"));
        return false;
    }
    pres->sceneManager = sceneManager.take();

    // Input (Qt3D).
    Qt3DInput::QInputSettings *inputSettings = new Qt3DInput::QInputSettings;
    inputSettings->setEventSource(this);
    pres->q3dscene.rootEntity->addComponent(inputSettings);

    // Input (profiling UI).
    pres->sceneManager->setProfileUiInputEventSource(this);

    // Try sizing the window to the presentation.
    Q3DSPresentation *pres3DS = pres->uipDocument->presentation();
    QSize winSize(pres3DS->presentationWidth(), pres3DS->presentationHeight());
    if (winSize.isEmpty())
        winSize = QSize(800, 480);
    resize(winSize);

    // Expose update signal
    connect(pres->q3dscene.frameAction, &Qt3DLogic::QFrameAction::triggered, this, &Q3DStudioWindow::sceneUpdated);

    // Set new root entity if the window was already up and running.
    if (isExposed())
        m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(pres->q3dscene.rootEntity));

    return true;
}

bool Q3DStudioWindow::loadSubPresentation(Presentation *pres)
{
    // Parse.
    QScopedPointer<Q3DSUipDocument> uipDocument(new Q3DSUipDocument);
    if (!uipDocument->loadUip(pres->uipFileName)) {
        Q3DSUtils::showMessage(QObject::tr("Failed to parse subpresentation"));
        return false;
    }
    pres->uipDocument = uipDocument.take();

    Qt3DRender::QFrameGraphNode *fgParent = m_presentations[0].q3dscene.subPresFrameGraphRoot;
    Qt3DCore::QNode *entityParent = m_presentations[0].q3dscene.rootEntity;

    Q3DSSceneManager::SceneBuilderParams params;
    params.flags = Q3DSSceneManager::SubPresentation;

    Q3DSPresentation *pres3DS = pres->uipDocument->presentation();
    params.outputSize = QSize(pres3DS->presentationWidth(), pres3DS->presentationHeight());
    params.outputDpr = 1;

    Qt3DRender::QRenderTargetSelector *rtSel = new Qt3DRender::QRenderTargetSelector(fgParent);
    Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
    Qt3DRender::QRenderTargetOutput *color = new Qt3DRender::QRenderTargetOutput;

    color->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
    pres->subPres.colorTex = new Qt3DRender::QTexture2D(entityParent);
    pres->subPres.colorTex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
    pres->subPres.colorTex->setWidth(pres3DS->presentationWidth());
    pres->subPres.colorTex->setHeight(pres3DS->presentationHeight());
    pres->subPres.colorTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    pres->subPres.colorTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    color->setTexture(pres->subPres.colorTex);

    Qt3DRender::QRenderTargetOutput *ds = new Qt3DRender::QRenderTargetOutput;
    ds->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::DepthStencil);
    pres->subPres.dsTex = new Qt3DRender::QTexture2D(entityParent);
    pres->subPres.dsTex->setFormat(Qt3DRender::QAbstractTexture::D24S8);
    pres->subPres.dsTex->setWidth(pres3DS->presentationWidth());
    pres->subPres.dsTex->setHeight(pres3DS->presentationHeight());
    pres->subPres.dsTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    pres->subPres.dsTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    ds->setTexture(pres->subPres.dsTex);

    rt->addOutput(color);
    rt->addOutput(ds);

    rtSel->setTarget(rt);
    params.frameGraphRoot = rtSel;

    QScopedPointer<Q3DSSceneManager> sceneManager(new Q3DSSceneManager(gfxLimits));
    pres->q3dscene = sceneManager->buildScene(pres->uipDocument->presentation(), params);
    if (!pres->q3dscene.rootEntity) {
        Q3DSUtils::showMessage(QObject::tr("Failed to build Qt3D scene for subpresentation"));
        return false;
    }
    pres->sceneManager = sceneManager.take();
    pres->subPres.sceneManager = pres->sceneManager;

    pres->q3dscene.rootEntity->setParent(entityParent);

    return true;
}

int Q3DStudioWindow::presentationCount() const
{
    return m_presentations.count();
}

int Q3DStudioWindow::indexOfSubPresentation(const QString &id) const
{
    for (int i = 0; i < m_presentations.count(); ++i) {
        if (m_presentations[i].subPres.id == id)
            return i;
    }
    return -1;
}

QString Q3DStudioWindow::uipFileName(int index) const
{
    return (index >= 0 && index < m_presentations.count()) ? m_presentations[index].uipFileName : QString();
}

Q3DSUipDocument *Q3DStudioWindow::uipDocument(int index) const
{
    return (index >= 0 && index < m_presentations.count()) ? m_presentations[index].uipDocument : nullptr;
}

Q3DSSceneManager *Q3DStudioWindow::sceneManager(int index) const
{
    return (index >= 0 && index < m_presentations.count()) ? m_presentations[index].sceneManager : nullptr;
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

void Q3DStudioWindow::keyPressEvent(QKeyEvent *e)
{
    // not ideal since the window needs focus which it often won't have. also no keyboard on embedded/mobile.
    if (e->key() == Qt::Key_F12 && !m_presentations.isEmpty()) {
        auto m = m_presentations[0].sceneManager;
        m->setProfileUiVisible(!m->isProfileUiVisible());
    }
}

void Q3DStudioWindow::mouseDoubleClickEvent(QMouseEvent *e)
{
    // Toggle with short double-clicks. This should work both with
    // touch and with mouse emulation via gamepads on Android. Just
    // using a single double-click would be too error-prone.

    if (!m_profilerActivateTimer.isValid()) {
        m_profilerActivateTimer.start();
        return;
    }

    if (m_profilerActivateTimer.restart() < 800
        && e->button() == Qt::LeftButton
        && !m_presentations.isEmpty())
    {
        auto m = m_presentations[0].sceneManager;
        m->setProfileUiVisible(!m->isProfileUiVisible());
    }
}

QT_END_NAMESPACE
