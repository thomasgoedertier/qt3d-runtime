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

#include "q3dspresentation_p.h"
#include "q3dsdatamodelparser_p.h"
#include "q3dsenummaps_p.h"
#include "q3dsuipparser_p.h"
#include "q3dsscenemanager_p.h"
#include <QXmlStreamReader>
#include <QLoggingCategory>
#include <functional>
#include <QtMath>
#include <QImage>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcUip)

namespace Q3DS {

bool convertToPropertyType(const QStringRef &value, Q3DS::PropertyType *type, int *componentCount, const char *desc, QXmlStreamReader *reader)
{
    if (componentCount)
        *componentCount = 1;
    bool ok = false;
    if (value == QStringLiteral("StringList")) {
        ok = true;
        *type = Q3DS::StringList;
    } else if (value == QStringLiteral("FloatRange")) {
        ok = true;
        *type = Q3DS::FloatRange;
    } else if (value == QStringLiteral("LongRange")) {
        ok = true;
        *type = Q3DS::LongRange;
    } else if (value == QStringLiteral("Float") || value == QStringLiteral("float")) {
        ok = true;
        *type = Q3DS::Float;
    } else if (value == QStringLiteral("Float2")) {
        ok = true;
        *type = Q3DS::Float2;
        if (componentCount)
            *componentCount = 2;
    } else if (value == QStringLiteral("Long")) {
        ok = true;
        *type = Q3DS::Long;
    } else if (value == QStringLiteral("Vector")) {
        ok = true;
        *type = Q3DS::Vector;
        if (componentCount)
            *componentCount = 3;
    } else if (value == QStringLiteral("Scale")) {
        ok = true;
        *type = Q3DS::Scale;
        if (componentCount)
            *componentCount = 3;
    } else if (value == QStringLiteral("Rotation")) {
        ok = true;
        *type = Q3DS::Rotation;
        if (componentCount)
            *componentCount = 3;
    } else if (value == QStringLiteral("Color")) {
        ok = true;
        *type = Q3DS::Color;
        if (componentCount)
            *componentCount = 3;
    } else if (value == QStringLiteral("Boolean")) {
        ok = true;
        *type = Q3DS::Boolean;
    } else if (value == QStringLiteral("Slide")) {
        ok = true;
        *type = Q3DS::Slide;
    } else if (value == QStringLiteral("Font")) {
        ok = true;
        *type = Q3DS::Font;
    } else if (value == QStringLiteral("FontSize")) {
        ok = true;
        *type = Q3DS::FontSize;
    } else if (value == QStringLiteral("String")) {
        ok = true;
        *type = Q3DS::String;
    } else if (value == QStringLiteral("MultiLineString")) {
        ok = true;
        *type = Q3DS::MultiLineString;
    } else if (value == QStringLiteral("ObjectRef")) {
        ok = true;
        *type = Q3DS::ObjectRef;
    } else if (value == QStringLiteral("Image")) {
        ok = true;
        *type = Q3DS::Image;
    } else if (value == QStringLiteral("Mesh")) {
        ok = true;
        *type = Q3DS::Mesh;
    } else if (value == QStringLiteral("Import")) {
        ok = true;
        *type = Q3DS::Import;
    } else if (value == QStringLiteral("Texture")) {
        ok = true;
        *type = Q3DS::Texture;
    } else if (value == QStringLiteral("Image2D")) {
        ok = true;
        *type = Q3DS::Image2D;
    } else if (value == QStringLiteral("Buffer")) {
        ok = true;
        *type = Q3DS::Buffer;
    } else if (value == QStringLiteral("Guid")) {
        ok = true;
        *type = Q3DS::Guid;
    } else if (value == QStringLiteral("StringListOrInt")) {
        ok = true;
        *type = Q3DS::StringListOrInt;
    } else if (value == QStringLiteral("Renderable")) {
        ok = true;
        *type = Q3DS::String;
    } else if (value == QStringLiteral("PathBuffer")) {
        ok = true;
        *type = Q3DS::String;
    } else if (value == QStringLiteral("ShadowMapResolution")) {
        ok = true;
        *type = Q3DS::Long;
    } else {
        *type = Q3DS::Unknown;
        if (reader)
            reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
    }
    return ok;
}

bool convertToInt(const QStringRef &value, int *v, const char *desc, QXmlStreamReader *reader)
{
    if (value.isEmpty()) {
        *v = 0;
        return true;
    }
    bool ok = false;
    *v = value.toInt(&ok);
    if (!ok && reader)
        reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
    return ok;
}

bool convertToInt32(const QStringRef &value, qint32 *v, const char *desc, QXmlStreamReader *reader)
{
    if (value.isEmpty()) {
        *v = 0;
        return true;
    }
    int vv;
    bool r = convertToInt(value, &vv, desc, reader);
    if (r)
        *v = qint32(vv);
    return r;
}

bool convertToBool(const QStringRef &value, bool *v, const char *desc, QXmlStreamReader *reader)
{
    Q_UNUSED(desc);
    Q_UNUSED(reader);
    *v = (value == QStringLiteral("True") || value == QStringLiteral("true")
          || value == QStringLiteral("Yes") || value == QStringLiteral("yes")
          || value == QStringLiteral("1"));
    return true;
}

bool convertToFloat(const QStringRef &value, float *v, const char *desc, QXmlStreamReader *reader)
{
    if (value.isEmpty()) {
        *v = 0;
        return true;
    }
    bool ok = false;
    *v = value.toFloat(&ok);
    if (!ok && reader)
        reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
    return ok;
}

bool convertToVector2D(const QStringRef &value, QVector2D *v, const char *desc, QXmlStreamReader *reader)
{
    QVector<QStringRef> floatStrings = value.split(' ', QString::SkipEmptyParts);
    if (floatStrings.count() != 2) {
        if (reader)
            reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
        return false;
    }
    float x;
    float y;
    if (!convertToFloat(floatStrings[0], &x, "Vector2D[x]", reader))
        return false;
    if (!convertToFloat(floatStrings[1], &y, "Vector2D[y]", reader))
        return false;
    v->setX(x);
    v->setY(y);
    return true;
}

bool convertToVector3D(const QStringRef &value, QVector3D *v, const char *desc, QXmlStreamReader *reader)
{
    QVector<QStringRef> floatStrings = value.split(' ', QString::SkipEmptyParts);
    if (floatStrings.count() != 3) {
        if (reader)
            reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
        return false;
    }
    float x;
    float y;
    float z;
    if (!convertToFloat(floatStrings[0], &x, "Vector3D[x]", reader))
        return false;
    if (!convertToFloat(floatStrings[1], &y, "Vector3D[y]", reader))
        return false;
    if (!convertToFloat(floatStrings[2], &z, "Vector3D[z]", reader))
        return false;
    v->setX(x);
    v->setY(y);
    v->setZ(z);
    return true;
}

int animatablePropertyTypeToMetaType(Q3DS::PropertyType type)
{
    switch (type) {
    case Float:
        return QMetaType::Float;
    case Long:
        return QVariant::Int;
    case Float2:
        return QVariant::Vector2D;
    case Vector:
    case Scale:
    case Rotation:
        return QVariant::Vector3D;
    case Color:
        return QVariant::Color;
    default:
        return QVariant::Invalid;
    }
}

QVariant convertToVariant(const QString &value, const Q3DSMaterial::PropertyElement &propMeta)
{
    switch (propMeta.type) {
    case StringList:
    case Slide:
    case Font:
    case String:
    case MultiLineString:
    case ObjectRef:
    case Image:
    case Mesh:
    case Import:
    case Texture:
    case Image2D:
    case Buffer:
    case Guid:
    case StringListOrInt:
    case Renderable:
    case PathBuffer:
        return value;
    case LongRange:
    case Long:
        return value.toInt();
    case FloatRange:
    case Float:
    case FontSize:
        return value.toFloat();
    case Float2:
    {
        QVector2D v;
        if (convertToVector2D(&value, &v))
            return v;
    }
        break;
    case Vector:
    case Scale:
    case Rotation:
    case Color:
    {
        QVector3D v;
        if (convertToVector3D(&value, &v))
            return v;
    }
        break;
    case Boolean:
    {
        bool v;
        if (convertToBool(&value, &v))
            return v;
    }
        break;
    case Enum:
    {
        int idx = propMeta.enumValues.indexOf(value);
        return idx >= 0 ? idx : 0;
    }
        break;
    default:
        break;
    }

    return QVariant();
}

} // namespace Q3DS

Q3DSGraphObjectAttached::~Q3DSGraphObjectAttached()
{

}

Q3DSGraphObject::Q3DSGraphObject(Q3DSGraphObject::Type type)
    : m_type(type)
{
}

Q3DSGraphObject::~Q3DSGraphObject()
{
    if (m_parent) {
        m_parent->removeChildNode(this);
        Q_ASSERT(!m_parent);
    }
    while (m_firstChild) {
        Q3DSGraphObject *child = m_firstChild;
        removeChildNode(child);
        Q_ASSERT(!child->m_parent);
        delete child;
    }
    Q_ASSERT(!m_firstChild && !m_lastChild);

    delete m_attached;
}

int Q3DSGraphObject::childCount() const
{
    int count = 0;
    Q3DSGraphObject *n = m_firstChild;
    while (n) {
        ++count;
        n = n->m_nextSibling;
    }
    return count;
}

