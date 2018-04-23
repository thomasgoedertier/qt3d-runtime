/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef Q3DSELEMENT_H
#define Q3DSELEMENT_H

#include <Qt3DStudioRuntime2/q3dsruntimeglobal.h>
#include <QtCore/qobject.h>
#include <QtCore/qvariant.h>

QT_BEGIN_NAMESPACE

class Q3DSElementPrivate;
class Q3DSPresentation;

class Q3DSV_EXPORT Q3DSElement : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString elementPath READ elementPath WRITE setElementPath NOTIFY elementPathChanged)

public:
    explicit Q3DSElement(QObject *parent = nullptr);
    explicit Q3DSElement(const QString &elementPath, QObject *parent = nullptr);
    Q3DSElement(Q3DSPresentation *presentation, const QString &elementPath, QObject *parent = nullptr);
    ~Q3DSElement();

    QString elementPath() const;

public Q_SLOTS:
    void setElementPath(const QString &elementPath);
    QVariant getAttribute(const QString &attributeName) const;
    void setAttribute(const QString &attributeName, const QVariant &value);
    void fireEvent(const QString &eventName);

Q_SIGNALS:
    void elementPathChanged();

protected:
    Q3DSElement(Q3DSElementPrivate &dd, QObject *parent);

private:
    Q_DISABLE_COPY(Q3DSElement)
    Q_DECLARE_PRIVATE(Q3DSElement)
};

QT_END_NAMESPACE

#endif // Q3DSELEMENT_H
