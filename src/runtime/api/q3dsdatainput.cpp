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

#include "q3dsdatainput_p.h"
#include "q3dspresentation.h"

QT_BEGIN_NAMESPACE

/*!
    \class Q3DSDataInput
    \inmodule 3dstudioruntime2
    \since Qt 3D Studio 2.0

    \brief Controls a data input entry in a Qt 3D Studio presentation.

    This class is a convenience class for controlling a data input in a presentation.

    \sa Q3DSPresentation
*/

/*!
    \internal
 */
Q3DSDataInput::Q3DSDataInput(QObject *parent)
    : QObject(*new Q3DSDataInputPrivate, parent)
{
}

/*!
    Constructs a Q3DSDataInput instance and initializes the \a name. The
    constructed instance is automatically associated with the specified \a
    presentation. An optional \a parent object can be specified.
 */
Q3DSDataInput::Q3DSDataInput(Q3DSPresentation *presentation, const QString &name, QObject *parent)
    : QObject(*new Q3DSDataInputPrivate, parent)
{
    Q_D(Q3DSDataInput);
    d->name = name;
    d->presentation = presentation;
}

/*!
    \internal
 */
Q3DSDataInput::Q3DSDataInput(Q3DSDataInputPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

/*!
    Destructor.
 */
Q3DSDataInput::~Q3DSDataInput()
{
}

/*!
    \property Q3DSDataInput::name

    Specifies the name of the controlled data input element in the
    presentation. This property must be set before setting the value property.
    The initial value is provided via the constructor in practice, but the name
    can also be changed later on, if desired.
 */
QString Q3DSDataInput::name() const
{
    Q_D(const Q3DSDataInput);
    return d->name;
}

void Q3DSDataInput::setName(const QString &name)
{
    Q_D(Q3DSDataInput);
    if (d->name != name) {
        d->name = name;
        emit nameChanged();
    }
}

/*!
    \property Q3DSDataInput::value

    Specifies the value of the controlled data input element in the
    presentation.

    The value of this property only accounts for changes done via the same
    Q3DSDataInput instance. If the value of the same data input in the
    presentation is changed elsewhere, for example via animations or
    Q3DSPresentation::setAttribute(), those changes are not reflected in the
    value of this property. Due to this uncertainty, this property treats all
    value sets as changes even if the newly set value is the same value as the
    previous value.
*/
QVariant Q3DSDataInput::value() const
{
    Q_D(const Q3DSDataInput);
    return d->value;
}

void Q3DSDataInput::setValue(const QVariant &value)
{
    Q_D(Q3DSDataInput);
    // Since properties controlled by data inputs can change without the
    // current value being reflected on the value of the DataInput element, we
    // allow setting the value to the same one it was previously and still
    // consider it a change. For example, when controlling timeline, the value
    // set to DataInput will only be the current value for one frame if
    // presentation has a running animation.
    d->value = value;
    d->sendValue();
    emit valueChanged();
}

void Q3DSDataInputPrivate::sendValue()
{
    if (!presentation || name.isEmpty())
        return;

    presentation->setDataInputValue(name, value);
}

/*!
    \qmltype DataInput
    \instantiates Q3DSDataInput
    \inqmlmodule QtStudio3D
    \ingroup 3dstudioruntime2

    \brief Controls a data input entry in a Qt 3D Studio presentation.

    This type is a convenience for controlling a data input in a presentation.
    Its functionality is equivalent to Presentation::setDataInputValue(),
    however it has a big advantage of being able to use QML property bindings,
    thus avoiding the need to having to resort to a JavaScript function call
    for every value change.

    As an example, compare the following two approaches:

    \qml
        Studio3D {
            ...
            Presentation {
                id: presentation
                ...
            }
        }

        Button {
            onClicked: presentation.setAttribute("SomeTextNode", "textstring", "Hello World")
        }
    \endqml

    \qml
        Studio3D {
            ...
            Presentation {
                id: presentation
                ...
                property string text: ""
                DataInput {
                    name: "inputForSomeTextNode"
                    value: presentation.text
                }
            }
        }

        Button {
            onClicked: presentation.text = "Hello World"
        }
    \endqml

    The latter assumes that a data input connection was made in Qt 3D Studio
    between the \c textstring property of \c SomeTextNode and a data input name
    \c inputForSomeTextNode. As the value is now set via a property, the full
    set of QML property bindings techniques are available.

    \sa Studio3D, Presentation
*/

/*!
    \qmlproperty string DataInput::name

    Specifies the name of the controlled data input element in the
    presentation. This property must be set as part of DataInput declaration,
    although it is changeable afterwards, if desired.
*/

/*!
    \qmlproperty variant DataInput::value

    Specifies the value of the controlled data input element in the presentation.

    The value of this property only accounts for changes done via the same
    DataInput instance. If the value of the underlying attribute in the
    presentation is changed elsewhere, for example via animations or
    Presentation::setAttribute(), those changes are not reflected in the value
    of this property. Due to this uncertainty, this property treats all value
    sets as changes even if the newly set value is the same value as the
    previous value.
*/

QT_END_NAMESPACE