Q3DSGraphObject *Q3DSGraphObject::childAtIndex(int idx) const
{
    Q3DSGraphObject *n = m_firstChild;
    while (idx && n) {
        --idx;
        n = n->m_nextSibling;
    }
    return n;
}

void Q3DSGraphObject::removeChildNode(Q3DSGraphObject *node)
{
    Q_ASSERT(node->parent() == this);

    Q3DSGraphObject *previous = node->m_previousSibling;
    Q3DSGraphObject *next = node->m_nextSibling;

    if (previous)
        previous->m_nextSibling = next;
    else
        m_firstChild = next;

    if (next)
        next->m_previousSibling = previous;
    else
        m_lastChild = previous;

    node->m_previousSibling = nullptr;
    node->m_nextSibling = nullptr;
    node->m_parent = nullptr;
}

void Q3DSGraphObject::prependChildNode(Q3DSGraphObject *node)
{
    Q_ASSERT_X(!node->m_parent, "Q3DSGraphObject::prependChildNode", "Q3DSGraphObject already has a parent");

    if (m_firstChild)
        m_firstChild->m_previousSibling = node;
    else
        m_lastChild = node;

    node->m_nextSibling = m_firstChild;
    m_firstChild = node;
    node->m_parent = this;
}

void Q3DSGraphObject::appendChildNode(Q3DSGraphObject *node)
{
    Q_ASSERT_X(!node->m_parent, "Q3DSGraphObject::appendChildNode", "Q3DSGraphObject already has a parent");

    if (m_lastChild)
        m_lastChild->m_nextSibling = node;
    else
        m_firstChild = node;

    node->m_previousSibling = m_lastChild;
    m_lastChild = node;
    node->m_parent = this;
}

int Q3DSGraphObject::addPropertyChangeObserver(PropertyChangeCallback callback)
{
    m_callbacks.append(callback);
    return m_callbacks.count() - 1;
}

void Q3DSGraphObject::removePropertyChangeObserver(int callbackId)
{
    m_callbacks[callbackId] = nullptr;
}

void Q3DSGraphObject::notifyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    const QSet<QString> keys = changeList->keys();
    const Q3DSPropertyChangeList::Flags flags = changeList->flags();
    for (auto f : m_callbacks) {
        if (f)
            f(this, keys, int(flags));
    }
}

QString Q3DSGraphObject::gex_typeAsString() const
{
    QString s;
    switch (type()) {
    case AnyObject:
        s = QLatin1String("Unknown");
        break;
    case Scene:
        s = QLatin1String("Scene");
        break;
    case Component:
        s = QLatin1String("Component");
        break;
    case Slide:
        s = QLatin1String("Slide");
        break;
    case Image:
        s = QLatin1String("Image");
        break;
    case DefaultMaterial:
        s = QLatin1String("DefaultMaterial");
        break;
    case ReferencedMaterial:
        s = QLatin1String("ReferencedMaterial");
        break;
    case CustomMaterial:
        s = QLatin1String("CustomMaterial");
        break;
    case Effect:
        s = QLatin1String("Effect");
        break;
    case Layer:
        s = QLatin1String("Layer");
        break;
    case Camera:
        s = QLatin1String("Camera");
        break;
    case Light:
        s = QLatin1String("Light");
        break;
    case Model:
        s = QLatin1String("Model");
        break;
    case Group:
        s = QLatin1String("Group");
        break;
    case Text:
        s = QLatin1String("Text");
        break;
    default:
        s = QLatin1String("UNKNOWN");
        break;
    }
    return s;
}

QStringList Q3DSGraphObject::gex_propertyNames() const
{
    return QStringList() << QLatin1String("id") << QLatin1String("starttime") << QLatin1String("endtime");
}

QVariantList Q3DSGraphObject::gex_propertyValues() const
{
    return QVariantList() << QString::fromUtf8(m_id) << m_startTime << m_endTime;
}

// The property conversion functions all follow the same pattern:
// 1. Check if a value is provided explicitly in the attribute list.
// 2. Then, when PropSetDefaults is set, see if the metadata provided a default value.
// 3. If all else fails, just return false. This is not fatal (and perfectly normal when PropSetDefaults is not set).

// V is const iterable with name() and value() on iter
template<typename T, typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags,
                   const QString &typeName, const QString &propName, Q3DS::PropertyType propType,
                   T *dst, std::function<bool(const QStringRef &, T *v)> convertFunc)
{
    auto it = std::find_if(attrs.cbegin(), attrs.cend(), [propName](const typename V::value_type &v) { return v.name() == propName; });
    if (it != attrs.cend()) {
        const QStringRef v = it->value();
        return convertFunc(it->value(), dst);
    } else if (flags.testFlag(Q3DSGraphObject::PropSetDefaults)) {
        Q3DSDataModelParser *dataModelParser = Q3DSDataModelParser::instance();
        if (dataModelParser) {
            const QVector<Q3DSDataModelParser::Property> *props = dataModelParser->propertiesForType(typeName);
            if (props) {
                auto it = std::find_if(props->cbegin(), props->cend(),
                                       [propName](const Q3DSDataModelParser::Property &v) { return v.name == propName; });
                if (it != props->cend()) {
                    Q_UNUSED(propType);
                    Q_ASSERT(it->type == propType);
                    return convertFunc(QStringRef(&it->defaultValue), dst);
                }
            }
        }
    }
    return false;
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, bool *dst)
{
    return ::parseProperty<bool>(attrs, flags, typeName, propName, Q3DS::Boolean, dst, [](const QStringRef &s, bool *v) { return Q3DS::convertToBool(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, qint32 *dst)
{
    return ::parseProperty<qint32>(attrs, flags, typeName, propName, Q3DS::Long, dst, [](const QStringRef &s, qint32 *v) { return Q3DS::convertToInt32(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, float *dst)
{
    return ::parseProperty<float>(attrs, flags, typeName, propName, Q3DS::Float, dst, [](const QStringRef &s, float *v) { return Q3DS::convertToFloat(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, QVector3D *dst)
{
    return ::parseProperty<QVector3D>(attrs, flags, typeName, propName, Q3DS::Vector, dst, [](const QStringRef &s, QVector3D *v) { return Q3DS::convertToVector3D(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, QColor *dst)
{
    QVector3D rgb;
    bool r = ::parseProperty<QVector3D>(attrs, flags, typeName, propName, Q3DS::Color, &rgb, [](const QStringRef &s, QVector3D *v) { return Q3DS::convertToVector3D(s, v); });
    if (r)
        *dst = QColor::fromRgbF(rgb.x(), rgb.y(), rgb.z());
    return r;
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, QString *dst)
{
    return ::parseProperty<QString>(attrs, flags, typeName, propName, Q3DS::String, dst, [](const QStringRef &s, QString *v) { *v = s.toString(); return true; });
}

template<typename V>
bool parseOpacityProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, float *dst)
{
    bool r = ::parseProperty<float>(attrs, flags, typeName, propName, Q3DS::Float, dst, [](const QStringRef &s, float *v) { return Q3DS::convertToFloat(s, v); });
    if (r)
        *dst /= 100.0f;
    return r;
}

template<typename V>
bool parseRotationProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, QVector3D *dst)
{
    return ::parseProperty<QVector3D>(attrs, flags, typeName, propName, Q3DS::Rotation, dst, [](const QStringRef &s, QVector3D *v) { return Q3DS::convertToVector3D(s, v); });
}

template<typename V>
bool parseImageProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, QString *dst)
{
    return ::parseProperty<QString>(attrs, flags, typeName, propName, Q3DS::Image, dst, [](const QStringRef &s, QString *v) { *v = s.toString(); return true; });
}

template<typename V>
bool parseMeshProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, QString *dst)
{
    return ::parseProperty<QString>(attrs, flags, typeName, propName, Q3DS::Mesh, dst, [](const QStringRef &s, QString *v) { *v = s.toString(); return true; });
}

template<typename V>
bool parseObjectRefProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, QString *dst)
{
    return ::parseProperty<QString>(attrs, flags, typeName, propName, Q3DS::ObjectRef, dst, [](const QStringRef &s, QString *v) { *v = s.toString(); return true; });
}

template<typename V>
bool parseMultiLineStringProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, QString *dst)
{
    return ::parseProperty<QString>(attrs, flags, typeName, propName, Q3DS::MultiLineString, dst, [](const QStringRef &s, QString *v) { *v = s.toString(); return true; });
}

template<typename V>
bool parseFontProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, QString *dst)
{
    return ::parseProperty<QString>(attrs, flags, typeName, propName, Q3DS::Font, dst, [](const QStringRef &s, QString *v) { *v = s.toString(); return true; });
}

template<typename V>
bool parseFontSizeProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, float *dst)
{
    return ::parseProperty<float>(attrs, flags, typeName, propName, Q3DS::FontSize, dst, [](const QStringRef &s, float *v) { return Q3DS::convertToFloat(s, v); });
}

struct StringOrInt {
    QString s;
    int n;
    bool isInt;
};

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, StringOrInt *dst)
{
    // StringListOrInt -> either an enum value or an int
    QString tmp;
    if (::parseProperty<QString>(attrs, flags, typeName, propName, Q3DS::StringListOrInt, &tmp,
                                 [](const QStringRef &s, QString *v) { *v = s.toString(); return true; }))
    {
        bool ok = false;
        int v = tmp.toInt(&ok);
        if (ok) {
            dst->isInt = true;
            dst->n = v;
        } else {
            dst->isInt = false;
            dst->s = tmp;
        }
        return true;
    }
    return false;
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSNode::RotationOrder *dst)
{
    return ::parseProperty<Q3DSNode::RotationOrder>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSNode::RotationOrder *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSNode::Orientation *dst)
{
    return ::parseProperty<Q3DSNode::Orientation>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSNode::Orientation *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs,Q3DSGraphObject:: PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSSlide::PlayMode *dst)
{
    return ::parseProperty<Q3DSSlide::PlayMode>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSSlide::PlayMode *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSSlide::InitialPlayState *dst)
{
    return ::parseProperty<Q3DSSlide::InitialPlayState>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSSlide::InitialPlayState *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSLayerNode::ProgressiveAA *dst)
{
    return ::parseProperty<Q3DSLayerNode::ProgressiveAA>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSLayerNode::ProgressiveAA *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSLayerNode::MultisampleAA *dst)
{
    return ::parseProperty<Q3DSLayerNode::MultisampleAA>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSLayerNode::MultisampleAA *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSLayerNode::LayerBackground *dst)
{
    return ::parseProperty<Q3DSLayerNode::LayerBackground>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSLayerNode::LayerBackground *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSLayerNode::BlendType *dst)
{
    return ::parseProperty<Q3DSLayerNode::BlendType>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSLayerNode::BlendType *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSLayerNode::HorizontalFields *dst)
{
    return ::parseProperty<Q3DSLayerNode::HorizontalFields>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSLayerNode::HorizontalFields *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSLayerNode::Units *dst)
{
    return ::parseProperty<Q3DSLayerNode::Units>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSLayerNode::Units *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSLayerNode::VerticalFields *dst)
{
    return ::parseProperty<Q3DSLayerNode::VerticalFields>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSLayerNode::VerticalFields *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSImage::MappingMode *dst)
{
    return ::parseProperty<Q3DSImage::MappingMode>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSImage::MappingMode *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSImage::TilingMode *dst)
{
    return ::parseProperty<Q3DSImage::TilingMode>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSImage::TilingMode *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSModelNode::Tessellation *dst)
{
    return ::parseProperty<Q3DSModelNode::Tessellation>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSModelNode::Tessellation *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSCameraNode::ScaleMode *dst)
{
    return ::parseProperty<Q3DSCameraNode::ScaleMode>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSCameraNode::ScaleMode *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSCameraNode::ScaleAnchor *dst)
{
    return ::parseProperty<Q3DSCameraNode::ScaleAnchor>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSCameraNode::ScaleAnchor *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSLightNode::LightType *dst)
{
    return ::parseProperty<Q3DSLightNode::LightType>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSLightNode::LightType *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSDefaultMaterial::ShaderLighting *dst)
{
    return ::parseProperty<Q3DSDefaultMaterial::ShaderLighting>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSDefaultMaterial::ShaderLighting *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSDefaultMaterial::BlendMode *dst)
{
    return ::parseProperty<Q3DSDefaultMaterial::BlendMode>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSDefaultMaterial::BlendMode *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSDefaultMaterial::SpecularModel *dst)
{
    return ::parseProperty<Q3DSDefaultMaterial::SpecularModel>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSDefaultMaterial::SpecularModel *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSTextNode::HorizontalAlignment *dst)
{
    return ::parseProperty<Q3DSTextNode::HorizontalAlignment>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSTextNode::HorizontalAlignment *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

template<typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags, const QString &typeName, const QString &propName, Q3DSTextNode::VerticalAlignment *dst)
{
    return ::parseProperty<Q3DSTextNode::VerticalAlignment>(attrs, flags, typeName, propName, Q3DS::Enum, dst, [](const QStringRef &s, Q3DSTextNode::VerticalAlignment *v) { return Q3DSEnumMap::enumFromStr(s, v); });
}

// Resolving of object references should be deferred. setProperties() is
// expected to just store the string, and only look up the object in
// resolveReferences() using this helper.
template<typename T>
void resolveRef(const QString &val, Q3DSGraphObject::Type type, T **obj, const Q3DSPresentation &pres)
{
    bool b = false;
    if (val.startsWith('#')) {
        Q3DSGraphObject *o = pres.object(val.mid(1).toUtf8());
        if (o) {
            if (type == Q3DSGraphObject::AnyObject || o->type() == type) {
                *obj = static_cast<T *>(o);
                b = true;
            }
        }
    }
    if (!b && !val.isEmpty())
        qWarning("Failed to resolve object reference %s", qPrintable(val));
}

template<typename V>
void Q3DSGraphObject::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Asset");
    parseProperty(attrs, flags, typeName, QStringLiteral("starttime"), &m_startTime);
    parseProperty(attrs, flags, typeName, QStringLiteral("endtime"), &m_endTime);
}

void Q3DSGraphObject::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    setProps(attrs, flags);
}

void Q3DSGraphObject::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    setProps(*changeList, 0);
}

