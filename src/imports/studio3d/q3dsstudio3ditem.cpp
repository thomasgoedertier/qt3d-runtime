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
#include "q3dspresentationitem_p.h"
#include <QSGNode>
#include <QLoggingCategory>
#include <QThread>
#include <QRunnable>
#include <QQuickWindow>
#include <QQuickRenderControl>
#include <QOffscreenSurface>
#include <QQmlFile>
#include <QQmlEngine>
#include <QQmlContext>
#include <QGuiApplication>
#include <q3dsviewersettings.h>
#include <private/q3dsengine_p.h>
#include <private/q3dsutils_p.h>
#include <private/q3dslogging_p.h>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QRenderSurfaceSelector>
#include <Qt3DRender/private/qrendersurfaceselector_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Studio3D
    \instantiates Q3DSStudio3DItem
    \inqmlmodule QtStudio3D
    \ingroup 3dstudioruntime2
    \inherits Item

    \brief Qt 3D Studio presentation viewer.

    This type enables developers to embed Qt 3D Studio presentations in Qt
    Quick.

    \section2 Example usage

    \qml
    Studio3D {
        id: studio3D
        Presentation {
            source: "qrc:///presentation.uia"
            SceneElement {
                id: scene
                elementPath: "Scene"
                currentSlideIndex: 2
            }
            Element {
                id: textLabel
                elementPath: "Scene.Layer.myLabel"
            }
        }
        ViewerSettings {
            showRenderStats: true
        }
        onRunningChanged: {
            console.log("Presentation ready!");
        }
    }
    \endqml
*/

/*!
    \qmlsignal Studio3D::frameUpdate()

    This signal is emitted each time a frame has been updated regardless of
    visibility. This allows a hidden Studio3D element to still process
    information every frame, even though the renderer is not rendering.

    The corresponding handler is \c onFrameUpdate.

    To prevent expensive handlers from being processed when hidden, add an
    early return to the top like:

    \qml
         onFrameUpdate: {
             if (!visible) return;
             ...
         }
    \endqml
*/

/*!
    \qmlsignal Studio3D::presentationReady()

    This signal is emitted when the viewer has been initialized and the
    presentation is ready to be shown. The difference to \c running property is
    that the viewer has to be visible for \c running to get \c{true}. This
    signal is useful for displaying splash screen while viewer is getting
    initialized.
*/

static bool engineCleanerRegistered = false;
static QSet<Q3DSEngine *> engineTracker;
static void engineCleaner()
{
    // We cannot go down with engines alive, mainly because some Qt 3D stuff
    // uses threads which need proper shutdown.
    QSet<Q3DSEngine *> strayEngines = std::move(engineTracker);
    for (Q3DSEngine *engine : strayEngines)
        delete engine;
}

Q3DSStudio3DItem::Q3DSStudio3DItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    if (!engineCleanerRegistered) {
        qAddPostRoutine(engineCleaner);
        engineCleanerRegistered = true;
    }

    setFlag(QQuickItem::ItemHasContents, true);

    // not strictly needed since we'll use Q3DSUtilsMessageRedirect but just in case
    Q3DSUtils::setDialogsEnabled(false);

    updateEventMasks();
}

Q3DSStudio3DItem::~Q3DSStudio3DItem()
{
}

/*!
    \qmlproperty Presentation Studio3D::presentation

    Accessor for the presentation. Applications are expected to create a single
    Presentation child object for Studio3D. If this is omitted, a presentation
    is created automatically.

    This property is read-only.
*/

Q3DSPresentationItem *Q3DSStudio3DItem::presentation() const
{
    return m_presentation;
}

/*!
    \qmlproperty bool Studio3D::running

    The value of this property is \c true when the viewer has been initialized
    and the presentation is running.

    This property is read-only.
*/

bool Q3DSStudio3DItem::isRunning() const
{
    return m_running;
}

QString Q3DSStudio3DItem::error() const
{
    return m_error;
}

Q3DSStudio3DItem::EventIgnoreFlags Q3DSStudio3DItem::ignoredEvents() const
{
    return m_eventIgnoreFlags;
}

void Q3DSStudio3DItem::setIgnoredEvents(EventIgnoreFlags flags)
{
    if (m_eventIgnoreFlags == flags)
        return;

    m_eventIgnoreFlags = flags;
    updateEventMasks();
    emit ignoredEventsChanged();
}

void Q3DSStudio3DItem::updateEventMasks()
{
    if (m_eventIgnoreFlags.testFlag(IgnoreMouseEvents)) {
        setAcceptedMouseButtons(Qt::NoButton);
        setAcceptHoverEvents(false);
    } else {
        setAcceptedMouseButtons(Qt::MouseButtonMask);
        setAcceptHoverEvents(true);
    }
}

