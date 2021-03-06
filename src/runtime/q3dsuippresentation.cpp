﻿/****************************************************************************
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

#include "q3dsuippresentation_p.h"
#include "q3dsdatamodelparser_p.h"
#include "q3dsenummaps_p.h"
#include "q3dsuipparser_p.h"
#include "q3dsscenemanager_p.h"
#include "q3dsutils_p.h"
#include "q3dslogging_p.h"
#include <QXmlStreamReader>
#include <QLoggingCategory>
#include <functional>
#include <QtMath>
#include <QImage>

#include <QtCore/qmetaobject.h>

QT_BEGIN_NAMESPACE

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
    } else if (value == QStringLiteral("Vector") || value == QStringLiteral("Float3")) {
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
    } else if (value == QStringLiteral("Boolean") || value == QStringLiteral("Bool")) {
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
    // Adjust values that are "almost" zero values to 0.0, as we don't really expect values with
    // more then 3-4 decimal points (note that qFuzzyIsNull does allow slightly higher resolution though).
    if (ok && qFuzzyIsNull(*v))
        *v = 0.0f;
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

QVariant convertToVariant(const QString &value, Q3DS::PropertyType type)
{
    switch (type) {
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
    default:
        break;
    }

    return QVariant();
}

QVariant convertToVariant(const QString &value, const Q3DSMaterial::PropertyElement &propMeta)
{
    switch (propMeta.type) {
    case Enum:
    {
        int idx = propMeta.enumValues.indexOf(value);
        return idx >= 0 ? idx : 0;
    }
    default:
        return convertToVariant(value, propMeta.type);
    }
}

QString convertFromVariant(const QVariant &value)
{
    switch (value.type()) {
    case QVariant::Vector2D:
    {
        const QVector2D v = value.value<QVector2D>();
        return QString(QLatin1String("%1 %2"))
                .arg(QString::number(v.x())).arg(QString::number(v.y()));
    }
    case QVariant::Vector3D:
    {
        const QVector3D v = value.value<QVector3D>();
        return QString(QLatin1String("%1 %2 %3"))
                .arg(QString::number(v.x())).arg(QString::number(v.y())).arg(QString::number(v.z()));
    }
    case QVariant::Color:
    {
        const QColor c = value.value<QColor>();
        const QVector3D v = QVector3D(c.redF(), c.greenF(), c.blueF());
        return QString(QLatin1String("%1 %2 %3"))
                .arg(QString::number(v.x())).arg(QString::number(v.y())).arg(QString::number(v.z()));
    }
    case QVariant::Bool:
        return value.toBool() ? QLatin1String("true") : QLatin1String("false");
    default:
        return value.toString();
    }
}

} // namespace Q3DS

Q3DSPropertyChange Q3DSPropertyChange::fromVariant(const QString &name, const QVariant &value)
{
    return Q3DSPropertyChange(name, Q3DS::convertFromVariant(value));
}

QString Q3DSGraphObjectEvents::pressureDownEvent()
{
    return QLatin1String("onPressureDown");
}

QString Q3DSGraphObjectEvents::pressureUpEvent()
{
    return QLatin1String("onPressureUp");
}

QString Q3DSGraphObjectEvents::tapEvent()
{
    return QLatin1String("onTap");
}

QString Q3DSGraphObjectEvents::slideEnterEvent()
{
    return QLatin1String("onSlideEnter");
}

QString Q3DSGraphObjectEvents::slideExitEvent()
{
    return QLatin1String("onSlideExit");
}

Q3DSGraphObjectAttached::~Q3DSGraphObjectAttached()
{
}

Q3DSGraphObject::Q3DSGraphObject(Q3DSGraphObject::Type type)
    : m_type(type)
{
}

Q3DSGraphObject::~Q3DSGraphObject()
{
    destroyGraph();
    delete m_attached;
}

void Q3DSGraphObject::destroyGraph()
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
    // keep m_parent so that markDirty can walk up the chain still
    node->markDirty(DirtyNodeRemoved);
    // now it can be nulled out
    node->m_parent = nullptr;
}

void Q3DSGraphObject::removeAllChildNodes()
{
    while (m_firstChild) {
        Q3DSGraphObject *node = m_firstChild;
        m_firstChild = node->m_nextSibling;
        node->m_nextSibling = nullptr;
        if (m_firstChild)
            m_firstChild->m_previousSibling = nullptr;
        else
            m_lastChild = nullptr;
        node->markDirty(DirtyNodeRemoved);
        node->m_parent = nullptr;
    }
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
    node->markDirty(DirtyNodeAdded);
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
    node->markDirty(DirtyNodeAdded);
}

void Q3DSGraphObject::insertChildNodeBefore(Q3DSGraphObject *node, Q3DSGraphObject *before)
{
    Q_ASSERT_X(!node->m_parent, "Q3DSGraphObject::insertChildNodeBefore", "Q3DSGraphObject already has a parent");
    Q_ASSERT_X(before && before->m_parent == this, "Q3DSGraphObject::insertChildNodeBefore", "The parent of \'before\' is wrong");

    Q3DSGraphObject *previous = before->m_previousSibling;
    if (previous)
        previous->m_nextSibling = node;
    else
        m_firstChild = node;

    node->m_previousSibling = previous;
    node->m_nextSibling = before;
    before->m_previousSibling = node;
    node->m_parent = this;
    node->markDirty(DirtyNodeAdded);
}

void Q3DSGraphObject::insertChildNodeAfter(Q3DSGraphObject *node, Q3DSGraphObject *after)
{
    Q_ASSERT_X(!node->m_parent, "Q3DSGraphObject::insertChildNodeAfter", "Q3DSGraphObject already has a parent");
    Q_ASSERT_X(after && after->m_parent == this, "Q3DSGraphObject::insertChildNodeAfter", "The parent of \'after\' is wrong");

    Q3DSGraphObject *next = after->m_nextSibling;
    if (next)
        next->m_previousSibling = node;
    else
        m_lastChild = node;

    node->m_nextSibling = next;
    node->m_previousSibling = after;
    after->m_nextSibling = node;
    node->m_parent = this;
    node->markDirty(DirtyNodeAdded);
}

void Q3DSGraphObject::reparentChildNodesTo(Q3DSGraphObject *newParent)
{
    for (Q3DSGraphObject *c = firstChild(); c; c = firstChild()) {
        removeChildNode(c);
        newParent->appendChildNode(c);
    }
}

void Q3DSGraphObject::markDirty(DirtyFlags bits)
{
    // Find the scene object or the master slide and notify it. The scene and
    // master slide themselves generate no such notifications (hence starting
    // from m_parent) since they are associated directly with the presentation.
    Q3DSGraphObject *p = m_parent;
    if (m_type != Slide) {
        while (p) {
            if (p->m_type == Q3DSGraphObject::Scene) {
                static_cast<Q3DSScene *>(p)->notifyNodeChange(this, bits);
                break;
            }
            p = p->m_parent;
        }
    } else {
        while (p) {
            if (!p->m_parent) {
                static_cast<Q3DSSlide *>(p)->notifySlideGraphChange(static_cast<Q3DSSlide *>(this), bits);
                break;
            }
            p = p->m_parent;
        }
    }
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

int Q3DSGraphObject::mapChangeFlags(const Q3DSPropertyChangeList &changeList)
{
    // Property change handlers may not want to rely on changeList.keys().
    // Let's give subclasses the possibility to look at the property names here
    // and create a (class-specific) bitmask accordingly. For example, Q3DSNode
    // can check if position or rotation or scale is present in the list, and
    // set a NodeTransformChanges bit, if so. Subclasses of Q3DSNode are free
    // to use custom mask values starting from
    // (1 << Q3DSNode::FIRST_FREE_PROPERTY_CHANGE_BIT).
    // The property change handler can then just check for the bit in question.
    //
    // Note that for many types of changes there won't be anything to do here
    // since it is often enough to know that _something_ has changed.

    Q_UNUSED(changeList);
    return 0;
}

void Q3DSGraphObject::notifyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    const QSet<QString> keys = changeList.keys();
    const int changeFlags = mapChangeFlags(changeList);
    for (auto f : m_callbacks) {
        if (f)
            f(this, keys, changeFlags);
    }
}

// Setters return a change object that can be passed straight to
// notifyPropertyChange (via the changelists' intializer list even). When the
// value does not change, the returned change object has isValid()==false, these
// are ignored by the changelist.

template <typename Member, typename Value>
static Q3DSPropertyChange createPropSetter(Member &member, const Value &value, const char *uipname)
{
    Q3DSPropertyChange result;
    if (member != value) {
        member = value;
        result = Q3DSPropertyChange(QLatin1String(uipname));
    }

    return result;
}

#define MEMBER_PAIR(M) M, M##_unresolved
template <typename Member, typename MemberUnresolved, typename Value>
static Q3DSPropertyChange createObjectRefSetter(Member &member,
                                                MemberUnresolved &memberUnresolved,
                                                const Value &value,
                                                const char *uipname)
{
    Q3DSPropertyChange result;
    if (member != value) {
        member = value;
        memberUnresolved = QLatin1String("#") + QString::fromUtf8(value->id());
        result = Q3DSPropertyChange(QLatin1String(uipname));
    }

    return result;
}

template <typename Member, typename Flag>
static Q3DSPropertyChange createPropFlagSetter(Member &member,
                                               const Flag &flag,
                                               bool bvalue,
                                               const char *uipname)
{
    Q3DSPropertyChange result;
    const bool wasSet = member & flag;
    if (wasSet != bvalue) { \
        if (bvalue) member |= flag; else member &= ~flag;
        result = Q3DSPropertyChange(QLatin1String(uipname));
    }

    return result;
}

Q3DSPropertyChange Q3DSGraphObject::setName(const QString &v)
{
    return createPropSetter(m_name, v, "name");
}

Q3DSPropertyChange Q3DSGraphObject::setStartTime(qint32 v)
{
    return createPropSetter(m_startTime, v, "starttime");
}

Q3DSPropertyChange Q3DSGraphObject::setEndTime(qint32 v)
{
    return createPropSetter(m_endTime, v, "endtime");
}

int Q3DSGraphObject::addEventHandler(const QString &event, EventCallback callback)
{
    QVector<EventCallback> &handlerList(m_eventHandlers[event]);
    handlerList.append(callback);
    return handlerList.count() - 1;
}

void Q3DSGraphObject::removeEventHandler(const QString &event, int callbackId)
{
    auto it = m_eventHandlers.find(event);
    if (it != m_eventHandlers.end())
        (*it)[callbackId] = nullptr;
}

void Q3DSGraphObject::processEvent(const Event &e)
{
    Event ev = e;
    ev.target = this;

    auto invokeCallbacks = [ev](const QVector<EventCallback> &v) {
        for (EventCallback callback : v) {
            if (callback)
                callback(ev);
        }
    };

    // first invoke the catch-all event callbacks
    auto it = m_eventHandlers.constFind(QString());
    if (it != m_eventHandlers.constEnd())
        invokeCallbacks(*it);

    // then the event-specific ones
    it = m_eventHandlers.constFind(ev.event);
    if (it != m_eventHandlers.constEnd())
        invokeCallbacks(*it);

    bool bubbleUp = true;
    // onSlideEnter/Exit for a component slide must not bubble to outside the component
    if (m_type == Component) {
        if (ev.event == Q3DSGraphObjectEvents::slideEnterEvent() || ev.event == Q3DSGraphObjectEvents::slideExitEvent())
            bubbleUp = false;
    }
    if (bubbleUp && m_parent)
        m_parent->processEvent(ev);
}

QByteArray Q3DSGraphObject::typeAsString() const
{
    const int idx = metaObject()->indexOfEnumerator("Type");
    return (idx != -1) ? QByteArray(metaObject()->enumerator(idx).valueToKey(m_type)) : QByteArrayLiteral("Undefined");
}

QVector<QByteArray> Q3DSGraphObject::propertyNames() const
{
    QVector<QByteArray> propertyNames;
    const int count = extraMetaData()->data.isNull() ? metaObject()->propertyCount()
                                                     : metaObject()->propertyCount() + extraMetaData()->data->propertyNames.size();
    propertyNames.reserve(count);
    for (int i = 0; i != metaObject()->propertyCount(); ++i)
        propertyNames << metaObject()->property(i).name();
    if (!extraMetaData()->data.isNull())
        propertyNames += extraMetaData()->data->propertyNames;
    return propertyNames;
}

QVector<QVariant> Q3DSGraphObject::propertyValues() const
{
    QVector<QVariant> propertyValues;
    const int count = extraMetaData()->data.isNull() ? metaObject()->propertyCount()
                                                     : metaObject()->propertyCount() + extraMetaData()->data->propertyNames.size();
    propertyValues.reserve(count);
    for (int i = 0; i != metaObject()->propertyCount(); ++i)
        propertyValues << metaObject()->property(i).readOnGadget(this);
    if (!extraMetaData()->data.isNull())
        propertyValues += extraMetaData()->data->propertyValues;
    return propertyValues;
}

QVariant Q3DSGraphObject::property(const char *name)
{
    if (!name)
        return QVariant();

    const int idx = metaObject()->indexOfProperty(name);
    if (idx != -1)
        return metaObject()->property(idx).readOnGadget(this);

    if (extraMetaData()->data.isNull())
        return QVariant();

    const int propIdx = extraMetaData()->data->propertyNames.indexOf(name);
    if (propIdx != -1)
        return extraMetaData()->data->propertyValues.at(propIdx);

    return QVariant();
}

bool Q3DSGraphObject::setProperty(const char *name, const QVariant &v)
{
    if (!name)
        return false;

    const int idx = metaObject()->indexOfProperty(name);
    if (idx != -1) {
        metaObject()->property(idx).writeOnGadget(this, v);
        return true;
    }

    if (extraMetaData()->data.isNull())
        extraMetaData()->data.reset(new Q3DSObjectExtraMetaData::Data);

    const int propIdx = extraMetaData()->data->propertyNames.indexOf(name);
    if (propIdx != -1) {
        if (v.isValid())
            extraMetaData()->data->propertyValues[propIdx] = v;
        else
            extraMetaData()->data->propertyNames.remove(idx);
    } else if (v.isValid()) {
        extraMetaData()->data->propertyNames.push_back(name);
        extraMetaData()->data->propertyValues.push_back(v);
    }

    return false;
}

QVector<QByteArray> Q3DSGraphObject::dynamicPropertyNames() const
{
    if (extraMetaData()->data.isNull())
        return  QVector<QByteArray>();

    return extraMetaData()->data->propertyNames;
}

QVector<QVariant> Q3DSGraphObject::dynamicPropertyValues() const
{
    if (extraMetaData()->data.isNull())
        return  QVector<QVariant>();

    return extraMetaData()->data->propertyValues;
}

QVariantMap Q3DSGraphObject::dynamicProperties() const
{
    QVariantMap dynProps;
    const int propCount = extraMetaData()->data.isNull() ? 0
                                                         : extraMetaData()->data->propertyNames.size();
    for (int i = 0; i != propCount; ++i) {
        const auto &name = QString::fromLatin1(extraMetaData()->data->propertyNames.at(i));
        const auto &value = extraMetaData()->data->propertyValues.at(i);
        dynProps[name] = value;
    }

    return dynProps;
}

void Q3DSGraphObject::clearDynamicProperties()
{
    Q3DSObjectExtraMetaData::Data *d = extraMetaData()->data.data();
    if (!d)
        return;

    d->propertyNames.clear();
    d->propertyValues.clear();
}

Q3DSPropertyChangeList Q3DSGraphObject::applyDynamicProperties(const QVariantMap &v)
{
    Q3DSPropertyChangeList changeList;
    if (extraMetaData()->data.isNull())
        return changeList;

    for (auto it = v.cbegin(); it != v.cend(); ++it) {
        const int idx = extraMetaData()->data->propertyNames.indexOf(it.key().toLatin1());
        if (idx != -1) {
            extraMetaData()->data->propertyValues[idx] = it.value();
            changeList.append(Q3DSPropertyChange(it.key()));
        }
    }
    return changeList;
}

// The property conversion functions all follow the same pattern:
// 1. Check if a value is provided explicitly in the attribute list.
// 2. Then, when PropSetDefaults is set, see if the metadata provided a default value.
// 3. If all else fails, just return false. This is not fatal (and perfectly normal when PropSetDefaults is not set).

// V is const iterable with name() and value() on iter
template<typename T, typename V>
bool parseProperty(const V &attrs, Q3DSGraphObject::PropSetFlags flags,
                   const QString &dataModelTypeName, const QString &propName, Q3DS::PropertyType propType,
                   T *dst, std::function<bool(const QStringRef &, T *v)> convertFunc)
{
    auto it = std::find_if(attrs.cbegin(), attrs.cend(), [propName](const typename V::value_type &v) { return v.name() == propName; });
    if (it != attrs.cend()) {
        const QStringRef v = it->value();
        return convertFunc(it->value(), dst);
    } else if (flags.testFlag(Q3DSGraphObject::PropSetDefaults)) {
        Q3DSDataModelParser *dataModelParser = Q3DSDataModelParser::instance();
        if (dataModelParser) {
            const QVector<Q3DSDataModelParser::Property> *props = dataModelParser->propertiesForType(dataModelTypeName);
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
void resolveRef(const QString &val, Q3DSGraphObject::Type type, T **obj, Q3DSUipPresentation &pres)
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

    // name is not parsed here since the data model metadata defines a default
    // value per type, so leave it to the subclasses
}

void Q3DSGraphObject::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    setProps(attrs, flags);
}

void Q3DSGraphObject::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    setProps(changeList, 0);
}

Q3DSScene::Q3DSScene()
    : Q3DSGraphObject(Q3DSGraphObject::Scene)
{
}

Q3DSScene::~Q3DSScene()
{
    // Do this here since markDirty in removeChildNode calls into
    // notifyNodeChange by casting to Q3DSScene. Prevent issues from already
    // being half-destructed.
    destroyGraph();
}

void Q3DSScene::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    // Asset properties (starttime, endtime) are not in use, hence no base call.

    const QString typeName = QStringLiteral("Scene");
    parseProperty(attrs, flags, typeName, QStringLiteral("bgcolorenable"), &m_useClearColor);
    parseProperty(attrs, flags, typeName, QStringLiteral("backgroundcolor"), &m_clearColor);

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

int Q3DSScene::addSceneChangeObserver(SceneChangeCallback callback)
{
    m_sceneChangeCallbacks.append(callback);
    return m_sceneChangeCallbacks.count() - 1;
}

void Q3DSScene::removeSceneChangeObserver(int callbackId)
{
    m_sceneChangeCallbacks[callbackId] = nullptr;
}

void Q3DSScene::notifyNodeChange(Q3DSGraphObject *obj, Q3DSGraphObject::DirtyFlags bits)
{
    for (auto f : m_sceneChangeCallbacks) {
        if (f) {
            if (bits.testFlag(DirtyNodeAdded))
                f(this, DirtyNodeAdded, obj);
            if (bits.testFlag(DirtyNodeRemoved))
                f(this, DirtyNodeRemoved, obj);
        }
    }
}

Q3DSPropertyChange Q3DSScene::setUseClearColor(bool v)
{
    return createPropSetter(m_useClearColor, v, "bgcolorenable");
}

Q3DSPropertyChange Q3DSScene::setClearColor(const QColor &v)
{
    return createPropSetter(m_clearColor, v, "backgroundcolor");
}

// Note that invalid changes are not added (great for an efficient yet simple
// notifyPropertyChanges({ setXxxxx() }) pattern!) hance assumptions about
// count() increasing after append should be avoided.

#ifdef Q_COMPILER_INITIALIZER_LISTS
Q3DSPropertyChangeList::Q3DSPropertyChangeList(std::initializer_list<Q3DSPropertyChange> args)
{
    for (const Q3DSPropertyChange &change : args) {
        if (change.isValid()) {
            m_changes.append(change);
            m_keys.insert(change.nameStr());
        }
    }
}
#endif

void Q3DSPropertyChangeList::append(const Q3DSPropertyChange &change)
{
    if (!change.isValid())
        return;

    m_changes.append(change);
    m_keys.insert(change.nameStr());
}

Q3DSSlide::Q3DSSlide()
    : Q3DSGraphObject(Q3DSGraphObject::Slide)
{
}

Q3DSSlide::~Q3DSSlide()
{
    // Do this here since markDirty in removeChildNode calls into
    // notifySlideGraphChange, if this is the master slide, by casting to
    // Q3DSSlide. Prevent issues from already being half-destructed.
    if (!parent())
        destroyGraph();

    qDeleteAll(m_propChanges);
}

template<typename V>
void Q3DSSlide::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Slide");
    parseProperty(attrs, flags, typeName, QStringLiteral("playmode"), &m_playMode);
    parseProperty(attrs, flags, typeName, QStringLiteral("initialplaystate"), &m_initialPlayState);

    StringOrInt pt;
    if (parseProperty(attrs, flags, typeName, QStringLiteral("playthroughto"), &pt)) {
        const bool isRef = (!pt.isInt && pt.s.startsWith(QLatin1Char('#')));
        const bool hasExplicitValue = (pt.isInt || isRef);
        if (hasExplicitValue) {
            m_playThrough = Value;
            m_playThroughValue = isRef ? QVariant::fromValue(pt.s) : QVariant::fromValue(pt.n);
        } else {
            Q3DSEnumMap::enumFromStr(QStringRef(&pt.s), &m_playThrough);
        }
    }

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSSlide::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSSlide::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(changeList, 0);
}

void Q3DSSlide::resolveReferences(Q3DSUipPresentation &pres)
{
    for (Q3DSAction &action : m_actions) {
        resolveRef(action.targetObject_unresolved, Q3DSGraphObject::AnyObject, &action.targetObject, pres);
        resolveRef(action.triggerObject_unresolved, Q3DSGraphObject::AnyObject, &action.triggerObject, pres);
    }
}

void Q3DSSlide::addObject(Q3DSGraphObject *obj)
{
    m_objects.insert(obj);
    SlideObjectChange change;
    change.type = SlideObjectAdded;
    change.obj = obj;
    notifySlideObjectChange(change);
}

void Q3DSSlide::removeObject(Q3DSGraphObject *obj)
{
    auto it = m_objects.find(obj);
    if (it != m_objects.end()) {
        m_objects.erase(it);
        SlideObjectChange change;
        change.type = SlideObjectRemoved;
        change.obj = obj;
        notifySlideObjectChange(change);
    }
}

void Q3DSSlide::addPropertyChanges(Q3DSGraphObject *target, Q3DSPropertyChangeList *changeList)
{
    m_propChanges.insert(target, changeList);
    SlideObjectChange change;
    change.type = SlidePropertyChangesAdded;
    change.obj = target;
    change.changeList = changeList;
    notifySlideObjectChange(change);
}

void Q3DSSlide::removePropertyChanges(Q3DSGraphObject *target)
{
    delete takePropertyChanges(target);
}

Q3DSPropertyChangeList *Q3DSSlide::takePropertyChanges(Q3DSGraphObject *target)
{
    auto it = m_propChanges.find(target);
    if (it != m_propChanges.end()) {
        Q3DSPropertyChangeList *propChanges = *it;
        m_propChanges.erase(it);
        SlideObjectChange change;
        change.type = SlidePropertyChangesRemoved;
        change.obj = target;
        change.changeList = propChanges;
        notifySlideObjectChange(change);
        return propChanges;
    }
    return nullptr;
}

void Q3DSSlide::addAnimation(const Q3DSAnimationTrack &track)
{
    m_anims.append(track);
    SlideObjectChange change;
    change.type = SlideAnimationAdded;
    change.animation = track;
    notifySlideObjectChange(change);
}

void Q3DSSlide::removeAnimation(const Q3DSAnimationTrack &track)
{
    const int idx = m_anims.indexOf(track);
    if (idx >= 0) {
        m_anims.removeAt(idx);
        SlideObjectChange change;
        change.type = SlideAnimationRemoved;
        change.animation = track;
        notifySlideObjectChange(change);
    }
}

void Q3DSSlide::addAction(const Q3DSAction &action)
{
    m_actions.append(action);
    SlideObjectChange change;
    change.type = SlideActionAdded;
    change.action = action;
    notifySlideObjectChange(change);
}

void Q3DSSlide::removeAction(const Q3DSAction &action)
{
    const int idx = m_actions.indexOf(action);
    if (idx >= 0) {
        m_actions.removeAt(idx);
        SlideObjectChange change;
        change.type = SlideActionRemoved;
        change.action = action;
        notifySlideObjectChange(change);
    }
}

// "slide change" would be quite confusing, hence using "slide graph change" instead
int Q3DSSlide::addSlideGraphChangeObserver(SlideGraphChangeCallback callback)
{
    Q_ASSERT(!parent()); // must be master (not a perfect check but better than nothing)

    m_slideGraphChangeCallbacks.append(callback);
    return m_slideGraphChangeCallbacks.count() - 1;
}

void Q3DSSlide::removeSlideGraphChangeObserver(int callbackId)
{
    m_slideGraphChangeCallbacks[callbackId] = nullptr;
}

// called from Q3DSGraphObject::markDirty()
void Q3DSSlide::notifySlideGraphChange(Q3DSSlide *slide, Q3DSGraphObject::DirtyFlags bits)
{
    Q_ASSERT(!parent()); // must be master

    for (auto f : m_slideGraphChangeCallbacks) {
        if (f) {
            if (bits.testFlag(DirtyNodeAdded))
                f(this, DirtyNodeAdded, slide);
            if (bits.testFlag(DirtyNodeRemoved))
                f(this, DirtyNodeRemoved, slide);
        }
    }
}

int Q3DSSlide::addSlideObjectChangeObserver(SlideObjectChangeCallback callback)
{
    m_slideObjectChangeCallbacks.append(callback);
    return m_slideObjectChangeCallbacks.count() - 1;
}

void Q3DSSlide::removeSlideObjectChangeObserver(int callbackId)
{
    m_slideObjectChangeCallbacks[callbackId] = nullptr;
}

void Q3DSSlide::notifySlideObjectChange(const SlideObjectChange &change)
{
    for (auto f : m_slideObjectChangeCallbacks) {
        if (f)
            f(this, change);
    }
}

Q3DSPropertyChange Q3DSSlide::setPlayMode(PlayMode v)
{
    return createPropSetter(m_playMode, v, "playmode");
}

Q3DSPropertyChange Q3DSSlide::setInitialPlayState(InitialPlayState v)
{
    return createPropSetter(m_initialPlayState, v, "initialplaystate");
}

Q3DSPropertyChange Q3DSSlide::setPlayThrough(PlayThrough v)
{
    return createPropSetter(m_playThrough, v, "playthroughto");
}

Q3DSPropertyChange Q3DSSlide::setPlayThroughValue(const QVariant &v)
{
    m_playThrough = Value;
    return createPropSetter(m_playThroughValue, v, "playthroughto");
}

Q3DSImage::Q3DSImage()
    : Q3DSGraphObject(Image)
{
}

template<typename V>
void Q3DSImage::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Image");
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
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseProperty(attrs, flags, typeName, QStringLiteral("endtime"), &m_endTime);
}

void Q3DSImage::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);
    calculateTextureTransform();
}

void Q3DSImage::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(changeList, 0);
    calculateTextureTransform();
}

void Q3DSImage::resolveReferences(Q3DSUipPresentation &presentation)
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
        m_sourcePath = presentation.assetFileName(m_sourcePath, nullptr);
    }
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
            qCDebug(lcPerf, "Perf. hint: Scanning image of size %dx%d manually for transparency. "
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

bool Q3DSImage::hasTransparency(Q3DSUipPresentation *presentation)
{
    if (m_scannedForTransparency)
        return m_hasTransparency;

    // Do a pre-load of the texture to check for transparency
    if (presentation->imageTransparencyHash().contains(sourcePath())) {
        m_hasTransparency = presentation->imageTransparencyHash()[sourcePath()];
    } else {
        m_hasTransparency = scanForTransparency(sourcePath());
        presentation->imageTransparencyHash()[sourcePath()] = m_hasTransparency;
    }
    m_scannedForTransparency = true;
    return m_hasTransparency;
}

bool Q3DSImage::hasPremultipliedAlpha() const
{
    if (!m_subPresentation.isEmpty())
        return true;

    return false;
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

Q3DSPropertyChange Q3DSImage::setSourcePath(const QString &v)
{
    if (!m_customImage.isNull())
        m_customImage = QImage();

    return createPropSetter(m_sourcePath, v, "sourcepath");
}

Q3DSPropertyChange Q3DSImage::setCustomImage(const QImage &v)
{
    if (!m_sourcePath.isEmpty())
        m_sourcePath = QString();

    return createPropSetter(m_customImage, v, "sourcepath");
}

Q3DSPropertyChange Q3DSImage::setScaleU(float v)
{
    return createPropSetter(m_scaleU, v, "scaleu");
}

Q3DSPropertyChange Q3DSImage::setScaleV(float v)
{
    return createPropSetter(m_scaleV, v, "scalev");
}

Q3DSPropertyChange Q3DSImage::setMappingMode(MappingMode v)
{
    return createPropSetter(m_mappingMode, v, "mappingmode");
}

Q3DSPropertyChange Q3DSImage::setHorizontalTiling(TilingMode v)
{
    return createPropSetter(m_tilingHoriz, v, "tilingmodehorz");
}

Q3DSPropertyChange Q3DSImage::setVerticalTiling(TilingMode v)
{
    return createPropSetter(m_tilingVert, v, "tilingmodevert");
}

Q3DSPropertyChange Q3DSImage::setRotationUV(float v)
{
    return createPropSetter(m_rotationUV, v, "rotationuv");
}

Q3DSPropertyChange Q3DSImage::setPositionU(float v)
{
    return createPropSetter(m_positionU, v, "positionu");
}

Q3DSPropertyChange Q3DSImage::setPositionV(float v)
{
    return createPropSetter(m_positionV, v, "positionv");
}

Q3DSPropertyChange Q3DSImage::setPivotU(float v)
{
    return createPropSetter(m_pivotU, v, "pivotu");
}

Q3DSPropertyChange Q3DSImage::setPivotV(float v)
{
    return createPropSetter(m_pivotV, v, "pivotv");
}

Q3DSPropertyChange Q3DSImage::setSubPresentation(const QString &v)
{
    return createPropSetter(m_subPresentation, v, "subpresentation");
}

Q3DSDefaultMaterial::Q3DSDefaultMaterial()
    : Q3DSGraphObject(Q3DSGraphObject::DefaultMaterial)
{
}

template<typename V>
void Q3DSDefaultMaterial::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Material");

    parseProperty(attrs, flags, typeName, QStringLiteral("shaderlighting"), &m_shaderLighting);
    parseProperty(attrs, flags, typeName, QStringLiteral("blendmode"), &m_blendMode);

    parseProperty(attrs, flags, typeName, QStringLiteral("vertexcolors"), &m_vertexColors);
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

    parseImageProperty(attrs, flags, typeName, QStringLiteral("roughnessmap"), &m_roughnessMap_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("fresnelPower"), &m_fresnelPower);
    parseProperty(attrs, flags, typeName, QStringLiteral("ior"), &m_ior);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("bumpmap"), &m_bumpMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("normalmap"), &m_normalMap_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("bumpamount"), &m_bumpAmount);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("displacementmap"), &m_displacementMap_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("displaceamount"), &m_displaceAmount);
    parseProperty(attrs, flags, typeName, QStringLiteral("opacity"), &m_opacity);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("opacitymap"), &m_opacityMap_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("emissivecolor"), &m_emissiveColor);
    parseProperty(attrs, flags, typeName, QStringLiteral("emissivepower"), &m_emissivePower);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("emissivemap"), &m_emissiveMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("emissivemap2"), &m_emissiveMap2_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("translucencymap"), &m_translucencyMap_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("translucentfalloff"), &m_translucentFalloff);
    parseProperty(attrs, flags, typeName, QStringLiteral("diffuselightwrap"), &m_diffuseLightWrap);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("lightmapindirect"), &m_lightmapIndirectMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("lightmapradiosity"), &m_lightmapRadiosityMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("lightmapshadow"), &m_lightmapShadowMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("iblprobe"), &m_lightProbe_unresolved);

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSDefaultMaterial::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSDefaultMaterial::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(changeList, 0);
}

void Q3DSDefaultMaterial::resolveReferences(Q3DSUipPresentation &pres)
{
    resolveRef(m_diffuseMap_unresolved, Q3DSGraphObject::Image, &m_diffuseMap, pres);
    resolveRef(m_diffuseMap2_unresolved, Q3DSGraphObject::Image, &m_diffuseMap2, pres);
    resolveRef(m_diffuseMap3_unresolved, Q3DSGraphObject::Image, &m_diffuseMap3, pres);
    resolveRef(m_specularReflection_unresolved, Q3DSGraphObject::Image, &m_specularReflection, pres);
    resolveRef(m_specularMap_unresolved, Q3DSGraphObject::Image, &m_specularMap, pres);
    resolveRef(m_roughnessMap_unresolved, Q3DSGraphObject::Image, &m_roughnessMap, pres);
    resolveRef(m_bumpMap_unresolved, Q3DSGraphObject::Image, &m_bumpMap, pres);
    resolveRef(m_normalMap_unresolved, Q3DSGraphObject::Image, &m_normalMap, pres);
    resolveRef(m_displacementMap_unresolved, Q3DSGraphObject::Image, &m_displacementMap, pres);
    resolveRef(m_opacityMap_unresolved, Q3DSGraphObject::Image, &m_opacityMap, pres);
    resolveRef(m_emissiveMap_unresolved, Q3DSGraphObject::Image, &m_emissiveMap, pres);
    resolveRef(m_emissiveMap2_unresolved, Q3DSGraphObject::Image, &m_emissiveMap2, pres);
    resolveRef(m_translucencyMap_unresolved, Q3DSGraphObject::Image, &m_translucencyMap, pres);

    resolveRef(m_lightmapIndirectMap_unresolved, Q3DSGraphObject::Image, &m_lightmapIndirectMap, pres);
    resolveRef(m_lightmapRadiosityMap_unresolved, Q3DSGraphObject::Image, &m_lightmapRadiosityMap, pres);
    resolveRef(m_lightmapShadowMap_unresolved, Q3DSGraphObject::Image, &m_lightmapShadowMap, pres);
    resolveRef(m_lightProbe_unresolved, Q3DSGraphObject::Image, &m_lightProbe, pres);
}

int Q3DSDefaultMaterial::mapChangeFlags(const Q3DSPropertyChangeList &changeList)
{
    int changeFlags = Q3DSGraphObject::mapChangeFlags(changeList);
    for (auto it = changeList.cbegin(), itEnd = changeList.cend(); it != itEnd; ++it) {
        if (it->nameStr() == QStringLiteral("blendmode"))
            changeFlags |= BlendModeChanges;
    }
    return changeFlags;
}

Q3DSPropertyChange Q3DSDefaultMaterial::setShaderLighting(ShaderLighting v)
{
    return createPropSetter(m_shaderLighting, v, "shaderlighting");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setBlendMode(BlendMode v)
{
    return createPropSetter(m_blendMode, v, "blendmode");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setDiffuse(const QColor &v)
{
    return createPropSetter(m_diffuse, v, "diffuse");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setDiffuseMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_diffuseMap), v, "diffusemap");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setDiffuseMap2(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_diffuseMap2), v, "diffusemap2");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setDiffuseMap3(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_diffuseMap3), v, "diffusemap3");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setSpecularReflection(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_specularReflection), v, "specularreflection");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setSpecularTint(const QColor &v)
{
    return createPropSetter(m_specularTint, v, "speculartint");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setSpecularAmount(float v)
{
    return createPropSetter(m_specularAmount, v, "specularamount");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setSpecularMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_specularMap), v, "specularmap");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setSpecularModel(SpecularModel v)
{
    return createPropSetter(m_specularModel, v, "specularmodel");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setSpecularRoughness(float v)
{
    return createPropSetter(m_specularRoughness, v, "specularroughness");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setRoughnessMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_roughnessMap), v, "roughnessmap");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setFresnelPower(float v)
{
    return createPropSetter(m_fresnelPower, v, "fresnelPower");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setIor(float v)
{
    return createPropSetter(m_ior, v, "ior");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setBumpMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_bumpMap), v, "bumpmap");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setNormalMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_normalMap), v, "normalmap");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setBumpAmount(float v)
{
    return createPropSetter(m_bumpAmount, v, "bumpamount");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setDisplacementMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_displacementMap), v, "displacementmap");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setDisplaceAmount(float v)
{
    return createPropSetter(m_displaceAmount, v, "displaceamount");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setOpacity(float v)
{
    return createPropSetter(m_opacity, v, "opacity");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setOpacityMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_opacityMap), v, "opacitymap");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setEmissiveColor(const QColor &v)
{
    return createPropSetter(m_emissiveColor, v, "emissivecolor");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setEmissivePower(float v)
{
    return createPropSetter(m_emissivePower, v, "emissivepower");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setEmissiveMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_emissiveMap), v, "emissivemap");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setEmissiveMap2(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_emissiveMap2), v, "emissivemap2");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setTranslucencyMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_translucencyMap), v, "translucencymap");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setTranslucentFalloff(float v)
{
    return createPropSetter(m_translucentFalloff, v, "translucentfalloff");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setDiffuseLightWrap(float v)
{
    return createPropSetter(m_diffuseLightWrap, v, "diffuselightwrap");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setLightmapIndirectMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightmapIndirectMap), v, "lightmapindirect");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setLightmapRadiosityMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightmapRadiosityMap), v, "lightmapradiosity");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setLightmapShadowMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightmapShadowMap), v, "lightmapshadow");
}

Q3DSPropertyChange Q3DSDefaultMaterial::setLightProbe(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightProbe), v, "iblprobe");
}

Q3DSReferencedMaterial::Q3DSReferencedMaterial()
    : Q3DSGraphObject(Q3DSGraphObject::ReferencedMaterial)
{
}

template<typename V>
void Q3DSReferencedMaterial::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("ReferencedMaterial");
    parseObjectRefProperty(attrs, flags, typeName, QStringLiteral("referencedmaterial"), &m_referencedMaterial_unresolved);

    parseImageProperty(attrs, flags, typeName, QStringLiteral("lightmapindirect"), &m_lightmapIndirectMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("lightmapradiosity"), &m_lightmapRadiosityMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("lightmapshadow"), &m_lightmapShadowMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("iblprobe"), &m_lightProbe_unresolved);

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSReferencedMaterial::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSReferencedMaterial::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(changeList, 0);
}

void Q3DSReferencedMaterial::resolveReferences(Q3DSUipPresentation &pres)
{
    // can be DefaultMaterial or CustomMaterial so stick with a generic object
    resolveRef(m_referencedMaterial_unresolved, Q3DSGraphObject::AnyObject, &m_referencedMaterial, pres);

    resolveRef(m_lightmapIndirectMap_unresolved, Q3DSGraphObject::Image, &m_lightmapIndirectMap, pres);
    resolveRef(m_lightmapRadiosityMap_unresolved, Q3DSGraphObject::Image, &m_lightmapRadiosityMap, pres);
    resolveRef(m_lightmapShadowMap_unresolved, Q3DSGraphObject::Image, &m_lightmapShadowMap, pres);
    resolveRef(m_lightProbe_unresolved, Q3DSGraphObject::Image, &m_lightProbe, pres);
}

Q3DSPropertyChange Q3DSReferencedMaterial::setReferencedMaterial(Q3DSGraphObject *v)
{
    return createPropSetter(m_referencedMaterial, v, "referencedmaterial");
}

Q3DSPropertyChange Q3DSReferencedMaterial::setLightmapIndirectMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightmapIndirectMap), v, "lightmapindirect");
}

Q3DSPropertyChange Q3DSReferencedMaterial::setLightmapRadiosityMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightmapRadiosityMap), v, "lightmapradiosity");
}

Q3DSPropertyChange Q3DSReferencedMaterial::setLightmapShadowMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightmapShadowMap), v, "lightmapshadow");
}

Q3DSPropertyChange Q3DSReferencedMaterial::setLightProbe(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightProbe), v, "iblprobe");
}

Q3DSCustomMaterialInstance::Q3DSCustomMaterialInstance()
    : Q3DSGraphObject(Q3DSGraphObject::CustomMaterial)
{
}

Q3DSCustomMaterialInstance::Q3DSCustomMaterialInstance(const Q3DSCustomMaterial &material)
    : Q3DSGraphObject(Q3DSGraphObject::CustomMaterial),
      m_material(material)
{
}

template<typename V>
void Q3DSCustomMaterialInstance::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("CustomMaterial");
    if (parseProperty(attrs, flags, typeName, QStringLiteral("class"), &m_material_unresolved))
        m_materialIsResolved = false;

    parseImageProperty(attrs, flags, typeName, QStringLiteral("lightmapindirect"), &m_lightmapIndirectMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("lightmapradiosity"), &m_lightmapRadiosityMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("lightmapshadow"), &m_lightmapShadowMap_unresolved);
    parseImageProperty(attrs, flags, typeName, QStringLiteral("iblprobe"), &m_lightProbe_unresolved);

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSCustomMaterialInstance::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);

    // Save attributes for the 2nd pass (resolveReferences) since they may
    // refer to custom properties defined in the custom material.
    for (const QXmlStreamAttribute &attr : attrs)
        m_pendingCustomProperties.append(Q3DSPropertyChange(attr.name().toString(), attr.value().toString()));
}

void Q3DSCustomMaterialInstance::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(changeList, 0);

    QVariantMap propChanges;
    for (const Q3DSPropertyChange &change : changeList) {
        const Q3DS::PropertyType type = m_material.properties().value(change.nameStr()).type;
        propChanges[change.nameStr()] = Q3DS::convertToVariant(change.valueStr(), type);
    }

    if (!propChanges.isEmpty())
        applyDynamicProperties(propChanges);
}

template<typename T>
void mapCustomPropertyFileNames(const QString &propName, QVariant *prop, const T &propMeta, const Q3DSUipPresentation &pres)
{
    Q3DS::PropertyType t = propMeta[propName].type;
    if (t == Q3DS::Texture) {
        const QString fn = prop->toString();
        if (!fn.isEmpty())
            *prop = pres.assetFileName(fn, nullptr);
    }
}

template<typename T>
void mapCustomPropertyFileNames(QVariantMap *propTab, const T &propMeta, const Q3DSUipPresentation &pres)
{
    for (auto it = propTab->begin(), ite = propTab->end(); it != ite; ++it) {
        Q3DS::PropertyType t = propMeta[it.key()].type;
        if (t == Q3DS::Texture) {
            const QString fn = it->toString();
            if (!fn.isEmpty())
                *it = pres.assetFileName(fn, nullptr);
        }
    }
}

static void importCustomProperties(Q3DSGraphObject *obj,
                                   const QMap<QString, Q3DSMaterial::PropertyElement> &customPropMeta,
                                   Q3DSPropertyChangeList *pendingCustomProperties,
                                   Q3DSUipPresentation &pres)
{
    // Remove all existing dynamic properties. This may seem unnecessary but
    // unfortunately it is needed since changing the source (class property) to
    // a different effect brings in a different set of custom properties, and
    // having the old ones lingering around causes trouble (or is inefficient
    // at best).
    obj->clearDynamicProperties();

    // Add the effect's dynamic properties with the default values first.
    for (auto it = customPropMeta.cbegin(); it != customPropMeta.cend(); ++it) {
        // Fix up the filenames to that no further adjustment is necessary from this point on.
        QVariant value = Q3DS::convertToVariant(it->defaultValue, *it);
        mapCustomPropertyFileNames(it.key(), &value, customPropMeta, pres);
        obj->setProperty(it.key().toLatin1(), value);
    }

    // Now go update any pending property values (from the .uip parsing).
    if (!pendingCustomProperties->isEmpty()) {
        QVariantMap propChanges;
        for (const Q3DSPropertyChange &change : *pendingCustomProperties) {
            const Q3DS::PropertyType type = customPropMeta.value(change.nameStr()).type;
            propChanges[change.nameStr()] = Q3DS::convertToVariant(change.valueStr(), type);
        }
        mapCustomPropertyFileNames(&propChanges, customPropMeta, pres);
        obj->applyDynamicProperties(propChanges);
        pendingCustomProperties->clear(); // clear, we won't apply these values again in case the source changes
    }
}

void Q3DSCustomMaterialInstance::resolveReferences(Q3DSUipPresentation &pres)
{
    // Only do the rest if sourcepath actually changed because the custom
    // property processing must only be done when there was a real change.
    if (m_materialIsResolved)
        return;

    m_materialIsResolved = true;

    if (m_material_unresolved.isEmpty())
        return;

    // keep the # if it's an id since it is otherwise treated as a filename
    // (that needs resolving when relative)
    QString idOrFilename = m_material_unresolved;
    if (!idOrFilename.startsWith(QLatin1Char('#')))
        idOrFilename = pres.assetFileName(idOrFilename, nullptr);

    m_material = pres.customMaterial(idOrFilename);
    if (m_material.isNull())
        return;

    importCustomProperties(this, m_material.properties(), &m_pendingCustomProperties, pres);

    resolveRef(m_lightmapIndirectMap_unresolved, Q3DSGraphObject::Image, &m_lightmapIndirectMap, pres);
    resolveRef(m_lightmapRadiosityMap_unresolved, Q3DSGraphObject::Image, &m_lightmapRadiosityMap, pres);
    resolveRef(m_lightmapShadowMap_unresolved, Q3DSGraphObject::Image, &m_lightmapShadowMap, pres);
    resolveRef(m_lightProbe_unresolved, Q3DSGraphObject::Image, &m_lightProbe, pres);
}

int Q3DSCustomMaterialInstance::mapChangeFlags(const Q3DSPropertyChangeList &changeList)
{
    int changeFlags = Q3DSGraphObject::mapChangeFlags(changeList);
    for (auto it = changeList.cbegin(), itEnd = changeList.cend(); it != itEnd; ++it) {
        if (it->nameStr() == QStringLiteral("class"))
            changeFlags |= SourceChanges;
    }
    return changeFlags;
}

Q3DSPropertyChange Q3DSCustomMaterialInstance::setSourcePath(const QString &v)
{
    Q3DSPropertyChange r = createPropSetter(m_material_unresolved, v, "class");
    const bool valueChanged = !r.nameStr().isEmpty();
    if (valueChanged)
        m_materialIsResolved = false;
    return r;
}

Q3DSPropertyChange Q3DSCustomMaterialInstance::setLightmapIndirectMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightmapIndirectMap), v, "lightmapindirect");
}

Q3DSPropertyChange Q3DSCustomMaterialInstance::setLightmapRadiosityMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightmapRadiosityMap), v, "lightmapradiosity");
}

Q3DSPropertyChange Q3DSCustomMaterialInstance::setLightmapShadowMap(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightmapShadowMap), v, "lightmapshadow");
}

Q3DSPropertyChange Q3DSCustomMaterialInstance::setLightProbe(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightProbe), v, "iblprobe");
}

Q3DSEffectInstance::Q3DSEffectInstance()
    : Q3DSGraphObject(Q3DSGraphObject::Effect)
{
}

Q3DSEffectInstance::Q3DSEffectInstance(const Q3DSEffect &effect)
    : Q3DSGraphObject(Q3DSGraphObject::Effect),
      m_effect(effect)
{
}

template<typename V>
void Q3DSEffectInstance::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Effect");
    if (parseProperty(attrs, flags, typeName, QStringLiteral("class"), &m_effect_unresolved))
        m_effectIsResolved = false;

    parseProperty(attrs, flags, typeName, QStringLiteral("eyeball"), &m_eyeballEnabled);

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSEffectInstance::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);

    // Save attributes for the 2nd pass (resolveReferences) since they may
    // refer to custom properties defined in the effect.
    for (const QXmlStreamAttribute &attr : attrs)
        m_pendingCustomProperties.append(Q3DSPropertyChange(attr.name().toString(), attr.value().toString()));

    // If this is on the master slide, store some rollback info.
    if (flags.testFlag(PropSetOnMaster)) {
        m_masterRollbackList.append(Q3DSPropertyChange(QLatin1String("eyeball"), m_eyeballEnabled ? QLatin1String("True")
                                                                                          : QLatin1String("False")));

    }
}

void Q3DSEffectInstance::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(changeList, 0);

    // could be a custom effect property
    QVariantMap propChanges;
    for (const Q3DSPropertyChange &change : changeList) {
        const Q3DS::PropertyType type = m_effect.properties().value(change.nameStr()).type;
        propChanges[change.nameStr()] = Q3DS::convertToVariant(change.valueStr(), type);
    }

    if (!propChanges.isEmpty())
        applyDynamicProperties(propChanges);
}

void Q3DSEffectInstance::resolveReferences(Q3DSUipPresentation &pres)
{
    // Only do the rest if sourcepath actually changed because the custom
    // property processing must only be done when there was a real change.
    if (m_effectIsResolved)
        return;

    m_effectIsResolved = true;

    if (m_effect_unresolved.isEmpty())
        return;

    // keep the # if it's an id since it is otherwise treated as a filename
    // (that needs resolving when relative)
    QString idOrFilename = m_effect_unresolved;
    if (!idOrFilename.startsWith(QLatin1Char('#')))
        idOrFilename = pres.assetFileName(idOrFilename, nullptr);

    m_effect = pres.effect(idOrFilename);
    if (m_effect.isNull())
        return;

    importCustomProperties(this, m_effect.properties(), &m_pendingCustomProperties, pres);
}

int Q3DSEffectInstance::mapChangeFlags(const Q3DSPropertyChangeList &changeList)
{
    int changeFlags = Q3DSGraphObject::mapChangeFlags(changeList);
    for (auto it = changeList.cbegin(), itEnd = changeList.cend(); it != itEnd; ++it) {
        if (it->nameStr() == QStringLiteral("eyeball"))
            changeFlags |= EyeBallChanges;
        else if (it->nameStr() == QStringLiteral("class"))
            changeFlags |= SourceChanges;
    }
    return changeFlags;
}

Q3DSPropertyChange Q3DSEffectInstance::setSourcePath(const QString &v)
{
    Q3DSPropertyChange r = createPropSetter(m_effect_unresolved, v, "class");
    const bool valueChanged = !r.nameStr().isEmpty();
    if (valueChanged)
        m_effectIsResolved = false;
    return r;
}

Q3DSPropertyChange Q3DSEffectInstance::setEyeballEnabled(bool v)
{
    return createPropSetter(m_eyeballEnabled, v, "eyeball");
}

Q3DSBehaviorInstance::Q3DSBehaviorInstance()
    : Q3DSGraphObject(Q3DSGraphObject::Behavior)
{
}

Q3DSBehaviorInstance::Q3DSBehaviorInstance(const Q3DSBehavior &behavior)
    : Q3DSGraphObject(Q3DSGraphObject::Behavior),
      m_behavior(behavior)
{
}

template<typename V>
void Q3DSBehaviorInstance::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Behavior");
    parseProperty(attrs, flags, typeName, QStringLiteral("class"), &m_behavior_unresolved);

    parseProperty(attrs, flags, typeName, QStringLiteral("eyeball"), &m_eyeballEnabled);

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSBehaviorInstance::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSGraphObject::setProperties(attrs, flags);
    setProps(attrs, flags);

    // Save attributes for the 2nd pass (resolveReferences) since they may
    // refer to custom properties defined in the behavior.
    for (const QXmlStreamAttribute &attr : attrs)
        m_pendingCustomProperties.append(Q3DSPropertyChange(attr.name().toString(), attr.value().toString()));
}

void Q3DSBehaviorInstance::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(changeList, 0);

    // could be a custom behavior property
    QVariantMap propChanges;
    for (const Q3DSPropertyChange &change : changeList) {
        const Q3DS::PropertyType type = m_behavior.properties().value(change.nameStr()).type;
        propChanges[change.nameStr()] = Q3DS::convertToVariant(change.valueStr(), type);
    }

    if (!propChanges.isEmpty())
        applyDynamicProperties(propChanges);
}

void Q3DSBehaviorInstance::resolveReferences(Q3DSUipPresentation &pres)
{
    // changing the behavior class dynamically is not supported. do it only once.
    if (!m_behaviorIsResolved && !m_behavior_unresolved.isEmpty()) {
        QString idOrFilename = m_behavior_unresolved;
        if (!idOrFilename.startsWith(QLatin1Char('#')))
            idOrFilename = pres.assetFileName(idOrFilename, nullptr);
        m_behavior = pres.behavior(idOrFilename);
        m_behaviorIsResolved = true;
        if (!m_behavior.isNull()) {
            // Now add the (dynamic) properties with the default values first.
            const auto props = m_behavior.properties();
            for (auto it = props.cbegin(); it != props.cend(); ++it) {
                const Q3DS::PropertyType type = m_behavior.properties().value(it.key()).type;
                // Fix up the filenames to that no further adjustment is necessary from this point on.
                QVariant value = Q3DS::convertToVariant(it->defaultValue, type);
                mapCustomPropertyFileNames(it.key(), &value, props, pres);
                setProperty(it.key().toLatin1(), value);
            }

            // Now go update any pending property values
            QVariantMap propChanges;
            for (const Q3DSPropertyChange &change : m_pendingCustomProperties) {
                const Q3DS::PropertyType type = props.value(change.nameStr()).type;
                propChanges[change.nameStr()] = Q3DS::convertToVariant(change.valueStr(), type);
            }
            mapCustomPropertyFileNames(&propChanges, props, pres);
            applyDynamicProperties(propChanges);
            m_pendingCustomProperties.clear();
        }
    }
}

int Q3DSBehaviorInstance::mapChangeFlags(const Q3DSPropertyChangeList &changeList)
{
    int changeFlags = Q3DSGraphObject::mapChangeFlags(changeList);
    for (auto it = changeList.cbegin(), itEnd = changeList.cend(); it != itEnd; ++it) {
        if (it->nameStr() == QStringLiteral("eyeball"))
            changeFlags |= EyeBallChanges;
    }
    return changeFlags;
}

Q3DSPropertyChange Q3DSBehaviorInstance::setEyeballEnabled(bool v)
{
    return createPropSetter(m_eyeballEnabled, v, "eyeball");
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
    parseProperty(attrs, flags, typeName, QStringLiteral("opacity"), &m_localOpacity);
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
        m_masterRollbackList.append(Q3DSPropertyChange(QLatin1String("eyeball"),
                                     m_flags.testFlag(Q3DSNode::Active)
                                     ? QLatin1String("True") : QLatin1String("False")));

    }
}

void Q3DSNode::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSGraphObject::applyPropertyChanges(changeList);
    setProps(changeList, 0);
}

int Q3DSNode::mapChangeFlags(const Q3DSPropertyChangeList &changeList)
{
    int changeFlags = Q3DSGraphObject::mapChangeFlags(changeList);
    for (auto it = changeList.cbegin(), itEnd = changeList.cend(); it != itEnd; ++it) {
        if (it->nameStr() == QStringLiteral("position")
                || it->nameStr() == QStringLiteral("rotation")
                || it->nameStr() == QStringLiteral("scale"))
        {
            changeFlags |= TransformChanges;
        } else if (it->nameStr() == QStringLiteral("opacity")) {
            changeFlags |= OpacityChanges;
        } else if (it->nameStr() == QStringLiteral("eyeball")) {
            changeFlags |= EyeballChanges;
        }
    }
    return changeFlags;
}

Q3DSPropertyChange Q3DSNode::setFlag(NodeFlag flag, bool v)
{
    if (flag == Active) {
        return setEyeballEnabled(v);
    } else if (flag == IgnoresParentTransform) {
        return setIgnoresParent(v);
    }
    return Q3DSPropertyChange();
}

Q3DSPropertyChange Q3DSNode::setEyeballEnabled(bool v)
{
    return createPropFlagSetter(m_flags, Active, v, "eyeball");
}

Q3DSPropertyChange Q3DSNode::setIgnoresParent(bool v)
{
    return createPropFlagSetter(m_flags, IgnoresParentTransform, v, "ignoresparent");
}

Q3DSPropertyChange Q3DSNode::setRotation(const QVector3D &v)
{
    return createPropSetter(m_rotation, v, "rotation");
}

Q3DSPropertyChange Q3DSNode::setPosition(const QVector3D &v)
{
    return createPropSetter(m_position, v, "position");
}

Q3DSPropertyChange Q3DSNode::setScale(const QVector3D &v)
{
    return createPropSetter(m_scale, v, "scale");
}

Q3DSPropertyChange Q3DSNode::setPivot(const QVector3D &v)
{
    return createPropSetter(m_pivot, v, "pivot");
}

Q3DSPropertyChange Q3DSNode::setLocalOpacity(float v)
{
    return createPropSetter(m_localOpacity, v, "opacity");
}

Q3DSPropertyChange Q3DSNode::setSkeletonId(int v)
{
    return createPropSetter(m_skeletonId, v, "boneid");
}

Q3DSPropertyChange Q3DSNode::setRotationOrder(RotationOrder v)
{
    return createPropSetter(m_rotationOrder, v, "rotationorder");
}

Q3DSPropertyChange Q3DSNode::setOrientation(Orientation v)
{
    return createPropSetter(m_orientation, v, "orientation");
}

Q3DSLayerNode::Q3DSLayerNode()
    : Q3DSNode(Q3DSGraphObject::Layer)
{
}

template<typename V>
void Q3DSLayerNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Layer");

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

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSLayerNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSLayerNode::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(changeList, 0);
}

void Q3DSLayerNode::resolveReferences(Q3DSUipPresentation &pres)
{
    Q3DSNode::resolveReferences(pres);
    resolveRef(m_lightProbe_unresolved, Q3DSGraphObject::Image, &m_lightProbe, pres);
    resolveRef(m_lightProbe2_unresolved, Q3DSGraphObject::Image, &m_lightProbe2, pres);
}

int Q3DSLayerNode::mapChangeFlags(const Q3DSPropertyChangeList &changeList)
{
    int changeFlags = Q3DSNode::mapChangeFlags(changeList);
    for (auto it = changeList.cbegin(), itEnd = changeList.cend(); it != itEnd; ++it) {
        if (it->nameStr().startsWith(QStringLiteral("ao"))
               || it->nameStr().startsWith(QStringLiteral("shadow")))
        {
            changeFlags |= AoOrShadowChanges;
        }
    }
    return changeFlags;
}

Q3DSPropertyChange Q3DSLayerNode::setLayerFlag(Flag flag, bool v)
{
    if (flag == DisableDepthTest) {
        return setDepthTestDisabled(v);
    } else if (flag == DisableDepthPrePass) {
        return setDepthPrePassDisabled(v);
    } else if (flag == TemporalAA) {
        return setTemporalAAEnabled(v);
    } else if (flag == FastIBL) {
        return setFastIBLEnabled(v);
    }
    return Q3DSPropertyChange();
}

Q3DSPropertyChange Q3DSLayerNode::setDepthTestDisabled(bool v)
{
    return createPropFlagSetter(m_layerFlags, DisableDepthTest, v, "disabledepthtest");
}

Q3DSPropertyChange Q3DSLayerNode::setDepthPrePassDisabled(bool v)
{
    return createPropFlagSetter(m_layerFlags, DisableDepthPrePass, v, "disabledepthprepass");
}

Q3DSPropertyChange Q3DSLayerNode::setTemporalAAEnabled(bool v)
{
    return createPropFlagSetter(m_layerFlags, TemporalAA, v, "temporalaa");
}

Q3DSPropertyChange Q3DSLayerNode::setFastIBLEnabled(bool v)
{
    return createPropFlagSetter(m_layerFlags, FastIBL, v, "fastibl");
}

Q3DSPropertyChange Q3DSLayerNode::setProgressiveAA(ProgressiveAA v)
{
    return createPropSetter(m_progressiveAA, v, "progressiveaa");
}

Q3DSPropertyChange Q3DSLayerNode::setMultisampleAA(MultisampleAA v)
{
    return createPropSetter(m_multisampleAA, v, "multisampleaa");
}

Q3DSPropertyChange Q3DSLayerNode::setLayerBackground(LayerBackground v)
{
    return createPropSetter(m_layerBackground, v, "background");
}

Q3DSPropertyChange Q3DSLayerNode::setBackgroundColor(const QColor &v)
{
    return createPropSetter(m_backgroundColor, v, "backgroundcolor");
}

Q3DSPropertyChange Q3DSLayerNode::setBlendType(BlendType v)
{
    return createPropSetter(m_blendType, v, "blendtype");
}

Q3DSPropertyChange Q3DSLayerNode::setHorizontalFields(HorizontalFields v)
{
    return createPropSetter(m_horizontalFields, v, "horzfields");
}

Q3DSPropertyChange Q3DSLayerNode::setLeft(float v)
{
    return createPropSetter(m_left, v, "left");
}

Q3DSPropertyChange Q3DSLayerNode::setLeftUnits(Units v)
{
    return createPropSetter(m_leftUnits, v, "leftunits");
}

Q3DSPropertyChange Q3DSLayerNode::setWidth(float v)
{
    return createPropSetter(m_width, v, "width");
}

Q3DSPropertyChange Q3DSLayerNode::setWidthUnits(Units v)
{
    return createPropSetter(m_widthUnits, v, "widthunits");
}

Q3DSPropertyChange Q3DSLayerNode::setRight(float v)
{
    return createPropSetter(m_right, v, "right");
}

Q3DSPropertyChange Q3DSLayerNode::setRightUnits(Units v)
{
    return createPropSetter(m_rightUnits, v, "rightunits");
}

Q3DSPropertyChange Q3DSLayerNode::setVerticalFields(VerticalFields v)
{
    return createPropSetter(m_verticalFields, v, "vertfields");
}

Q3DSPropertyChange Q3DSLayerNode::setTop(float v)
{
    return createPropSetter(m_top, v, "top");
}

Q3DSPropertyChange Q3DSLayerNode::setTopUnits(Units v)
{
    return createPropSetter(m_topUnits, v, "topunits");
}

Q3DSPropertyChange Q3DSLayerNode::setHeight(float v)
{
    return createPropSetter(m_height, v, "height");
}

Q3DSPropertyChange Q3DSLayerNode::setHeightUnits(Units v)
{
    return createPropSetter(m_heightUnits, v, "heightunits");
}

Q3DSPropertyChange Q3DSLayerNode::setBottom(float v)
{
    return createPropSetter(m_bottom, v, "bottom");
}

Q3DSPropertyChange Q3DSLayerNode::setBottomUnits(Units v)
{
    return createPropSetter(m_bottomUnits, v, "bottomunits");
}

Q3DSPropertyChange Q3DSLayerNode::setSourcePath(const QString &v)
{
    return createPropSetter(m_sourcePath, v, "sourcepath");
}

Q3DSPropertyChange Q3DSLayerNode::setAoStrength(float v)
{
    return createPropSetter(m_aoStrength, v, "aostrength");
}

Q3DSPropertyChange Q3DSLayerNode::setAoDistance(float v)
{
    return createPropSetter(m_aoDistance, v, "aodistance");
}

Q3DSPropertyChange Q3DSLayerNode::setAoSoftness(float v)
{
    return createPropSetter(m_aoSoftness, v, "aosoftness");
}

Q3DSPropertyChange Q3DSLayerNode::setAoBias(float v)
{
    return createPropSetter(m_aoBias, v, "aobias");
}

Q3DSPropertyChange Q3DSLayerNode::setAoSampleRate(int v)
{
    return createPropSetter(m_aoSampleRate, v, "aosamplerate");
}

Q3DSPropertyChange Q3DSLayerNode::setAoDither(bool v)
{
    return createPropSetter(m_aoDither, v, "aodither");
}

Q3DSPropertyChange Q3DSLayerNode::setShadowStrength(float v)
{
    return createPropSetter(m_shadowStrength, v, "shadowstrength");
}

Q3DSPropertyChange Q3DSLayerNode::setShadowDist(float v)
{
    return createPropSetter(m_shadowDist, v, "shadowdist");
}

Q3DSPropertyChange Q3DSLayerNode::setShadowSoftness(float v)
{
    return createPropSetter(m_shadowSoftness, v, "shadowsoftness");
}

Q3DSPropertyChange Q3DSLayerNode::setShadowBias(float v)
{
    return createPropSetter(m_shadowBias, v, "shadowbias");
}

Q3DSPropertyChange Q3DSLayerNode::setLightProbe(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightProbe), v, "lightprobe");
}

Q3DSPropertyChange Q3DSLayerNode::setProbeBright(float v)
{
    return createPropSetter(m_probeBright, v, "probebright");
}

Q3DSPropertyChange Q3DSLayerNode::setProbeHorizon(float v)
{
    return createPropSetter(m_probeHorizon, v, "probehorizon");
}

Q3DSPropertyChange Q3DSLayerNode::setProbeFov(float v)
{
    return createPropSetter(m_probeFov, v, "probefov");
}

Q3DSPropertyChange Q3DSLayerNode::setLightProbe2(Q3DSImage *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_lightProbe2), v, "lightprobe2");
}

Q3DSPropertyChange Q3DSLayerNode::setProbe2Fade(float v)
{
    return createPropSetter(m_probe2Fade, v, "probe2fade");
}

Q3DSPropertyChange Q3DSLayerNode::setProbe2Window(float v)
{
    return createPropSetter(m_probe2Window, v, "probe2window");
}

Q3DSPropertyChange Q3DSLayerNode::setProbe2Pos(float v)
{
    return createPropSetter(m_probe2Pos, v, "probe2pos");
}

Q3DSCameraNode::Q3DSCameraNode()
    : Q3DSNode(Q3DSGraphObject::Camera)
{
    m_position = QVector3D(0, 0, -600);
}

template<typename V>
void Q3DSCameraNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Camera");

    parseProperty(attrs, flags, typeName, QStringLiteral("orthographic"), &m_orthographic);
    parseProperty(attrs, flags, typeName, QStringLiteral("fov"), &m_fov);
    parseProperty(attrs, flags, typeName, QStringLiteral("clipnear"), &m_clipNear);
    parseProperty(attrs, flags, typeName, QStringLiteral("clipfar"), &m_clipFar);
    parseProperty(attrs, flags, typeName, QStringLiteral("scalemode"), &m_scaleMode);
    parseProperty(attrs, flags, typeName, QStringLiteral("scaleanchor"), &m_scaleAnchor);

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
    parseProperty(attrs, flags, typeName, QStringLiteral("position"), &m_position);
}

void Q3DSCameraNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSCameraNode::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(changeList, 0);
}

Q3DSPropertyChange Q3DSCameraNode::setOrthographic(bool v)
{
    return createPropSetter(m_orthographic, v, "orthographic");
}

Q3DSPropertyChange Q3DSCameraNode::setFov(float v)
{
    return createPropSetter(m_fov, v, "fov");
}

Q3DSPropertyChange Q3DSCameraNode::setClipNear(float v)
{
    return createPropSetter(m_clipNear, v, "clipnear");
}

Q3DSPropertyChange Q3DSCameraNode::setClipFar(float v)
{
    return createPropSetter(m_clipFar, v, "clipfar");
}

Q3DSPropertyChange Q3DSCameraNode::setScaleMode(ScaleMode v)
{
    return createPropSetter(m_scaleMode, v, "scalemode");
}

Q3DSPropertyChange Q3DSCameraNode::setScaleAnchor(ScaleAnchor v)
{
    return createPropSetter(m_scaleAnchor, v, "scaleanchor");
}

Q3DSLightNode::Q3DSLightNode()
    : Q3DSNode(Q3DSGraphObject::Light)
{
}

template<typename V>
void Q3DSLightNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Light");

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

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSLightNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSLightNode::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(changeList, 0);
}

void Q3DSLightNode::resolveReferences(Q3DSUipPresentation &pres)
{
    Q3DSNode::resolveReferences(pres);
    resolveRef(m_scope_unresolved, Q3DSGraphObject::AnyObject, &m_scope, pres);
}

Q3DSPropertyChange Q3DSLightNode::setLightType(LightType v)
{
    return createPropSetter(m_lightType, v, "lighttype");
}

Q3DSPropertyChange Q3DSLightNode::setScope(Q3DSGraphObject *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_scope), v, "scope");
}

Q3DSPropertyChange Q3DSLightNode::setDiffuse(const QColor &v)
{
    return createPropSetter(m_lightDiffuse, v, "lightdiffuse");
}

Q3DSPropertyChange Q3DSLightNode::setSpecular(const QColor &v)
{
    return createPropSetter(m_lightSpecular, v, "lightspecular");
}

Q3DSPropertyChange Q3DSLightNode::setAmbient(const QColor &v)
{
    return createPropSetter(m_lightAmbient, v, "lightambient");
}

Q3DSPropertyChange Q3DSLightNode::setBrightness(float v)
{
    return createPropSetter(m_brightness, v, "brightness");
}

Q3DSPropertyChange Q3DSLightNode::setLinearFade(float v)
{
    return createPropSetter(m_linearFade, v, "linearfade");
}

Q3DSPropertyChange Q3DSLightNode::setExpFade(float v)
{
    return createPropSetter(m_expFade, v, "expfade");
}

Q3DSPropertyChange Q3DSLightNode::setAreaWidth(float v)
{
    return createPropSetter(m_areaWidth, v, "areawidth");
}

Q3DSPropertyChange Q3DSLightNode::setAreaHeight(float v)
{
    return createPropSetter(m_areaHeight, v, "areaheight");
}

Q3DSPropertyChange Q3DSLightNode::setCastShadow(bool v)
{
    return createPropSetter(m_castShadow, v, "castshadow");
}

Q3DSPropertyChange Q3DSLightNode::setShadowFactor(float v)
{
    return createPropSetter(m_shadowFactor, v, "shdwfactor");
}

Q3DSPropertyChange Q3DSLightNode::setShadowFilter(float v)
{
    return createPropSetter(m_shadowFilter, v, "shdwfilter");
}

Q3DSPropertyChange Q3DSLightNode::setShadowMapRes(int v)
{
    return createPropSetter(m_shadowMapRes, v, "shdwmapres");
}

Q3DSPropertyChange Q3DSLightNode::setShadowBias(float v)
{
    return createPropSetter(m_shadowBias, v, "shdwbias");
}

Q3DSPropertyChange Q3DSLightNode::setShadowMapFar(float v)
{
    return createPropSetter(m_shadowMapFar, v, "shdwmapfar");
}

Q3DSPropertyChange Q3DSLightNode::setShadowMapFov(float v)
{
    return createPropSetter(m_shadowMapFov, v, "shdwmapfov");
}

Q3DSModelNode::Q3DSModelNode()
    : Q3DSNode(Q3DSGraphObject::Model)
{
}

Q3DSModelNode::~Q3DSModelNode()
{
    delete m_customMesh;
}

template<typename V>
void Q3DSModelNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Model");
    parseMeshProperty(attrs, flags, typeName, QStringLiteral("sourcepath"), &m_mesh_unresolved);
    parseProperty(attrs, flags, typeName, QStringLiteral("poseroot"), &m_skeletonRoot);
    parseProperty(attrs, flags, typeName, QStringLiteral("tessellation"), &m_tessellation);
    parseProperty(attrs, flags, typeName, QStringLiteral("edgetess"), &m_edgeTess);
    parseProperty(attrs, flags, typeName, QStringLiteral("innertess"), &m_innerTess);

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSModelNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSModelNode::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(changeList, 0);
}

int Q3DSModelNode::mapChangeFlags(const Q3DSPropertyChangeList &changeList)
{
    int changeFlags = Q3DSNode::mapChangeFlags(changeList);
    for (auto it = changeList.cbegin(), itEnd = changeList.cend(); it != itEnd; ++it) {
        if (it->nameStr() == QStringLiteral("sourcepath"))
            changeFlags |= MeshChanges;
    }
    return changeFlags;
}

void Q3DSModelNode::resolveReferences(Q3DSUipPresentation &pres)
{
    Q3DSNode::resolveReferences(pres);
    if (!m_mesh_unresolved.isEmpty()) {
        if (m_mesh_unresolved != QStringLiteral("#Custom")) {
            int part = 1;
            const QString fn = pres.assetFileName(m_mesh_unresolved, &part);
            m_mesh = pres.mesh(fn, part); // this caches; cheap if already used the same mesh somewhere
        } else {
            // ### should this be cached? (would need a cache key then)
            if (m_customMesh)
                m_mesh = Q3DSMeshLoader::loadMesh(*m_customMesh, &m_customMeshMapping);
        }
    }
}

Q3DSPropertyChange Q3DSModelNode::setMesh(const QString &v)
{
    return createPropSetter(m_mesh_unresolved, v, "sourcepath");
}

Q3DSPropertyChange Q3DSModelNode::setSkeletonRoot(int v)
{
    return createPropSetter(m_skeletonRoot, v, "poseroot");
}

Q3DSPropertyChange Q3DSModelNode::setTessellation(Tessellation v)
{
    return createPropSetter(m_tessellation, v, "tessellation");
}

Q3DSPropertyChange Q3DSModelNode::setEdgeTess(float v)
{
    return createPropSetter(m_edgeTess, v, "edgetess");
}

Q3DSPropertyChange Q3DSModelNode::setInnerTess(float v)
{
    return createPropSetter(m_innerTess, v, "innertess");
}

Q3DSPropertyChange Q3DSModelNode::setCustomMesh(Q3DSGeometry *geom)
{
    if (m_customMesh != geom)
        delete m_customMesh;

    m_customMesh = geom; // takes ownership

    // like setMesh but note that the m_mesh_unresolved value does not change
    // between setCustomMesh calls, yet there may be a change in the custom
    // geometry. so generate a prop.change always.
    m_mesh_unresolved = QLatin1String("#Custom");
    return Q3DSPropertyChange(QLatin1String("sourcepath"));
}

void Q3DSModelNode::updateCustomMeshBuffer(int bufferIdx)
{
    Q3DSMeshLoader::updateMeshBuffer(*m_customMesh, m_customMeshMapping, bufferIdx);
}

void Q3DSModelNode::updateCustomMeshBuffer(int bufferIdx, int offset, int size)
{
    Q3DSMeshLoader::updateMeshBuffer(*m_customMesh, m_customMeshMapping, bufferIdx, offset, size);
}

Q3DSGroupNode::Q3DSGroupNode()
    : Q3DSNode(Q3DSGraphObject::Group)
{
}

template<typename V>
void Q3DSGroupNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Group");

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSGroupNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSGroupNode::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(changeList, 0);
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

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSComponentNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSComponentNode::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(changeList, 0);
}

void Q3DSComponentNode::resolveReferences(Q3DSUipPresentation &pres)
{
    // There is an own little slide graph in each component, make sure object
    // refs in actions are resolved in these as well.
    pres.forAllObjectsOfType(m_masterSlide, Q3DSGraphObject::Slide, [&](Q3DSGraphObject *obj) {
        obj->resolveReferences(pres);
    });
}

void Q3DSComponentNode::setCurrentSlide(Q3DSSlide *slide)
{
    if (m_currentSlide == slide)
        return;

    m_currentSlide = slide;
}

Q3DSTextNode::Q3DSTextNode()
    : Q3DSNode(Q3DSGraphObject::Text)
{
}

template<typename V>
void Q3DSTextNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Text");
    parseMultiLineStringProperty(attrs, flags, typeName, QStringLiteral("textstring"), &m_text);
    parseProperty(attrs, flags, typeName, QStringLiteral("textcolor"), &m_color);
    parseFontProperty(attrs, flags, typeName, QStringLiteral("font"), &m_font);
    parseFontSizeProperty(attrs, flags, typeName, QStringLiteral("size"), &m_size);
    parseProperty(attrs, flags, typeName, QStringLiteral("horzalign"), &m_horizAlign);
    parseProperty(attrs, flags, typeName, QStringLiteral("vertalign"), &m_vertAlign);
    parseProperty(attrs, flags, typeName, QStringLiteral("leading"), &m_leading);
    parseProperty(attrs, flags, typeName, QStringLiteral("tracking"), &m_tracking);

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

void Q3DSTextNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSTextNode::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(changeList, 0);
}

int Q3DSTextNode::mapChangeFlags(const Q3DSPropertyChangeList &changeList)
{
    int changeFlags = Q3DSNode::mapChangeFlags(changeList);
    for (auto it = changeList.cbegin(), itEnd = changeList.cend(); it != itEnd; ++it) {
        if (it->nameStr() == QStringLiteral("textstring")
                || it->nameStr() == QStringLiteral("leading")
                || it->nameStr() == QStringLiteral("tracking")
                || it->nameStr() == QStringLiteral("size")
                || it->nameStr() == QStringLiteral("font")
                || it->nameStr() == QStringLiteral("horzalign")
                || it->nameStr() == QStringLiteral("vertalign"))
        {
            changeFlags |= TextureImageDepChanges;
        }
    }
    return changeFlags;
}

Q3DSPropertyChange Q3DSTextNode::setText(const QString &v)
{
    return createPropSetter(m_text, v, "textstring");
}

Q3DSPropertyChange Q3DSTextNode::setColor(const QColor &v)
{
    return createPropSetter(m_color, v, "textcolor");
}

Q3DSPropertyChange Q3DSTextNode::setFont(const QString &v)
{
    return createPropSetter(m_font, v, "font");
}

Q3DSPropertyChange Q3DSTextNode::setSize(float v)
{
    return createPropSetter(m_size, v, "size");
}

Q3DSPropertyChange Q3DSTextNode::setHorizontalAlignment(HorizontalAlignment v)
{
    return createPropSetter(m_horizAlign, v, "horzalign");
}

Q3DSPropertyChange Q3DSTextNode::setVerticalAlignment(VerticalAlignment v)
{
    return createPropSetter(m_vertAlign, v, "vertalign");
}

Q3DSPropertyChange Q3DSTextNode::setLeading(float v)
{
    return createPropSetter(m_leading, v, "leading");
}

Q3DSPropertyChange Q3DSTextNode::setTracking(float v)
{
    return createPropSetter(m_tracking, v, "tracking");
}

Q3DSAliasNode::Q3DSAliasNode()
    : Q3DSNode(Q3DSNode::Alias)
{
}

void Q3DSAliasNode::setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags)
{
    Q3DSNode::setProperties(attrs, flags);
    setProps(attrs, flags);
}

void Q3DSAliasNode::applyPropertyChanges(const Q3DSPropertyChangeList &changeList)
{
    Q3DSNode::applyPropertyChanges(changeList);
    setProps(changeList, 0);
}

void Q3DSAliasNode::resolveReferences(Q3DSUipPresentation &pres)
{
    Q3DSNode::resolveReferences(pres);
    resolveRef(m_referencedNode_unresolved, Q3DSGraphObject::AnyObject, &m_referencedNode, pres);
}

Q3DSPropertyChange Q3DSAliasNode::setReferencedNode(Q3DSGraphObject *v)
{
    return createObjectRefSetter(MEMBER_PAIR(m_referencedNode), v, "referencednode");
}

template<typename V>
void Q3DSAliasNode::setProps(const V &attrs, PropSetFlags flags)
{
    const QString typeName = QStringLiteral("Alias");
    parseObjectRefProperty(attrs, flags, typeName, QStringLiteral("referencednode"), &m_referencedNode_unresolved);

    // Different default value.
    parseProperty(attrs, flags, typeName, QStringLiteral("name"), &m_name);
}

Q3DSUipPresentation::Q3DSUipPresentation()
    : d(new Q3DSUipPresentationData)
{
}

Q3DSUipPresentation::~Q3DSUipPresentation()
{
    delete d->scene;
    delete d->masterSlide;
}

void Q3DSUipPresentation::reset()
{
    delete d->scene;
    delete d->masterSlide;
    d.reset(new Q3DSUipPresentationData);
}

QString Q3DSUipPresentation::sourceFile() const
{
    return d->sourceFile;
}

QString Q3DSUipPresentation::name() const
{
    return d->name;
}

QString Q3DSUipPresentation::author() const
{
    return d->author;
}

QString Q3DSUipPresentation::company() const
{
    return d->company;
}

int Q3DSUipPresentation::presentationWidth() const
{
    return d->presentationWidth;
}

int Q3DSUipPresentation::presentationHeight() const
{
    return d->presentationHeight;
}

Q3DSUipPresentation::Rotation Q3DSUipPresentation::presentationRotation() const
{
    return d->presentationRotation;
}

bool Q3DSUipPresentation::maintainAspectRatio() const
{
    return d->maintainAspectRatio;
}

void Q3DSUipPresentation::setAuthor(const QString &author)
{
    d->author = author;
}

void Q3DSUipPresentation::setCompany(const QString &company)
{
    d->company = company;
}

void Q3DSUipPresentation::setPresentationWidth(int w)
{
    d->presentationWidth = w;
}

void Q3DSUipPresentation::setPresentationHeight(int h)
{
    d->presentationHeight = h;
}

void Q3DSUipPresentation::setPresentationRotation(Rotation r)
{
    d->presentationRotation = r;
}

void Q3DSUipPresentation::setMaintainAspectRatio(bool maintain)
{
    d->maintainAspectRatio = maintain;
}

Q3DSScene *Q3DSUipPresentation::scene() const
{
    return d->scene;
}

Q3DSSlide *Q3DSUipPresentation::masterSlide() const
{
    return d->masterSlide;
}

Q3DSGraphObject *Q3DSUipPresentation::getObject(const QByteArray &id) const
{
    return d->objects.value(id);
}

Q3DSGraphObject *Q3DSUipPresentation::getObjectByName(const QString &name) const
{
    for (auto it = d->objects.cbegin(), itEnd = d->objects.cend(); it != itEnd; ++it) {
        if ((*it)->name() == name)
            return *it;
    }
    return nullptr;
}

namespace  {
struct ClonedObject {
    Q3DSGraphObject *original;
    Q3DSGraphObject *clone;
};

bool isAliasDefinedProperty(const QByteArray &propertyName) {
    return (propertyName == QByteArrayLiteral("rotation") ||
            propertyName == QByteArrayLiteral("position") ||
            propertyName == QByteArrayLiteral("scale") ||
            propertyName == QByteArrayLiteral("piviot") ||
            propertyName == QByteArrayLiteral("opacity"));
}

QVector<ClonedObject> cloneObjects(Q3DSGraphObject *target,
                                   const QString &idPrefix,
                                   Q3DSUipPresentation *presentation,
                                   Q3DSGraphObject *parent,
                                   bool isRoot = true)
{
    QVector<ClonedObject> clonedObjects;

    if (!target || !presentation)
        return clonedObjects;

    Q3DSGraphObject *object = nullptr;
    QString id = idPrefix + QString::fromUtf8(target->id());
    if (target->type() == Q3DSGraphObject::Camera) {
        object = new Q3DSCameraNode;
    } else if (target->type() == Q3DSGraphObject::Light) {
        object = new Q3DSLightNode;
    } else if (target->type() == Q3DSGraphObject::Model) {
        object = new Q3DSModelNode;
    } else if (target->type() == Q3DSGraphObject::Group) {
        object = new Q3DSGroupNode;
    } else if (target->type() == Q3DSGraphObject::Component) {
        object = new Q3DSComponentNode;
    } else if (target->type() == Q3DSGraphObject::Text) {
        object = new Q3DSTextNode;
    } else if (target->type() == Q3DSGraphObject::DefaultMaterial) {
        object = new Q3DSDefaultMaterial;
    } else if (target->type() == Q3DSGraphObject::ReferencedMaterial) {
        object = new Q3DSReferencedMaterial;
    } else if (target->type() == Q3DSGraphObject::CustomMaterial) {
        object = new Q3DSCustomMaterialInstance;
    } else if (target->type() == Q3DSGraphObject::Behavior) {
        object = new Q3DSBehaviorInstance;
    } else if (target->type() == Q3DSGraphObject::Image) {
        object = new Q3DSImage;
    } else if (target->type() == Q3DSGraphObject::Alias) {
        // The editor should prevent this situation, but still possible
        qWarning("Q3DSUipPresentation: Alias nodes can not reference objects with Aliases '%s'", target->id().constData());
    } else {
        qWarning("Q3DSUipPresentation: Tried to clone an unhandled type in Alias '%s'", target->id().constData());
    }
    if (object) {
        Q3DSPropertyChangeList props;
        const auto propertyNames = target->propertyNames();
        const auto propertyValues = target->propertyValues();
        for (int i = 0; i < propertyNames.count() ; ++i) {
            const auto propertyName = propertyNames.at(i);
            const auto propertyValue = propertyValues.at(i);
            // for the root item, don't apply Transform and opacity properties
            // the properties from the Alias node is used for clones
            if (isRoot && isAliasDefinedProperty(propertyName))
                continue;
            props.append(Q3DSPropertyChange::fromVariant(QString::fromLatin1(propertyName), propertyValue));
        }
        object->applyPropertyChanges(props);
        object->setName(id);
        presentation->registerObject(id.toLatin1(), object);
        object->addDataInputControlledProperties(*target->dataInputControlledProperties());
        presentation->registerDataInputTarget(object);
        if (parent)
            parent->appendChildNode(object);
    }

    // Also create clones of children
    auto child = target->firstChild();
    while (child) {
        clonedObjects.append(cloneObjects(child, idPrefix, presentation, object, false));
        child = child->nextSibling();
    }

    ClonedObject clonedObject;
    clonedObject.original = target;
    clonedObject.clone = object;

    clonedObjects.append(clonedObject);

    // resolve any new references
    object->resolveReferences(*presentation);

    return clonedObjects;
}

void cloneStates(Q3DSAliasNode *alias,
                 const QVector<Q3DSSlide*> &rootSlides,
                 const QVector<ClonedObject> &clonedObjects,
                 Q3DSUipPresentation *presentation)
{
    Q_UNUSED(presentation)
    // The last object in the clonedObjects list is the root object for
    // the alias. We need to make sure that don't add any property changes
    // to the transfrom or opacity properties from the orginal item as these
    // are defined by the alias node itself.
    const auto rootObject = clonedObjects.last();

    // Figure out which slides contain alias references
    for (auto slide : rootSlides) {
        auto currentSlide = slide;
        while (currentSlide) {
            if (currentSlide->objects().contains(alias)) {
                // make sure the clonedOjbects get added to this slide as well
                for (const auto clonedObject : clonedObjects) {
                    currentSlide->addObject(clonedObject.clone);
                    // Properties
                    Q3DSPropertyChangeList *propertyChanges = currentSlide->propertyChanges().value(clonedObject.original, nullptr);
                    if (propertyChanges) {
                        // Create copy
                        Q3DSPropertyChangeList *propertyChangeListCopy = nullptr;
                        if (clonedObject.original == rootObject.original) {
                            // Copy changes that dont effect transform or opacity
                            propertyChangeListCopy = new Q3DSPropertyChangeList;
                            for (auto propertyChange : *propertyChanges) {
                                const QString propertyName = propertyChange.name().toString();
                                if (isAliasDefinedProperty(propertyName.toLatin1()))
                                    continue;
                                propertyChangeListCopy->append(Q3DSPropertyChange(propertyChange));
                            }
                        } else {
                            // Regular copy
                            propertyChangeListCopy = new Q3DSPropertyChangeList(*propertyChanges);
                        }
                        currentSlide->addPropertyChanges(clonedObject.clone, propertyChangeListCopy);
                    }
                    // Animations
                    for (const auto &animation : currentSlide->animations()) {
                        if (animation.target() == clonedObject.original) {
                            // make a copy and set its target to the clone object
                            if (clonedObject.original == rootObject.original) {
                                // If this is the root node and the animation effects
                                // a transform or opacity property, then we skip adding
                                // it to the copy
                                const auto property = animation.property();
                                if (property.contains(QStringLiteral("rotation")) ||
                                    property.contains(QStringLiteral("position")) ||
                                    property.contains(QStringLiteral("scale")) ||
                                    property.contains(QStringLiteral("piviot")) ||
                                    property.contains(QStringLiteral("opacity")))
                                    continue;
                            }

                            Q3DSAnimationTrack cloneAnimation(animation.type(), clonedObject.clone, animation.property());
                            cloneAnimation.setDynamic(animation.isDynamic());
                            cloneAnimation.setKeyFrames(animation.keyFrames());
                            currentSlide->addAnimation(cloneAnimation);
                        }
                    }

                    // Actions
                    for (auto action : currentSlide->actions()) {
                        if (action.owner == clonedObject.original) {
                            // ### make a copy who's owner is the clone object
                        }
                    }
                }
            }
            if (currentSlide == slide)
                currentSlide = static_cast<Q3DSSlide*>(currentSlide->firstChild());
            else
                currentSlide = static_cast<Q3DSSlide*>(currentSlide->nextSibling());
        }
    }
}
}

void Q3DSUipPresentation::resolveAliases()
{
    // For each Alias Node in the scene create a copy of the object subtree
    QVector<Q3DSAliasNode*> aliases;
    Q3DSUipPresentation::forAllObjectsOfType(scene(), Q3DSGraphObject::Alias, [&aliases](Q3DSGraphObject *node){
        aliases.append(static_cast<Q3DSAliasNode*>(node));
    });
    for (auto alias : aliases) {
        if (!alias->referencedNode())
            continue;

        QString aliasPrefix = QString::fromUtf8(alias->id()) + QStringLiteral("_");
        // Create referenceNode subtree clone
        auto clonedObjects = cloneObjects(alias->referencedNode(), aliasPrefix, this, alias);

        // Create state entities for clones

        // Create a list of root slides (master + component masters)
        QVector<Q3DSSlide *> rootSlides;
        rootSlides << masterSlide();
        Q3DSUipPresentation::forAllObjectsOfType(scene(), Q3DSGraphObject::Component, [&rootSlides](Q3DSGraphObject *object){
           auto component = static_cast<Q3DSComponentNode*>(object);
           if (component)
               rootSlides << component->masterSlide();
        });

        cloneStates(alias, rootSlides, clonedObjects, this);
    }

}

void Q3DSUipPresentation::updateObjectStateForSubTrees()
{
    forAllObjectsInSubTree(scene(), [](Q3DSGraphObject *obj) {
        if (obj->type() != Q3DSGraphObject::Layer)
            return;
        Q3DSLayerNode *layer = static_cast<Q3DSLayerNode *>(obj);
        if (!layer->sourcePath().isEmpty()) {
            // Sub-presentation, set all objects to be inactive
            forAllObjectsInSubTree(obj, [layer](Q3DSGraphObject *obj) {
                if (obj == layer)
                    return;
                obj->m_state = Q3DSGraphObject::Disabled;
            });
        }
    });
}

void Q3DSUipPresentation::addImplicitPropertyChanges()
{
    // TODO: There might be other cases where we need to generate the property changes,
    // this only deals with the start and end times for now.
    forAllObjectsInSubTree(scene(), [](Q3DSGraphObject *obj) {
        if ((obj->type() == Q3DSGraphObject::Slide && !obj->parent()) || obj->type() == Q3DSGraphObject::Component) {
            const bool isComponent = (obj->type() == Q3DSGraphObject::Component);
            Q3DSSlide *master = isComponent ? static_cast<Q3DSComponentNode *>(obj)->masterSlide() : static_cast<Q3DSSlide *>(obj);
            Q3DSSlide *slide = static_cast<Q3DSSlide *>(master->firstChild());
            while (slide) {
                for (auto o : master->objects()) {
                    if (!o->isNode())
                        continue;

                    auto propChanges = slide->propertyChanges();
                    const auto foundIt = propChanges.constFind(o);

                    struct StartAndEndTime {
                        qint32 hasStart = false;
                        qint32 hasEnd = false;
                        const qint32 defaultStart = 0;
                        const qint32 defaultEnd = 10000;
                    } startAndEndTime;

                    if (foundIt != propChanges.cend()) {
                        std::find_if(foundIt.value()->cbegin(), foundIt.value()->cend(), [&startAndEndTime](const Q3DSPropertyChange &change) {
                            if (startAndEndTime.hasStart && startAndEndTime.hasEnd)
                                return true;

                            if (change.name() == QLatin1String("starttime"))
                                startAndEndTime.hasStart = true;
                            else if (change.name() == QLatin1String("endtime"))
                                startAndEndTime.hasEnd = true;

                            return false;
                        });
                    }

                    // No prop change for object, then add default values 0s and 10s
                    if (!startAndEndTime.hasStart) {
                        const auto propChange = Q3DSPropertyChange::fromVariant(QLatin1String("starttime"), QVariant::fromValue(startAndEndTime.defaultStart));
                        if (foundIt != propChanges.constEnd()) {
                            auto propChangeList = slide->takePropertyChanges(o);
                            propChangeList->append(propChange);
                            slide->addPropertyChanges(o, propChangeList);
                        } else {
                            slide->addPropertyChanges(o, new Q3DSPropertyChangeList({propChange}));
                        }
                    }

                    if (!startAndEndTime.hasEnd) {
                        const auto propChange = Q3DSPropertyChange::fromVariant(QLatin1String("endtime"), QVariant::fromValue(startAndEndTime.defaultEnd));
                        if (foundIt != propChanges.constEnd()) {
                            auto propChangeList = slide->takePropertyChanges(o);
                            propChangeList->append(propChange);
                            slide->addPropertyChanges(o, propChangeList);
                        } else {
                            slide->addPropertyChanges(o, new Q3DSPropertyChangeList({propChange}));
                        }
                    }
                }
                slide = static_cast<Q3DSSlide *>(slide->nextSibling());
            }
        }
    });
}

QHash<QString, bool> &Q3DSUipPresentation::imageTransparencyHash()
{
    return m_imageTransparencyHash;
}

/*
    Maps a raw XML filename ref like "./Headphones/meshes/Headphones.mesh#1"
    onto a fully qualified filename that can be opened as-is (even if the uip
    is in qrc etc.), and also decodes the optional part index.
 */
