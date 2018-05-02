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

#ifndef Q3DSPRESENTATIONITEM_P_H
#define Q3DSPRESENTATIONITEM_P_H

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

#include <Qt3DStudioRuntime2/q3dspresentation.h>
#include <QtQml/qqmllist.h>

QT_BEGIN_NAMESPACE

class Q3DSPresentationItemPrivate;

class Q3DSPresentationItem : public Q3DSPresentation
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<QObject> qmlChildren READ qmlChildren DESIGNABLE false)
    Q_CLASSINFO("DefaultProperty", "qmlChildren")

public:
    explicit Q3DSPresentationItem(QObject *parent = nullptr);
    ~Q3DSPresentationItem();

    QQmlListProperty<QObject> qmlChildren();

    void preStudio3DPresentationLoaded();
    void studio3DPresentationLoaded();

public Q_SLOTS:
    static void appendQmlChildren(QQmlListProperty<QObject> *list, QObject *obj);

protected:
    Q3DSPresentationItem(Q3DSPresentationItemPrivate &dd, QObject *parent);

private:
    Q_DISABLE_COPY(Q3DSPresentationItem)
    Q_DECLARE_PRIVATE(Q3DSPresentationItem)
};

QT_END_NAMESPACE

#endif // Q3DSPRESENTATIONITEM_P_H
