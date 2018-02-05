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

#include "q3dsabstractdocument_p.h"

QT_BEGIN_NAMESPACE

Q3DSAbstractDocument::Q3DSAbstractDocument()
{
}

void Q3DSAbstractDocument::setId(const QString &id)
{
    m_id = id;
}

QString Q3DSAbstractDocument::id() const
{
    return m_id;
}

void Q3DSAbstractDocument::setSource(const QString &source)
{
    m_source = source;
}

QString Q3DSAbstractDocument::source() const
{
    return m_source;
}

void Q3DSAbstractDocument::setSourceData(const QByteArray &data)
{
    m_sourceData = data;
}

QByteArray Q3DSAbstractDocument::sourceData() const
{
    return m_sourceData;
}

QT_END_NAMESPACE
