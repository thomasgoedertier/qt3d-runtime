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

#include "q3dssceneelement_p.h"
#include <private/q3dspresentation_p.h>

QT_BEGIN_NAMESPACE

Q3DSSceneElement::Q3DSSceneElement(QObject *parent)
    : Q3DSElement(*new Q3DSSceneElementPrivate, parent)
{
}

Q3DSSceneElement::Q3DSSceneElement(const QString &elementPath, QObject *parent)
    : Q3DSElement(*new Q3DSSceneElementPrivate, parent)
{
    Q_D(Q3DSSceneElement);
    d->elementPath = elementPath;
}

Q3DSSceneElement::Q3DSSceneElement(Q3DSPresentation *presentation, const QString &elementPath, QObject *parent)
    : Q3DSElement(*new Q3DSSceneElementPrivate, parent)
{
    Q_D(Q3DSSceneElement);
    d->elementPath = elementPath;
    d->presentation = presentation;
}

Q3DSSceneElement::Q3DSSceneElement(Q3DSSceneElementPrivate &dd, QObject *parent)
    : Q3DSElement(dd, parent)
{
}

Q3DSSceneElement::~Q3DSSceneElement()
{
}

int Q3DSSceneElement::currentSlideIndex() const
{
    Q_D(const Q3DSSceneElement);
    return d->currentSlideIndex;
}

int Q3DSSceneElement::previousSlideIndex() const
{
    Q_D(const Q3DSSceneElement);
    return d->previousSlideIndex;
}

QString Q3DSSceneElement::currentSlideName() const
{
    Q_D(const Q3DSSceneElement);
    return d->currentSlideName;
}

QString Q3DSSceneElement::previousSlideName() const
{
    Q_D(const Q3DSSceneElement);
    return d->previousSlideName;
}

void Q3DSSceneElement::setCurrentSlideIndex(int currentSlideIndex)
{
    Q_D(Q3DSSceneElement);
    // This is also exposed as a property so we may need to defer applying the
    // value transparently to the user code. (relevant with QML + Studio3D)
    if (d->presentation && !d->elementPath.isEmpty())
        d->presentation->goToSlide(d->elementPath, currentSlideIndex);
    else
        d->pendingSlideSetIndex = currentSlideIndex; // defer to sendPendingValues()
}

void Q3DSSceneElement::setCurrentSlideName(const QString &currentSlideName)
{
    Q_D(Q3DSSceneElement);
    if (d->presentation && !d->elementPath.isEmpty())
        d->presentation->goToSlide(d->elementPath, currentSlideName);
    else
        d->pendingSlideSetName = currentSlideName; // defer to sendPendingValues()
}

void Q3DSSceneElement::goToSlide(bool next, bool wrap)
{
    Q_D(Q3DSSceneElement);
    if (d->presentation && !d->elementPath.isEmpty())
        d->presentation->goToSlide(d->elementPath, next, wrap);
}

void Q3DSSceneElement::goToTime(float timeSeconds)
{
    Q_D(Q3DSSceneElement);
    if (d->presentation && !d->elementPath.isEmpty())
        d->presentation->goToTime(d->elementPath, timeSeconds);
}

void Q3DSSceneElementPrivate::sendPendingValues()
{
    if (!presentation || elementPath.isEmpty())
        return;

    if (pendingSlideSetIndex >= 0) {
        presentation->goToSlide(elementPath, pendingSlideSetIndex);
        pendingSlideSetIndex = -1;
    }
    if (!pendingSlideSetName.isEmpty()) {
        presentation->goToSlide(elementPath, pendingSlideSetName);
        pendingSlideSetName.clear();
    }
}

void Q3DSSceneElementPrivate::_q_onSlideEntered(const QString &contextElemPath, int index, const QString &name)
{
    Q_Q(Q3DSSceneElement);
    Q_ASSERT(presentation);

    // only care about slide changes for the scene or component our elementPath specifies
    if (!Q3DSPresentationPrivate::get(presentation)->compareElementPath(contextElemPath, elementPath))
        return;

    const bool notifyCurrent = currentSlideIndex != index;
    const bool notifyPrevious = previousSlideIndex != currentSlideIndex;

    previousSlideIndex = currentSlideIndex;
    previousSlideName = currentSlideName;

    currentSlideIndex = index;
    currentSlideName = name;

    if (notifyPrevious) {
        emit q->previousSlideIndexChanged(previousSlideIndex);
        emit q->previousSlideNameChanged(previousSlideName);
    }

    if (notifyCurrent) {
        emit q->currentSlideIndexChanged(currentSlideIndex);
        emit q->currentSlideNameChanged(currentSlideName);
    }
}

void Q3DSSceneElementPrivate::setPresentation(Q3DSPresentation *pres)
{
    Q_Q(Q3DSSceneElement);
    presentation = pres;
    QObject::connect(presentation, SIGNAL(slideEntered(QString,int,QString)),
                     q, SLOT(_q_onSlideEntered(QString,int,QString)));
}

QT_END_NAMESPACE

#include "moc_q3dssceneelement.cpp"
