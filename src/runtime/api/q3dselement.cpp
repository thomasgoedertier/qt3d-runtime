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

#include "q3dselement_p.h"
#include "q3dspresentation.h"

QT_BEGIN_NAMESPACE

/*!
    \class Q3DSElement
    \inmodule 3dstudioruntime2
    \since Qt 3D Studio 2.0

    \brief Controls a scene object (node) in a Qt 3D Studio presentation.

    This class is a convenience class for controlling the properties of a scene
    object (such as, model, material, camera, layer) in a Qt 3D Studio
    presentation.

    \note The functionality of Q3DSElement is equivalent to
    Q3DSPresentation::setAttribute(), Q3DSPresentation::getAttribute(), and
    Q3DSPresentation::fireEvent().

    \sa Q3DSPresentation, Q3DSWidget, Q3DSSurfaceViewer, Q3DSSceneElement
 */

/*!
    \internal
 */
Q3DSElement::Q3DSElement(QObject *parent)
    : QObject(*new Q3DSElementPrivate, parent)
{
}

/*!
    Constructs a Q3DSElement instance controlling the scene object specified by
    \a elementPath. An optional \a parent object can be specified. The
    constructed instance is automatically associated with the specified \a
    presentation. An optional \a parent object can be specified.
 */
Q3DSElement::Q3DSElement(Q3DSPresentation *presentation, const QString &elementPath, QObject *parent)
    : QObject(*new Q3DSElementPrivate, parent)
{
    Q_D(Q3DSElement);
    d->elementPath = elementPath;
    d->presentation = presentation;
}

/*!
    \internal
 */
Q3DSElement::Q3DSElement(Q3DSElementPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

/*!
    Destructor.
 */
Q3DSElement::~Q3DSElement()
{
}

/*!
    \property Q3DSElement::elementPath

    Holds the element path of the presentation element.

    An element path refers to an object in the scene either by name or id. The
    latter is rarely used in application code since the unique IDs are not
    exposed in the Qt 3D Studio application. To refer to an object by id,
    prepend \c{#} to the name. Applications will typically refer to objects by
    name.

    Names are not necessarily unique, however. To access an object with a
    non-unique name, the path can be specified, for example,
    \c{Scene.Layer.Camera}. Here the right camera object gets chosen even if
    the scene contains other layers with the default camera names (for instance
    \c{Scene.Layer2.Camera}).

    If the object is renamed to a unique name in the Qt 3D Studio application's
    Timeline view, the path can be omitted. For example, if the camera in
    question was renamed to \c MyCamera, applications can then simply pass \c
    MyCamera as the element path.

    To access an object in a sub-presentation, prepend the name of the
    sub-presentation followed by a colon, for example,
    \c{SubPresentationOne:Scene.Layer.Camera}.
 */
QString Q3DSElement::elementPath() const
{
    Q_D(const Q3DSElement);
    return d->elementPath;
}

void Q3DSElement::setElementPath(const QString &elementPath)
{
    Q_D(Q3DSElement);
    if (d->elementPath != elementPath) {
        d->elementPath = elementPath;
        emit elementPathChanged();
    }
}

/*!
    Returns the current value of an attribute (property) of the scene
    object specified by elementPath.

    The \a attributeName is the \l{Attribute Names}{scripting name} of the attribute.
 */
QVariant Q3DSElement::getAttribute(const QString &attributeName) const
{
    Q_D(const Q3DSElement);
    if (d->presentation && !d->elementPath.isEmpty())
        return d->presentation->getAttribute(d->elementPath, attributeName);

    return QVariant();
}

/*!
    Sets the \a value of an attribute (property) of the scene object
    specified by elementPath.

    The \a attributeName is the \l{Attribute Names}{scripting name} of the attribute.
 */
void Q3DSElement::setAttribute(const QString &attributeName, const QVariant &value)
{
    Q_D(Q3DSElement);
    if (d->presentation && !d->elementPath.isEmpty())
        d->presentation->setAttribute(d->elementPath, attributeName, value);
}

/*!
    Dispatches an event with \a eventName on the scene object
    specified by elementPath.

    Appropriate actions created in Qt 3D Studio or callbacks registered using
    the registerForEvent() method in attached (behavior) scripts will be
    executed in response to the event.
 */
void Q3DSElement::fireEvent(const QString &eventName)
{
    Q_D(Q3DSElement);
    if (d->presentation && !d->elementPath.isEmpty())
        d->presentation->fireEvent(d->elementPath, eventName);
}

void Q3DSElementPrivate::setPresentation(Q3DSPresentation *pres)
{
    presentation = pres;
}

/*!
    \qmltype Element
    \instantiates Q3DSElement
    \inqmlmodule QtStudio3D
    \ingroup 3dstudioruntime2
    \brief Control type for elements in a Qt 3D Studio presentation.

    This type is a convenience for controlling the properties of a scene object
    (such as, model, material, camera, layer) in a Qt 3D Studio presentation.

    \note The functionality of Element is equivalent to
    Presentation::setAttribute(), Presentation::getAttribute() and
    Presentation::fireEvent().

    \sa Studio3D, Presentation, SceneElement
*/

/*!
    \qmlproperty string Element::elementPath

    Holds the element path of the presentation element.

    An element path refers to an object in the scene either by name or id. The
    latter is rarely used in application code since the unique IDs are not
    exposed in the Qt 3D Studio application. To refer to an object by id,
    prepend \c{#} to the name. Applications will typically refer to objects by
    name.

    Names are not necessarily unique, however. To access an object with a
    non-unique name, the path can be specified, for example,
    \c{Scene.Layer.Camera}. Here the right camera object gets chosen even if
    the scene contains other layers with the default camera names (for instance
    \c{Scene.Layer2.Camera}).

    If the object is renamed to a unique name in the Qt 3D Studio application's
    Timeline view, the path can be omitted. For example, if the camera in
    question was renamed to \c MyCamera, applications can then simply pass \c
    MyCamera as the element path.

    To access an object in a sub-presentation, prepend the name of the
    sub-presentation followed by a colon, for example,
    \c{SubPresentationOne:Scene.Layer.Camera}.
*/

/*!
    \qmlmethod variant Element::getAttribute(string attributeName)

    Returns the current value of an attribute (property) of the scene object
    specified by this Element instance. The \a attributeName is the
    \l{Attribute Names}{scripting name} of the attribute.
 */

/*!
    \qmlmethod void Element::setAttribute(string attributeName, variant value)

    Sets the \a value of an attribute (property) of the scene object specified
    by this Element instance. The \a attributeName is the \l{Attribute
    Names}{scripting name} of the attribute.
*/

/*!
    \qmlmethod void Element::fireEvent(string eventName)

    Dispatches an event with \a eventName on the scene object
    specified by elementPath.

    Appropriate actions created in Qt 3D Studio or callbacks registered using
    the registerForEvent() method in attached \c{behavior scripts} will be
    executed in response to the event.
*/

QT_END_NAMESPACE
