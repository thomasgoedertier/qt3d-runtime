/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "q3dspresentation_p.h"
#include "q3dsengine_p.h"

QT_BEGIN_NAMESPACE

// Unlike in 3DS1, Q3DSPresentation here does not own the engine. This is due
// to the delicate lifetime management needs due to Qt 3D under the hood: for
// instance the Studio3D element has to carefully manage the underlying
// Q3DSEngine in ways that are different from what the widget or surfaceviewer
// APIs need. Therefore the presentation here is just a mere collection of
// data, any actual engine-related behavior is provided by the
// Q3DSPresentationController (for common functionality), or individually by
// Studio3D, Q3DSWidget, or Q3DSSurfaceViewer.

Q3DSPresentation::Q3DSPresentation(QObject *parent)
    : QObject(*new Q3DSPresentationPrivate, parent)
{
}

Q3DSPresentation::Q3DSPresentation(Q3DSPresentationPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

Q3DSPresentation::~Q3DSPresentation()
{
}

QUrl Q3DSPresentation::source() const
{
    Q_D(const Q3DSPresentation);
    return d->source;
}

void Q3DSPresentation::setSource(const QUrl &source)
{
    Q_D(Q3DSPresentation);
    if (d->source == source)
        return;

    d->source = source;
    if (d->controller)
        d->controller->handlePresentationSource(source);

    emit sourceChanged();
}

void Q3DSPresentation::reload()
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationReload();
}

void Q3DSPresentation::setDataInputValue(const QString &name, const QVariant &value)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleDataInputValue(name, value);
}

void Q3DSPresentation::fireEvent(const QString &elementPath, const QString &eventName)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleFireEvent(elementPath, eventName);
}

// These event forwarders are not stricly needed, Studio3D et al are fine
// without them. However, they are there in 3DS1 and can become handy to feed
// arbitrary, application-generated events into the engine.

void Q3DSPresentation::keyPressEvent(QKeyEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationKeyPressEvent(e);
}

void Q3DSPresentation::keyReleaseEvent(QKeyEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationKeyReleaseEvent(e);
}

void Q3DSPresentation::mousePressEvent(QMouseEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationMousePressEvent(e);
}

void Q3DSPresentation::mouseMoveEvent(QMouseEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationMouseMoveEvent(e);
}

void Q3DSPresentation::mouseReleaseEvent(QMouseEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationMouseReleaseEvent(e);
}

void Q3DSPresentation::mouseDoubleClickEvent(QMouseEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationMouseDoubleClickEvent(e);
}

#if QT_CONFIG(wheelevent)
void Q3DSPresentation::wheelEvent(QWheelEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationWheelEvent(e);
}
#endif

void Q3DSPresentationPrivate::setController(Q3DSPresentationController *c)
{
    if (controller == c)
        return;

    controller = c;
    controller->handlePresentationSource(source);
}

void Q3DSPresentationController::initializePresentationController(Q3DSEngine *engine, Q3DSPresentation *presentation)
{
    m_pcEngine = engine;

    QObject::connect(engine, &Q3DSEngine::customSignalEmitted, presentation, &Q3DSPresentation::customSignalEmitted);
    QObject::connect(engine, &Q3DSEngine::slideEntered, presentation, &Q3DSPresentation::slideEntered);
    QObject::connect(engine, &Q3DSEngine::slideExited, presentation, &Q3DSPresentation::slideExited);
}

void Q3DSPresentationController::handlePresentationKeyPressEvent(QKeyEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleKeyPressEvent(e);
}

void Q3DSPresentationController::handlePresentationKeyReleaseEvent(QKeyEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleKeyReleaseEvent(e);
}

void Q3DSPresentationController::handlePresentationMousePressEvent(QMouseEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleMousePressEvent(e);
}

void Q3DSPresentationController::handlePresentationMouseMoveEvent(QMouseEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleMouseMoveEvent(e);
}

void Q3DSPresentationController::handlePresentationMouseReleaseEvent(QMouseEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleMouseReleaseEvent(e);
}

void Q3DSPresentationController::handlePresentationMouseDoubleClickEvent(QMouseEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleMouseDoubleClickEvent(e);
}

#if QT_CONFIG(wheelevent)
void Q3DSPresentationController::handlePresentationWheelEvent(QWheelEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleWheelEvent(e);
}
#endif

void Q3DSPresentationController::handleDataInputValue(const QString &name, const QVariant &value)
{
    if (m_pcEngine)
        m_pcEngine->setDataInputValue(name, value);
}

void Q3DSPresentationController::handleFireEvent(const QString &elementPath, const QString &eventName)
{
    if (m_pcEngine) {
        // Assume that the path is in the main presentation when no explicit
        // presentation is specified in the path.
        Q3DSUipPresentation *pres = m_pcEngine->presentation(0);
        Q3DSGraphObject *target = m_pcEngine->findObjectByNameOrPath(nullptr, pres, elementPath, &pres);
        // pres is now the actual presentation (which is important to know
        // since the event queuing needs the scenemanager).
        if (target)
            m_pcEngine->fireEvent(target, pres, eventName);
    }
}

QT_END_NAMESPACE
