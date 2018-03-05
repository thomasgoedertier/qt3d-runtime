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

#ifndef Q3DSBEHAVIOROBJECT_P_H
#define Q3DSBEHAVIOROBJECT_P_H

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

#include <QObject>
#include <QVariant>
#include <QJSValue>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>

QT_BEGIN_NAMESPACE

class Q3DSBehaviorObject : public QObject
{
    Q_OBJECT

public:
    Q3DSBehaviorObject(QObject *parent = nullptr);

    Q_INVOKABLE float getDeltaTime();
    Q_INVOKABLE float getAttribute(const QString &attribute);
    Q_INVOKABLE void setAttribute(const QString &attribute, const QVariant &value);
    Q_INVOKABLE void setAttribute(const QString &handle, const QString &attribute,
                                  const QVariant &value);
    Q_INVOKABLE void fireEvent(const QString &event);
    Q_INVOKABLE void registerForEvent(const QString &event, const QJSValue &function);
    Q_INVOKABLE void registerForEvent(const QString &handle, const QString &event,
                                      const QJSValue &function);
    Q_INVOKABLE void unregisterForEvent(const QString &event);
    Q_INVOKABLE void unregisterForEvent(const QString &handle, const QString &event);
    Q_INVOKABLE QVector2D getMousePosition();
    Q_INVOKABLE QMatrix4x4 calculateGlobalTransform(const QString &handle = QString());
    Q_INVOKABLE QVector3D lookAt(const QVector3D &target);
    Q_INVOKABLE QVector3D matrixToEuler(const QMatrix4x4 &matrix);
    Q_INVOKABLE QString getParent(const QString &handle = QString());
    Q_REVISION(1) Q_INVOKABLE void setDataInputValue(const QString &name, const QVariant &value);

signals:
    void initialize();
    void activate();
    void update();
    void deactivate();

private:
};

QT_END_NAMESPACE

#endif // Q3DSBEHAVIOROBJECT_P_H
