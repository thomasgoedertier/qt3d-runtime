/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
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

#include "q3dssurfaceviewer_p.h"
#include "q3dsengine_p.h"
#include <QLoggingCategory>
#include <QWindow>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QPlatformSurfaceEvent>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QRenderSurfaceSelector>
#include <Qt3DRender/private/qrendersurfaceselector_p.h>
#include <Qt3DCore/QAspectEngine>
#include <Qt3DRender/private/qrenderaspect_p.h>
#include <QtQml/QQmlFile>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lc3DSSurface, "q3ds.surface")

Q3DSSurfaceViewer::Q3DSSurfaceViewer(QObject *parent)
    : QObject(*new Q3DSSurfaceViewerPrivate, parent)
{
}

Q3DSSurfaceViewer::Q3DSSurfaceViewer(Q3DSSurfaceViewerPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

Q3DSSurfaceViewer::~Q3DSSurfaceViewer()
{
    destroy();
}

// There are two create() variants and this is intentional: merging them is not
// possible since just using a default value of 0 for fboId would be wrong: the
// "default framebuffer" may not be 0 on some platforms (iOS) so we need to
// differentiate between the cases of "use the default fb whatever that is" and
// "use this custom FBO as-is". Changing to int would be wrong too since the ID
// is a GLuint in practice.

bool Q3DSSurfaceViewer::create(QSurface *surface, QOpenGLContext *context)
{
    Q_D(Q3DSSurfaceViewer);
    return d->doCreate(surface, context, 0, false);
}

bool Q3DSSurfaceViewer::create(QSurface *surface, QOpenGLContext *context, uint fboId)
{
    Q_D(Q3DSSurfaceViewer);
    return d->doCreate(surface, context, fboId, true);
}

bool Q3DSSurfaceViewerPrivate::doCreate(QSurface *s, QOpenGLContext *c, uint id, bool idValid)
{
    if (presentation->source().isEmpty()) {
        qWarning("Q3DSSurfaceViewer: Presentation source must be set before calling create()");
        return false;
    }
    if (!s) {
        qWarning("Q3DSSurfaceViewer: Null surface passed to create()");
        return false;
    }
    if (!c) {
        qWarning("Q3DSSurfaceViewer: Null context passed to create()");
        return false;
    }

    surface = s;
    context = c;
    fbo = idValid ? id : c->defaultFramebufferObject();

    destroyEngine();

    return createEngine();
}

void Q3DSSurfaceViewer::destroy()
{
    Q_D(Q3DSSurfaceViewer);
    if (d->engine)
        d->destroyEngine();

    d->surface = nullptr;
    d->context = nullptr;
    d->fbo = 0;
}

Q3DSPresentation *Q3DSSurfaceViewer::presentation() const
{
    Q_D(const Q3DSSurfaceViewer);
    return d->presentation;
}

QString Q3DSSurfaceViewer::error() const
{
    Q_D(const Q3DSSurfaceViewer);
    return d->error;
}

bool Q3DSSurfaceViewer::isRunning() const
{
    Q_D(const Q3DSSurfaceViewer);
    return d->engine && d->sourceLoaded;
}

QSize Q3DSSurfaceViewer::size() const
{
    Q_D(const Q3DSSurfaceViewer);
    return d->requestedSize;
}

void Q3DSSurfaceViewer::setSize(const QSize &size)
{
    Q_D(Q3DSSurfaceViewer);
    if (d->requestedSize != size) {
        d->requestedSize = size;
        emit sizeChanged();
    }
}

bool Q3DSSurfaceViewer::autoSize() const
{
    Q_D(const Q3DSSurfaceViewer);
    return d->autoSize;
}

void Q3DSSurfaceViewer::setAutoSize(bool autoSize)
{
    Q_D(Q3DSSurfaceViewer);
    if (d->autoSize != autoSize) {
        d->autoSize = autoSize;
        emit autoSizeChanged();
    }
}

int Q3DSSurfaceViewer::updateInterval() const
{
    Q_D(const Q3DSSurfaceViewer);
    return d->updateInterval;
}

void Q3DSSurfaceViewer::setUpdateInterval(int interval)
{
    Q_D(Q3DSSurfaceViewer);
    if (d->updateInterval != interval) {
        d->updateInterval = interval;
        emit updateIntervalChanged();
    }

    if (d->updateInterval >= 0) {
        d->updateTimer.setInterval(d->updateInterval);
        d->updateTimer.start();
    } else {
        d->updateTimer.stop();
    }
}

uint Q3DSSurfaceViewer::fboId() const
{
    Q_D(const Q3DSSurfaceViewer);
    return d->fbo;
}

QSurface *Q3DSSurfaceViewer::surface() const
{
    Q_D(const Q3DSSurfaceViewer);
    return d->surface;
}

QOpenGLContext *Q3DSSurfaceViewer::context() const
{
    Q_D(const Q3DSSurfaceViewer);
    return d->context;
}

extern Q_GUI_EXPORT QImage qt_gl_read_framebuffer(const QSize &size, bool alpha_format, bool include_alpha);

void Q3DSSurfaceViewer::update()
{
    Q_D(Q3DSSurfaceViewer);
    if (!d->engine || !d->sourceLoaded)
        return;

    if (d->surface->surfaceClass() == QSurface::Window && !static_cast<QWindow *>(d->surface)->isExposed())
        return;

    if (!d->context->makeCurrent(d->surface)) {
        qWarning("Q3DSSurfaceViewer: Failed to make context current");
        return;
    }

    if (!d->renderAspect) {
        qCDebug(lc3DSSurface, "Creating renderer");
        d->renderAspect = new Qt3DRender::QRenderAspect(Qt3DRender::QRenderAspect::Synchronous);
        d->engine->aspectEngine()->registerAspect(d->renderAspect);
        auto renderAspectD = static_cast<Qt3DRender::QRenderAspectPrivate *>(Qt3DRender::QRenderAspectPrivate::get(d->renderAspect));
        renderAspectD->renderInitialize(d->context);
        d->context->makeCurrent(d->surface);
        d->engine->start();
    }

    const uint defaultFbo = d->context->defaultFramebufferObject();
    QSize sz;
    qreal dpr;
    // Pick the size from the window when autoSize is enabled. However, a
    // QOffscreenSize's size() is useless and must not be relied upon.
    if (d->autoSize && d->surface->surfaceClass() == QSurface::Window && d->fbo == defaultFbo) {
        QWindow *w = static_cast<QWindow *>(d->surface);
        dpr = w->devicePixelRatio();
        sz = w->size() * dpr;
        if (d->requestedSize != sz) {
            d->requestedSize = sz;
            emit sizeChanged();
        }
    } else {
        sz = d->requestedSize;
        dpr = 1.0f;
    }
    if (d->actualSize != sz) {
        d->actualSize = sz;
        d->engine->resize(sz, dpr, true);
        d->sendResizeToQt3D(sz);
    }

    QOpenGLFunctions *f = d->context->functions();
    f->glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);

    auto renderAspectD = static_cast<Qt3DRender::QRenderAspectPrivate *>(Qt3DRender::QRenderAspectPrivate::get(d->renderAspect));
    renderAspectD->renderSynchronous();

    if (d->wantsGrab) {
        d->context->makeCurrent(d->surface);
        f->glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
        d->grabImage = qt_gl_read_framebuffer(d->actualSize, false, false);
    }

    emit frameUpdate();
}

