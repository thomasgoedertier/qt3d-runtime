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

#include "q3dsbehaviorobject_p.h"
#include "q3dsengine_p.h"
#include <QMetaMethod>
#include <QQuaternion>
#include <qmath.h>
#include <functional>

QT_BEGIN_NAMESPACE

Q3DSBehaviorObject::Q3DSBehaviorObject(QObject *parent)
    : QObject(parent)
{
}

void Q3DSBehaviorObject::init(Q3DSEngine *engine,
                              Q3DSUipPresentation *presentation,
                              Q3DSBehaviorInstance *behaviorInstance)
{
    m_engine = engine;
    m_presentation = presentation;
    m_behaviorInstance = behaviorInstance;
}

void Q3DSBehaviorObject::prepareUpdate(float dt)
{
    m_deltaTime = dt;
}

void Q3DSBehaviorObject::call(const QString &function)
{
    const QString normalized = function + QLatin1String("()");
    const int idx = metaObject()->indexOfMethod(normalized.toUtf8().constData());
    if (idx >= 0)
        metaObject()->method(idx).invoke(this);
}

float Q3DSBehaviorObject::getDeltaTime()
{
    return m_deltaTime;
}

QVariant Q3DSBehaviorObject::getAttribute(const QString &attribute)
{
    return getAttribute(QString(), attribute);
}

// Note that this API does not work with Qt.vector2d or such. Vectors and
// colors are represented as "x y z" strings. In practice this is not an issue
// since scripts are expected to access these per-component, e.g.
// getAttribute("rotation.x") or getAttribute("backgroundcolor.r")

QVariant Q3DSBehaviorObject::getAttribute(const QString &handle, const QString &attribute)
{
    Q3DSGraphObject *obj = m_engine->findObjectByHashIdOrNameOrPath(m_behaviorInstance->parent(), m_presentation, handle);
    if (!obj) {
        qWarning("getAttribute: Invalid object reference %s", qPrintable(handle));
        return 0;
    }

    if (attribute.contains(QLatin1Char('.'))) {
        // for example, rotation.x
        const QStringList vecCompRef = attribute.split(QLatin1Char('.'), QString::SkipEmptyParts);
        if (vecCompRef.count() != 2)
            return 0;
        const QVariant value = obj->propertyValue(vecCompRef[0]);
        switch (value.type()) {
        case QVariant::Vector2D:
            if (vecCompRef[1] == QStringLiteral("x"))
                return value.value<QVector2D>().x();
            else if (vecCompRef[1] == QStringLiteral("y"))
                return value.value<QVector2D>().y();
            return 0;
        case QVariant::Vector3D:
            if (vecCompRef[1] == QStringLiteral("x"))
                return value.value<QVector3D>().x();
            else if (vecCompRef[1] == QStringLiteral("y"))
                return value.value<QVector3D>().y();
            else if (vecCompRef[1] == QStringLiteral("z"))
                return value.value<QVector3D>().z();
            return 0;
        case QVariant::Color:
            if (vecCompRef[1] == QStringLiteral("x") || vecCompRef[1] == QStringLiteral("r"))
                return value.value<QColor>().redF();
            else if (vecCompRef[1] == QStringLiteral("y") || vecCompRef[1] == QStringLiteral("g"))
                return value.value<QColor>().greenF();
            else if (vecCompRef[1] == QStringLiteral("z") || vecCompRef[1] == QStringLiteral("b"))
                return value.value<QColor>().blueF();
            return 0;
        default:
            return 0;
        }
    }

    const QVariant value = obj->propertyValue(attribute);
    if (value.type() == QVariant::Vector2D || value.type() == QVariant::Vector3D || value.type() == QVariant::Color)
        return Q3DS::convertFromVariant(value);
    return value;
}

void Q3DSBehaviorObject::setAttribute(const QString &attribute, const QVariant &value)
{
    setAttribute(QString(), attribute, value);
}