Q3DSScene::Q3DSScene()
    : Q3DSGraphObject(Q3DSGraphObject::Scene)
{
}

void Q3DSScene::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    // Asset properties (starttime, endtime) are not in use, hence no base call.

    const QString typeName = QStringLiteral("Scene");
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseProperty(attrs, flags, typeName, QStringLiteral("bgcolorenable"), &m_useClearColor);
    parseProperty(attrs, flags, typeName, QStringLiteral("backgroundcolor"), &m_clearColor);
}

QStringList Q3DSScene::gex_propertyNames() const
{
    QStringList s = Q3DSGraphObject::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("useClearColor") << QLatin1String("clearColor");
    return s;
}

QVariantList Q3DSScene::gex_propertyValues() const
{
    QVariantList s = Q3DSGraphObject::gex_propertyValues();
    s << m_name << m_useClearColor << m_clearColor;
    return s;
}

void Q3DSPropertyChangeList::append(const Q3DSPropertyChange &change)
{
    m_changes.append(change);
    m_keys.insert(change.nameStr());

    if (change.nameStr() == QStringLiteral("position")
            || change.nameStr() == QStringLiteral("rotation")
            || change.nameStr() == QStringLiteral("scale"))
    {
        m_flags |= NodeTransformChanges;
    } else if (change.nameStr() == QStringLiteral("opacity")) {
        m_flags |= NodeOpacityChanges;
    } else if (change.nameStr() == QStringLiteral("eyeball")) {
        m_flags |= EyeballChanges;
    } else if (change.nameStr() == QStringLiteral("textstring")
               || change.nameStr() == QStringLiteral("leading")
               || change.nameStr() == QStringLiteral("tracking"))
    {
        m_flags |= TextTextureImageDepChanges;
    } else if (change.nameStr().startsWith(QStringLiteral("ao"))
               || change.nameStr().startsWith(QStringLiteral("shadow")))
    {
        m_flags |= AoOrShadowChanges;
    } else if (change.nameStr() == QStringLiteral("blendmode"))
    {
        m_flags |= BlendModeChanges;
    }
}

Q3DSSlide::Q3DSSlide()
    : Q3DSGraphObject(Q3DSGraphObject::Slide)
{
}

Q3DSSlide::~Q3DSSlide()
{
    qDeleteAll(m_propChanges);
}

void Q3DSSlide::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);

    const QString typeName = QStringLiteral("Slide");

    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseProperty(attrs, flags, typeName, QStringLiteral("playmode"), &m_playMode);
    parseProperty(attrs, flags, typeName, QStringLiteral("initialplaystate"), &m_initialPlayState);

    StringOrInt pt;
    if (parseProperty(attrs, flags, typeName, QStringLiteral("playthroughto"), &pt)) {
        if (pt.isInt) {
            m_playThroughHasExplicitValue = true;
            m_playThroughValue = pt.n;
        } else {
            m_playThroughHasExplicitValue = false;
            Q3DSEnumMap::enumFromStr(QStringRef(&pt.s), &m_playThrough);
        }
    }
}

void Q3DSSlide::addObject(Q3DSGraphObject *obj)
{
    m_objects.insert(obj);
}

QStringList Q3DSSlide::gex_propertyNames() const
{
    QStringList s = Q3DSGraphObject::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("playMode") << QLatin1String("initialPlayState");
    return s;
}

QVariantList Q3DSSlide::gex_propertyValues() const
{
    QVariantList s = Q3DSGraphObject::gex_propertyValues();
    s << m_name << m_playMode << m_initialPlayState;
    return s;
}

Q3DSImage::Q3DSImage()
    : Q3DSGraphObject(Image)
{
}

template<typename V>
void Q3DSImage::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Image");

    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseProperty(attrs, flags, typeName, QStringLiteral("sourcepath"), &m_sourcePath);
    parseProperty(attrs, flags, typeName, QStringLiteral("scaleu"), &m_scaleU);
    parseProperty(attrs, flags, typeName, QStringLiteral("scalev"), &m_scaleV);
    parseProperty(attrs, flags, typeName, QStringLiteral("mappingmode"), &m_mappingMode);
    parseProperty(attrs, flags, typeName, QStringLiteral("tilingmodehorz"), &m_tilingHoriz);
    parseProperty(attrs, flags, typeName, QStringLiteral("tilingmodevert"), &m_tilingVert);
    parseProperty(attrs, flags, typeName, QStringLiteral("rotationuv"), &m_rotationUV);
    parseProperty(attrs, flags, typeName, QStringLiteral("positionu"), &m_positionU);
    parseProperty(attrs, flags, typeName, QStringLiteral("positionv"), &m_positionV);
    parseProperty(attrs, flags, typeName, QStringLiteral("pivotu"), &m_pivotU);
    parseProperty(attrs, flags, typeName, QStringLiteral("pivotv"), &m_pivotV);
    parseProperty(attrs, flags, typeName, QStringLiteral("subpresentation"), &m_subPresentation);

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("endtime"), &m_endTime);
}

