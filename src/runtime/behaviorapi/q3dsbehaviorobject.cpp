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

float Q3DSBehaviorObject::getDeltaTime()
{
    return 0;
}

// Object reference format:
//
// (presentationName:)?(parent|this|(Scene|Slide)(\..)*|(.)*)
//
// An empty string or "this" refers to the parent of the behavior instance
// (i.e. the object to which the behavior was attached to in the editor). This
// is the most common use case.
//
// presentationName is either "main" for the main presentation, or the id from
// the .uia for sub-presentations.
//
// For non-unique names one would specify a full path like "Scene.Layer.Camera".
// "Scene" refers to the scene object, "Slide" to the master slide.
//
// For accessing objects that were renamed to a unique name in the editor, we
// also allow a simple flat reference like "MyCamera" since relying on absolute
// paths is just silly and not necessary at all.

Q3DSGraphObject *Q3DSBehaviorObject::findObject(const QString &attribute)
{
    Q_ASSERT(m_behaviorInstance);

    QString attr = attribute;
    Q3DSUipPresentation *pres = m_presentation;
    if (attribute.contains(QLatin1Char(':'))) {
        const QStringList presentationPathPair = attribute.split(QLatin1Char(':'), QString::SkipEmptyParts);
        if (presentationPathPair.count() < 2)
            return nullptr;
        pres = m_engine->presentationByName(presentationPathPair[0]);
        attr = presentationPathPair[1];
    }

    bool firstElem = true;
    Q3DSGraphObject *obj = m_behaviorInstance->parent();
    for (const QString &s : attr.split(QLatin1Char('.'), QString::SkipEmptyParts)) {
        if (firstElem) {
            firstElem = false;
            if (s == QStringLiteral("parent"))
                obj = m_behaviorInstance->parent() ? m_behaviorInstance->parent()->parent() : nullptr;
            else if (s == QStringLiteral("this"))
                obj = m_behaviorInstance->parent();
            else if (s == QStringLiteral("Scene"))
                obj = pres->scene();
            else if (s == QStringLiteral("Slide"))
                obj = pres->masterSlide();
            else
                obj = pres->objectByName(s);
        } else {
            if (!obj)
                return nullptr;
            if (s == QStringLiteral("parent")) {
                obj = obj->parent();
            } else {
                for (Q3DSGraphObject *child = obj->firstChild(); child; child = child->nextSibling()) {
                    if (child->name() == s) {
                        obj = child;
                        break;
                    }
                }
            }
        }
    }

    return obj;
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
    Q3DSGraphObject *obj = findObject(handle);
    if (!obj)
        return 0;

    if (attribute.contains(QLatin1Char('.'))) {
        // for example, rotation.x
        const QStringList vecCompRef = attribute.split(QLatin1Char('.'), QString::SkipEmptyParts);
        if (vecCompRef.count() != 2)
            return 0;
        const int idx = obj->propertyNames().indexOf(vecCompRef[0]);
        if (idx < 0)
            return 0;
        const QVariant value = obj->propertyValues().at(idx);
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

    const int idx = obj->propertyNames().indexOf(attribute);
    if (idx < 0)
        return 0;
    const QVariant value = obj->propertyValues().at(idx);
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
    Q3DSGraphObject *obj = findObject(handle);
    if (!obj)
        return;

    Q3DSPropertyChangeList cl;
    if (attribute.contains(QLatin1Char('.'))) {
        // for example, setAttribute("rotation.y", 45)
        const QStringList vecCompRef = attribute.split(QLatin1Char('.'), QString::SkipEmptyParts);
        if (vecCompRef.count() == 2) {
            const int idx = obj->propertyNames().indexOf(vecCompRef[0]);
            if (idx >= 0) {
                QVariant newValue = obj->propertyValues().at(idx);
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
                cl.append(Q3DSPropertyChange::fromVariant(attribute, newValue));
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

void Q3DSBehaviorObject::registerForEvent(const QString &handle, const QString &event, const QJSValue &function)
{
    Q_UNUSED(handle); Q_UNUSED(event); Q_UNUSED(function);
}

void Q3DSBehaviorObject::unregisterForEvent(const QString &event)
{
    unregisterForEvent(QString(), event);
}

void Q3DSBehaviorObject::unregisterForEvent(const QString &handle, const QString &event)
{
    Q_UNUSED(handle); Q_UNUSED(event);
}

QVector2D Q3DSBehaviorObject::getMousePosition()
{
    return QVector2D();
}

QMatrix4x4 Q3DSBehaviorObject::calculateGlobalTransform()
{
    return calculateGlobalTransform(QString());
}

QMatrix4x4 Q3DSBehaviorObject::calculateGlobalTransform(const QString &handle)
{
    Q_UNUSED(handle);
    return QMatrix4x4();
}

QVector3D Q3DSBehaviorObject::lookAt(const QVector3D &target)
{
    Q_UNUSED(target);
    return QVector3D();
}

QVector3D Q3DSBehaviorObject::matrixToEuler(const QMatrix4x4 &matrix)
{
    Q_UNUSED(matrix);
    return QVector3D();
}

QString Q3DSBehaviorObject::getParent()
{
    return getParent(QString());
}

QString Q3DSBehaviorObject::getParent(const QString &handle)
{
    Q_UNUSED(handle);
    return QString();
}

void Q3DSBehaviorObject::setDataInputValue(const QString &name, const QVariant &value)
{
    m_engine->setDataInputValue(name, value);
}

QT_END_NAMESPACE