void Q3DSBehaviorObject::setAttribute(const QString &handle, const QString &attribute, const QVariant &value)
{
    Q3DSGraphObject *obj = m_engine->findObjectByHashIdOrNameOrPath(m_behaviorInstance->parent(), m_presentation, handle);
    if (!obj) {
        qWarning("setAttribute: Invalid object reference %s", qPrintable(handle));
        return;
    }

    Q3DSPropertyChangeList cl;
    if (attribute.contains(QLatin1Char('.'))) {
        // for example, setAttribute("rotation.y", 45)
        const QStringList vecCompRef = attribute.split(QLatin1Char('.'), QString::SkipEmptyParts);
        if (vecCompRef.count() == 2) {
            QVariant newValue = obj->propertyValue(vecCompRef[0]);
            if (!newValue.isNull()) {
                switch (newValue.type()) {
                case QVariant::Vector2D:
                {
                    QVector2D v = newValue.value<QVector2D>();
                    if (vecCompRef[1] == QStringLiteral("x"))
                        v.setX(value.toFloat());
                    else if (vecCompRef[1] == QStringLiteral("y"))
                        v.setY(value.toFloat());
                    newValue = v;
                }
                    break;
                case QVariant::Vector3D:
                {
                    QVector3D v = newValue.value<QVector3D>();
                    if (vecCompRef[1] == QStringLiteral("x"))
                        v.setX(value.toFloat());
                    else if (vecCompRef[1] == QStringLiteral("y"))
                        v.setY(value.toFloat());
                    else if (vecCompRef[1] == QStringLiteral("z"))
                        v.setZ(value.toFloat());
                    newValue = v;
                }
                    break;
                case QVariant::Color:
                {
                    QColor v = newValue.value<QColor>();
                    if (vecCompRef[1] == QStringLiteral("x") || vecCompRef[1] == QStringLiteral("r"))
                        v.setRedF(value.toFloat());
                    else if (vecCompRef[1] == QStringLiteral("y") || vecCompRef[1] == QStringLiteral("g"))
                        v.setGreenF(value.toFloat());
                    else if (vecCompRef[1] == QStringLiteral("z") || vecCompRef[1] == QStringLiteral("b"))
                        v.setBlueF(value.toFloat());
                    newValue = v;
                }
                    break;
                default:
                    break;
                }
                cl.append(Q3DSPropertyChange::fromVariant(vecCompRef[0], newValue));
            }
        }
    } else {
        // for example, setAttribute("rotation", "0 45 0")
        cl.append(Q3DSPropertyChange(attribute, value.toString()));
    }
    obj->applyPropertyChanges(cl);
    obj->notifyPropertyChanges(cl);
}

void Q3DSBehaviorObject::fireEvent(const QString &event)
{
    Q_UNUSED(event);
}

void Q3DSBehaviorObject::registerForEvent(const QString &event, const QJSValue &function)
{
    registerForEvent(QString(), event, function);
}

void Q3DSBehaviorObject::eventHandler(const Q3DSGraphObject::Event &ev)
{
    EventDef e(ev.target, ev.event);
    auto it = m_eventHandlers.find(e);
    if (it != m_eventHandlers.end())
        it->function.call();
}

void Q3DSBehaviorObject::registerForEvent(const QString &handle, const QString &event, const QJSValue &function)
{
    Q3DSGraphObject *obj = m_engine->findObjectByHashIdOrNameOrPath(m_behaviorInstance->parent(), m_presentation, handle);
    if (!obj) {
        qWarning("registerForEvent: Invalid object reference %s", qPrintable(handle));
        return;
    }

    if (!function.isCallable()) {
        qWarning("registerForEvent: Function is not callable");
        return;
    }

    EventDef e(obj, event);
    if (m_eventHandlers.contains(e)) {
        qWarning("registerForEvent: Already registered a handler for event %s on %s",
                 qPrintable(event), handle.isEmpty() ? "this" : qPrintable(handle));
        return;
    }

    EventHandlerData d;
    d.function = function;
    d.callbackId = obj->addEventHandler(event, std::bind(&Q3DSBehaviorObject::eventHandler, this, std::placeholders::_1));
    m_eventHandlers.insert(e, d);
}

void Q3DSBehaviorObject::unregisterForEvent(const QString &event)
{
    unregisterForEvent(QString(), event);
}

void Q3DSBehaviorObject::unregisterForEvent(const QString &handle, const QString &event)
{
    Q3DSGraphObject *obj = m_engine->findObjectByHashIdOrNameOrPath(m_behaviorInstance->parent(), m_presentation, handle);
    if (!obj) {
        qWarning("unregisterForEvent: Invalid object reference %s", qPrintable(handle));
        return;
    }

    EventDef e(obj, event);
    auto it = m_eventHandlers.find(e);
    if (it != m_eventHandlers.end()) {
        obj->removeEventHandler(event, it->callbackId);
        m_eventHandlers.erase(it);
    }
}