void Q3DSImage::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);
    calculateTextureTransform();
}

void Q3DSImage::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
    calculateTextureTransform();
}

void Q3DSImage::resolveReferences(Q3DSPresentation &presentation, Q3DSUipParser &parser)
{
    if (!m_sourcePath.isEmpty()) {
        // We'll use this chance to check if the image contains has an alpha channel.
        // Note: This is set in the UIP file, so we're not actually loading or checking
        // the actual image file here.
        const auto &it = presentation.imageBuffer().constFind(m_sourcePath);
        if (it != presentation.imageBuffer().constEnd()) {
            m_hasTransparency = *it;
            m_scannedForTransparency = true;
        }
        m_sourcePath = parser.assetFileName(m_sourcePath, nullptr);
    }
}

QStringList Q3DSImage::gex_propertyNames() const
{
    QStringList s = Q3DSGraphObject::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("sourcePath") << QLatin1String("scaleU") << QLatin1String("scaleV")
      << QLatin1String("mappingMode") << QLatin1String("tilingHoriz") << QLatin1String("tilingVert")
      << QLatin1String("rotationUV") << QLatin1String("positionU") << QLatin1String("positionV")
      << QLatin1String("pivotU") << QLatin1String("pivotV") << QLatin1String("subPresentation");
    return s;
}

QVariantList Q3DSImage::gex_propertyValues() const
{
    QVariantList s = Q3DSGraphObject::gex_propertyValues();
    s << m_name << m_sourcePath << m_scaleU << m_scaleV << m_mappingMode << m_tilingHoriz << m_tilingVert
      << m_rotationUV << m_positionU << m_positionV << m_pivotU << m_pivotV << m_subPresentation;
    return s;
}

namespace  {
#if 0
    bool scanImageForAlpha(const uchar *data, int width, int height, unsigned pixelSizeInBytes, unsigned alphaSizeInBits, bool isAlphaFirst = false)
    {
        const quint8 *rowPtr = reinterpret_cast<const quint8 *>(data);
        bool hasAlpha = false;
        if (alphaSizeInBits == 0)
            return hasAlpha;
        if (pixelSizeInBytes != 2 && pixelSizeInBytes != 4) {
            Q_UNREACHABLE();
            return false;
        }
        if (alphaSizeInBits > 8) {
            Q_UNREACHABLE();
            return false;
        }

        unsigned alphaShift = pixelSizeInBytes * 8 - alphaSizeInBits;
        unsigned maxAlphaValue = (1 << alphaSizeInBits) - 1;

        for (int rowIdx = 0; rowIdx < height && hasAlpha == false; ++rowIdx) {
            for (int idx = 0; idx < width && hasAlpha == false; ++idx, rowPtr += pixelSizeInBytes) {
                unsigned pixelValue = 0;
                if (pixelSizeInBytes == 2)
                    pixelValue = *(reinterpret_cast<const quint16 *>(rowPtr));
                else
                    pixelValue = *(reinterpret_cast<const quint32 *>(rowPtr));
                if (!isAlphaFirst)
                    pixelValue = pixelValue << alphaShift; // ### this logic is wrong
                else
                    pixelValue = pixelValue >> alphaShift;

                if (pixelValue < maxAlphaValue)
                    hasAlpha = true;
            }
        }
        return hasAlpha;
    }
#endif

    bool scanForTransparency(const QString &imageSource)
    {
        if (imageSource.isEmpty()) // empty (e.g. because subpresentation) -> assume transparency
            return true;
        QImage textureImage(imageSource);
        // If the texture fails to load with QImage, assume it has transparency
        if (textureImage.isNull())
            return true;
        // Formats without alpha channels dont support transparency
        if (!textureImage.hasAlphaChannel())
            return false;

        if (textureImage.width() > 1024 || textureImage.height() > 1024)
            qCDebug(lcUip, "Perf. hint: Scanning image of size %dx%d manually for transparency. "
                    "Prefer setting hasTransparency in the ImageBuffer element instead.",
                    textureImage.width(), textureImage.height());

        bool hasTransparency = false;
        for (int y = 0; y < textureImage.height(); ++y) {
            for (int x = 0; x < textureImage.width(); ++x) {
                // ### This might be expensive, so it might be worth checking
                // the alpha channels via constBits instead (based on format).
                if (textureImage.pixelColor(x, y).alpha() < 255)
                    return true;
            }
        }

#if 0
        switch (textureImage.format()) {
        case QImage::Format_ARGB32:
            hasTransparency = scanImageForAlpha(textureImage.constBits(), textureImage.width(), textureImage.height(), 4, 8, true);
            break;
        case QImage::Format_ARGB32_Premultiplied:
            hasTransparency = scanImageForAlpha(textureImage.constBits(), textureImage.width(), textureImage.height(), 4, 8, true);
            break;
        case QImage::Format_ARGB8565_Premultiplied:
            hasTransparency = scanImageForAlpha(textureImage.constBits(), textureImage.width(), textureImage.height(), 3, 8, true);
            break;
        case QImage::Format_ARGB6666_Premultiplied:
            hasTransparency = scanImageForAlpha(textureImage.constBits(), textureImage.width(), textureImage.height(), 3, 6, true);
            break;
        case QImage::Format_ARGB8555_Premultiplied:
            hasTransparency = scanImageForAlpha(textureImage.constBits(), textureImage.width(), textureImage.height(), 3, 8, true);
            break;
        case QImage::Format_ARGB4444_Premultiplied:
            hasTransparency = scanImageForAlpha(textureImage.constBits(), textureImage.width(), textureImage.height(), 2, 4, true);
            break;
        case QImage::Format_RGBA8888:
            hasTransparency = scanImageForAlpha(textureImage.constBits(), textureImage.width(), textureImage.height(), 4, 8, false);
            break;
        case QImage::Format_RGBA8888_Premultiplied:
            hasTransparency = scanImageForAlpha(textureImage.constBits(), textureImage.width(), textureImage.height(), 4, 8, false);
            break;
        case QImage::Format_A2BGR30_Premultiplied:
            hasTransparency = scanImageForAlpha(textureImage.constBits(), textureImage.width(), textureImage.height(), 4, 2, true);
            break;
        case QImage::Format_A2RGB30_Premultiplied:
            hasTransparency = scanImageForAlpha(textureImage.constBits(), textureImage.width(), textureImage.height(), 4, 2, true);
            break;
        case QImage::Format_Alpha8:
            hasTransparency = scanImageForAlpha(textureImage.constBits(), textureImage.width(), textureImage.height(), 1, 8, true);
            break;
        default:
            break;
        }
#endif
        return hasTransparency;
    }
}

bool Q3DSImage::hasTransparency()
{
    if (m_scannedForTransparency)
        return m_hasTransparency;

    // Do a pre-load of the texture to check for transparency
    m_hasTransparency = scanForTransparency(sourcePath());
    m_scannedForTransparency = true;
    return m_hasTransparency;
}

void Q3DSImage::calculateTextureTransform()
{
    m_textureTransform.setToIdentity();
    QMatrix4x4 translation;
    QMatrix4x4 rotation;
    QMatrix4x4 scale;
    // NB! operator() is row-major, data()/constData() are column-major
    translation(0, 3) = m_positionU;
    translation(1, 3) = m_positionV;
    scale(0, 0) = m_scaleU;
    scale(1, 1) = m_scaleV;
    rotation.rotate(m_rotationUV, QVector3D(0, 0, 1));
    m_textureTransform(0, 3) = m_pivotU;
    m_textureTransform(1, 3) = m_pivotV;
    m_textureTransform *= rotation;
    m_textureTransform *= scale;
    m_textureTransform *= translation;
}

Q3DSDefaultMaterial::Q3DSDefaultMaterial()
    : Q3DSGraphObject(Q3DSGraphObject::DefaultMaterial)
{
}

template<typename V>
void Q3DSDefaultMaterial::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Material");
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);

    parseProperty(attrs, flags, typeName, QStringLiteral("shaderlighting"), &m_shaderLighting);
    parseProperty(attrs, flags, typeName, QStringLiteral("blendmode"), &m_blendMode);
    parseProperty(attrs, flags, typeName, QStringLiteral("diffuse"), &m_diffuse);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("diffusemap"), &m_diffuseMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("diffusemap2"), &m_diffuseMap2_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("diffusemap3"), &m_diffuseMap3_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("specularreflection"), &m_specularReflection_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("speculartint"), &m_specularTint);
    parseProperty(attrs, flags, typeName, QStringLiteral("specularamount"), &m_specularAmount);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("specularmap"), &m_specularMap_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("specularmodel"), &m_specularModel);
    parseProperty(attrs, flags, typeName, QStringLiteral("specularroughness"), &m_specularRoughness);
    parseProperty(attrs, flags, typeName, QStringLiteral("fresnelPower"), &m_fresnelPower);
    parseProperty(attrs, flags, typeName, QStringLiteral("ior"), &m_ior);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("bumpmap"), &m_bumpMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("normalmap"), &m_normalMap_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("bumpamount"), &m_bumpAmount);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("displacementmap"), &m_displacementMap_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("displaceamount"), &m_displaceAmount);
    parseOpacityProperty(attrs, flags, typeName, QStringLiteral("opacity"), &m_opacity);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("opacitymap"), &m_opacityMap_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("emissivecolor"), &m_emissiveColor);
    parseProperty(attrs, flags, typeName, QStringLiteral("emissivepower"), &m_emissivePower);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("emissivemap"), &m_emissiveMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("emissivemap2"), &m_emissiveMap2_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("translucencymap"), &m_translucencyMap_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("translucentfalloff"), &m_translucentFalloff);
    parseProperty(attrs, flags, typeName, QStringLiteral("diffuselightwrap"), &m_diffuseLightWrap);
}