QString Q3DSUipPresentation::assetFileName(const QString &xmlFileNameRef, int *part) const
{
    QString rawName = xmlFileNameRef;
    if (rawName.startsWith('#')) {
        // Can be a built-in primitive ref, like #Cube.
        if (part)
            *part = 1;
        return rawName;
    }

    if (rawName.contains('#')) {
        int pos = rawName.lastIndexOf('#');
        bool ok = false;
        int idx = rawName.mid(pos + 1).toInt(&ok);
        if (!ok) {
            Q3DSUtils::showMessage(QObject::tr("Invalid part index '%1'").arg(rawName));
            return QString();
        }
        if (part)
            *part = idx;
        rawName = rawName.left(pos);
    } else {
        // If no part is specified return -1 so the mesh parser can decide which
        // part is the best.  This will usually be 1 but for older versions
        // of the editor multi-meshes were used for revisions, and we would
        // need to return the last part in the list, not the first.
        if (part)
            *part = -1;
    }

    rawName.replace('\\', '/');
    if (rawName.startsWith(QStringLiteral("./")))
        rawName = rawName.mid(2);

    if (QFileInfo(rawName).isAbsolute())
        return rawName;

    QString path;
    if (d->sourceFile.isEmpty()) {
        path = QFileInfo(rawName).canonicalFilePath();
    } else {
        QString fn = QFileInfo(d->sourceFile).canonicalPath();
        fn += QLatin1Char('/');
        fn += rawName;
        path = QFileInfo(fn).absoluteFilePath();
    }
    return path;
}