void Q3DSBehaviorObject::setDataInputValue(const QString &name, const QVariant &value)
{
    m_engine->setDataInputValue(name, value);
}

/*!
    \qmltype Behavior
    \inqmlmodule QtStudio3D
    \ingroup 3dstudioruntime2

    \brief Technology Preview Behavior Integration

    This is a technology preview (API may change in upcoming version) of how
    Qt 3D Studio could support writing custom behavior scripts using QML and
    JavaScript. It enables interacting with the runtime using the Behavior
    QML class exposed to each behavior script.

    In QML behavior script, the integration to Qt 3D Studio is established by using
    the metadata tag system similar to the \l {file-formats-effects.html}{effect}
    and \l {file-formats-material.html}{material} files.

    \badcode
[[
<Property name="somePropertyName" ... />

<Handler name="someHandlerName" ... />

<Event name="onSomeEvent" ... />
...
]]
    \endcode

    Secondly, the QML behavior script needs access to the QML module.
    \badcode
import QtStudio3D.Behavior 2.0
    \endcode

    Finally, the Behavior type needs to be implemented in the qml script.
    \badcode
Behavior {
    id: mybehavior

    function onInitialize() {
        ...
    }

    function onActivate() {
        ...
    }

    function onUpdate() {
        ...
    }

    function onDeactivate() {
        ...
    }

    function someHandlerName() {
        ...
        fireEvent("onSomeEvent")
    }
}
    \endcode
*/

/*!
    \qmlmethod float Behavior::getDeltaTime()

    Returns the delta time between this and previous frame in milliseconds.
*/

/*!
    \qmlmethod var Behavior::getAttribute(string attribute)

    Returns the value of the given \a attribute.
*/

/*!
    \qmlmethod var Behavior::getAttribute(string handle, string attribute)

    Returns the value of the given \a attribute for a given \a handle.
*/

/*!
    \qmlmethod void Behavior::setAttribute(string attribute, var value)

    Sets the \a value of the given \a attribute.
*/

/*!
    \qmlmethod void Behavior::setAttribute(string handle, string attribute, var value)

    Sets the \a value of the given \a attribute for a given \a handle.
*/

/*!
    \qmlmethod void Behavior::fireEvent(string event)

    Fires the given \a event.
*/

/*!
    \qmlmethod void Behavior::registerForEvent(string event, QJSValue function)

    Registers the script for an \a event with the handler \a function.
*/

/*!
    \qmlmethod void Behavior::registerForEvent(string handle, string event, QJSValue function)

    Registers the script for an \a event with the handler \a function for a given \a handle.
*/

/*!
    \qmlmethod void Behavior::unregisterForEvent(string event)

    Unregisters the script from an \a event.
*/

/*!
    \qmlmethod void Behavior::unregisterForEvent(string handle, string event)

    Unregisters the script from an \a event for a given \a handle.
*/

/*!
    \qmlmethod void Behavior::setDataInputValue(string name, variant value)

    Sets the \a value of the data input identified with the \a name.
 */

/*!
    \qmlsignal void Behavior::onInitialize()

    This signal is emitted when the script becomes active the first time.
    If multiple behaviors match this, the signal for parent elements will
    occur before their children/descendants. \note Each behavior will
    only have its \c{onInitialize} signaled once, even if it is deactivated and
    later reactivated.
 */

/*!
    \qmlsignal void Behavior::onActivate()

    This signal is emitted when the script becomes active.
    Any behaviors which were not active last frame that are active
    this frame will have their \c{onActivate} signaled. If
    multiple behaviors match this, the signal for parent elements will
    occur before their descendants.
 */

/*!
    \qmlsignal void Behavior::onDeactivate()

    This signal is emitted when the script becomes inactive.
    Any behaviors which were active last frame that are not active
    this frame will have their \c{onDeactivate} signaled. If
    multiple behaviors match this, the signal for parent elements will
    occur before their descendants.
 */

/*!
    \qmlsignal void Behavior::onUpdate()

    This signal is emitted on each frame when the script is active.
    Any behaviors that are active this frame will have their
    \c{onUpdate} signaled. If multiple behaviors match this, the signal
    for parent elements will occur before their descendants.
 */

QT_END_NAMESPACE
