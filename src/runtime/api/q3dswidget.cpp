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

#include "q3dswidget_p.h"
#include <private/q3dsengine_p.h>
#include <private/q3dslogging_p.h>
#include <QLoggingCategory>
#include <QScopedValueRollback>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QRenderSurfaceSelector>
#include <Qt3DRender/private/qrendersurfaceselector_p.h>
#include <Qt3DCore/QAspectEngine>
#include <Qt3DRender/private/qrenderaspect_p.h>
#include <QtGui/private/qopenglcontext_p.h>
#include <QtQml/QQmlFile>

QT_BEGIN_NAMESPACE

/*!
    \class Q3DSWidget
    \inmodule 3dstudioruntime2
    \since Qt 3D Studio 2.0
    \inherits QOpenGLWidget

    \brief A widget that renders Qt 3D Studio presentations using OpenGL.

    Q3DSWidget is a widget that can be used to embed Qt 3D Studio presentations
    into QWidget-based applications.

    Q3DSWidget is used to specify a render widget for Qt 3D Studio
    presentation. It subclasses QOpenGLWidget, which means all considerations
    that should be taken when working with QOpenGLWidget apply to Q3DSWidget as
    well. Refer to the QOpenGLWidget documentation for details.

    \section2 Example Usage

    \code
    Q3DSWidget *viewer = new Q3DSWidget(parentWidget);
    viewer->presentation()->setSource(QUrl(QStringLiteral("qrc:/my_presentation.uip")));
    viewer->setUpdateInterval(0);

    // Register a scene element object for slide management (optional)
    Q3DSSceneElement scene(viewer->presentation(), QStringLiteral("Scene"));

    // Register an element object for attribute setting (optional)
    Q3DSElement element(viewer->presentation(), QStringLiteral("myCarModel"));
    \endcode

    \sa Q3DSSurfaceViewer
*/

Q3DSWidget::Q3DSWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      d_ptr(new Q3DSWidgetPrivate(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

Q3DSWidget::~Q3DSWidget()
{
    delete d_ptr;
}

/*!
    \fn Q3DSWidget::frameUpdate()

    This signal is emitted each time a frame has been rendered.
*/

/*!
    \fn Q3DSWidget::presentationLoaded()

    This signal is emitted when the viewer has been initialized and the
    presentation is ready to be shown.
*/

/*!
    \property Q3DSWidget::presentation

    Returns the presentation object used by the Q3DSWidget.

    This property is read-only.
 */
Q3DSPresentation *Q3DSWidget::presentation() const
{
    Q_D(const Q3DSWidget);
    return d->presentation;
}

/*!
    Returns the settings object used by the Q3DSWidget.
 */
Q3DSViewerSettings *Q3DSWidget::settings() const
{
    Q_D(const Q3DSWidget);
    return d->viewerSettings;
}

/*!
    \property Q3DSWidget::error

    Contains the text for the error message that was generated during the
    loading of the presentation. When no error occurred or there is no
    presentation loaded, the value is an empty string.

    This property is read-only.
 */
QString Q3DSWidget::error() const
{
    Q_D(const Q3DSWidget);
    return d->error;
}

/*!
    \property Q3DSWidget::running

    The value of this property is \c true when the viewer has been initialized
    and the presentation is running.

    This property is read-only.
*/
bool Q3DSWidget::isRunning() const
{
    Q_D(const Q3DSWidget);
    return d->engine && d->sourceLoaded;
}

/*!
    \property Q3DSWidget::updateInterval

    Holds the viewer update interval in milliseconds. If the value is negative,
    the viewer doesn't update the presentation automatically.

    The default value is 0, meaning automatic updates are enabled.

    \sa QWidget::update()
*/
int Q3DSWidget::updateInterval() const
{
    Q_D(const Q3DSWidget);
    return d->updateInterval;
}

void Q3DSWidget::setUpdateInterval(int interval)
{
    Q_D(Q3DSWidget);
    if (d->updateInterval == interval)
        return;

    d->updateInterval = interval;
    emit updateIntervalChanged();

    if (d->updateInterval >= 0) {
        d->updateTimer.setInterval(d->updateInterval);
        d->updateTimer.start();
    } else {
        d->updateTimer.stop();
    }
}

/*!
    \internal
 */
void Q3DSWidget::keyPressEvent(QKeyEvent *event)
{
    Q_D(Q3DSWidget);
    if (d->engine)
        d->engine->handleKeyPressEvent(event);
}

/*!
    \internal
 */
void Q3DSWidget::keyReleaseEvent(QKeyEvent *event)
{
    Q_D(Q3DSWidget);
    if (d->engine)
        d->engine->handleKeyReleaseEvent(event);
}

/*!
    \internal
 */
void Q3DSWidget::mousePressEvent(QMouseEvent *event)
{
    Q_D(Q3DSWidget);
    if (d->engine)
        d->engine->handleMousePressEvent(event);
}

/*!
    \internal
 */
void Q3DSWidget::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(Q3DSWidget);
    if (d->engine)
        d->engine->handleMouseMoveEvent(event);
}

/*!
    \internal
 */
void Q3DSWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(Q3DSWidget);
    if (d->engine)
        d->engine->handleMouseReleaseEvent(event);
}

