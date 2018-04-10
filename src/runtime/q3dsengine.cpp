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

#include "q3dsengine_p.h"
#include "q3dsuiaparser_p.h"
#include "q3dsuipparser_p.h"
#include "q3dsutils_p.h"
#include "q3dsinputmanager_p.h"

#include <QLoggingCategory>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcontext.h>

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
#include <Qt3DRender/QRenderCapture>
#include <Qt3DRender/QScreenRayCaster>

#include <Qt3DInput/QInputSettings>

#include <Qt3DQuickScene2D/qscene2d.h>

#include <Qt3DCore/private/qaspectengine_p.h>

#ifndef GL_MAX_DRAW_BUFFERS
#define GL_MAX_DRAW_BUFFERS               0x8824
#endif

// The input aspect is disabled, as it's currently not used.
// In addittion to not being used, it's causing random deadlocks in the CI...
#define QT_3DS_ENABLE_INPUT_ASPECT 0

static void initResources()
{
    static bool done = false;
    if (!done) {
        done = true;
        Q_INIT_RESOURCE(q3dsres);
    }
}

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcUip)
Q_DECLARE_LOGGING_CATEGORY(lcPerf)

static const int MAX_LOG_MESSAGE_LENGTH = 1000;
static const int MAX_LOG_LENGTH = 10000;

static Q3DSGraphicsLimits gfxLimits;

static QMutex q3ds_msg_mutex;
static QStringList q3ds_msg_buf;
static QtMessageHandler q3ds_prev_msg_handler = nullptr;
static Q3DSEngine *q3ds_msg_engine = nullptr;

void Q3DSEngine::clearLog()
{
    QMutexLocker locker(&q3ds_msg_mutex);
    q3ds_msg_buf.clear();
}

static void q3ds_msg_handler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (q3ds_prev_msg_handler)
        q3ds_prev_msg_handler(type, ctx, msg);

    QMutexLocker locker(&q3ds_msg_mutex);

    QString decoratedMsg;
    if (ctx.category) {
        decoratedMsg += QString::fromUtf8(ctx.category);
        decoratedMsg += QLatin1String(": ");
    }
    decoratedMsg += msg.left(MAX_LOG_MESSAGE_LENGTH);

    while (q3ds_msg_buf.count() > MAX_LOG_LENGTH)
        q3ds_msg_buf.removeFirst();

    q3ds_msg_buf.append(decoratedMsg);

    const bool canLog = q3ds_msg_engine && q3ds_msg_engine->presentationCount() && q3ds_msg_engine->sceneManager(0);
    if (canLog) {
        Q3DSSceneManager *sm = q3ds_msg_engine->sceneManager(0);
        static Q3DSSceneManager *active_sm = nullptr;
        // send buffered messages if this is the first time we see the scene manager
        if (active_sm != sm) {
            active_sm = sm;
            for (const QString &oldMsg : q3ds_msg_buf)
                sm->addLog(oldMsg);
        }
        sm->addLog(decoratedMsg);
    }
}

Q3DSEngine::Q3DSEngine()
{
    initResources();

    QMutexLocker locker(&q3ds_msg_mutex);
    q3ds_msg_engine = this;
    // Install the message handler once, it is then set for ever. Engines come
    // and go and their construction and destruction may overlap even. So don't
    // be smarter than this.
    if (!q3ds_prev_msg_handler) {
        q3ds_prev_msg_handler = qInstallMessageHandler(q3ds_msg_handler);
        // Here we would also need to enable all severity levels for q3ds.*
        // (since we want to see qCDebugs in release builds etc.) but there's
        // no need since that's the default since our category names do not
        // start with qt.*
    }
}

Q3DSEngine::~Q3DSEngine()
{
    destroy();

    QMutexLocker locker(&q3ds_msg_mutex);
    if (q3ds_msg_engine == this)
        q3ds_msg_engine = nullptr;
}

static void initGraphicsLimits(QOpenGLContext *ctx)
{
    qDebug() << "Actual format is" << ctx->format();
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

    gfxLimits.format = ctx->format();

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
        return ctx.format();
    }

    // Basic: Stick with 3.3 for now to keep less fortunate, Mesa-based systems happy
    fmt.setVersion(3, 3);
    ctx.setFormat(fmt);
    if (ctx.create() && ctx.format().version() >= qMakePair(3, 3)) {
        qDebug("Requesting OpenGL 3.3 core context succeeded");
        initGraphicsLimits(&ctx);
        return ctx.format();
    }

    qDebug("Impending doom");
    gfxLimits.versionedContextFailed = true;
    return fmt;
}

static QSurfaceFormat findIdealGLESVersion()
{
    QSurfaceFormat fmt;

    // Advanced: Try 3.1 (so we get compute shaders for instance)
    fmt.setVersion(3, 1);
    QOpenGLContext ctx;
    ctx.setFormat(fmt);

    // Now, it's important to check the format with the actual version (parsed
    // back from GL_VERSION) since some implementations, ANGLE for instance,
    // are broken and succeed the 3.1 context request even though they only
    // support and return a 3.0 context. This is against the spec since 3.0 is
    // obviously not backwards compatible with 3.1, but hey...
    if (ctx.create() && ctx.format().version() >= qMakePair(3, 1)) {
        qDebug("Requesting OpenGL ES 3.1 context succeeded");
        initGraphicsLimits(&ctx);
        return ctx.format();
    }

    // Basic: OpenGL ES 3.0 is a hard requirement at the moment since we can
    // only generate 300 es shaders, uniform buffers are mandatory.
    fmt.setVersion(3, 0);
    ctx.setFormat(fmt);
    if (ctx.create() && ctx.format().version() >= qMakePair(3, 0)) {
        qDebug("Requesting OpenGL ES 3.0 context succeeded");
        initGraphicsLimits(&ctx);
        return ctx.format();
    }

    qDebug("Impending doom");
    gfxLimits.versionedContextFailed = true;
    return fmt;
}

QSurfaceFormat Q3DSEngine::surfaceFormat()
{
    static const QSurfaceFormat f = [] {
        QSurfaceFormat fmt;
        if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) { // works in dynamic gl builds too because there's a qguiapp already
            fmt = findIdealGLVersion();
        } else {
            fmt = findIdealGLESVersion();
        }
        fmt.setDepthBufferSize(24);
        fmt.setStencilBufferSize(8);
        // Ignore MSAA here as that is a per-layer setting.
        return fmt;
    }();
    return f;
}