void Q3DSDefaultMaterial::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSDefaultMaterial::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

void Q3DSDefaultMaterial::resolveReferences(Q3DSPresentation &pres, Q3DSUipParser &)
{
    resolveRef(m_diffuseMap_unresolved, Q3DSGraphObject::Image, &m_diffuseMap, pres);
    resolveRef(m_diffuseMap2_unresolved, Q3DSGraphObject::Image, &m_diffuseMap2, pres);
    resolveRef(m_diffuseMap3_unresolved, Q3DSGraphObject::Image, &m_diffuseMap3, pres);
    resolveRef(m_specularReflection_unresolved, Q3DSGraphObject::Image, &m_specularReflection, pres);
    resolveRef(m_specularMap_unresolved, Q3DSGraphObject::Image, &m_specularMap, pres);
    resolveRef(m_bumpMap_unresolved, Q3DSGraphObject::Image, &m_bumpMap, pres);
    resolveRef(m_normalMap_unresolved, Q3DSGraphObject::Image, &m_normalMap, pres);
    resolveRef(m_displacementMap_unresolved, Q3DSGraphObject::Image, &m_displacementMap, pres);
    resolveRef(m_opacityMap_unresolved, Q3DSGraphObject::Image, &m_opacityMap, pres);
    resolveRef(m_emissiveMap_unresolved, Q3DSGraphObject::Image, &m_emissiveMap, pres);
    resolveRef(m_emissiveMap2_unresolved, Q3DSGraphObject::Image, &m_emissiveMap2, pres);
    resolveRef(m_translucencyMap_unresolved, Q3DSGraphObject::Image, &m_translucencyMap, pres);
}

QStringList Q3DSDefaultMaterial::gex_propertyNames() const
{
    QStringList s = Q3DSGraphObject::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("shaderLighting") << QLatin1String("blendMode") << QLatin1String("diffuse")
      << QLatin1String("diffuseMap") << QLatin1String("diffuseMap2") << QLatin1String("diffuseMap3")
      << QLatin1String("specularReflection") << QLatin1String("specularTint") << QLatin1String("specularAmount")
      << QLatin1String("specularMap") << QLatin1String("specularModel")
      << QLatin1String("specularRoughness") << QLatin1String("fresnelPower") << QLatin1String("ior") << QLatin1String("bumpMap")
      << QLatin1String("normalMap") << QLatin1String("bumpAmount") << QLatin1String("displacementMap")
      << QLatin1String("displaceAmount") << QLatin1String("opacity") << QLatin1String("opacityMap") << QLatin1String("emissiveColor")
      << QLatin1String("emissivePower") << QLatin1String("emissiveMap") << QLatin1String("emissiveMap2")
      << QLatin1String("translucencyMap") << QLatin1String("translucentFalloff") << QLatin1String("diffuseLightWrap");
    return s;
}

QVariantList Q3DSDefaultMaterial::gex_propertyValues() const
{
    QVariantList s = Q3DSGraphObject::gex_propertyValues();
    s << m_name << m_shaderLighting << m_blendMode << m_diffuse << m_diffuseMap_unresolved << m_diffuseMap2_unresolved << m_diffuseMap3_unresolved
      << m_specularReflection_unresolved << m_specularTint << m_specularAmount << m_specularMap_unresolved << m_specularModel
      << m_specularRoughness << m_fresnelPower << m_ior << m_bumpMap_unresolved << m_normalMap_unresolved << m_bumpAmount << m_displacementMap_unresolved
      << m_displaceAmount << m_opacity << m_opacityMap_unresolved << m_emissiveColor << m_emissivePower << m_emissiveMap_unresolved << m_emissiveMap2_unresolved
      << m_translucencyMap_unresolved << m_translucentFalloff << m_diffuseLightWrap;
    return s;
}

Q3DSReferencedMaterial::Q3DSReferencedMaterial()
    : Q3DSGraphObject(Q3DSGraphObject::ReferencedMaterial)
{
}

template<typename V>
void Q3DSReferencedMaterial::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("ReferencedMaterial");
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseObjectRefProperty(attrs, flags, typeName, QStringLiteral("referencedmaterial"), &m_referencedMaterial_unresolved);
}

void Q3DSReferencedMaterial::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSReferencedMaterial::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

void Q3DSReferencedMaterial::resolveReferences(Q3DSPresentation &pres, Q3DSUipParser &)
{
    // can be DefaultMaterial or CustomMaterial so stick with a generic object
    resolveRef(m_referencedMaterial_unresolved, Q3DSGraphObject::AnyObject, &m_referencedMaterial, pres);
}

QStringList Q3DSReferencedMaterial::gex_propertyNames() const
{
    QStringList s = Q3DSGraphObject::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("referencedmaterial");
    return s;
}

QVariantList Q3DSReferencedMaterial::gex_propertyValues() const
{
    QVariantList s = Q3DSGraphObject::gex_propertyValues();
    s << m_name << m_referencedMaterial_unresolved;
    return s;
}

Q3DSCustomMaterialInstance::Q3DSCustomMaterialInstance()
    : Q3DSGraphObject(Q3DSGraphObject::CustomMaterial)
{
}

template<typename V>
void Q3DSCustomMaterialInstance::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("CustomMaterial");
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseProperty(attrs, flags, typeName, QStringLiteral("class"), &m_material_unresolved);
}

void Q3DSCustomMaterialInstance::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);

    // Save attributes for the 2nd pass (resolveReferences) since they may
    // refer to custom properties defined in the custom material.
    for (const QXmlStreamAttribute &attr : attrs)
        m_attrs.append(Q3DSPropertyChange(attr.name().toString(), attr.value().toString()));
}

void Q3DSCustomMaterialInstance::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

static void fillCustomProperties(const QMap<QString, Q3DSMaterial::PropertyElement> &propMeta,
                                 QVariantMap *propTab,
                                 const Q3DSPropertyChangeList &instanceProps,
                                 const Q3DSUipParser &parser)
{
    // Take all properties from the metadata and fill them all into the
    // instance-specific table either with the default value or the
    // presentation-provided, instance-specific one.
    for (auto propMetaIt = propMeta.cbegin(), propMetaIte = propMeta.cend(); propMetaIt != propMetaIte; ++propMetaIt) {
        bool found = false;
        for (auto it = instanceProps.cbegin(), ite = instanceProps.cend(); it != ite; ++it) {
            if (it->nameStr() == propMetaIt.key()) {
                found = true;
                propTab->insert(it->nameStr(), Q3DS::convertToVariant(it->valueStr(), *propMetaIt));
                break;
            }
        }
        if (!found)
            propTab->insert(propMetaIt.key(), Q3DS::convertToVariant(propMetaIt->defaultValue, *propMetaIt));
    }

    // Fix up the filenames to that no further adjustment is necessary from this point on.
    for (auto it = propTab->begin(), ite = propTab->end(); it != ite; ++it) {
        Q3DS::PropertyType t = propMeta[it.key()].type;
        if (t == Q3DS::Texture) {
            const QString fn = it->toString();
            if (!fn.isEmpty())
                *it = parser.assetFileName(fn, nullptr);
        }
    }
}

void Q3DSCustomMaterialInstance::resolveReferences(Q3DSPresentation &pres, Q3DSUipParser &parser)
{
    if (m_material_unresolved.startsWith('#')) {
        m_material = pres.customMaterial(m_material_unresolved.mid(1).toUtf8());
        fillCustomProperties(m_material.properties(), &m_materialPropertyVals, m_attrs, parser);
    }
}

QStringList Q3DSCustomMaterialInstance::gex_propertyNames() const
{
    QStringList s = Q3DSGraphObject::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("class");
    return s;
}

QVariantList Q3DSCustomMaterialInstance::gex_propertyValues() const
{
    QVariantList s = Q3DSGraphObject::gex_propertyValues();
    s << m_name << m_material_unresolved;
    return s;
}

Q3DSEffectInstance::Q3DSEffectInstance()
    : Q3DSGraphObject(Q3DSGraphObject::Effect)
{
}

template<typename V>
void Q3DSEffectInstance::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Effect");
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseProperty(attrs, flags, typeName, QStringLiteral("class"), &m_effect_unresolved);
}

void Q3DSEffectInstance::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);

    // Save attributes for the 2nd pass (resolveReferences) since they may
    // refer to custom properties defined in the effect.
    for (const QXmlStreamAttribute &attr : attrs)
        m_attrs.append(Q3DSPropertyChange(attr.name().toString(), attr.value().toString()));
}

void Q3DSEffectInstance::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

void Q3DSEffectInstance::resolveReferences(Q3DSPresentation &pres, Q3DSUipParser &parser)
{
    if (m_effect_unresolved.startsWith('#')) {
        m_effect = pres.effect(m_effect_unresolved.mid(1).toUtf8());
        if (!m_effect.isNull())
            fillCustomProperties(m_effect.properties(), &m_effectPropertyVals, m_attrs, parser);
    }
}