QImage Q3DSSurfaceViewer::grab(const QRect &rect)
{
    Q_D(Q3DSSurfaceViewer);
    if (!d->engine)
        return QImage();

    d->wantsGrab = true;
    update();
    d->wantsGrab = false;

    // rect is expected to be in pixels since anything else is inherently
    // broken due to surfaceviewer handling multiple different cases (window,
    // window with targeting FBO, offscreen surface with targeting FBO) with
    // different dpr concepts (window has one, the others don't).

    return rect.isValid() ? d->grabImage.copy(rect) : d->grabImage;
}

Q3DSSurfaceViewerPrivate::Q3DSSurfaceViewerPrivate()
    : presentation(new Q3DSPresentation)
{
    Q3DSPresentationPrivate::get(presentation)->setController(this);
}

Q3DSSurfaceViewerPrivate::~Q3DSSurfaceViewerPrivate()
{
    delete presentation;
}

bool Q3DSSurfaceViewerPrivate::createEngine()
{
    Q_Q(Q3DSSurfaceViewer);
    if (!context || !surface)
        return false;

    Q_ASSERT(!engine);

    engine = new Q3DSEngine;

    Q3DSEngine::Flags flags = Q3DSEngine::WithoutRenderAspect;
    if (sourceFlags.testFlag(Q3DSPresentationController::Profiling))
        flags |= Q3DSEngine::EnableProfiling;

    engine->setFlags(flags);
    engine->setAutoToggleProfileUi(false); // up to the app to control this via the API instead

    switch (surface->surfaceClass()) {
    case QSurface::Window:
        windowOrOffscreenSurface = static_cast<QWindow *>(surface);
        break;
    case QSurface::Offscreen:
        windowOrOffscreenSurface = static_cast<QOffscreenSurface *>(surface);
        break;
    default:
        qWarning("Q3DSSurfaceViewer: Got surface with unknown class (neither QWindow nor QOffscreenSurface)");
        return false;
    }
    engine->setSurface(windowOrOffscreenSurface);

    qCDebug(lc3DSSurface, "Created engine %p", engine);

    initializePresentationController(engine, presentation);

    const QString fn = QQmlFile::urlToLocalFileOrQrc(source);
    qCDebug(lc3DSSurface, "source is now %s", qPrintable(fn));

    actualSize = QSize();
    if (autoSize && surface->surfaceClass() == QSurface::Window) {
        actualSize = static_cast<QWindow *>(surface)->size();
        if (!actualSize.isEmpty())
            engine->resize(actualSize);
    }

    QObject::connect(engine, &Q3DSEngine::presentationLoaded, q, &Q3DSSurfaceViewer::presentationLoaded);

    QString err;
    sourceLoaded = engine->setSource(fn, &err);
    if (!sourceLoaded) {
        qWarning("Q3DSSurfaceViewer: Failed to load %s\n%s", qPrintable(fn), qPrintable(err));
        error = err;
        emit q->errorChanged();
        emit q->runningChanged();
        return false;
    }

    if (!error.isEmpty()) {
        error.clear();
        emit q->errorChanged();
    }

    if (actualSize.isEmpty())
        actualSize = engine->implicitSize();

    sendResizeToQt3D(actualSize);

    surfaceWatcher.reset(new Q3DSSurfaceWatcher(this));

    if (!updateTimerInitialized) {
        updateTimerInitialized = true;
        QObject::connect(&updateTimer, &QTimer::timeout, q, &Q3DSSurfaceViewer::update);
    }

    emit q->runningChanged();

    return true;
}