namespace Q3DS {
QSurfaceFormat surfaceFormat()
{
    return Q3DSEngine::surfaceFormat();
}

Q3DSGraphicsLimits graphicsLimits()
{
    Q3DSEngine::surfaceFormat();
    return gfxLimits;
}
}

void Q3DSEngine::createAspectEngine()
{
    m_aspectEngine.reset(new Qt3DCore::QAspectEngine);
    if (!m_flags.testFlag(WithoutRenderAspect))
        m_aspectEngine->registerAspect(new Qt3DRender::QRenderAspect);
#if QT_3DS_ENABLE_INPUT_ASPECT
    m_aspectEngine->registerAspect(new Qt3DInput::QInputAspect);
#endif
    m_aspectEngine->registerAspect(new Qt3DAnimation::QAnimationAspect);
    m_aspectEngine->registerAspect(new Qt3DLogic::QLogicAspect);
}

QString Q3DSEngine::source() const
{
    return m_source;
}

qint64 Q3DSEngine::behaviorLoadTimeMsecs() const
{
    return m_behaviorLoadTime;
}

qint64 Q3DSEngine::totalLoadTimeMsecs() const
{
    return m_loadTime;
}

void Q3DSEngine::setFlags(Flags flags)
{
    // applies to the next setSource()
    m_flags = flags;
}

void Q3DSEngine::setFlag(Flag flag, bool enabled)
{
    // applies to the next setSource()
    m_flags.setFlag(flag, enabled);
}

void Q3DSEngine::setSharedSubPresentationQmlEngine(QQmlEngine *qmlEngine)
{
    m_qmlSubPresentationEngine = qmlEngine;
    m_ownsQmlSubPresentationEngine = false;
}

void Q3DSEngine::setSharedBehaviorQmlEngine(QQmlEngine *qmlEngine)
{
    m_behaviorQmlEngine = qmlEngine;
    m_ownsBehaviorQmlEngine = false;
}

bool Q3DSEngine::setSource(const QString &uipOrUiaFileName, QString *error)
{
    if (!m_surface) {
        Q3DSUtils::showMessage(tr("setSource: Cannot be called without setSurface"));
        return false;
    }

    // What we want from Q3DSUtils::showMessage() is to do a qWarning and then
    // collect the string in 'error' as well. No dialog boxes.
    // Except that error == null should still lead to the default behavior.
    Q3DSUtilsMessageRedirect msgRedir(error);

    m_sourceLoadTimer.start();

    // no check for m_source being the same - must reload no matter what

    prepareForReload();

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
        UipPresentation pres;
        pres.uipDocument = new Q3DSUipDocument;
        pres.uipDocument->setSource(m_source);
        m_uipPresentations.append(pres);
    } else {
        Q3DSUiaParser uiaParser;
        Q3DSUiaParser::Uia uiaDoc = uiaParser.parse(uia);
        if (!parseUiaDocument(uiaDoc, sourcePrefix)) {
            Q3DSUtils::showMessage(QObject::tr("Failed to parse application file"));
            return false;
        }
    }

    return loadPresentations();
}

bool Q3DSEngine::setDocument(const Q3DSUipDocument &uipDocument, QString *error)
{
    if (!m_surface) {
        Q3DSUtils::showMessage(tr("setDocument: Cannot be called without setSurface"));
        return false;
    }

    Q3DSUtilsMessageRedirect msgRedir(error);

    m_sourceLoadTimer.start();

    prepareForReload();

    if (!uipDocument.source().isEmpty()) {
        // When document has filename set, load presentation using it
        return setSource(uipDocument.source());
    } else if (!uipDocument.sourceData().isEmpty()) {
        // When document has data, load presentation using it
        UipPresentation pres;
        pres.uipDocument = new Q3DSUipDocument;
        pres.uipDocument->setId(uipDocument.id());
        pres.uipDocument->setSourceData(uipDocument.sourceData());
        m_uipPresentations.append(pres);
        return loadPresentations();
    }

    Q3DSUtils::showMessage(QObject::tr(
               "Unable to load uip document without source() or sourceData()"));
    return false;
}

bool Q3DSEngine::setDocument(const Q3DSUiaDocument &uiaDocument, QString *error)
{
    if (!m_surface) {
        Q3DSUtils::showMessage(tr("setDocument: Cannot be called without setSurface"));
        return false;
    }

    Q3DSUtilsMessageRedirect msgRedir(error);

    m_sourceLoadTimer.start();

    prepareForReload();

    if (uiaDocument.uipDocuments().isEmpty()) {
        Q3DSUtils::showMessage(QObject::tr("Unable to load uia document without uip subdocuments"));
        return false;
    }

    for (const Q3DSUipDocument &doc : uiaDocument.uipDocuments()) {
        UipPresentation pres;
        pres.uipDocument = new Q3DSUipDocument;
        pres.uipDocument->setId(doc.id());
        pres.uipDocument->setSource(doc.source());
        pres.uipDocument->setSourceData(doc.sourceData());
        m_uipPresentations.append(pres);
    }
    for (const Q3DSQmlDocument &doc : uiaDocument.qmlDocuments()) {
        QmlPresentation pres;
        pres.qmlDocument = new Q3DSQmlDocument;
        pres.qmlDocument->setId(doc.id());
        pres.qmlDocument->setSource(doc.source());
        pres.qmlDocument->setSourceData(doc.sourceData());
        m_qmlPresentations.append(pres);
    }

    for (int i = 0; i < m_uipPresentations.size(); ++i) {
        auto doc = m_uipPresentations[i];
        doc.subPres.id = doc.uipDocument->id();
        if (!uiaDocument.initialDocumentId().isEmpty() &&
                doc.uipDocument->id() == uiaDocument.initialDocumentId()) {
            // initial (main) presentation must be first
            m_uipPresentations.move(i, 0);
        }
        qCDebug(lcUip, "Registered subpresentation %s as %s",
                qPrintable(doc.uipDocument->source()), qPrintable(doc.uipDocument->id()));
    }

    for (int i = 0; i < m_qmlPresentations.size(); ++i) {
        auto &doc = m_qmlPresentations[i];
        doc.subPres.id = doc.qmlDocument->id();
        qCDebug(lcUip, "Registered qml subpresentation %s as %s",
                qPrintable(doc.qmlDocument->source()), qPrintable(doc.qmlDocument->id()));
    }

    m_dataInputEntries = uiaDocument.dataInputEntries();
    qCDebug(lcUip, "Registered %d data input entries", m_dataInputEntries.count());

    return loadPresentations();
}

