/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef Q3DSDATAMODELPARSER_P_H
#define Q3DSDATAMODELPARSER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "q3dsabstractxmlparser_p.h"
#include "q3dsuippresentation_p.h"
#include "q3dspresentationcommon_p.h"
#include <QDebug>

QT_BEGIN_NAMESPACE

class Q3DSV_PRIVATE_EXPORT Q3DSDataModelParser : public Q3DSAbstractXmlParser
{
public:
    struct Property
    {
        QString name;
        Q3DS::PropertyType type = Q3DS::Unknown;
        int componentCount = 1;
        QString typeStr;
        QStringList enumValues;
        QString defaultValue;
        bool animatable = true;
    };

    static Q3DSDataModelParser *instance();

    const QVector<Property> *propertiesForType(const QString &typeName);

private:
    Q3DSDataModelParser();
    void parseMetaData();
    void parseProperty(QVector<Property> *props);

    bool m_valid = false;

    QHash<QString, QVector<Property> > m_props;
};

QDebug operator<<(QDebug dbg, const Q3DSDataModelParser::Property &prop);

QT_END_NAMESPACE

#endif // Q3DSDATAMODELPARSER_P_H