void Q3DSSurfaceViewerPrivate::destroyEngine()
{
    if (!context || !surface)
        return;

    surfaceWatcher.reset();

    context->makeCurrent(surface);

    if (renderAspect) {
        qCDebug(lc3DSSurface, "Destroying renderer");
        auto renderAspectD = static_cast<Qt3DRender::QRenderAspectPrivate *>(Qt3DRender::QRenderAspectPrivate::get(renderAspect));
        renderAspectD->renderShutdown();
        renderAspect = nullptr;
        context->makeCurrent(surface);
    }

    if (engine) {
        qCDebug(lc3DSSurface, "Destroying engine %p", engine);
        delete engine;
        engine = nullptr;
    }
}

void Q3DSSurfaceViewerPrivate::handlePresentationSource(const QUrl &newSource, SourceFlags flags)
{
    if (newSource == source)
        return;

    destroyEngine();

    source = newSource;
    sourceFlags = flags;

    if (surface && context)
        createEngine();
}

void Q3DSSurfaceViewerPrivate::handlePresentationReload()
{
    if (source.isEmpty())
        return;

    destroyEngine();

    if (surface && context)
        createEngine();
}

void Q3DSSurfaceViewerPrivate::sendResizeToQt3D(const QSize &size)
{
    Qt3DCore::QEntity *rootEntity = engine->rootEntity();
    if (rootEntity) {
        Qt3DRender::QRenderSurfaceSelector *surfaceSelector = Qt3DRender::QRenderSurfaceSelectorPrivate::find(rootEntity);
        qCDebug(lc3DSSurface, "Setting external render target size on surface selector %p", surfaceSelector);
        if (surfaceSelector) {
            surfaceSelector->setExternalRenderTargetSize(size);
            float dpr = 1;
            if (surface->surfaceClass() == QSurface::Window)
                dpr = float(static_cast<QWindow *>(surface)->devicePixelRatio());
            surfaceSelector->setSurfacePixelRatio(dpr);
        }
    }
}

Q3DSSurfaceWatcher::Q3DSSurfaceWatcher(Q3DSSurfaceViewerPrivate *p)
    : d(p)
{
    Q_ASSERT(d->context && d->windowOrOffscreenSurface);

    d->windowOrOffscreenSurface->installEventFilter(this);

    connect(d->context, &QOpenGLContext::aboutToBeDestroyed, this, [this]() {
        d->destroyEngine();
    });
}

bool Q3DSSurfaceWatcher::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::PlatformSurface) {
        QPlatformSurfaceEvent *e = static_cast<QPlatformSurfaceEvent *>(event);
        if (e->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            d->destroyEngine();
    }
    return QObject::eventFilter(watched, event);
}

QT_END_NAMESPACE