bool Q3DSEngine::setPresentations(const QVector<Q3DSUipPresentation *> &presentations)
{
    if (!m_surface) {
        Q3DSUtils::showMessage(tr("setPresentations: Cannot be called without setSurface"));
        return false;
    }

    m_sourceLoadTimer.start();

    prepareForReload();
    if (presentations.isEmpty())
        return false;

    UipPresentation mainPres;
    mainPres.presentation = presentations[0];
    if (mainPres.presentation->name().isEmpty())
        mainPres.presentation->setName(QLatin1String("main"));
    mainPres.presentation->setDataInputEntries(&m_dataInputEntries);
    if (buildUipPresentationScene(&mainPres))
        m_uipPresentations.append(mainPres);
    else
        return false;

    for (int i = 1; i < presentations.count(); ++i) {
        // ### this isn't enough, needs ids and such
        UipPresentation subPres;
        subPres.presentation = presentations[i];
        subPres.presentation->setDataInputEntries(&m_dataInputEntries);
        if (buildSubUipPresentationScene(&subPres)) {
            m_uipPresentations.append(subPres);
        } else {
            m_uipPresentations.clear();
            return false;
        }
    }

    // ### qml?

    finalizePresentations();

    return true;
}

bool Q3DSEngine::loadPresentations()
{
    if (m_uipPresentations.isEmpty())
        return false;

    if (!loadUipPresentation(&m_uipPresentations[0])) {
        m_uipPresentations.clear();
        return false;
    }

    for (int i = 1; i < m_uipPresentations.count(); ++i)
        loadSubUipPresentation(&m_uipPresentations[i]);

    for (QmlPresentation &qmlDocument : m_qmlPresentations)
        loadSubQmlPresentation(&qmlDocument);

    finalizePresentations();

    return true;
}

void Q3DSEngine::finalizePresentations()
{
    QVector<Q3DSSubPresentation> subPresentations;
    for (const UipPresentation &pres : m_uipPresentations) {
        if (!pres.subPres.id.isEmpty() && pres.subPres.colorTex)
            subPresentations.append(pres.subPres);
    }
    for (const QmlPresentation &pres : m_qmlPresentations) {
        if (!pres.subPres.id.isEmpty() && pres.subPres.colorTex)
            subPresentations.append(pres.subPres);
    }

    m_uipPresentations[0].sceneManager->finalizeMainScene(subPresentations);

    loadBehaviors();

    if (m_aspectEngine.isNull())
        createAspectEngine();

    m_loadTime = m_sourceLoadTimer.elapsed();
    qCDebug(lcPerf, "Total setSource time (incl. subpresentations + Qt3D scene building): %lld ms", m_loadTime);

    emit presentationLoaded();
}

bool Q3DSEngine::loadUipPresentation(UipPresentation *pres)
{
    Q_ASSERT(pres);
    Q_ASSERT(pres->uipDocument);
    // Parse.
    if (!parseUipDocument(pres)) {
        Q3DSUtils::showMessage(QObject::tr("Failed to parse main presentation"));
        return false;
    }
    return buildUipPresentationScene(pres);
}

bool Q3DSEngine::buildUipPresentationScene(UipPresentation *pres)
{
    // Presentation is ready. Build the Qt3D scene. This will also activate the first sub-slide.
    Q3DSSceneManager::SceneBuilderParams params;
    params.flags = 0;
    if (m_flags.testFlag(Force4xMSAA))
        params.flags |= Q3DSSceneManager::Force4xMSAA;
    if (m_flags.testFlag(EnableProfiling))
        params.flags |= Q3DSSceneManager::EnableProfiling;

    // Take the size from the presentation.
    Q3DSUipPresentation *pres3DS = pres->presentation;
    m_implicitSize = QSize(pres3DS->presentationWidth(), pres3DS->presentationHeight());
    if (m_implicitSize.isEmpty())
        m_implicitSize = QSize(800, 480);

    // If a size was provided via resize(), stick to that.
    QSize effectiveSize;
    qreal effectiveDpr;
    if (!m_size.isEmpty()) {
        effectiveSize = m_size;
        effectiveDpr = m_dpr;
    } else {
        effectiveSize = m_implicitSize;
        effectiveDpr = 1; // ### this is questionable
    }

    params.outputSize = effectiveSize;
    params.outputDpr = effectiveDpr;
    params.surface = m_surface;
    params.engine = this;

    QScopedPointer<Q3DSSceneManager> sceneManager(new Q3DSSceneManager);
    pres->q3dscene = sceneManager->buildScene(pres->presentation, params);
    if (!pres->q3dscene.rootEntity) {
        Q3DSUtils::showMessage(QObject::tr("Failed to build Qt3D scene"));
        return false;
    }
    pres->sceneManager = sceneManager.take();

#if QT_3DS_ENABLE_INPUT_ASPECT
    // Input (Qt3D).
    Qt3DInput::QInputSettings *inputSettings = new Qt3DInput::QInputSettings;
    inputSettings->setEventSource(m_surface);
    pres->q3dscene.rootEntity->addComponent(inputSettings);
#endif

    // Input (profiling UI). Do not let it capture events from the window,
    // instead use a dummy object to which we (the engine) sends events as it
    // sees fit. This ensures a single path of receiving input events.
    pres->sceneManager->setProfileUiInputEventSource(&m_profileUiEventSource);

    // Generate a resize to make sure everything size-related gets updated.
    // (avoids issues with camera upon loading new scenes)
    pres->sceneManager->updateSizes(effectiveSize, effectiveDpr);

    // Expose update signal
    connect(pres->q3dscene.frameAction, &Qt3DLogic::QFrameAction::triggered, this, [this](float dt) {
        behaviorFrameUpdate(dt);
        emit nextFrameStarting();
    });

    // Insert Render Capture node to framegraph for grabbing
    m_capture = new Qt3DRender::QRenderCapture();
    pres->q3dscene.frameGraphRoot->setParent(m_capture);
    pres->q3dscene.frameGraphRoot = m_capture;
    pres->q3dscene.renderSettings->setActiveFrameGraph(pres->q3dscene.frameGraphRoot);

    // Setup picking backend
    pres->q3dscene.renderSettings->pickingSettings()->setPickMethod(Qt3DRender::QPickingSettings::PrimitivePicking);

    // Set new root entity if the engine was already up and running.
    if (!m_aspectEngine.isNull())
        m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(pres->q3dscene.rootEntity));

    if (m_autoStart)
        pres->sceneManager->prepareAnimators();

    return true;
}