void Q3DSUipPresentation::setSourceFile(const QString &s)
{
    d->sourceFile = s;
}

void Q3DSUipPresentation::setName(const QString &s)
{
    d->name = s;
}

void Q3DSUipPresentation::setScene(Q3DSScene *p)
{
    d->scene = p;
}

void Q3DSUipPresentation::setMasterSlide(Q3DSSlide *p)
{
    d->masterSlide = p;
}

void Q3DSUipPresentation::setLoadTime(qint64 ms)
{
    d->loadTime = ms;
}

void Q3DSUipPresentation::registerImageBuffer(const QString &sourcePath, bool hasTransparency)
{
    d->imageBuffers[sourcePath] = hasTransparency;
}

bool Q3DSUipPresentation::registerObject(const QByteArray &id, Q3DSGraphObject *p)
{
    if (d->objects.contains(id)) {
        qWarning("Q3DSUipPresentation: Multiple registrations for object id '%s'", id.constData());
        return false;
    }
    p->m_id = id;
    d->objects[id] = p;
    return true;
}

void Q3DSUipPresentation::unregisterObject(const QByteArray &id)
{
    d->objects.remove(id);
}

template<typename T, typename ParserT>
bool loadMeta(const QByteArray &id, const QString &assetFilename, QHash<QByteArray, T> *dst)
{
    ParserT p;
    bool ok = false;
    T eff = p.parse(assetFilename, &ok);
    if (!ok) {
        qWarning("Failed to parse metadata %s", qPrintable(assetFilename));
        return false;
    }
    dst->insert(id, eff);
    return true;
}

