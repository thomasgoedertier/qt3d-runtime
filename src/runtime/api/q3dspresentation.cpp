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
        d->controller->handlePresentationSource(source, d->sourceFlags(), d->inlineQmlSubPresentations);

    emit sourceChanged();
}

bool Q3DSPresentation::isProfilingEnabled() const
{
    Q_D(const Q3DSPresentation);
    return d->profiling;
}

void Q3DSPresentation::setProfilingEnabled(bool enable)
{
    // In this API "profiling" means both the scene manager's EnableProfiling
    // (enables Qt3D QObject tracking; must be set before building the scene)
    // and the ImGui-based profile UI (that can be toggled at any time in the
    // private API - for simplicity there's a single flag for both here, which
    // must be set up front). Defaults to disabled.

    Q_D(Q3DSPresentation);
    if (d->profiling != enable) {
        d->profiling = enable; // no effect until next setSource()
        emit profilingEnabledChanged();
    }
}

bool Q3DSPresentation::isProfileUiVisible() const
{
    Q_D(const Q3DSPresentation);
    return d->profiling ? d->profileUiVisible : false;
}

void Q3DSPresentation::setProfileUiVisible(bool visible)
{
    Q_D(Q3DSPresentation);
    if (d->profiling && d->profileUiVisible != visible) {
        d->profileUiVisible = visible;
        if (d->controller)
            d->controller->handleSetProfileUiVisible(d->profileUiVisible, d->profileUiScale);
        emit profileUiVisibleChanged();
    }
}

float Q3DSPresentation::profileUiScale() const
{
    Q_D(const Q3DSPresentation);
    return d->profileUiScale;
}

void Q3DSPresentation::setProfileUiScale(float scale)
{
    Q_D(Q3DSPresentation);
    if (d->profileUiScale != scale) {
        d->profileUiScale = scale;
        if (d->controller)
            d->controller->handleSetProfileUiVisible(d->profileUiVisible, d->profileUiScale);
        emit profileUiScaleChanged();
    }
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

void Q3DSPresentation::goToTime(const QString &elementPath, float timeSeconds)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleGoToTime(elementPath, timeSeconds);
}

void Q3DSPresentation::goToSlide(const QString &elementPath, const QString &name)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleGoToSlideByName(elementPath, name);
}

void Q3DSPresentation::goToSlide(const QString &elementPath, int index)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleGoToSlideByIndex(elementPath, index);
}

void Q3DSPresentation::goToSlide(const QString &elementPath, bool next, bool wrap)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleGoToSlideByDirection(elementPath, next, wrap);
}

QVariant Q3DSPresentation::getAttribute(const QString &elementPath, const QString &attributeName)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        return d->controller->handleGetAttribute(elementPath, attributeName);

    return QVariant();
}

void Q3DSPresentation::setAttribute(const QString &elementPath, const QString &attributeName, const QVariant &value)
{
    Q_D(Q3DSPresentation);
    if (d->controller)
        d->controller->handleSetAttribute(elementPath, attributeName, value);
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

void Q3DSPresentation::touchEvent(QTouchEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationTouchEvent(e);
}

#if QT_CONFIG(tabletevent)
void Q3DSPresentation::tabletEvent(QTabletEvent *e)
{
    Q_D(Q3DSPresentation);
    if (d->controller && !d->source.isEmpty())
        d->controller->handlePresentationTabletEvent(e);
}
#endif

void Q3DSPresentationPrivate::setController(Q3DSPresentationController *c)
{
    if (controller == c)
        return;

    controller = c;
    controller->handlePresentationSource(source, sourceFlags(), inlineQmlSubPresentations);
}

Q3DSPresentationController::SourceFlags Q3DSPresentationPrivate::sourceFlags() const
{
    Q3DSPresentationController::SourceFlags flags = 0;
    if (profiling)
        flags |= Q3DSPresentationController::Profiling;

    return flags;
}

bool Q3DSPresentationPrivate::compareElementPath(const QString &a, const QString &b) const
{
    return controller ? controller->compareElementPath(a, b) : false;
}

void Q3DSPresentationPrivate::registerInlineQmlSubPresentations(const QVector<Q3DSInlineQmlSubPresentation *> &list)
{
    inlineQmlSubPresentations += list;
}

QT_END_NAMESPACE