bool Q3DSEngine::loadSubUipPresentation(UipPresentation *pres)
{
    Q_ASSERT(pres);
    Q_ASSERT(pres->uipDocument);
    // Parse.
    if (!parseUipDocument(pres)) {
        Q3DSUtils::showMessage(QObject::tr("Failed to parse subpresentation"));
        return false;
    }
    return buildSubUipPresentationScene(pres);
}

bool Q3DSEngine::buildSubUipPresentationScene(UipPresentation *pres)
{
    Qt3DRender::QFrameGraphNode *fgParent = m_uipPresentations[0].q3dscene.subPresFrameGraphRoot;
    Qt3DCore::QNode *entityParent = m_uipPresentations[0].q3dscene.rootEntity;

    Q3DSSceneManager::SceneBuilderParams params;
    params.flags = Q3DSSceneManager::SubPresentation;
    if (m_flags.testFlag(EnableProfiling))
        params.flags |= Q3DSSceneManager::EnableProfiling;

    Q3DSUipPresentation *pres3DS = pres->presentation;
    params.outputSize = QSize(pres3DS->presentationWidth(), pres3DS->presentationHeight());
    params.outputDpr = 1;
    params.engine = this;

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

    QScopedPointer<Q3DSSceneManager> sceneManager(new Q3DSSceneManager);
    pres->q3dscene = sceneManager->buildScene(pres->presentation, params);
    if (!pres->q3dscene.rootEntity) {
        Q3DSUtils::showMessage(QObject::tr("Failed to build Qt3D scene for subpresentation"));
        return false;
    }
    pres->sceneManager = sceneManager.take();
    pres->subPres.sceneManager = pres->sceneManager;

    pres->q3dscene.rootEntity->setParent(entityParent);

    if (m_autoStart)
        pres->sceneManager->prepareAnimators();

    return true;
}

bool Q3DSEngine::loadSubQmlPresentation(QmlPresentation *pres)
{
    Q_ASSERT(pres);
    Q_ASSERT(pres->qmlDocument);
    if (!m_qmlSubPresentationEngine) {
        m_qmlSubPresentationEngine = new QQmlEngine;
        m_ownsQmlSubPresentationEngine = true;
    }

    Qt3DCore::QNode *entityParent = m_uipPresentations[0].q3dscene.rootEntity;
    QQmlComponent *component;
    QString qmlSource = pres->qmlDocument->source();
    if (!qmlSource.isEmpty()) {
        QUrl sourceUrl;
        // Support also files from resources
        QFileInfo fi(qmlSource);
        if (fi.isAbsolute()) {
            sourceUrl = QUrl::fromLocalFile(fi.absoluteFilePath());
        } else {
            sourceUrl = qmlSource;
        }
        component = new QQmlComponent(m_qmlSubPresentationEngine, sourceUrl);
    } else {
        component = new QQmlComponent(m_qmlSubPresentationEngine);
        component->setData(pres->qmlDocument->sourceData(), QUrl());
    }

    if (component->isReady()) {
        int width = 0;
        int height = 0;
        QQuickItem *item = static_cast<QQuickItem *>(component->create());
        if (!item) {
            qCDebug(lcUip, "Failed to load qml. Root is not a quick item.");
            delete component;
            delete item;
            return false;
        }

        pres->scene2d = new Qt3DRender::Quick::QScene2D(entityParent);
        pres->scene2d->setItem(item);
        item->setParent(pres->scene2d);
        Qt3DRender::QRenderTargetOutput *color
                = new Qt3DRender::QRenderTargetOutput(pres->scene2d);

        color->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
        pres->subPres.colorTex = new Qt3DRender::QTexture2D(entityParent);
        pres->subPres.colorTex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
        pres->subPres.colorTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
        pres->subPres.colorTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
        color->setTexture(pres->subPres.colorTex);

        pres->scene2d->setOutput(color);

        width = int(item->width());
        height = int(item->height());
        if (!width) width = 128;
        if (!height) height = 128;

        pres->subPres.colorTex->setWidth(width);
        pres->subPres.colorTex->setHeight(height);

        delete component;
    } else if (component->isLoading()) {
        int width = 128;
        int height = 128;

        /* Must create these already here in order to link the texture to wherever it is used */
        pres->scene2d = new Qt3DRender::Quick::QScene2D(entityParent);
        Qt3DRender::QRenderTargetOutput *color
                = new Qt3DRender::QRenderTargetOutput(pres->scene2d);

        color->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
        pres->subPres.colorTex = new Qt3DRender::QTexture2D(entityParent);
        pres->subPres.colorTex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
        pres->subPres.colorTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
        pres->subPres.colorTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
        color->setTexture(pres->subPres.colorTex);

        pres->scene2d->setOutput(color);

        pres->subPres.colorTex->setWidth(width);
        pres->subPres.colorTex->setHeight(height);

        QObject::connect(component, &QQmlComponent::statusChanged,
                         [&, component, pres](QQmlComponent::Status status) {
            if (status == QQmlComponent::Status::Ready) {
                QQuickItem *item = static_cast<QQuickItem *>(component->create());
                if (item) {
                    pres->scene2d->setItem(item);
                    item->setParent(pres->scene2d);
                    int width = int(item->width());
                    int height = int(item->height());

                    pres->subPres.colorTex->setWidth(width);
                    pres->subPres.colorTex->setHeight(height);
                } else {
                    qCDebug(lcUip, "Failed to load qml. Root is not a quick item.");
                    delete item;
                    pres->scene2d->deleteLater();
                    component->deleteLater();
                }
            } else {
                qCDebug(lcUip, "Failed to load qml: %s", qPrintable(component->errorString()));
                pres->scene2d->deleteLater();
                component->deleteLater();
            }
        });
    } else {
        qCDebug(lcUip, "Failed to load qml: %s", qPrintable(component->errorString()));
        delete component;
        return false;
    }

    return true;
}