bool Q3DSUipPresentation::loadCustomMaterial(const QByteArray &id, const QString &assetFilename)
{
    return loadMeta<Q3DSCustomMaterial, Q3DSCustomMaterialParser>(id, assetFilename, &d->customMaterials);
}

Q3DSCustomMaterial Q3DSUipPresentation::customMaterial(const QString &idOrFilename)
{
    const QByteArray key = idOrFilename.toUtf8();
    if (!key.startsWith('#') && !d->customMaterials.contains(key))
        loadCustomMaterial(key, idOrFilename);

    return d->customMaterials.value(key);
}

bool Q3DSUipPresentation::loadEffect(const QByteArray &id, const QString &assetFilename)
{
    return loadMeta<Q3DSEffect, Q3DSEffectParser>(id, assetFilename, &d->effects);
}

Q3DSEffect Q3DSUipPresentation::effect(const QString &idOrFilename)
{
    // We either have an id (starting with #) for an already loaded effect (when
    // defined in .uip), or a filename which may or may not be already parsed.

    const QByteArray key = idOrFilename.toUtf8();
    if (!key.startsWith('#') && !d->effects.contains(key))
        loadEffect(key, idOrFilename);

    return d->effects.value(key);
}

bool Q3DSUipPresentation::loadBehavior(const QByteArray &id, const QString &assetFilename)
{
    return loadMeta<Q3DSBehavior, Q3DSBehaviorParser>(id, assetFilename, &d->behaviors);
}