void Q3DSStudio3DItem::componentComplete()
{
    const auto childObjs = children();
    for (QObject *child : childObjs) {
        if (Q3DSPresentationItem *presentation = qobject_cast<Q3DSPresentationItem *>(child)) {
            if (m_presentation)
                qWarning("Studio3D: Duplicate Presentation");
            else
                m_presentation = presentation;
        } else if (Q3DSViewerSettings *viewerSettings = qobject_cast<Q3DSViewerSettings *>(child)) {
            if (m_viewerSettings) {
                qWarning("Studio3D: Duplicate ViewerSettings");
            } else {
                m_viewerSettings = viewerSettings;
                connect(m_viewerSettings, &Q3DSViewerSettings::showRenderStatsChanged, m_viewerSettings, [this] {
                    if (m_engine)
                        m_engine->setProfileUiVisible(m_viewerSettings->isShowingRenderStats());
                });
            }
        }
    }

    if (!m_presentation)
        m_presentation = new Q3DSPresentationItem(this);

    // setController may lead to creating the engine hence this must happen before
    m_presentation->preStudio3DPresentationLoaded();

    Q3DSPresentationPrivate::get(m_presentation)->setController(this);

    QQuickItem::componentComplete();
}

void Q3DSStudio3DItem::handlePresentationSource(const QUrl &source,
                                                SourceFlags flags,
                                                const QVector<Q3DSInlineQmlSubPresentation *> &inlineSubPres)
{
    if (source == m_source)
        return;

    if (!m_source.isEmpty())
        releaseEngineAndRenderer();

    m_source = source;
    m_sourceFlags = flags;
    m_inlineQmlSubPresentations = inlineSubPres;

    if (window()) // else defer to itemChange()
        createEngine();
}

void Q3DSStudio3DItem::handlePresentationReload()
{
    if (m_source.isEmpty())
        return;

    releaseEngineAndRenderer();

    if (window())
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

        if (w->rendererInterface()->graphicsApi() != QSGRendererInterface::OpenGL) {
            qWarning("Studio3D: Qt Quick not running with OpenGL; this is not supported atm");
            m_error = QLatin1String("Studio3D requires OpenGL");
            emit errorChanged();
            return;
        }

        m_engine = new Q3DSEngine;
        engineTracker.insert(m_engine);

        // Rendering will be driven manually from the Quick render thread via the QRenderAspect.
        // We create the render aspect ourselves on the Quick render thread.
        Q3DSEngine::Flags flags = Q3DSEngine::WithoutRenderAspect;
        if (m_sourceFlags.testFlag(Q3DSPresentationController::Profiling))
            flags |= Q3DSEngine::EnableProfiling;

        m_engine->setFlags(flags);
        m_engine->setAutoToggleProfileUi(false); // up to the app to control this via the API instead

        // Use our QQmlEngine for QML subpresentations and behavior scripts.
        QQmlEngine *qmlEngine = QQmlEngine::contextForObject(this)->engine();
        m_engine->setSharedSubPresentationQmlEngine(qmlEngine);
        m_engine->setSharedBehaviorQmlEngine(qmlEngine);

        initializePresentationController(m_engine, m_presentation);

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

        qCDebug(lcStudio3D, "created engine %p", m_engine);

        connect(m_engine, &Q3DSEngine::presentationLoaded, this, [this]() {
            if (m_viewerSettings && m_viewerSettings->isShowingRenderStats())
                m_engine->setProfileUiVisible(true);
            m_presentation->studio3DPresentationLoaded();
            if (!m_running) {
                m_running = true;
                emit runningChanged();
            }
            emit presentationReady();
        });
        connect(m_engine, &Q3DSEngine::nextFrameStarting, this, [this]() {
            emit frameUpdate();
        });
    }

    const QString fn = QQmlFile::urlToLocalFileOrQrc(m_source);
    qCDebug(lcStudio3D) << "source is now" << fn;
    const QSize sz(width(), height());
    if (!sz.isEmpty())
        m_engine->resize(sz);

    QString err;
    m_sourceLoaded = m_engine->setSource(fn, &err, m_inlineQmlSubPresentations);
    if (m_sourceLoaded) {
        if (!m_error.isEmpty()) {
            m_error.clear();
            emit errorChanged();
        }

        if (!sz.isEmpty())
            sendResizeToQt3D(sz);

        // cannot start() here, that must be deferred

    } else {
        qWarning("Studio3D: Failed to load %s\n%s", qPrintable(fn), qPrintable(err));
        m_error = err;
        emit errorChanged();
    }

    update();
}

QSGNode *Q3DSStudio3DItem::updatePaintNode(QSGNode *node, QQuickItem::UpdatePaintNodeData *)
{
    // this on the render thread; the engine lives on the gui thread and should
    // be ready at this point - unless there's no source set yet (or it failed)

    if (!m_engine || !m_sourceLoaded) {
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
                // render thread (or main if non-threaded render loop)
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
        Q_ASSERT(!m_renderer);
        qCDebug(lcStudio3D, "destroying engine %p", m_engine);
        engineTracker.remove(m_engine);
        delete m_engine; // recreate on next window change - if we are still around, that is
        m_engine = nullptr;
        if (m_running) {
            m_running = false;
            emit runningChanged();
        }
    }
}

