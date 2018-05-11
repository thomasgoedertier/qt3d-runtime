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

/*!
    \class Q3DSSceneElement
    \inherits Q3DSElement
    \inmodule 3dstudioruntime2
    \since Qt 3D Studio 2.0

    \brief Controls the special Scene or Component scene objects in a Qt 3D
    Studio presentation.

    This class is a convenience class for controlling the properties of Scene
    and Component objects in the scene. These are special since they have a
    time context, meaning they control a timline and a set of associated
    slides.

    \note The functionality of Q3DSSceneElement is equivalent to
    Q3DSPresentation::goToTime() and Q3DSPresentation::goToSlide().

    \sa Q3DSPresentation, Q3DSWidget, Q3DSSurfaceViewer, Q3DSElement
 */

/*!
    \internal
 */
Q3DSSceneElement::Q3DSSceneElement(QObject *parent)
    : Q3DSElement(*new Q3DSSceneElementPrivate, parent)
{
}

/*!
    Constructs a Q3DSSceneElement instance and associated it with the object
    specified by \a elementPath and the given \a presentation. An optional \a
    parent object can be specified.
 */
Q3DSSceneElement::Q3DSSceneElement(Q3DSPresentation *presentation, const QString &elementPath, QObject *parent)
    : Q3DSElement(*new Q3DSSceneElementPrivate, parent)
{
    Q_D(Q3DSSceneElement);
    d->elementPath = elementPath;
    d->presentation = presentation;
}

/*!
    \internal
 */
Q3DSSceneElement::Q3DSSceneElement(Q3DSSceneElementPrivate &dd, QObject *parent)
    : Q3DSElement(dd, parent)
{
}

/*!
   Destructor.
 */
Q3DSSceneElement::~Q3DSSceneElement()
{
}

/*!
    \property Q3DSSceneElement::currentSlideIndex

    Holds the index of the currently active slide of the tracked time context.

    \note If this property is set to something else than the default slide for
    the scene at the initial declaration of SceneElement, a changed signal for
    the default slide may stil be emitted before the slide changes to the
    desired one. This happens in order to ensure we end up with the index of
    the slide that is actually shown even if the slide specified in the initial
    declaration is invalid.
*/
int Q3DSSceneElement::currentSlideIndex() const
{
    Q_D(const Q3DSSceneElement);
    return d->currentSlideIndex;
}

/*!
    \property Q3DSSceneElement::previousSlideIndex

    Holds the index of the previously active slide of the tracked time context.

    This property is read-only.
*/
int Q3DSSceneElement::previousSlideIndex() const
{
    Q_D(const Q3DSSceneElement);
    return d->previousSlideIndex;
}

/*!
    \property Q3DSSceneElement::currentSlideName

    Holds the name of the currently active slide of the tracked time context.

    \note If this property is set to something else than the default slide for
    the scene at the initial declaration of SceneElement, a changed signal for
    the default slide may stil be emitted before the slide changes to the
    desired one. This happens in order to ensure we end up with the index of
    the slide that is actually shown even if the slide specified in the initial
    declaration is invalid.
*/
QString Q3DSSceneElement::currentSlideName() const
{
    Q_D(const Q3DSSceneElement);
    return d->currentSlideName;
}

/*!
    \property Q3DSSceneElement::previousSlideName

    Holds the name of the previously active slide of the tracked time context.

    This property is read-only.
*/
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

/*!
    Requests a time context (a Scene or a Component object) to change to the
    next or previous slide, depending on the value of \a next. If the context
    is already at the last or first slide, \a wrap defines if wrapping over to
    the first or last slide, respectively, occurs.
 */
void Q3DSSceneElement::goToSlide(bool next, bool wrap)
{
    Q_D(Q3DSSceneElement);
    if (d->presentation && !d->elementPath.isEmpty())
        d->presentation->goToSlide(d->elementPath, next, wrap);
}

