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

#ifndef Q3DSUIADOCUMENT_P_H
#define Q3DSUIADOCUMENT_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "q3dsruntimeglobal_p.h"
#include "q3dsuipdocument_p.h"
#include "q3dsqmldocument_p.h"
#include <QVector>
#include <QString>

QT_BEGIN_NAMESPACE

struct Q3DSV_PRIVATE_EXPORT Q3DSDataInputEntry
{
    enum Type {
        TypeString,
        TypeRangedNumber
    };

    QString name;
    Type type = TypeString;
    float minValue = 0;
    float maxValue = 0;
};

Q_DECLARE_TYPEINFO(Q3DSDataInputEntry, Q_MOVABLE_TYPE);

class Q3DSV_PRIVATE_EXPORT Q3DSUiaDocument
{
public:
    Q3DSUiaDocument() = default;

    void setInitialDocumentId(const QString &id);
    QString initialDocumentId() const;

    void setDataInputEntries(const QVector<Q3DSDataInputEntry> &entries);
    const QVector<Q3DSDataInputEntry> dataInputEntries() const;

    void addSubDocument(const Q3DSUipDocument &uipDocument);
    void addSubDocument(const Q3DSQmlDocument &qmlDocument);

    // Remove all subdocuments
    void clear();

    const QVector<Q3DSUipDocument> uipDocuments() const;
    const QVector<Q3DSQmlDocument> qmlDocuments() const;

private:
    QString m_initialDocumentId;
    QVector<Q3DSUipDocument> m_uipDocuments;
    QVector<Q3DSQmlDocument> m_qmlDocuments;
    QVector<Q3DSDataInputEntry> m_dataInputEntries;
};

QT_END_NAMESPACE

#endif // Q3DSUIADOCUMENT_P_H