/*!
    \internal
 */
void Q3DSWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_D(Q3DSWidget);
    if (d->engine)
        d->engine->handleMouseDoubleClickEvent(event);
}

#if QT_CONFIG(wheelevent)
/*!
    \internal
 */
void Q3DSWidget::wheelEvent(QWheelEvent *event)
{
    Q_D(Q3DSWidget);
    if (d->engine)
        d->engine->handleWheelEvent(event);
}
#endif

/*!
    \internal
 */
void Q3DSWidget::initializeGL()
{
}

/*!
    \internal
 */
void Q3DSWidget::resizeGL(int w, int h)
{
    Q_D(Q3DSWidget);
    const QSize sz(w, h);
    if (!sz.isEmpty() && d->engine && d->sourceLoaded) {
        d->engine->resize(sz);
        d->sendResizeToQt3D(sz);
    }
}

/*!
    \internal
 */
void Q3DSWidget::paintGL()
{
    Q_D(Q3DSWidget);
    if (d->source.isEmpty())
        return;

    // Prevent Qt3D getting confused due its somewhat limited FBO vs. surface
    // render target logic. We don't care if QOpenGLFBO::bindDefault() binds
    // the wrong FBO anyway since we'll do a makeCurrent+bind each time after
    // crossing over to Qt3D land.
    QOpenGLContextPrivate *ctxD = QOpenGLContextPrivate::get(context());
    QScopedValueRollback<GLuint> defaultFboRedirectRollback(ctxD->defaultFboRedirect, 0);

    if (d->needsInit) {
        d->needsInit = false;
        if (!d->engine)
            d->createEngine();

        if (!d->renderAspect || !d->sourceLoaded)
            return;

        auto renderAspectD = static_cast<Qt3DRender::QRenderAspectPrivate *>(Qt3DRender::QRenderAspectPrivate::get(d->renderAspect));
        renderAspectD->renderInitialize(context());
        makeCurrent();

        d->engine->start();
    }

    if (!d->renderAspect || !d->sourceLoaded)
        return;

    auto renderAspectD = static_cast<Qt3DRender::QRenderAspectPrivate *>(Qt3DRender::QRenderAspectPrivate::get(d->renderAspect));
    renderAspectD->renderSynchronous();
    makeCurrent();

    emit frameUpdate();
}

Q3DSWidgetPrivate::Q3DSWidgetPrivate(Q3DSWidget *q)
    : q_ptr(q),
      presentation(new Q3DSPresentation),
      viewerSettings(new Q3DSViewerSettings)
{
    Q3DSPresentationPrivate::get(presentation)->setController(this);

    QObject::connect(viewerSettings, &Q3DSViewerSettings::showRenderStatsChanged, viewerSettings, [this] {
        if (engine)
            engine->setProfileUiVisible(viewerSettings->isShowingRenderStats());
    });

    typedef void (QWidget::*QWidgetVoidSlot)();
    QObject::connect(&updateTimer, &QTimer::timeout, q, static_cast<QWidgetVoidSlot>(&QWidget::update));
    updateTimer.setInterval(updateInterval);
    updateTimer.start();
}