/*!
    Moves the timeline for a time context (a Scene or a Component element) to a
    specific position. The position is given in seconds in \a timeSeconds.
 */
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

/*!
    \qmltype SceneElement
    \instantiates Q3DSSceneElement
    \inherits Element
    \inqmlmodule QtStudio3D
    \ingroup 3dstudioruntime2
    \brief Control type for scene and component elements in a Qt 3D Studio presentation.

    This type is a convenience type for managing the slides of a single
    time context (a Scene or a Component element) of a presentation.

    All methods provided by this type are queued and handled asynchronously before the next
    frame is displayed.

    \sa Studio3D, Presentation, Element
*/

/*!
    \qmlproperty int SceneElement::currentSlideIndex

    Holds the index of the currently active slide of the tracked time context.

    Changing the current slide via this property is asynchronous. The property
    value will not actually change until the next frame has been processed, and
    even then only if the new slide was valid.

    \note If this property is set to something else than the default slide for the scene at the
    initial declaration of SceneElement, you will still get an extra changed signal for the
    default slide before the slide changes to the desired one. This happens in order to ensure
    we end up with the index of the slide that is actually shown even if the slide specified in the
    initial declaration is invalid.
*/

/*!
    \qmlproperty int SceneElement::previousSlideIndex

    Holds the index of the previously active slide of the tracked time context.

    This property is read-only.
*/

/*!
    \qmlproperty string SceneElement::currentSlideName

    Holds the name of the currently active slide of the tracked time context.

    Changing the current slide via this property is asynchronous. The property
    value will not actually change until the next frame has been processed, and
    even then only if the new slide was valid.

    \note If this property is set to something else than the default slide for the scene at the
    initial declaration of SceneElement, you will still get an extra changed signal for the
    default slide before the slide changes to the desired one. This happens in order to ensure
    we end up with the name of the slide that is actually shown even if the slide specified in the
    initial declaration is invalid.
*/

/*!
    \qmlproperty string SceneElement::previousSlideName

    Holds the name of the previously active slide of the tracked time context.

    This property is read-only.
*/

/*!
    \qmlsignal SceneElement::currentSlideIndexChanged(int currentSlideIndex)

    This signal is emitted when the current slide changes.
    The new value is provided in the \a currentSlideIndex parameter.

    This signal is always emitted with currentSlideNameChanged.

    The corresponding handler is \c onCurrentSlideIndexChanged.
*/

/*!
    \qmlsignal SceneElement::previousSlideIndexChanged(int previousSlideIndex)

    This signal is emitted when the previous slide changes.
    The new value is provided in the \a previousSlideIndex parameter.

    This signal is always emitted with previousSlideNameChanged.

    The corresponding handler is \c onPreviousSlideIndexChanged.
*/

/*!
    \qmlsignal SceneElement::currentSlideNameChanged(string currentSlideName)

    This signal is emitted when the current slide changes.
    The new value is provided in the \a currentSlideName parameter.

    This signal is always emitted with currentSlideIndexChanged.

    The corresponding handler is \c onCurrentSlideNameChanged.
*/

/*!
    \qmlsignal SceneElement::previousSlideNameChanged(string previousSlideName)

    This signal is emitted when the previous slide changes.
    The new value is provided in the \a previousSlideName parameter.

    This signal is always emitted with previousSlideIndexChanged.

    The corresponding handler is \c onPreviousSlideNameChanged.
*/

/*!
    \qmlmethod void SceneElement::goToSlide(bool next, bool wrap)

    Requests a time context (a Scene or a Component element) to change to the next or the
    previous slide, depending on the value of \a next. If the context is already at the
    last or first slide, \a wrap defines if change occurs to the opposite end.
*/

/*!
    \qmlmethod void SceneElement::goToTime(string elementPath, real time)

    Sets a time context (a Scene or a Component element) to a specific playback \a time in seconds.

    For behavior details, see Presentation::goToTime() documentation.
*/

QT_END_NAMESPACE

#include "moc_q3dssceneelement.cpp"
