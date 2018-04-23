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

Q3DSElement::Q3DSElement(QObject *parent)
    : QObject(*new Q3DSElementPrivate, parent)
{
}

Q3DSElement::Q3DSElement(const QString &elementPath, QObject *parent)
    : QObject(*new Q3DSElementPrivate, parent)
{
    Q_D(Q3DSElement);
    d->elementPath = elementPath;
}

Q3DSElement::Q3DSElement(Q3DSPresentation *presentation, const QString &elementPath, QObject *parent)
    : QObject(*new Q3DSElementPrivate, parent)
{
    Q_D(Q3DSElement);
    d->elementPath = elementPath;
    d->presentation = presentation;
}

Q3DSElement::Q3DSElement(Q3DSElementPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

Q3DSElement::~Q3DSElement()
{
}

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

void Q3DSElement::setAttribute(const QString &attributeName, const QVariant &value)
{
    Q_D(Q3DSElement);
    if (d->presentation && !d->elementPath.isEmpty())
        d->presentation->setAttribute(d->elementPath, attributeName, value);
}

void Q3DSElement::fireEvent(const QString &eventName)
{
    Q_D(Q3DSElement);
    if (d->presentation && !d->elementPath.isEmpty())
        d->presentation->fireEvent(d->elementPath, eventName);
}

QT_END_NAMESPACE