QStringList Q3DSEffectInstance::gex_propertyNames() const
{
    QStringList s = Q3DSGraphObject::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("class");
    return s;
}

QVariantList Q3DSEffectInstance::gex_propertyValues() const
{
    QVariantList s = Q3DSGraphObject::gex_propertyValues();
    s << m_name << m_effect_unresolved;
    return s;
}

Q3DSNode::Q3DSNode(Type type)
    : Q3DSGraphObject(type)
{
}

template<typename V>
void Q3DSNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Node");

    bool b;
    if (parseProperty(attrs, flags, typeName, QStringLiteral("eyeball"), &b))
        m_flags.setFlag(Active, b);
    if (parseProperty(attrs, flags, typeName, QStringLiteral("ignoresparent"), &b))
        m_flags.setFlag(IgnoresParentTransform, b);

    parseRotationProperty(attrs, flags, typeName, QStringLiteral("rotation"), &m_rotation);
    parseProperty(attrs, flags, typeName, QStringLiteral("position"), &m_position);
    parseProperty(attrs, flags, typeName, QStringLiteral("scale"), &m_scale);
    parseProperty(attrs, flags, typeName, QStringLiteral("pivot"), &m_pivot);
    parseOpacityProperty(attrs, flags, typeName, QStringLiteral("opacity"), &m_localOpacity);
    parseProperty(attrs, flags, typeName, QStringLiteral("boneid"), &m_skeletonId);
    parseProperty(attrs, flags, typeName, QStringLiteral("rotationorder"), &m_rotationOrder);
    parseProperty(attrs, flags, typeName, QStringLiteral("orientation"), &m_orientation);
}

void Q3DSNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);

    // If this is on the master slide, store some rollback info.
    if (flags.testFlag(PropSetOnMaster)) {
        if (m_masterRollbackList.isNull())
            m_masterRollbackList.reset(new Q3DSPropertyChangeList);
        m_masterRollbackList->append(Q3DSPropertyChange(QLatin1String("eyeball"),
                                     m_flags.testFlag(Q3DSNode::Active)
                                     ? QLatin1String("True") : QLatin1String("False")));

    }
}

void Q3DSNode::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

QStringList Q3DSNode::gex_propertyNames() const
{
    QStringList s = Q3DSGraphObject::gex_propertyNames();
    s << QLatin1String("flags") << QLatin1String("rotation") << QLatin1String("position") << QLatin1String("scale")
      << QLatin1String("pivot") << QLatin1String("opacity")
      << QLatin1String("skeletonId") << QLatin1String("rotationOrder") << QLatin1String("orientation");
    return s;
}

QVariantList Q3DSNode::gex_propertyValues() const
{
    QVariantList s = Q3DSGraphObject::gex_propertyValues();
    s << int(m_flags) << m_rotation << m_position << m_scale << m_pivot << m_localOpacity
      << m_skeletonId << m_rotationOrder << m_orientation;
    return s;
}

Q3DSLayerNode::Q3DSLayerNode()
    : Q3DSNode(Q3DSGraphObject::Layer)
{
}

template<typename V>
void Q3DSLayerNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Layer");

    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);

    bool b;
    if (parseProperty(attrs, flags, typeName, QStringLiteral("disabledepthtest"), &b))
        m_layerFlags.setFlag(DisableDepthTest, b);
    if (parseProperty(attrs, flags, typeName, QStringLiteral("disabledepthprepass"), &b))
        m_layerFlags.setFlag(DisableDepthPrePass, b);

    parseProperty(attrs, flags, typeName, QStringLiteral("progressiveaa"), &m_progressiveAA);
    parseProperty(attrs, flags, typeName, QStringLiteral("multisampleaa"), &m_multisampleAA);

    if (parseProperty(attrs, flags, typeName, QStringLiteral("temporalaa"), &b))
        m_layerFlags.setFlag(TemporalAA, b);

    parseProperty(attrs, flags, typeName, QStringLiteral("background"), &m_layerBackground);
    parseProperty(attrs, flags, typeName, QStringLiteral("backgroundcolor"), &m_backgroundColor);
    parseProperty(attrs, flags, typeName, QStringLiteral("blendtype"), &m_blendType);

    parseProperty(attrs, flags, typeName, QStringLiteral("horzfields"), &m_horizontalFields);
    parseProperty(attrs, flags, typeName, QStringLiteral("left"), &m_left);
    parseProperty(attrs, flags, typeName, QStringLiteral("leftunits"), &m_leftUnits);
    parseProperty(attrs, flags, typeName, QStringLiteral("width"), &m_width);
    parseProperty(attrs, flags, typeName, QStringLiteral("widthunits"), &m_widthUnits);
    parseProperty(attrs, flags, typeName, QStringLiteral("right"), &m_right);
    parseProperty(attrs, flags, typeName, QStringLiteral("rightunits"), &m_rightUnits);
    parseProperty(attrs, flags, typeName, QStringLiteral("vertfields"), &m_verticalFields);
    parseProperty(attrs, flags, typeName, QStringLiteral("top"), &m_top);
    parseProperty(attrs, flags, typeName, QStringLiteral("topunits"), &m_topUnits);
    parseProperty(attrs, flags, typeName, QStringLiteral("height"), &m_height);
    parseProperty(attrs, flags, typeName, QStringLiteral("heightunits"), &m_heightUnits);
    parseProperty(attrs, flags, typeName, QStringLiteral("bottom"), &m_bottom);
    parseProperty(attrs, flags, typeName, QStringLiteral("bottomunits"), &m_bottomUnits);

    parseProperty(attrs, flags, typeName, QStringLiteral("sourcepath"), &m_sourcePath);

    // SSAO
    parseProperty(attrs, flags, typeName, QStringLiteral("aostrength"), &m_aoStrength);
    parseProperty(attrs, flags, typeName, QStringLiteral("aodistance"), &m_aoDistance);
    parseProperty(attrs, flags, typeName, QStringLiteral("aosoftness"), &m_aoSoftness);
    parseProperty(attrs, flags, typeName, QStringLiteral("aobias"), &m_aoBias);
    parseProperty(attrs, flags, typeName, QStringLiteral("aosamplerate"), &m_aoSampleRate);
    parseProperty(attrs, flags, typeName, QStringLiteral("aodither"), &m_aoDither);

    // SSDO (these are always hidden in the application, it seems, and so SSDO cannot be enabled in practice)
    parseProperty(attrs, flags, typeName, QStringLiteral("shadowstrength"), &m_shadowStrength);
    parseProperty(attrs, flags, typeName, QStringLiteral("shadowdist"), &m_shadowDist);
    parseProperty(attrs, flags, typeName, QStringLiteral("shadowsoftness"), &m_shadowSoftness);
    parseProperty(attrs, flags, typeName, QStringLiteral("shadowbias"), &m_shadowBias);

    // IBL
    parseImageProperty(attrs, flags, typeName, QStringLiteral("lightprobe"), &m_lightProbe_unresolved);
    parseProperty(attrs, flags, typeName, QStringLiteral("probebright"), &m_probeBright);
    if (parseProperty(attrs, flags, typeName, QStringLiteral("fastibl"), &b))
        m_layerFlags.setFlag(FastIBL, b);
    parseProperty(attrs, flags, typeName, QStringLiteral("probehorizon"), &m_probeHorizon);
    parseProperty(attrs, flags, typeName, QStringLiteral("probefov"), &m_probeFov);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("lightprobe2"), &m_lightProbe2_unresolved);
    parseProperty(attrs, flags, typeName, QStringLiteral("probe2fade"), &m_probe2Fade);
    parseProperty(attrs, flags, typeName, QStringLiteral("probe2window"), &m_probe2Window);
    parseProperty(attrs, flags, typeName, QStringLiteral("probe2pos"), &m_probe2Pos);
}

void Q3DSLayerNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSLayerNode::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

void Q3DSLayerNode::resolveReferences(Q3DSPresentation &pres, Q3DSUipParser &parser)
{
    Q3DSNode::resolveReferences(pres, parser);
    resolveRef(m_lightProbe_unresolved, Q3DSGraphObject::Image, &m_lightProbe, pres);
    resolveRef(m_lightProbe2_unresolved, Q3DSGraphObject::Image, &m_lightProbe2, pres);
}

QStringList Q3DSLayerNode::gex_propertyNames() const
{
    QStringList s = Q3DSNode::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("layerFlags") << QLatin1String("progressiveAA") << QLatin1String("multisampleAA")
      << QLatin1String("layerBackground") << QLatin1String("backgroundColor")
      << QLatin1String("blendType") << QLatin1String("horizontalFields") << QLatin1String("left") << QLatin1String("leftUnits")
      << QLatin1String("width") << QLatin1String("widthUnits")
      << QLatin1String("right") << QLatin1String("rightUnits") << QLatin1String("verticalFields")
      << QLatin1String("top") << QLatin1String("topUnits") << QLatin1String("height") << QLatin1String("heightUnits")
      << QLatin1String("bottom") << QLatin1String("bottomUnits") << QLatin1String("sourcePath")
      << QLatin1String("shadowStrength") << QLatin1String("shadowDist") << QLatin1String("shadowSoftness")
      << QLatin1String("shadowBias") << QLatin1String("lightProbe") << QLatin1String("probeBright") << QLatin1String("probeHorizon")
      << QLatin1String("proveFov") << QLatin1String("lightProbe2") << QLatin1String("probe2Fade") << QLatin1String("probe2Window") << QLatin1String("probe2Pos");
    return s;
}

