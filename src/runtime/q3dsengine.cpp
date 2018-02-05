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
#include "q3dsutils_p.h"

#include <QLoggingCategory>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QtQml/qqmlengine.h>

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

#include <Qt3DInput/QInputSettings>

#include <Qt3DQuickScene2D/qscene2d.h>

#include <Qt3DCore/private/qaspectengine_p.h>

#ifndef GL_MAX_DRAW_BUFFERS
#define GL_MAX_DRAW_BUFFERS               0x8824
#endif

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

static Q3DSGraphicsLimits gfxLimits;

Q3DSEngine::Q3DSEngine()
{
    initResources();
}

Q3DSEngine::~Q3DSEngine()
{
    destroy();
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

QSurfaceFormat Q3DSEngine::surfaceFormat()
{
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
}

namespace Q3DS {
QSurfaceFormat surfaceFormat()
{
    return Q3DSEngine::surfaceFormat();
}
}

void Q3DSEngine::createAspectEngine()
{
    m_aspectEngine.reset(new Qt3DCore::QAspectEngine);
    if (!m_flags.testFlag(WithoutRenderAspect))
        m_aspectEngine->registerAspect(new Qt3DRender::QRenderAspect);
    m_aspectEngine->registerAspect(new Qt3DInput::QInputAspect);
    m_aspectEngine->registerAspect(new Qt3DAnimation::QAnimationAspect);
    m_aspectEngine->registerAspect(new Qt3DLogic::QLogicAspect);
}

QString Q3DSEngine::source() const
{
    return m_source;
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

bool Q3DSEngine::setSource(const QString &uipOrUiaFileName)
{
    if (!m_surface) {
        Q3DSUtils::showMessage(tr("setSource: Cannot be called without setSurface"));
        return false;
    }

    QElapsedTimer sourceLoadTimer;
    sourceLoadTimer.start();

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
            if (p.type == Q3DSUiaParser::Uia::Presentation::Uip) {
                Presentation pres;
                pres.subPres.id = p.id;
                // assume the .uip name in the .uia is relative to the .uia's location
                pres.uipFileName = sourcePrefix + p.source;
                qCDebug(lcUip, "Registered subpresentation %s as %s", qPrintable(pres.uipFileName), qPrintable(p.id));
                if (p.id == uiaDoc.initialPresentationId) // initial (main) presentation must be m_presentations[0]
                    m_presentations.prepend(pres);
                else
                    m_presentations.append(pres);
            } else if (p.type == Q3DSUiaParser::Uia::Presentation::Qml) {
                QmlPresentation pres;
                pres.subPres.id = p.id;
                pres.previewFileName = sourcePrefix + p.source;
                qCDebug(lcUip, "Registered qml subpresentation %s as %s",
                        qPrintable(pres.previewFileName), qPrintable(p.id));
                m_qmlPresentations.append(pres);
            }
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
    for (QmlPresentation &pres : m_qmlPresentations)
        loadQmlSubPresentation(&pres);

    QVector<Q3DSSubPresentation> subPresentations;
    for (const Presentation &pres : m_presentations) {
        if (!pres.subPres.id.isEmpty() && pres.subPres.colorTex)
            subPresentations.append(pres.subPres);
    }
    for (const QmlPresentation &pres : m_qmlPresentations) {
        if (!pres.subPres.id.isEmpty() && pres.subPres.colorTex)
            subPresentations.append(pres.subPres);
    }
    m_presentations[0].sceneManager->finalizeMainScene(subPresentations);

    if (m_aspectEngine.isNull())
        createAspectEngine();

    m_loadTime = sourceLoadTimer.elapsed();
    qCDebug(lcUip, "Total setSource time (incl. subpresentations + Qt3D scene building): %lld ms", m_loadTime);

    emit presentationLoaded();
    return true;
}

bool Q3DSEngine::setSourceData(const QByteArray &data)
{
    if (!m_surface) {
        Q3DSUtils::showMessage(tr("setSourceData: Cannot be called without setSurface"));
        return false;
    }

    prepareForReload();
    m_source.clear();

    // No uia supported here, data must be uip
    Presentation pres;
    pres.uipData = data;

    m_presentations.append(pres);

    if (!loadPresentation(&m_presentations[0])) {
        m_presentations.clear();
        return false;
    }

    QVector<Q3DSSubPresentation> emptySubPresentations;
    m_presentations[0].sceneManager->finalizeMainScene(emptySubPresentations);

    if (m_aspectEngine.isNull())
        createAspectEngine();

    emit presentationLoaded();
    return true;
}

bool Q3DSEngine::loadPresentation(Presentation *pres)
{
    // Parse.
    QScopedPointer<Q3DSUipDocument> uipDocument(new Q3DSUipDocument);
    if (!pres->uipFileName.isEmpty()) {
        if (!uipDocument->loadUip(pres->uipFileName)) {
            Q3DSUtils::showMessage(QObject::tr("Failed to parse main presentation from file"));
            return false;
        }
    } else {
        if (!uipDocument->loadUipData(pres->uipData)) {
            Q3DSUtils::showMessage(QObject::tr("Failed to parse main presentation from data"));
            return false;
        }
    }

    pres->uipDocument = uipDocument.take();

    // Presentation is ready. Build the Qt3D scene. This will also activate the first sub-slide.
    Q3DSSceneManager::SceneBuilderParams params;
    params.flags = 0;
    if (m_flags.testFlag(Force4xMSAA))
        params.flags |= Q3DSSceneManager::Force4xMSAA;
    if (m_flags.testFlag(EnableProfiling))
        params.flags |= Q3DSSceneManager::EnableProfiling;

    // Take the size from the presentation.
    Q3DSPresentation *pres3DS = pres->uipDocument->presentation();
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

    QScopedPointer<Q3DSSceneManager> sceneManager(new Q3DSSceneManager(gfxLimits));
    pres->q3dscene = sceneManager->buildScene(pres->uipDocument->presentation(), params);
    if (!pres->q3dscene.rootEntity) {
        Q3DSUtils::showMessage(QObject::tr("Failed to build Qt3D scene"));
        return false;
    }
    pres->sceneManager = sceneManager.take();

    // Input (Qt3D).
    Qt3DInput::QInputSettings *inputSettings = new Qt3DInput::QInputSettings;
    inputSettings->setEventSource(m_surface);
    pres->q3dscene.rootEntity->addComponent(inputSettings);

    // Input (profiling UI). Do not let it capture events from the window,
    // instead use a dummy object to which we (the engine) sends events as it
    // sees fit. This ensures a single path of receiving input events.
    pres->sceneManager->setProfileUiInputEventSource(&m_profileUiEventSource);

    // Generate a resize to make sure everything size-related gets updated.
    // (avoids issues with camera upon loading new scenes)
    pres->sceneManager->updateSizes(effectiveSize, effectiveDpr);

    // Expose update signal
    connect(pres->q3dscene.frameAction, &Qt3DLogic::QFrameAction::triggered, this, &Q3DSEngine::nextFrameStarting);

    // Insert Render Capture node to framegraph for grabbing
    m_capture = new Qt3DRender::QRenderCapture();
    pres->q3dscene.frameGraphRoot->setParent(m_capture);
    pres->q3dscene.frameGraphRoot = m_capture;
    pres->q3dscene.renderSettings->setActiveFrameGraph(pres->q3dscene.frameGraphRoot);

    // Set new root entity if the engine was already up and running.
    if (!m_aspectEngine.isNull())
        m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(pres->q3dscene.rootEntity));

    return true;
}

bool Q3DSEngine::loadSubPresentation(Presentation *pres)
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
    if (m_flags.testFlag(EnableProfiling))
        params.flags |= Q3DSSceneManager::EnableProfiling;

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

bool Q3DSEngine::loadQmlSubPresentation(QmlPresentation *pres)
{
    if (m_qmlEngine.isNull())
        m_qmlEngine.reset(new QQmlEngine);

    Qt3DCore::QNode *entityParent = m_presentations[0].q3dscene.rootEntity;
    QQmlComponent *component = new QQmlComponent(m_qmlEngine.data(),
                                                 QUrl::fromLocalFile(pres->previewFileName));

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

void Q3DSEngine::destroy()
{
    for (Presentation &pres : m_presentations) {
        delete pres.sceneManager;
        delete pres.uipDocument;
    }
    m_presentations.clear();
    m_capture = nullptr;

    // wish I knew why this is needed. Qt 3D tends not to shut down its threads correctly on exit otherwise.
    if (m_aspectEngine)
        Qt3DCore::QAspectEnginePrivate::get(m_aspectEngine.data())->exitSimulationLoop();

    m_aspectEngine.reset();
}

void Q3DSEngine::prepareForReload()
{
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
}

QString Q3DSEngine::uipFileName(int index) const
{
    return (index >= 0 && index < m_presentations.count()) ? m_presentations[index].uipFileName : QString();
}

Q3DSUipDocument *Q3DSEngine::uipDocument(int index) const
{
    return (index >= 0 && index < m_presentations.count()) ? m_presentations[index].uipDocument : nullptr;
}

Q3DSSceneManager *Q3DSEngine::sceneManager(int index) const
{
    return (index >= 0 && index < m_presentations.count()) ? m_presentations[index].sceneManager : nullptr;
}

Qt3DCore::QAspectEngine *Q3DSEngine::aspectEngine() const
{
    return m_aspectEngine.data();
}

Qt3DCore::QEntity *Q3DSEngine::rootEntity() const
{
    return m_presentations.isEmpty() ? nullptr : m_presentations[0].q3dscene.rootEntity;
}

void Q3DSEngine::setOnDemandRendering(bool enabled)
{
    m_presentations[0].q3dscene.renderSettings->setRenderPolicy(enabled ? Qt3DRender::QRenderSettings::OnDemand
                                                                        : Qt3DRender::QRenderSettings::Always);
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
    if (!m_presentations.isEmpty()) {
        Q_ASSERT(!m_aspectEngine.isNull());
        if (m_aspectEngine->rootEntity() != m_presentations[0].q3dscene.rootEntity)
            m_aspectEngine->setRootEntity(Qt3DCore::QEntityPtr(m_presentations[0].q3dscene.rootEntity));
        return true;
    }
    return false;
}

void Q3DSEngine::resize(const QSize &size, qreal dpr)
{
    m_size = size;
    m_dpr = dpr;
    if (!m_presentations.isEmpty())
        m_presentations[0].sceneManager->updateSizes(m_size, m_dpr);
}

void Q3DSEngine::handleKeyPressEvent(QKeyEvent *e)
{
    QCoreApplication::sendEvent(&m_profileUiEventSource, e);

    // not ideal since the window needs focus which it often won't have. also no keyboard on embedded/mobile.
    if (e->key() == Qt::Key_F10 && !m_presentations.isEmpty()) {
        auto m = m_presentations[0].sceneManager;
        m->setProfileUiVisible(!m->isProfileUiVisible());
    }
}

void Q3DSEngine::handleKeyReleaseEvent(QKeyEvent *e)
{
    QCoreApplication::sendEvent(&m_profileUiEventSource, e);
}

void Q3DSEngine::handleMousePressEvent(QMouseEvent *e)
{
    QCoreApplication::sendEvent(&m_profileUiEventSource, e);
}

void Q3DSEngine::handleMouseMoveEvent(QMouseEvent *e)
{
    QCoreApplication::sendEvent(&m_profileUiEventSource, e);
}

void Q3DSEngine::handleMouseReleaseEvent(QMouseEvent *e)
{
    QCoreApplication::sendEvent(&m_profileUiEventSource, e);
}

void Q3DSEngine::handleMouseDoubleClickEvent(QMouseEvent *e)
{
    QCoreApplication::sendEvent(&m_profileUiEventSource, e);

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

#if QT_CONFIG(wheelevent)
void Q3DSEngine::handleWheelEvent(QWheelEvent *e)
{
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

QT_END_NAMESPACE
