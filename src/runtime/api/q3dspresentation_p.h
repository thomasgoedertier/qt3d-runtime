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

#ifndef Q3DSPRESENTATION_P_H
#define Q3DSPRESENTATION_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtStudio3D API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/q3dsruntimeglobal_p.h>
#include "q3dspresentation.h"
#include <private/qobject_p.h>
#include <QVariant>

QT_BEGIN_NAMESPACE

class Q3DSV_PRIVATE_EXPORT Q3DSPresentationController
{
public:
    virtual ~Q3DSPresentationController() { }

    virtual void handlePresentationSource(const QUrl &source) = 0;
    virtual void handlePresentationReload() = 0;
    virtual void handleDataInputValue(const QString &name, const QVariant &value) = 0;

    virtual void handlePresentationKeyPressEvent(QKeyEvent *e) = 0;
    virtual void handlePresentationKeyReleaseEvent(QKeyEvent *e) = 0;
    virtual void handlePresentationMousePressEvent(QMouseEvent *e) = 0;
    virtual void handlePresentationMouseMoveEvent(QMouseEvent *e) = 0;
    virtual void handlePresentationMouseReleaseEvent(QMouseEvent *e) = 0;
    virtual void handlePresentationMouseDoubleClickEvent(QMouseEvent *e) = 0;
#if QT_CONFIG(wheelevent)
    virtual void handlePresentationWheelEvent(QWheelEvent *e) = 0;
#endif
};

class Q3DSV_PRIVATE_EXPORT Q3DSPresentationPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(Q3DSPresentation)

public:
    static Q3DSPresentationPrivate *get(Q3DSPresentation *p) { return p->d_func(); }

    void setController(Q3DSPresentationController *c);

    QUrl source;
    Q3DSPresentationController *controller = nullptr;
};

QT_END_NAMESPACE

#endif // Q3DSPRESENTATION_P_H
