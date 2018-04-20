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

#include "q3dspresentationitem_p.h"
#include <private/q3dsdatainput_p.h>

QT_BEGIN_NAMESPACE

Q3DSPresentationItem::Q3DSPresentationItem(QObject *parent)
    : Q3DSPresentation(parent)
{
}

Q3DSPresentationItem::~Q3DSPresentationItem()
{
}

QQmlListProperty<QObject> Q3DSPresentationItem::qmlChildren()
{
    return QQmlListProperty<QObject>(this, nullptr, &appendQmlChildren, nullptr, nullptr, nullptr);
}

void Q3DSPresentationItem::appendQmlChildren(QQmlListProperty<QObject> *list, QObject *obj)
{
    if (Q3DSPresentationItem *item = qobject_cast<Q3DSPresentationItem *>(list->object)) {
        obj->setParent(item);
        if (Q3DSDataInput *dataInput = qobject_cast<Q3DSDataInput *>(obj))
            Q3DSDataInputPrivate::get(dataInput)->presentation = item;
    }
}

void Q3DSPresentationItem::studio3DPresentationLoaded()
{
    for (QObject *obj : children()) {
        if (Q3DSDataInput *dataInput = qobject_cast<Q3DSDataInput *>(obj))
            Q3DSDataInputPrivate::get(dataInput)->sendValue();
    }
}

QT_END_NAMESPACE