Q3DSWidgetPrivate::~Q3DSWidgetPrivate()
{
    destroyEngine();
    delete viewerSettings;
    delete presentation;
}

void Q3DSWidgetPrivate::createEngine()
{
    engine = new Q3DSEngine;

    Q3DSEngine::Flags flags = Q3DSEngine::WithoutRenderAspect;
    if (sourceFlags.testFlag(Q3DSPresentationController::Profiling))
        flags |= Q3DSEngine::EnableProfiling;

    engine->setFlags(flags);
    engine->setAutoToggleProfileUi(false); // up to the app to control this via the API instead

    engine->setSurface(q_ptr->window()->windowHandle());
    qCDebug(lc3DSWidget, "Created engine %p", engine);

    initializePresentationController(engine, presentation);

    const QString fn = QQmlFile::urlToLocalFileOrQrc(source);
    qCDebug(lc3DSWidget, "source is now %s", qPrintable(fn));
    const QSize sz = q_ptr->size();
    if (!sz.isEmpty())
        engine->resize(sz);

    QObject::connect(engine, &Q3DSEngine::presentationLoaded, engine, [this] {
        if (viewerSettings->isShowingRenderStats())
            engine->setProfileUiVisible(true);
        emit q_ptr->presentationLoaded();
    });

    QString err;
    sourceLoaded = engine->setSource(fn, &err, inlineQmlSubPresentations);
    if (sourceLoaded) {
        if (!error.isEmpty()) {
            error.clear();
            emit q_ptr->errorChanged();
        }

        if (!sz.isEmpty())
            sendResizeToQt3D(sz);

        renderAspect = new Qt3DRender::QRenderAspect(Qt3DRender::QRenderAspect::Synchronous);
        engine->aspectEngine()->registerAspect(renderAspect);

        emit q_ptr->runningChanged();
    } else {
        qWarning("Q3DSWidget: Failed to load %s\n%s", qPrintable(fn), qPrintable(err));
        error = err;
        emit q_ptr->errorChanged();
        emit q_ptr->runningChanged();
    }
}

void Q3DSWidgetPrivate::destroyEngine()
{
    q_ptr->makeCurrent();

    if (renderAspect) {
        auto renderAspectD = static_cast<Qt3DRender::QRenderAspectPrivate *>(Qt3DRender::QRenderAspectPrivate::get(renderAspect));
        renderAspectD->renderShutdown();
        renderAspect = nullptr;
        q_ptr->makeCurrent();
    }

    if (engine) {
        qCDebug(lc3DSWidget, "Destroying engine %p", engine);
        delete engine;
        engine = nullptr;
    }
}

void Q3DSWidgetPrivate::handlePresentationSource(const QUrl &newSource,
                                                 SourceFlags flags,
                                                 const QVector<Q3DSInlineQmlSubPresentation *> &inlineSubPres)
{
    if (newSource == source)
        return;

    if (!source.isEmpty())
        destroyEngine();

    source = newSource;
    sourceFlags = flags;
    inlineQmlSubPresentations = inlineSubPres;

    needsInit = true;
    q_ptr->update();
}

void Q3DSWidgetPrivate::handlePresentationReload()
{
    if (source.isEmpty())
        return;

    destroyEngine();

    needsInit = true;
    q_ptr->update();
}

void Q3DSWidgetPrivate::sendResizeToQt3D(const QSize &size)
{
    Qt3DCore::QEntity *rootEntity = engine->rootEntity();
    if (rootEntity) {
        Qt3DRender::QRenderSurfaceSelector *surfaceSelector = Qt3DRender::QRenderSurfaceSelectorPrivate::find(rootEntity);
        qCDebug(lc3DSWidget, "Setting external render target size on surface selector %p", surfaceSelector);
        if (surfaceSelector) {
            surfaceSelector->setExternalRenderTargetSize(size);
            surfaceSelector->setSurfacePixelRatio(q_ptr->devicePixelRatioF());
        }
    }
}

QT_END_NAMESPACE
