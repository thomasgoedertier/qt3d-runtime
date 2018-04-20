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

Q3DSDataInput::Q3DSDataInput(QObject *parent)
    : QObject(*new Q3DSDataInputPrivate, parent)
{
}

Q3DSDataInput::Q3DSDataInput(const QString &name, QObject *parent)
    : QObject(*new Q3DSDataInputPrivate, parent)
{
    Q_D(Q3DSDataInput);
    d->name = name;
}

Q3DSDataInput::Q3DSDataInput(Q3DSPresentation *presentation, const QString &name, QObject *parent)
    : QObject(*new Q3DSDataInputPrivate, parent)
{
    Q_D(Q3DSDataInput);
    d->name = name;
    d->presentation = presentation;
}

Q3DSDataInput::Q3DSDataInput(Q3DSDataInputPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

Q3DSDataInput::~Q3DSDataInput()
{
}

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

QT_END_NAMESPACE
