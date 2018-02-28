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

#ifndef Q3DSBEHAVIOR_P_H
#define Q3DSBEHAVIOR_P_H

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
#include "q3dspresentationcommon_p.h"
#include <QVector>

QT_BEGIN_NAMESPACE

class Q3DSV_PRIVATE_EXPORT Q3DSBehavior
{
public:
    Q3DSBehavior();
    bool isNull() const;

    QString qmlCode() const { return m_qmlCode; }

    struct Property {
        QString name;
        QString formalName;
        Q3DS::PropertyType type = Q3DS::Float;
        QString defaultValue;
        QString publishLevel;
        QString description;
    };

    struct Handler {
        QString name;
        QString formalName;
        QString category;
        QString description;
    };

    const QVector<Q3DSBehavior::Property> &properties() const { return m_properties; }
    const QVector<Q3DSBehavior::Handler> &handlers() const { return m_handlers; }

private:
    QString m_qmlCode;
    QVector<Property> m_properties;
    QVector<Handler> m_handlers;

    friend class Q3DSBehaviorParser;
};

Q_DECLARE_TYPEINFO(Q3DSBehavior::Property, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSBehavior::Handler, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSBehavior, Q_MOVABLE_TYPE);

class Q3DSV_PRIVATE_EXPORT Q3DSBehaviorParser
{
public:
    Q3DSBehavior parse(const QString &filename, bool *ok = nullptr);
};

QT_END_NAMESPACE

#endif // Q3DSBEHAVIOR_P_H