Q3DSBehavior Q3DSUipPresentation::behavior(const QString &idOrFilename)
{
    const QByteArray key = idOrFilename.toUtf8();
    if (!key.startsWith('#') && !d->behaviors.contains(key))
        loadBehavior(key, idOrFilename);

    return d->behaviors.value(key);
}

MeshList Q3DSUipPresentation::mesh(const QString &assetFilename, int part)
{
    Q3DSUipPresentationData::MeshId id(assetFilename, part);
    if (d->meshes.contains(id))
        return d->meshes.value(id);

    QElapsedTimer t;
    t.start();
    MeshList m = Q3DSMeshLoader::loadMesh(assetFilename, part, false);
    qCDebug(lcPerf, "Mesh %s loaded in %lld ms", qPrintable(assetFilename), t.elapsed());
    d->meshesLoadTime += t.elapsed();

    d->meshes.insert(id, m);
    return m;
}

const Q3DSUipPresentation::ImageBufferMap &Q3DSUipPresentation::imageBuffer() const
{
    return d->imageBuffers;
}

qint64 Q3DSUipPresentation::loadTimeMsecs() const
{
    return d->loadTime;
}

qint64 Q3DSUipPresentation::meshesLoadTimeMsecs() const
{
    return d->meshesLoadTime;
}