class EngineReleaser : public QObject
{
public:
    EngineReleaser(Q3DSEngine *engine)
        : m_engine(engine)
    { }
    ~EngineReleaser() {
        if (engineTracker.contains(m_engine)) {
            qCDebug(lcStudio3D, "async release: destroying engine %p", m_engine);
            engineTracker.remove(m_engine);
            delete m_engine;
        }

        // Here the destruction of the old engine and the creation of a new one
        // will overlap (if the item survives, that is). So don't bother with
        // m_running.
    }

private:
    Q3DSEngine *m_engine;
};

class RendererReleaser : public QRunnable
{
public:
    RendererReleaser(Q3DSStudio3DRenderer *r, EngineReleaser *er)
        : m_renderer(r),
          m_engineReleaser(er)
    { }
    void run() override {
        delete m_renderer;

        // now, if this is on the render thread (Qt Quick with threaded render
        // loop) and the application is exiting, the deleteLater may not
        // actually be executed ever. Hence the need for the post routine and
        // engineTracker. However, if the application stays alive and we are
        // cleaning up for another reason, this is just fine since the engine
        // will eventually get deleted fine by the main thread.
        m_engineReleaser->deleteLater();
    }

private:
    Q3DSStudio3DRenderer *m_renderer;
    EngineReleaser *m_engineReleaser;
};

void Q3DSStudio3DItem::releaseResources()
{
    qCDebug(lcStudio3D, "releaseResources");

    // this may not be an application exit; if this is just a window change then allow continuing
    // by eventually creating new engine and renderer objects

    releaseEngineAndRenderer();
}

void Q3DSStudio3DItem::releaseEngineAndRenderer()
{
    if (!window() || !m_renderer)
        return;

    // renderer must be destroyed first (on the Quick render thread)
    if (m_renderer->thread() == QThread::currentThread()) {
        delete m_renderer;
        m_renderer = nullptr;
        destroyEngine();
    } else {
        // by the time the runnable runs we (the item) may already be gone; that's fine, just pass the renderer ref
        EngineReleaser *er = new EngineReleaser(m_engine);
        RendererReleaser *rr = new RendererReleaser(m_renderer, er);
        window()->scheduleRenderJob(rr, QQuickWindow::BeforeSynchronizingStage);
        m_renderer = nullptr;
        m_engine = nullptr;
    }
}

void Q3DSStudio3DItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    if (!newGeometry.isEmpty() && m_engine && newGeometry.size() != oldGeometry.size()) {
        const QSize sz = newGeometry.size().toSize();
        m_engine->resize(sz);
        sendResizeToQt3D(sz);
    }
}

void Q3DSStudio3DItem::sendResizeToQt3D(const QSize &size)
{
    Qt3DCore::QEntity *rootEntity = m_engine->rootEntity();
    if (rootEntity) {
        Qt3DRender::QRenderSurfaceSelector *surfaceSelector = Qt3DRender::QRenderSurfaceSelectorPrivate::find(rootEntity);
        qCDebug(lcStudio3D, "Setting external render target size on surface selector %p", surfaceSelector);
        if (surfaceSelector) {
            surfaceSelector->setExternalRenderTargetSize(size);
            surfaceSelector->setSurfacePixelRatio(window()->effectiveDevicePixelRatio());
        }
    }
}

void Q3DSStudio3DItem::keyPressEvent(QKeyEvent *event)
{
    if (!m_eventIgnoreFlags.testFlag(IgnoreKeyboardEvents) && m_engine)
        m_engine->handleKeyPressEvent(event);
}

void Q3DSStudio3DItem::keyReleaseEvent(QKeyEvent *event)
{
    if (!m_eventIgnoreFlags.testFlag(IgnoreKeyboardEvents) && m_engine)
        m_engine->handleKeyReleaseEvent(event);
}

void Q3DSStudio3DItem::mousePressEvent(QMouseEvent *event)
{
    if (!m_eventIgnoreFlags.testFlag(IgnoreMouseEvents) && m_engine)
        m_engine->handleMousePressEvent(event);
}

void Q3DSStudio3DItem::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_eventIgnoreFlags.testFlag(IgnoreMouseEvents) && m_engine)
        m_engine->handleMouseMoveEvent(event);
}

void Q3DSStudio3DItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_eventIgnoreFlags.testFlag(IgnoreMouseEvents) && m_engine)
        m_engine->handleMouseReleaseEvent(event);
}

void Q3DSStudio3DItem::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_eventIgnoreFlags.testFlag(IgnoreMouseEvents) && m_engine)
        m_engine->handleMouseDoubleClickEvent(event);
}

#if QT_CONFIG(wheelevent)
void Q3DSStudio3DItem::wheelEvent(QWheelEvent *event)
{
    if (!m_eventIgnoreFlags.testFlag(IgnoreWheelEvents) && m_engine)
        m_engine->handleWheelEvent(event);
}
#endif

void Q3DSStudio3DItem::hoverMoveEvent(QHoverEvent *event)
{
    if (m_eventIgnoreFlags.testFlag(IgnoreMouseEvents))
        return;

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

void Q3DSStudio3DItem::touchEvent(QTouchEvent *event)
{
    if (!m_eventIgnoreFlags.testFlag(IgnoreMouseEvents) && m_engine)
        m_engine->handleTouchEvent(event);
}

QT_END_NAMESPACE