QVariantList Q3DSLayerNode::gex_propertyValues() const
{
    QVariantList s = Q3DSNode::gex_propertyValues();
    s << m_name << int(m_layerFlags) << m_progressiveAA << m_multisampleAA << m_layerBackground << m_backgroundColor
      << m_blendType << m_horizontalFields << m_left << m_leftUnits << m_width << m_widthUnits
      << m_right << m_rightUnits << m_verticalFields << m_top << m_topUnits << m_height << m_heightUnits
      << m_bottom << m_bottomUnits << m_sourcePath << m_shadowStrength << m_shadowDist << m_shadowSoftness
      << m_shadowBias << m_lightProbe_unresolved << m_probeBright << m_probeHorizon << m_probeFov << m_lightProbe2_unresolved
      << m_probe2Fade << m_probe2Window << m_probe2Pos;
    return s;
}

Q3DSCameraNode::Q3DSCameraNode()
    : Q3DSNode(Q3DSGraphObject::Camera)
{
}

template<typename V>
void Q3DSCameraNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Camera");

    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseProperty(attrs, flags, typeName, QStringLiteral("orthographic"), &m_orthographic);
    parseProperty(attrs, flags, typeName, QStringLiteral("fov"), &m_fov);
    parseProperty(attrs, flags, typeName, QStringLiteral("clipnear"), &m_clipNear);
    parseProperty(attrs, flags, typeName, QStringLiteral("clipfar"), &m_clipFar);
    parseProperty(attrs, flags, typeName, QStringLiteral("scalemode"), &m_scaleMode);
    parseProperty(attrs, flags, typeName, QStringLiteral("scaleanchor"), &m_scaleAnchor);

    // Re-parse position since the default value in the metadata for this respecified property is different.
    parseProperty(attrs, flags, typeName, QStringLiteral("position"), &m_position);
}

void Q3DSCameraNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSCameraNode::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

QStringList Q3DSCameraNode::gex_propertyNames() const
{
    QStringList s = Q3DSNode::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("orthographic") << QLatin1String("fov")
      << QLatin1String("clipNear") << QLatin1String("clipFar") << QLatin1String("scaleMode") << QLatin1String("scaleAnchor");
    return s;
}

QVariantList Q3DSCameraNode::gex_propertyValues() const
{
    QVariantList s = Q3DSNode::gex_propertyValues();
    s << m_name << m_orthographic << m_fov << m_clipNear << m_clipFar << m_scaleMode << m_scaleAnchor;
    return s;
}

Q3DSLightNode::Q3DSLightNode()
    : Q3DSNode(Q3DSGraphObject::Light)
{
}

template<typename V>
void Q3DSLightNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Light");

    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseObjectRefProperty(attrs, flags, typeName, QStringLiteral("scope"), &m_scope_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("lighttype"), &m_lightType);
    parseProperty(attrs, flags, typeName, QStringLiteral("lightdiffuse"), &m_lightDiffuse);
    parseProperty(attrs, flags, typeName, QStringLiteral("lightspecular"), &m_lightSpecular);
    parseProperty(attrs, flags, typeName, QStringLiteral("lightambient"), &m_lightAmbient);
    parseProperty(attrs, flags, typeName, QStringLiteral("brightness"), &m_brightness);
    parseProperty(attrs, flags, typeName, QStringLiteral("linearfade"), &m_linearFade);
    parseProperty(attrs, flags, typeName, QStringLiteral("expfade"), &m_expFade);
    parseProperty(attrs, flags, typeName, QStringLiteral("areawidth"), &m_areaWidth);
    parseProperty(attrs, flags, typeName, QStringLiteral("areaheight"), &m_areaHeight);
    parseProperty(attrs, flags, typeName, QStringLiteral("castshadow"), &m_castShadow);
    parseProperty(attrs, flags, typeName, QStringLiteral("shdwfactor"), &m_shadowFactor);
    parseProperty(attrs, flags, typeName, QStringLiteral("shdwfilter"), &m_shadowFilter);
    parseProperty(attrs, flags, typeName, QStringLiteral("shdwmapres"), &m_shadowMapRes);
    parseProperty(attrs, flags, typeName, QStringLiteral("shdwbias"), &m_shadowBias);
    parseProperty(attrs, flags, typeName, QStringLiteral("shdwmapfar"), &m_shadowMapFar);
    parseProperty(attrs, flags, typeName, QStringLiteral("shdwmapfov"), &m_shadowMapFov);
}

void Q3DSLightNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSLightNode::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

void Q3DSLightNode::resolveReferences(Q3DSPresentation &pres, Q3DSUipParser &parser)
{
    Q3DSNode::resolveReferences(pres, parser);
    resolveRef(m_scope_unresolved, Q3DSGraphObject::AnyObject, &m_scope, pres);
}

QStringList Q3DSLightNode::gex_propertyNames() const
{
    QStringList s = Q3DSNode::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("scope") << QLatin1String("lightType") << QLatin1String("lightDiffuse")
      << QLatin1String("lightSpecular") << QLatin1String("lightAmbient") << QLatin1String("brightness") << QLatin1String("linearFade")
      << QLatin1String("expFade") << QLatin1String("areaWidth") << QLatin1String("areaHeight") << QLatin1String("castShadow")
      << QLatin1String("shadowFactor") << QLatin1String("shadowFilter") << QLatin1String("shadowMapRes") << QLatin1String("shadowBias")
      << QLatin1String("shadowMapFar") << QLatin1String("shadowMapFov");
    return s;
}

QVariantList Q3DSLightNode::gex_propertyValues() const
{
    QVariantList s = Q3DSNode::gex_propertyValues();
    s << m_name << m_scope_unresolved << m_lightType << m_lightDiffuse << m_lightSpecular << m_lightAmbient
      << m_brightness << m_linearFade << m_expFade << m_areaWidth << m_areaHeight << m_castShadow
      << m_shadowFactor << m_shadowFilter << m_shadowMapRes << m_shadowBias << m_shadowMapFar << m_shadowMapFov;
    return s;
}

Q3DSModelNode::Q3DSModelNode()
    : Q3DSNode(Q3DSGraphObject::Model)
{
}

template<typename V>
void Q3DSModelNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Model");
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseMeshProperty(attrs, flags, typeName, QStringLiteral("sourcepath"), &m_mesh_unresolved);
    parseProperty(attrs, flags, typeName, QStringLiteral("poseroot"), &m_skeletonRoot);
    parseProperty(attrs, flags, typeName, QStringLiteral("tessellation"), &m_tessellation);
    parseProperty(attrs, flags, typeName, QStringLiteral("edgetess"), &m_edgeTess);
    parseProperty(attrs, flags, typeName, QStringLiteral("innertess"), &m_innerTess);
}

void Q3DSModelNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSModelNode::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

void Q3DSModelNode::resolveReferences(Q3DSPresentation &pres, Q3DSUipParser &parser)
{
    Q3DSNode::resolveReferences(pres, parser);
    if (!m_mesh_unresolved.isEmpty()) {
        int part = 1;
        QString fn = parser.assetFileName(m_mesh_unresolved, &part);
        m_mesh = pres.mesh(fn, part);
    }
}

QStringList Q3DSModelNode::gex_propertyNames() const
{
    QStringList s = Q3DSNode::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("sourcePath") << QLatin1String("skeletonRoot")
      << QLatin1String("tessellation") << QLatin1String("edgeTess") << QLatin1String("innerTess");
    return s;
}

QVariantList Q3DSModelNode::gex_propertyValues() const
{
    QVariantList s = Q3DSNode::gex_propertyValues();
    s << m_name << m_mesh_unresolved << m_skeletonRoot << m_tessellation << m_edgeTess << m_innerTess;
    return s;
}

Q3DSGroupNode::Q3DSGroupNode()
    : Q3DSNode(Q3DSGraphObject::Group)
{
}

template<typename V>
void Q3DSGroupNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Group");
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSGroupNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSGroupNode::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

QStringList Q3DSGroupNode::gex_propertyNames() const
{
    QStringList s = Q3DSNode::gex_propertyNames();
    s << QLatin1String("name");
    return s;
}

QVariantList Q3DSGroupNode::gex_propertyValues() const
{
    QVariantList s = Q3DSNode::gex_propertyValues();
    s << m_name;
    return s;
}

Q3DSComponentNode::Q3DSComponentNode()
    : Q3DSNode(Q3DSGraphObject::Component)
{
}

Q3DSComponentNode::~Q3DSComponentNode()
{
    delete m_masterSlide;
}

template<typename V>
void Q3DSComponentNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Component");
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSComponentNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSComponentNode::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

void Q3DSComponentNode::setCurrentSlide(Q3DSSlide *slide)
{
    if (m_currentSlide == slide)
        return;

    qCDebug(lcUip, "Setting new current slide %s", slide->id().constData());
    m_currentSlide = slide;
}

QStringList Q3DSComponentNode::gex_propertyNames() const
{
    QStringList s = Q3DSNode::gex_propertyNames();
    s << QLatin1String("name");
    return s;
}

QVariantList Q3DSComponentNode::gex_propertyValues() const
{
    QVariantList s = Q3DSNode::gex_propertyValues();
    s << m_name;
    return s;
}

Q3DSTextNode::Q3DSTextNode()
    : Q3DSNode(Q3DSGraphObject::Text)
{
}

