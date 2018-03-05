/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#include "q3dsbehaviorobject_p.h"

QT_BEGIN_NAMESPACE

Q3DSBehaviorObject::Q3DSBehaviorObject(QObject *parent)
    : QObject(parent)
{
}

float Q3DSBehaviorObject::getDeltaTime()
{
    return 0;
}

float Q3DSBehaviorObject::getAttribute(const QString &attribute)
{
    Q_UNUSED(attribute);
    return 0;
}

void Q3DSBehaviorObject::setAttribute(const QString &attribute, const QVariant &value)
{
    Q_UNUSED(attribute); Q_UNUSED(value);
}

void Q3DSBehaviorObject::setAttribute(const QString &handle, const QString &attribute, const QVariant &value)
{
    Q_UNUSED(handle);Q_UNUSED(attribute);Q_UNUSED(value);
}

void Q3DSBehaviorObject::fireEvent(const QString &event)
{
    Q_UNUSED(event);
}

void Q3DSBehaviorObject::registerForEvent(const QString &event, const QJSValue &function)
{
    Q_UNUSED(event); Q_UNUSED(function);
}

void Q3DSBehaviorObject::registerForEvent(const QString &handle, const QString &event, const QJSValue &function)
{
    Q_UNUSED(handle); Q_UNUSED(event); Q_UNUSED(function);
}

void Q3DSBehaviorObject::unregisterForEvent(const QString &event)
{
    Q_UNUSED(event);
}

void Q3DSBehaviorObject::unregisterForEvent(const QString &handle, const QString &event)
{
    Q_UNUSED(handle); Q_UNUSED(event);
}

QVector2D Q3DSBehaviorObject::getMousePosition()
{
    return QVector2D();
}

QMatrix4x4 Q3DSBehaviorObject::calculateGlobalTransform(const QString &handle)
{
    Q_UNUSED(handle);
    return QMatrix4x4();
}

QVector3D Q3DSBehaviorObject::lookAt(const QVector3D &target)
{
    Q_UNUSED(target);
    return QVector3D();
}

QVector3D Q3DSBehaviorObject::matrixToEuler(const QMatrix4x4 &matrix)
{
    Q_UNUSED(matrix);
    return QVector3D();
}

QString Q3DSBehaviorObject::getParent(const QString &handle)
{
    Q_UNUSED(handle);
    return QString();
}

void Q3DSBehaviorObject::setDataInputValue(const QString &name, const QVariant &value)
{
    Q_UNUSED(name); Q_UNUSED(value);
}

QT_END_NAMESPACE
