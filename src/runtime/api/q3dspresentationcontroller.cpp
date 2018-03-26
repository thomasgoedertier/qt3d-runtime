/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
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
#include "q3dsengine_p.h"

QT_BEGIN_NAMESPACE

void Q3DSPresentationController::initializePresentationController(Q3DSEngine *engine, Q3DSPresentation *presentation)
{
    m_pcEngine = engine;

    QObject::connect(engine, &Q3DSEngine::customSignalEmitted, engine,
                     [engine, presentation](Q3DSGraphObject *obj, const QString &name)
    {
        emit presentation->customSignalEmitted(engine->makePath(obj), name);
    });
    QObject::connect(engine, &Q3DSEngine::slideEntered, engine,
                     [engine, presentation](Q3DSGraphObject *context, int index, const QString &name)
    {
        emit presentation->slideEntered(engine->makePath(context), index, name);
    });
    QObject::connect(engine, &Q3DSEngine::slideExited, engine,
                     [engine, presentation](Q3DSGraphObject *context, int index, const QString &name)
    {
        emit presentation->slideExited(engine->makePath(context), index, name);
    });
}

void Q3DSPresentationController::handlePresentationKeyPressEvent(QKeyEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleKeyPressEvent(e);
}

void Q3DSPresentationController::handlePresentationKeyReleaseEvent(QKeyEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleKeyReleaseEvent(e);
}

void Q3DSPresentationController::handlePresentationMousePressEvent(QMouseEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleMousePressEvent(e);
}

void Q3DSPresentationController::handlePresentationMouseMoveEvent(QMouseEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleMouseMoveEvent(e);
}

void Q3DSPresentationController::handlePresentationMouseReleaseEvent(QMouseEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleMouseReleaseEvent(e);
}

void Q3DSPresentationController::handlePresentationMouseDoubleClickEvent(QMouseEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleMouseDoubleClickEvent(e);
}

#if QT_CONFIG(wheelevent)
void Q3DSPresentationController::handlePresentationWheelEvent(QWheelEvent *e)
{
    if (m_pcEngine)
        m_pcEngine->handleWheelEvent(e);
}
#endif

void Q3DSPresentationController::handleDataInputValue(const QString &name, const QVariant &value)
{
    if (m_pcEngine)
        m_pcEngine->setDataInputValue(name, value);
}

void Q3DSPresentationController::handleFireEvent(const QString &elementPath, const QString &eventName)
{
    if (!m_pcEngine)
        return;

    // Assume that the path is in the main presentation when no explicit
    // presentation is specified in the path.
    Q3DSUipPresentation *pres = m_pcEngine->presentation(0);
    Q3DSGraphObject *target = m_pcEngine->findObjectByNameOrPath(nullptr, pres, elementPath, &pres);
    // pres is now the actual presentation (which is important to know
    // since the event queuing needs the scenemanager).
    if (target)
        m_pcEngine->fireEvent(target, pres, eventName);
}

void Q3DSPresentationController::handleGoToTime(const QString &elementPath, float timeSeconds)
{
    if (!m_pcEngine)
        return;

    Q3DSUipPresentation *pres = m_pcEngine->presentation(0);
    Q3DSGraphObject *context = m_pcEngine->findObjectByNameOrPath(nullptr, pres, elementPath, &pres);
    if (context)
        m_pcEngine->goToTime(context, pres, timeSeconds * 1000);
}

void Q3DSPresentationController::handleGoToSlideByName(const QString &elementPath, const QString &name)
{
    if (!m_pcEngine)
        return;

    Q3DSUipPresentation *pres = m_pcEngine->presentation(0);
    Q3DSGraphObject *context = m_pcEngine->findObjectByNameOrPath(nullptr, pres, elementPath, &pres);
    if (context)
        m_pcEngine->goToSlideByName(context, pres, name);
}

void Q3DSPresentationController::handleGoToSlideByIndex(const QString &elementPath, int index)
{
    if (!m_pcEngine)
        return;

    Q3DSUipPresentation *pres = m_pcEngine->presentation(0);
    Q3DSGraphObject *context = m_pcEngine->findObjectByNameOrPath(nullptr, pres, elementPath, &pres);
    if (context)
        m_pcEngine->goToSlideByIndex(context, pres, index);
}

void Q3DSPresentationController::handleGoToSlideByDirection(const QString &elementPath, bool next, bool wrap)
{
    if (!m_pcEngine)
        return;

    Q3DSUipPresentation *pres = m_pcEngine->presentation(0);
    Q3DSGraphObject *context = m_pcEngine->findObjectByNameOrPath(nullptr, pres, elementPath, &pres);
    if (context)
        m_pcEngine->goToSlideByDirection(context, pres, next, wrap);
}

QVariant Q3DSPresentationController::handleGetAttribute(const QString &elementPath, const QString &attribute)
{
    if (!m_pcEngine)
        return QVariant();

    Q3DSGraphObject *obj = m_pcEngine->findObjectByNameOrPath(nullptr, m_pcEngine->presentation(0), elementPath);
    if (!obj) {
        qWarning("No such object %s", qPrintable(elementPath));
        return QVariant();
    }

    return obj->propertyValue(attribute);
}

void Q3DSPresentationController::handleSetAttribute(const QString &elementPath, const QString &attributeName, const QVariant &value)
{
    if (!m_pcEngine)
        return;

    Q3DSGraphObject *obj = m_pcEngine->findObjectByNameOrPath(nullptr, m_pcEngine->presentation(0), elementPath);
    if (!obj) {
        qWarning("No such object %s", qPrintable(elementPath));
        return;
    }

    // this handles QVector2D and QVector3D as expected for vec2 and vec3 properties
    Q3DSPropertyChangeList cl { Q3DSPropertyChange::fromVariant(attributeName, value) };
    obj->applyPropertyChanges(cl);
    obj->notifyPropertyChanges(cl);
}

void Q3DSPresentationController::handleSetProfileUiVisible(bool visible, float scale)
{
    if (m_pcEngine) {
        m_pcEngine->setProfileUiVisible(visible);
        m_pcEngine->configureProfileUi(scale);
    }
}

QT_END_NAMESPACE