template<typename V>
void Q3DSTextNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Text");
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseMultiLineStringProperty(attrs, flags, typeName, QStringLiteral("textstring"), &m_text);
    parseProperty(attrs, flags, typeName, QStringLiteral("textcolor"), &m_color);
    parseFontProperty(attrs, flags, typeName, QStringLiteral("font"), &m_font);
    parseFontSizeProperty(attrs, flags, typeName, QStringLiteral("size"), &m_size);
    parseProperty(attrs, flags, typeName, QStringLiteral("horzalign"), &m_horizAlign);
    parseProperty(attrs, flags, typeName, QStringLiteral("vertalign"), &m_vertAlign);
    parseProperty(attrs, flags, typeName, QStringLiteral("leading"), &m_leading);
    parseProperty(attrs, flags, typeName, QStringLiteral("tracking"), &m_tracking);
}

void Q3DSTextNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSTextNode::applyPropertyChanges(const Q3DSPropertyChangeList *changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(*changeList, 0);
}

QStringList Q3DSTextNode::gex_propertyNames() const
{
    QStringList s = Q3DSNode::gex_propertyNames();
    s << QLatin1String("name") << QLatin1String("textstring") << QLatin1String("textcolor") << QLatin1String("font")
      << QLatin1String("size") << QLatin1String("horzalign") << QLatin1String("vertalign") << QLatin1String("leading") << QLatin1String("tracking");
    return s;
}

QVariantList Q3DSTextNode::gex_propertyValues() const
{
    QVariantList s = Q3DSNode::gex_propertyValues();
    s << m_name << m_text << m_color << m_font << m_size << m_horizAlign << m_vertAlign << m_leading << m_tracking;
    return s;
}

Q3DSPresentation::Q3DSPresentation()
    : d(new Q3DSPresentationData)
{
}

Q3DSPresentation::~Q3DSPresentation()
{
    delete d->scene;
    delete d->masterSlide;
}

void Q3DSPresentation::reset()
{
    delete d->scene;
    delete d->masterSlide;
    d.reset(new Q3DSPresentationData);
}

QString Q3DSPresentation::sourceFile() const
{
    return d->sourceFile;
}

QString Q3DSPresentation::author() const
{
    return d->author;
}

QString Q3DSPresentation::company() const
{
    return d->company;
}

int Q3DSPresentation::presentationWidth() const
{
    return d->presentationWidth;
}

int Q3DSPresentation::presentationHeight() const
{
    return d->presentationHeight;
}

Q3DSPresentation::Rotation Q3DSPresentation::presentationRotation() const
{
    return d->presentationRotation;
}

bool Q3DSPresentation::maintainAspectRatio() const
{
    return d->maintainAspectRatio;
}

Q3DSScene *Q3DSPresentation::scene() const
{
    return d->scene;
}

Q3DSSlide *Q3DSPresentation::masterSlide() const
{
    return d->masterSlide;
}

Q3DSGraphObject *Q3DSPresentation::object(const QByteArray &id) const
{
    return d->objects.value(id);
}

void Q3DSPresentation::setSourceFile(const QString &s)
{
    d->sourceFile = s;
}

void Q3DSPresentation::setAuthor(const QString &s)
{
    d->author = s;
}

void Q3DSPresentation::setCompany(const QString &s)
{
    d->company = s;
}

void Q3DSPresentation::setPresentationWidth(int w)
{
    d->presentationWidth = w;
}

void Q3DSPresentation::setPresentationHeight(int h)
{
    d->presentationHeight = h;
}

void Q3DSPresentation::setPresentationRotation(Q3DSPresentation::Rotation r)
{
    d->presentationRotation = r;
}

void Q3DSPresentation::setMaintainAspectRatio(bool b)
{
    d->maintainAspectRatio = b;
}

void Q3DSPresentation::setScene(Q3DSScene *p)
{
    d->scene = p;
}

void Q3DSPresentation::setMasterSlide(Q3DSSlide *p)
{
    d->masterSlide = p;
}

void Q3DSPresentation::setLoadTime(qint64 ms)
{
    d->loadTime = ms;
}

void Q3DSPresentation::registerImageBuffer(const QString &sourcePath, bool hasTransparency)
{
    d->imageBuffers[sourcePath] = hasTransparency;
}

void Q3DSPresentation::registerObject(const QByteArray &id, Q3DSGraphObject *p)
{
    if (d->objects.contains(id))
        qWarning("Q3DSPresentation: Multiple registrations for object id '%s'", id.constData());

    p->m_id = id;
    d->objects[id] = p;
}

bool Q3DSPresentation::loadCustomMaterial(const QStringRef &id, const QStringRef &, const QString &assetFilename)
{
    Q3DSCustomMaterialParser p;
    bool ok = false;
    Q3DSCustomMaterial mat = p.parse(assetFilename, &ok);
    if (!ok) {
        qWarning("Failed to parse custom material %s", qPrintable(assetFilename));
        return false;
    }
    d->customMaterials.insert(id.toUtf8(), mat);
    return true;
}

Q3DSCustomMaterial Q3DSPresentation::customMaterial(const QByteArray &id) const
{
    return d->customMaterials.value(id);
}

bool Q3DSPresentation::loadEffect(const QStringRef &id, const QStringRef &, const QString &assetFilename)
{
    Q3DSEffectParser p;
    bool ok = false;
    Q3DSEffect eff = p.parse(assetFilename, &ok);
    if (!ok) {
        qWarning("Failed to parse effect %s", qPrintable(assetFilename));
        return false;
    }
    d->effects.insert(id.toUtf8(), eff);
    return true;
}

Q3DSEffect Q3DSPresentation::effect(const QByteArray &id) const
{
    return d->effects.value(id);
}

MeshList Q3DSPresentation::mesh(const QString &assetFilename, int part)
{
    Q3DSPresentationData::MeshId id(assetFilename, part);
    if (d->meshes.contains(id))
        return d->meshes.value(id);

    MeshList m = Q3DSMeshLoader::loadMesh(assetFilename, part, false);
    d->meshes.insert(id, m);
    return m;
}

const Q3DSPresentation::ImageBufferMap &Q3DSPresentation::imageBuffer() const
{
    return d->imageBuffers;
}

qint64 Q3DSPresentation::loadTimeMsecs() const
{
    return d->loadTime;
}

void Q3DSPresentation::applySlidePropertyChanges(Q3DSSlide *slide) const
{
    auto changeList = slide->propertyChanges();
    qCDebug(lcUip, "Applying %d property changes from slide %s", changeList->count(), slide->id().constData());
    if (!changeList)
        return;

    for (auto it = changeList->cbegin(), ite = changeList->cend(); it != ite; ++it) {
        for (auto change = it.value()->begin(); change != it.value()->end(); change++) {
            qCDebug(lcUip) << "\t" << it.key() << "applying property change:" << change->name() << change->value();
        }
        it.key()->applyPropertyChanges(it.value());
    }

    for (auto it = changeList->cbegin(), ite = changeList->cend(); it != ite; ++it)
        it.key()->notifyPropertyChanges(it.value());
}

void Q3DSPresentation::forAllObjectsOfType(Q3DSGraphObject *root,
                                           Q3DSGraphObject::Type type,
                                           std::function<void(Q3DSGraphObject *)> f)
{
    Q3DSGraphObject *obj = root;
    while (obj) {
        if (obj->type() == type)
            f(obj);
        forAllObjectsOfType(obj->firstChild(), type, f);
        obj = obj->nextSibling();
    }
}

void Q3DSPresentation::forAllNodes(Q3DSGraphObject *root,
                                   std::function<void(Q3DSNode *)> f)
{
    Q3DSGraphObject *obj = root;
    while (obj) {
        if (obj->isNode())
            f(static_cast<Q3DSNode *>(obj));
        forAllNodes(obj->firstChild(), f);
        obj = obj->nextSibling();
    }
}

void Q3DSPresentation::forAllLayers(Q3DSScene *scene, std::function<void(Q3DSLayerNode *)> f, bool reverse)
{
    QVector<Q3DSLayerNode *> layers;
    Q3DSGraphObject *obj = scene->firstChild();
    while (obj) {
        if (obj->type() == Q3DSGraphObject::Layer) {
            Q3DSLayerNode *layer3DS = static_cast<Q3DSLayerNode *>(obj);
            // include hidden layers as well -> no check for Q3DSNode::Active
            if (!reverse)
                f(layer3DS);
            else
                layers.append(layer3DS);
        } else {
            qWarning("Child %p of Scene is not a Layer", obj);
        }
        obj = obj->nextSibling();
    }
    if (reverse) {
        for (auto it = layers.crbegin(), ite = layers.crend(); it != ite; ++it)
            f(*it);
    }
}

void Q3DSPresentation::forAllModels(Q3DSGraphObject *obj,
                                    std::function<void(Q3DSModelNode *)> f,
                                    bool includeHidden)
{
    while (obj) {
        if (obj->type() == Q3DSGraphObject::Model) {
            Q3DSModelNode *model = static_cast<Q3DSModelNode *>(obj);
            if (includeHidden || model->flags().testFlag(Q3DSNode::Active))
                f(model);
        }
        forAllModels(obj->firstChild(), f, includeHidden);
        obj = obj->nextSibling();
    }
}

void Q3DSPresentation::forAllImages(std::function<void (Q3DSImage *)> f)
{
    for (Q3DSGraphObject *obj : qAsConst(d->objects)) {
        if (obj->type() == Q3DSGraphObject::Image)
            f(static_cast<Q3DSImage *>(obj));
    }
}

QT_END_NAMESPACE