bool Q3DSEngine::parseUipDocument(UipPresentation *pres)
{
    if (pres->presentation) {
        delete pres->presentation;
        pres->presentation = nullptr;
    }

    Q3DSUipParser parser;
    if (!pres->uipDocument->source().isEmpty())
        pres->presentation = parser.parse(pres->uipDocument->source(), pres->uipDocument->id());
    else if (!pres->uipDocument->sourceData().isEmpty())
        pres->presentation = parser.parseData(pres->uipDocument->sourceData(), pres->uipDocument->id());

    // Expose the data input metadata to the presentation.
    if (pres->presentation)
        pres->presentation->setDataInputEntries(&m_dataInputEntries);

    return (pres->presentation != nullptr);
}

bool Q3DSEngine::parseUiaDocument(Q3DSUiaParser::Uia &uiaDoc, const QString &sourcePrefix)
{
    if (!uiaDoc.isValid())
        return false;

    for (const Q3DSUiaParser::Uia::Presentation &p : uiaDoc.presentations) {
        if (p.type == Q3DSUiaParser::Uia::Presentation::Uip) {
            Q3DSUipDocument *uipDoc = new Q3DSUipDocument;
            // assume the .uip name in the .uia is relative to the .uia's location
            uipDoc->setSource(sourcePrefix + p.source);
            uipDoc->setId(p.id);
            UipPresentation pres;
            pres.subPres.id = p.id;
            pres.uipDocument = uipDoc;
            qCDebug(lcUip, "Registered subpresentation %s as %s",
                    qPrintable(uipDoc->source()), qPrintable(p.id));
            // initial (main) presentation must be m_uipPresentations[0]
            if (p.id == uiaDoc.initialPresentationId)
                m_uipPresentations.prepend(pres);
            else
                m_uipPresentations.append(pres);
        } else if (p.type == Q3DSUiaParser::Uia::Presentation::Qml) {
            Q3DSQmlDocument *qmlDoc = new Q3DSQmlDocument;
            qmlDoc->setSource(sourcePrefix + p.source);
            qmlDoc->setId(p.id);
            QmlPresentation pres;
            pres.subPres.id = p.id;
            pres.qmlDocument = qmlDoc;
            qCDebug(lcUip, "Registered qml subpresentation %s as %s",
                    qPrintable(qmlDoc->source()), qPrintable(p.id));
            m_qmlPresentations.append(pres);
        }
    }
    if (m_uipPresentations.isEmpty())
        return false;

    m_dataInputEntries = uiaDoc.dataInputEntries;
    qCDebug(lcUip, "Registered %d data input entries", m_dataInputEntries.count());

    return true;
}

void Q3DSEngine::destroy()
{
    for (const auto &h : m_behaviorHandles)
        destroyBehaviorHandle(h);
    m_behaviorHandles.clear();

    if (m_ownsBehaviorQmlEngine) {
        m_ownsBehaviorQmlEngine = false;
        delete m_behaviorQmlEngine;
    }
    m_behaviorQmlEngine = nullptr;

    for (UipPresentation &pres : m_uipPresentations) {
        delete pres.sceneManager;
        // Nulling out may seem unnecessary due to the clear() below but it is
        // a must - code invoked from a scenemanager dtor may still call
        // presentation(). Make sure they see a null scenemanager et al.
        pres.sceneManager = nullptr;
        delete pres.uipDocument;
        pres.uipDocument = nullptr;
        delete pres.presentation;
        pres.presentation = nullptr;
    }
    m_uipPresentations.clear();
    m_capture = nullptr;

    for (QmlPresentation &pres : m_qmlPresentations)
        delete pres.qmlDocument;

    m_qmlPresentations.clear();

    if (m_ownsQmlSubPresentationEngine) {
        m_ownsQmlSubPresentationEngine = false;
        delete m_qmlSubPresentationEngine;
    }
    m_qmlSubPresentationEngine = nullptr;

    // wish I knew why this is needed. Qt 3D tends not to shut down its threads correctly on exit otherwise.
    if (m_aspectEngine)
        Qt3DCore::QAspectEnginePrivate::get(m_aspectEngine.data())->exitSimulationLoop();

    m_aspectEngine.reset();
}

void Q3DSEngine::prepareForReload()
{
    for (const auto &h : m_behaviorHandles)
        destroyBehaviorHandle(h);
    m_behaviorHandles.clear();

    if (!m_uipPresentations.isEmpty()) {
        for (UipPresentation &pres : m_uipPresentations) {
            if (pres.sceneManager)
                pres.sceneManager->prepareEngineReset();
        }

        Q3DSSceneManager::prepareEngineResetGlobal();
        Qt3DCore::QAspectEnginePrivate::get(m_aspectEngine.data())->exitSimulationLoop();
        createAspectEngine();

        for (UipPresentation &pres : m_uipPresentations) {
            delete pres.sceneManager;
        }
        m_uipPresentations.clear();
    } else {
        Q3DSSceneManager::prepareEngineResetGlobal();
    }
}

int Q3DSEngine::presentationCount() const
{
    return m_uipPresentations.count();
}

QString Q3DSEngine::uipFileName(int index) const
{
    return (index >= 0 && index < m_uipPresentations.count()) ?
                m_uipPresentations[index].uipDocument->source() : QString();
}

Q3DSUipDocument *Q3DSEngine::uipDocument(int index) const
{
    return (index >= 0 && index < m_uipPresentations.count()) ?
                m_uipPresentations[index].uipDocument : nullptr;
}

Q3DSUipPresentation *Q3DSEngine::presentation(int index) const
{
    return (index >= 0 && index < m_uipPresentations.count()) ?
                m_uipPresentations[index].presentation : nullptr;
}

Q3DSUipPresentation *Q3DSEngine::presentationByName(const QString &name) const
{
    for (const auto &p : m_uipPresentations) {
        if (p.presentation && p.presentation->name() == name)
            return p.presentation;
    }
    return nullptr;
}

