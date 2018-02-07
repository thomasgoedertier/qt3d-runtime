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

#ifndef Q3DSPRESENTATION_H
#define Q3DSPRESENTATION_H

#include <Qt3DStudioRuntime2/q3dsruntimeglobal.h>
#include <QtCore/qobject.h>
#include <QtCore/qurl.h>

QT_BEGIN_NAMESPACE

class Q3DSPresentationPrivate;
class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

class Q3DSV_EXPORT Q3DSPresentation : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)

public:
    explicit Q3DSPresentation(QObject *parent = nullptr);
    ~Q3DSPresentation();

    QUrl source() const;
    void setSource(const QUrl &source);

    Q_INVOKABLE void reload();

    void keyPressEvent(QKeyEvent *e);
    void keyReleaseEvent(QKeyEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void mouseDoubleClickEvent(QMouseEvent *e);
#if QT_CONFIG(wheelevent)
    void wheelEvent(QWheelEvent *e);
#endif

Q_SIGNALS:
    void sourceChanged();

protected:
    Q3DSPresentation(Q3DSPresentationPrivate &dd, QObject *parent);

private:
    Q_DISABLE_COPY(Q3DSPresentation)
    Q_DECLARE_PRIVATE(Q3DSPresentation)
};

QT_END_NAMESPACE

#endif // Q3DSPRESENTATION_H