void Q3DSUipPresentation::setDataInputEntries(const Q3DSDataInputEntry::Map *entries)
{
    d->dataInputEntries = entries;
}

const Q3DSDataInputEntry::Map *Q3DSUipPresentation::dataInputEntries() const
{
    return d->dataInputEntries;
}

// Each Q3DSGraphObject has a table of the properties that are controlled
// by data input. However, walking the scene graph on every data input
// value change is not ideal. Therefore we maintain a central map in the
// presentation as well.

const Q3DSUipPresentation::DataInputMap *Q3DSUipPresentation::dataInputMap() const
{
    return &d->dataInputMap;
}

void Q3DSUipPresentation::registerDataInputTarget(Q3DSGraphObject *obj)
{
    auto di = obj->dataInputControlledProperties();
    for (auto it = di->cbegin(); it != di->cend(); ++it)
        d->dataInputMap.insert(it.key(), obj);
}

void Q3DSUipPresentation::removeDataInputTarget(Q3DSGraphObject *obj)
{
    for (auto it = d->dataInputMap.begin(); it != d->dataInputMap.end(); ++it) {
        if (it.value() == obj)
            d->dataInputMap.erase(it);
    }
}

void Q3DSUipPresentation::notifyPropertyChanges(const Q3DSSlide::PropertyChanges &changeList) const
{
    for (auto it = changeList.cbegin(), ite = changeList.cend(); it != ite; ++it)
        it.key()->notifyPropertyChanges(*it.value());
}