Q3DSSceneManager *Q3DSEngine::sceneManager(int index) const
{
    return (index >= 0 && index < m_uipPresentations.count()) ?
                m_uipPresentations[index].sceneManager : nullptr;
}

Qt3DCore::QAspectEngine *Q3DSEngine::aspectEngine() const
{
    return m_aspectEngine.data();
}

Qt3DCore::QEntity *Q3DSEngine::rootEntity() const
{
    return m_uipPresentations.isEmpty() ? nullptr : m_uipPresentations[0].q3dscene.rootEntity;
}

void Q3DSEngine::setOnDemandRendering(bool enabled)
{
    m_uipPresentations[0].q3dscene.renderSettings->setRenderPolicy(enabled ?
        Qt3DRender::QRenderSettings::OnDemand : Qt3DRender::QRenderSettings::Always);
}

QSize Q3DSEngine::implicitSize() const
{
    return m_implicitSize;
}

void Q3DSEngine::setSurface(QObject *surface)
{
    m_surface = surface;
}

QObject *Q3DSEngine::surface() const
{
    return m_surface;
}

bool Q3DSEngine::start()
{
    if (!m_uipPresentations.isEmpty()) {
        Q_ASSERT(!m_aspectEngine.isNull());
        if (m_aspectEngine->rootEntity() != m_uipPresentations[0].q3dscene.rootEntity)
            m_aspectEngine->setRootEntity(
                        Qt3DCore::QEntityPtr(m_uipPresentations[0].q3dscene.rootEntity));
        return true;
    }
    return false;
}

void Q3DSEngine::resize(const QSize &size, qreal dpr, bool forceSynchronous)
{
    m_size = size;
    m_dpr = dpr;
    if (!m_uipPresentations.isEmpty())
        m_uipPresentations[0].sceneManager->updateSizes(m_size, m_dpr, forceSynchronous);
}

void Q3DSEngine::setAutoStart(bool autoStart)
{
    m_autoStart = autoStart;
}

bool Q3DSEngine::isProfileUiVisible() const
{
    Q3DSSceneManager *sm = !m_uipPresentations.isEmpty() ? m_uipPresentations[0].sceneManager : nullptr;
    return sm && sm->isProfileUiVisible();
}

void Q3DSEngine::handleKeyPressEvent(QKeyEvent *e)
{
    bool forwardEvent = isProfileUiVisible();

    if (m_autoToggleProfileUi) {
        Q3DSSceneManager *sm = !m_uipPresentations.isEmpty() ? m_uipPresentations[0].sceneManager : nullptr;

        // Built-in shortcuts. These work even when there is only a plain
        // QWindow and no external UI is provided to control this. The downside
        // is that the window needs focus (so may need a click first).

        Qt::KeyboardModifiers mods = e->modifiers();
        if (sm && e->key() == Qt::Key_F10 && mods == Qt::NoModifier) {
            sm->setProfileUiVisible(!sm->isProfileUiVisible());
            forwardEvent = false;
        }

        if (sm && e->key() == Qt::Key_QuoteLeft) {
            const bool v = !sm->isProfileUiVisible();
            sm->setProfileUiVisible(v, v);
            forwardEvent = false; // prevent the backtick from showing up in the console
        }

        if (sm && e->key() == Qt::Key_F10 && mods == Qt::AltModifier) {
            m_profileUiScale -= 0.2f;
            sm->configureProfileUi(m_profileUiScale);
            forwardEvent = false;
        }

        if (sm && e->key() == Qt::Key_F10 && mods == Qt::ControlModifier) {
            m_profileUiScale += 0.2f;
            sm->configureProfileUi(m_profileUiScale);
            forwardEvent = false;
        }
    }

    if (forwardEvent)
        QCoreApplication::sendEvent(&m_profileUiEventSource, e);
}

void Q3DSEngine::handleKeyReleaseEvent(QKeyEvent *e)
{
    if (isProfileUiVisible())
        QCoreApplication::sendEvent(&m_profileUiEventSource, e);
}

void Q3DSEngine::handleMousePressEvent(QMouseEvent *e)
{
    m_lastMousePressPos = e->pos();

    if (isProfileUiVisible()) {
        QCoreApplication::sendEvent(&m_profileUiEventSource, e);
        return;
    }

    if (m_uipPresentations.isEmpty())
        return;

    auto inputManager = m_uipPresentations[0].sceneManager->inputManager();
    inputManager->handleMousePressEvent(e);
}

void Q3DSEngine::handleMouseMoveEvent(QMouseEvent *e)
{
    if (isProfileUiVisible()) {
        QCoreApplication::sendEvent(&m_profileUiEventSource, e);
        return;
    }

    if (m_uipPresentations.isEmpty())
        return;

    auto inputManager = m_uipPresentations[0].sceneManager->inputManager();
    inputManager->handleMouseMoveEvent(e);
}

void Q3DSEngine::handleMouseReleaseEvent(QMouseEvent *e)
{
    m_lastMousePressPos = e->pos();

    if (isProfileUiVisible()) {
        QCoreApplication::sendEvent(&m_profileUiEventSource, e);
        return;
    }

    if (m_uipPresentations.isEmpty())
        return;

    auto inputManager = m_uipPresentations[0].sceneManager->inputManager();
    inputManager->handleMouseReleaseEvent(e);
}

void Q3DSEngine::handleMouseDoubleClickEvent(QMouseEvent *e)
{
    if (isProfileUiVisible())
        QCoreApplication::sendEvent(&m_profileUiEventSource, e);

    if (!m_autoToggleProfileUi)
        return;

    // Toggle with short double-clicks. This should work both with
    // touch and with mouse emulation via gamepads on Android. Just
    // using a single double-click would be too error-prone.

    if (!m_profilerActivateTimer.isValid()) {
        m_profilerActivateTimer.start();
        return;
    }

    if (m_profilerActivateTimer.restart() < 800
        && e->button() == Qt::LeftButton
        && !m_uipPresentations.isEmpty())
    {
        auto m = m_uipPresentations[0].sceneManager;
        m->setProfileUiVisible(!m->isProfileUiVisible());
    }
}

