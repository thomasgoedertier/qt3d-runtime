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

#include "q3dsinlineqmlsubpresentation_p.h"

QT_BEGIN_NAMESPACE

Q3DSInlineQmlSubPresentation::Q3DSInlineQmlSubPresentation(QObject *parent)
    : QObject(parent)
{
}

Q3DSInlineQmlSubPresentation::~Q3DSInlineQmlSubPresentation()
{
}

QString Q3DSInlineQmlSubPresentation::presentationId() const
{
    return m_presentationId;
}

QQuickItem *Q3DSInlineQmlSubPresentation::item() const
{
    return m_item;
}

void Q3DSInlineQmlSubPresentation::setPresentationId(const QString &presentationId)
{
    if (m_presentationId != presentationId) {
        m_presentationId = presentationId;
        emit presentationIdChanged();
    }
}

void Q3DSInlineQmlSubPresentation::setItem(QQuickItem *item)
{
    if (m_item != item) {
        m_item = item;
        emit itemChanged();
    }
}

QT_END_NAMESPACE
