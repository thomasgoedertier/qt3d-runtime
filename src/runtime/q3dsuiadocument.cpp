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

#include "q3dsuiadocument_p.h"

QT_BEGIN_NAMESPACE

void Q3DSUiaDocument::setInitialDocumentId(const QString &id)
{
    m_initialDocumentId = id;
}

QString Q3DSUiaDocument::initialDocumentId() const
{
    return m_initialDocumentId;
}

void Q3DSUiaDocument::setDataInputEntries(const Q3DSDataInputEntry::Map &entries)
{
    m_dataInputEntries = entries;
}

const Q3DSDataInputEntry::Map Q3DSUiaDocument::dataInputEntries() const
{
    return m_dataInputEntries;
}

void Q3DSUiaDocument::addSubDocument(const Q3DSUipDocument &uipDocument)
{
    m_uipDocuments.append(uipDocument);
}

void Q3DSUiaDocument::addSubDocument(const Q3DSQmlDocument &qmlDocument)
{
    m_qmlDocuments.append(qmlDocument);
}

void Q3DSUiaDocument::clear()
{
    m_uipDocuments.clear();
    m_qmlDocuments.clear();
    m_dataInputEntries.clear();
}

const QVector<Q3DSUipDocument> Q3DSUiaDocument::uipDocuments() const
{
    return m_uipDocuments;
}

const QVector<Q3DSQmlDocument> Q3DSUiaDocument::qmlDocuments() const
{
    return m_qmlDocuments;
}

QT_END_NAMESPACE