#if QT_CONFIG(wheelevent)
void Q3DSEngine::handleWheelEvent(QWheelEvent *e)
{
    if (isProfileUiVisible())
        QCoreApplication::sendEvent(&m_profileUiEventSource, e);
}
#endif

void Q3DSEngine::requestGrab()
{
    if (m_capture) {
        Qt3DRender::QRenderCaptureReply *captureReply = m_capture->requestCapture();
        m_captureConnections.insert(captureReply, QObject::connect(captureReply, &Qt3DRender::QRenderCaptureReply::completed, [=](){
            QObject::disconnect(m_captureConnections.value(captureReply));
            m_captureConnections.remove(captureReply);
            emit grabReady(captureReply->image());
            delete captureReply;
        }));
    }
}

void Q3DSEngine::setProfileUiVisible(bool visible)
{
    if (!m_uipPresentations.isEmpty() && m_uipPresentations[0].sceneManager)
        m_uipPresentations[0].sceneManager->setProfileUiVisible(visible);
}

void Q3DSEngine::configureProfileUi(float scale)
{
    if (!m_uipPresentations.isEmpty() && m_uipPresentations[0].sceneManager) {
        m_profileUiScale = scale;
        m_uipPresentations[0].sceneManager->configureProfileUi(scale);
    }
}

void Q3DSEngine::setDataInputValue(const QString &name, const QVariant &value)
{
    for (const UipPresentation &pres : qAsConst(m_uipPresentations)) {
        if (pres.sceneManager)
            pres.sceneManager->setDataInputValue(name, value);
    }
}

void Q3DSEngine::fireEvent(Q3DSGraphObject *target, Q3DSUipPresentation *presentation, const QString &event)
{
    for (const UipPresentation &pres : qAsConst(m_uipPresentations)) {
        if (pres.presentation == presentation) {
            if (pres.sceneManager)
                pres.sceneManager->queueEvent(Q3DSGraphObject::Event(target, event));
            break;
        }
    }
}

void Q3DSEngine::goToTime(Q3DSGraphObject *context, Q3DSUipPresentation *presentation, float milliseconds)
{
    for (const UipPresentation &pres : qAsConst(m_uipPresentations)) {
        if (pres.presentation == presentation) {
            if (pres.sceneManager)
                pres.sceneManager->goToTime(context, milliseconds);
            break;
        }
    }
}

void Q3DSEngine::goToSlideByName(Q3DSGraphObject *context, Q3DSUipPresentation *presentation, const QString &name)
{
    for (const UipPresentation &pres : qAsConst(m_uipPresentations)) {
        if (pres.presentation == presentation) {
            if (pres.sceneManager)
                pres.sceneManager->changeSlideByName(context, name);
            break;
        }
    }
}

void Q3DSEngine::goToSlideByIndex(Q3DSGraphObject *context, Q3DSUipPresentation *presentation, int index)
{
    for (const UipPresentation &pres : qAsConst(m_uipPresentations)) {
        if (pres.presentation == presentation) {
            if (pres.sceneManager)
                pres.sceneManager->changeSlideByIndex(context, index);
            break;
        }
    }
}

void Q3DSEngine::goToSlideByDirection(Q3DSGraphObject *context, Q3DSUipPresentation *presentation, bool next, bool wrap)
{
    for (const UipPresentation &pres : qAsConst(m_uipPresentations)) {
        if (pres.presentation == presentation) {
            if (pres.sceneManager)
                pres.sceneManager->changeSlideByDirection(context, next, wrap);
            break;
        }
    }
}

void Q3DSEngine::loadBehaviorInstance(Q3DSBehaviorInstance *behaviorInstance,
                                      Q3DSUipPresentation *pres,
                                      BehaviorLoadedCallback callback)
{
    auto handleError = [behaviorInstance, callback](const QString &error) {
        qCWarning(lcUip, "Failed to load QML code for behavior instance %s: %s",
                  behaviorInstance->id().constData(), qPrintable(error));
        behaviorInstance->setQmlErrorString(error);
        if (callback)
            callback(behaviorInstance, error);
    };

    QElapsedTimer loadTime;
    loadTime.start();

    const Q3DSBehavior *behavior = behaviorInstance->behavior();
    if (behavior->qmlCode().isEmpty()) {
        handleError(QLatin1String("No QML source code present"));
        return;
    }

    auto component = new QQmlComponent(m_behaviorQmlEngine);
    component->setData(behavior->qmlCode().toUtf8(), behavior->qmlSourceUrl());

    auto buildComponent = [=]() {
        Q3DSBehaviorHandle h;
        h.behaviorInstance = behaviorInstance;
        h.component = component;

        QQmlContext *context = new QQmlContext(m_behaviorQmlEngine, component);
        QObject *obj = component->beginCreate(context);
        if (!obj) {
            handleError(QLatin1String("Failed to create behavior object"));
            delete context;
            return;
        }
        h.object = qobject_cast<Q3DSBehaviorObject *>(obj);
        if (!h.object) {
            handleError(QLatin1String("QML root item is not a Behavior"));
            delete obj;
            delete context;
            return;
        }
        h.object->init(this, pres, behaviorInstance);
        h.updateProperties();
        component->completeCreate();

        m_behaviorHandles.insert(behaviorInstance, h);

        m_behaviorLoadTime += loadTime.elapsed();
        qCDebug(lcUip, "Loaded QML code for behavior %s in %lld ms",
                behaviorInstance->id().constData(), loadTime.elapsed());

        if (callback)
            callback(behaviorInstance, QString());
    };

    if (component->isReady()) {
        buildComponent();
    } else if (component->isLoading()) {
        QObject::connect(component, &QQmlComponent::statusChanged,
                         [=](QQmlComponent::Status status) {
            if (status == QQmlComponent::Status::Ready) {
                buildComponent();
            } else {
                handleError(component->errorString());
                delete component;
            }
        });
    } else {
        handleError(component->errorString());
        delete component;
    }
}

void Q3DSEngine::unloadBehaviorInstance(Q3DSBehaviorInstance *behaviorInstance)
{
    auto it = m_behaviorHandles.find(behaviorInstance);
    if (it != m_behaviorHandles.end()) {
        if (it->behaviorInstance && it->object) {
            qDebug(lcUip, "Unloading QML code for behavior %s", it->behaviorInstance->id().constData());
            emit it->object->deactivate();
        }
        destroyBehaviorHandle(*it);
        m_behaviorHandles.erase(it);
    }
}