void Q3DSUipPresentation::applyPropertyChanges(const Q3DSSlide::PropertyChanges &changeList) const
{
    for (auto it = changeList.cbegin(), ite = changeList.cend(); it != ite; ++it) {
        for (auto change = it.value()->begin(); change != it.value()->end(); change++)
            qCDebug(lcUipProp) << "\t" << it.key() << "applying property change:" << change->name() << change->value();

        it.key()->applyPropertyChanges(*it.value());
    }
}

void Q3DSUipPresentation::applySlidePropertyChanges(Q3DSSlide *slide) const
{
    const auto &changeList = slide->propertyChanges();
    qCDebug(lcUip, "Applying %d property changes from slide %s", changeList.count(), slide->id().constData());
    applyPropertyChanges(changeList);
    notifyPropertyChanges(changeList);
}

void Q3DSUipPresentation::forAllObjects(Q3DSGraphObject *root,
                                        std::function<void(Q3DSGraphObject *)> f)
{
    Q3DSGraphObject *obj = root;
    while (obj) {
        f(obj);
        forAllObjects(obj->firstChild(), f);
        obj = obj->nextSibling();
    }
}

static void forAllObjectsInSubTree_helper(Q3DSGraphObject *obj,
                                          std::function<void(Q3DSGraphObject *)> f,
                                          bool isSubTreeRoot)
{
    while (obj) {
        f(obj);
        forAllObjectsInSubTree_helper(obj->firstChild(), f, false);
        if (isSubTreeRoot)
            break;
        else
            obj = obj->nextSibling();
    }
}

