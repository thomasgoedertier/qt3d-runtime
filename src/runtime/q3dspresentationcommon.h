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

#ifndef Q3DSPRESENTATIONCOMMON_H
#define Q3DSPRESENTATIONCOMMON_H

#include <QString>
#include <QVector2D>
#include <QVector3D>

QT_BEGIN_NAMESPACE

class QXmlStreamReader;

namespace Q3DS {

enum PropertyType {     // value format
    Unknown = 0,
    StringList,         // String
    FloatRange,         // Float
    LongRange,          // Long
    Float,              // Float
    Long,               // Long
    Float2,             // Float2
    Vector,             // Float3
    Scale,              // Float3
    Rotation,           // Float3
    Color,              // Float3
    Boolean,            // Bool
    Slide,              // String
    Font,               // String
    FontSize,           // Float
    String,             // String
    MultiLineString,    // String
    ObjectRef,          // ObjectRef
    Image,              // String
    Mesh,               // String
    Import,             // String
    Texture,            // String
    Image2D,            // String
    Buffer,             // String
    Guid,               // Long4
    StringListOrInt,    // StringOrInt
    Renderable,         // String
    PathBuffer,         // String
    Enum                // depends on name; data model only
};

bool convertToPropertyType(const QStringRef &value, Q3DS::PropertyType *type, int *componentCount, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToFloat(const QStringRef &value, float *v, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToInt(const QStringRef &value, int *v, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToInt32(const QStringRef &value, qint32 *v, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToBool(const QStringRef &value, bool *v, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToVector2D(const QStringRef &value, QVector2D *v, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToVector3D(const QStringRef &value, QVector3D *v, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
int animatablePropertyTypeToMetaType(Q3DS::PropertyType type);
QVariant convertToVariant(const QString &value, Q3DS::PropertyType type);

} // namespace Q3DS

QT_END_NAMESPACE

#endif // Q3DSPRESENTATIONCOMMON_H