void Q3DSEngine::destroyBehaviorHandle(const Q3DSBehaviorHandle &h)
{
    delete h.object;
    delete h.component;
}

void Q3DSEngine::loadBehaviors()
{
    m_behaviorLoadTime = 0;

    struct BehavInstDesc {
        Q3DSBehaviorInstance *behaviorInstance;
        Q3DSUipPresentation *presentation;
    };
    QVector<BehavInstDesc> behaviorInstances;
    for (int i = 0, ie = presentationCount(); i != ie; ++i) {
        BehavInstDesc desc;
        desc.presentation = presentation(i);
        Q3DSUipPresentation::forAllObjectsOfType(desc.presentation->scene(),
                                                 Q3DSGraphObject::Behavior,
                                                 [&behaviorInstances, &desc](Q3DSGraphObject *obj)
        {
            desc.behaviorInstance = static_cast<Q3DSBehaviorInstance *>(obj);
            behaviorInstances.append(desc);
        });
    }

    qCDebug(lcUip, "Found %d behavior instances in total", behaviorInstances.count());
    if (behaviorInstances.isEmpty())
        return;

    if (!m_behaviorQmlEngine) {
        m_behaviorQmlEngine = new QQmlEngine;
        m_ownsBehaviorQmlEngine = true;
    }
    qmlRegisterType<Q3DSBehaviorObject>("QtStudio3D.Behavior", 1, 0, "Behavior");
    qmlRegisterType<Q3DSBehaviorObject, 1>("QtStudio3D.Behavior", 1, 1, "Behavior");
    qmlRegisterType<Q3DSBehaviorObject, 2>("QtStudio3D.Behavior", 2, 0, "Behavior");

    for (const BehavInstDesc &desc : behaviorInstances) {
        if (desc.behaviorInstance->active()) // skip if eyeball==false
            loadBehaviorInstance(desc.behaviorInstance, desc.presentation);
    }
}

void Q3DSBehaviorHandle::updateProperties() const
{
    // Push all custom property values from the behavior instance to the
    // QObject (and so to QML).
    const QVariantMap props = behaviorInstance->customProperties();
    for (auto it = props.cbegin(), itEnd = props.cend(); it != itEnd; ++it) {
        const QByteArray name = it.key().toUtf8();
        const QVariant value = it.value();
        object->setProperty(name.constData(), value);
    }
}

void Q3DSEngine::behaviorFrameUpdate(float dt)
{
    // Called once per frame. Update properties and emit signals on the object
    // when necessary.

    for (auto &h : m_behaviorHandles) {
        h.updateProperties();

        const bool active = h.behaviorInstance->active();

        if (active && !h.initialized) {
            h.initialized = true;
            emit h.object->initialize();
        }

        if (active != h.active) {
            h.active = active;
            if (active)
                emit h.object->activate();
            else
                emit h.object->deactivate(); // this is unreachable in practice, emitted from unloadBehaviorInstance instead
        }

        if (active) {
            h.object->prepareUpdate(dt);
            emit h.object->update();
        }
    }
}

// Object reference format used by behaviors and some public API:
//
// (presentationName:)?(parent|this|(Scene|Slide)(\..)*|(.)*)
//
// An empty string or "this" refers to the parent of the 'thisObject' (e.g. the
// object to which the behavior was attached (parented) to in the editor). This
// is the most common use case.
//
// presentationName is either "main" for the main presentation, or the id from
// the .uia for sub-presentations.
//
// For non-unique names one would specify a full path like "Scene.Layer.Camera".
// "Scene" refers to the scene object, "Slide" to the master slide.
//
// For accessing objects that were renamed to a unique name in the editor, we
// also allow a simple flat reference like "MyCamera" since relying on absolute
// paths is just silly and not necessary at all.
//
// In addition to all the above, referencing by id via #id is also supported.

Q3DSGraphObject *Q3DSEngine::findObjectByHashIdOrNameOrPath(Q3DSGraphObject *thisObject,
                                                            Q3DSUipPresentation *defaultPresentation,
                                                            const QString &idOrNameOrPath,
                                                            Q3DSUipPresentation **actualPresentation)
{
    Q3DSUipPresentation *pres = defaultPresentation;
    QString attr = idOrNameOrPath;
    if (attr.contains(QLatin1Char(':'))) {
        const QStringList presentationPathPair = attr.split(QLatin1Char(':'), QString::SkipEmptyParts);
        if (presentationPathPair.count() < 2)
            return nullptr;
        pres = presentationByName(presentationPathPair[0]);
        attr = presentationPathPair[1];
    }

    if (actualPresentation)
        *actualPresentation = pres;

    bool firstElem = true;
    Q3DSGraphObject *obj = thisObject;
    for (const QString &s : attr.split(QLatin1Char('.'), QString::SkipEmptyParts)) {
        if (firstElem) {
            firstElem = false;
            if (s == QStringLiteral("parent")) {
                obj = thisObject ? thisObject->parent() : nullptr;
            } else if (s == QStringLiteral("this")) {
                obj = thisObject;
            } else if (s == QStringLiteral("Scene")) {
                obj = pres->scene();
            } else if (s == QStringLiteral("Slide")) {
                obj = pres->masterSlide();
            } else {
                if (s.startsWith(QLatin1Char('#')))
                    obj = pres->object(s.mid(1).toUtf8());
                else
                    obj = pres->objectByName(s);
            }
        } else {
            if (!obj)
                return nullptr;
            if (s == QStringLiteral("parent")) {
                obj = obj->parent();
            } else {
                for (Q3DSGraphObject *child = obj->firstChild(); child; child = child->nextSibling()) {
                    if ((s.startsWith(QLatin1Char('#')) && s.mid(1).toUtf8() == child->id())
                            || child->name() == s) {
                        obj = child;
                        break;
                    }
                }
            }
        }
    }

    return obj;
}

QString Q3DSEngine::makePath(Q3DSGraphObject *obj)
{
    QString path = obj->name();
    obj = obj->parent();
    while (obj) {
        path.prepend(obj->name() + QLatin1Char('.'));
        obj = obj->parent();
    }
    return path;
}

QT_END_NAMESPACE