void Q3DSUipPresentation::forAllObjectsInSubTree(Q3DSGraphObject *root,
                                                 std::function<void(Q3DSGraphObject *)> f)
{
    forAllObjectsInSubTree_helper(root, f, true);
}

void Q3DSUipPresentation::forAllObjectsOfType(Q3DSGraphObject *root,
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

void Q3DSUipPresentation::forAllNodes(Q3DSGraphObject *root,
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

void Q3DSUipPresentation::forAllLayers(Q3DSScene *scene, std::function<void(Q3DSLayerNode *)> f, bool reverse)
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
        }
        obj = obj->nextSibling();
    }
    if (reverse) {
        for (auto it = layers.crbegin(), ite = layers.crend(); it != ite; ++it)
            f(*it);
    }
}

void Q3DSUipPresentation::forAllModels(Q3DSGraphObject *obj,
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

Q3DSGraphObject *Q3DSUipPresentation::newObject(const char *type, const QByteArray &id)
{
    Q3DSGraphObject *obj = nullptr;

    if (type == QByteArrayLiteral("Scene"))
        obj = newObject<Q3DSScene>(id);
    else if (type == QByteArrayLiteral("Slide"))
        obj = newObject<Q3DSSlide>(id);
    else if (type == QByteArrayLiteral("Image"))
        obj = newObject<Q3DSImage>(id);
    else if (type == QByteArrayLiteral("DefaultMaterial"))
        obj = newObject<Q3DSDefaultMaterial>(id);
    else if (type == QByteArrayLiteral("ReferencedMaterial"))
        obj = newObject<Q3DSReferencedMaterial>(id);
    else if (type == QByteArrayLiteral("CustomMaterial"))
        obj = newObject<Q3DSCustomMaterialInstance>(id);
    else if (type == QByteArrayLiteral("Effect"))
        obj = newObject<Q3DSEffectInstance>(id);
    else if (type == QByteArrayLiteral("Behavior"))
        obj = newObject<Q3DSBehaviorInstance>(id);
    else if (type == QByteArrayLiteral("Layer"))
        obj = newObject<Q3DSLayerNode>(id);
    else if (type == QByteArrayLiteral("Camera"))
        obj = newObject<Q3DSCameraNode>(id);
    else if (type == QByteArrayLiteral("Light"))
        obj = newObject<Q3DSLightNode>(id);
    else if (type == QByteArrayLiteral("Model"))
        obj = newObject<Q3DSModelNode>(id);
    else if (type == QByteArrayLiteral("Group"))
        obj = newObject<Q3DSGroupNode>(id);
    else if (type == QByteArrayLiteral("Text"))
        obj = newObject<Q3DSTextNode>(id);
    else if (type == QByteArrayLiteral("Component"))
        obj = newObject<Q3DSComponentNode>(id);
    else if (type == QByteArrayLiteral("Alias"))
        obj = newObject<Q3DSAliasNode>(id);

    return obj;
}

QT_END_NAMESPACE
