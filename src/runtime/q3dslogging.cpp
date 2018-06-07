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

#include "q3dslogging_p.h"

QT_BEGIN_NAMESPACE

Q3DSV_PRIVATE_EXPORT Q_LOGGING_CATEGORY(lcUip, "q3ds.uip")
Q3DSV_PRIVATE_EXPORT Q_LOGGING_CATEGORY(lcUipProp, "q3ds.uipprop")
Q3DSV_PRIVATE_EXPORT Q_LOGGING_CATEGORY(lcScene, "q3ds.scene")
Q3DSV_PRIVATE_EXPORT Q_LOGGING_CATEGORY(lcAnim, "q3ds.anim")
Q3DSV_PRIVATE_EXPORT Q_LOGGING_CATEGORY(lcPerf, "q3ds.perf")
Q3DSV_PRIVATE_EXPORT Q_LOGGING_CATEGORY(lcSlidePlayer, "q3ds.slideplayer")
Q3DSV_PRIVATE_EXPORT Q_LOGGING_CATEGORY(lcInput, "q3ds.input")
Q3DSV_PRIVATE_EXPORT Q_LOGGING_CATEGORY(lcProf, "q3ds.profileui")
Q3DSV_PRIVATE_EXPORT Q_LOGGING_CATEGORY(lcStudio3D, "q3ds.studio3d")
Q3DSV_PRIVATE_EXPORT Q_LOGGING_CATEGORY(lc3DSSurface, "q3ds.surface")
Q3DSV_PRIVATE_EXPORT Q_LOGGING_CATEGORY(lc3DSWidget, "q3ds.widget")

namespace Q3DS {
QByteArrayList loggingCategoryNames()
{
    return {
        QByteArrayLiteral("q3ds.perf"),
        QByteArrayLiteral("q3ds.uip"),
        QByteArrayLiteral("q3ds.uipprop"),
        QByteArrayLiteral("q3ds.scene"),
        QByteArrayLiteral("q3ds.anim"),
        QByteArrayLiteral("q3ds.slideplayer"),
        QByteArrayLiteral("q3ds.input"),
        QByteArrayLiteral("q3ds.profileui"),
        QByteArrayLiteral("q3ds.studio3d"),
        QByteArrayLiteral("q3ds.surface"),
        QByteArrayLiteral("q3ds.widget")
    };
}

void setLogging(bool enabled)
{
    const_cast<QLoggingCategory &>(lcUip()).setEnabled(QtDebugMsg, enabled);
    const_cast<QLoggingCategory &>(lcUipProp()).setEnabled(QtDebugMsg, enabled);
    const_cast<QLoggingCategory &>(lcScene()).setEnabled(QtDebugMsg, enabled);
    const_cast<QLoggingCategory &>(lcAnim()).setEnabled(QtDebugMsg, enabled);
    const_cast<QLoggingCategory &>(lcPerf()).setEnabled(QtDebugMsg, enabled);
    const_cast<QLoggingCategory &>(lcSlidePlayer()).setEnabled(QtDebugMsg, enabled);
    const_cast<QLoggingCategory &>(lcInput()).setEnabled(QtDebugMsg, enabled);
    const_cast<QLoggingCategory &>(lcProf()).setEnabled(QtDebugMsg, enabled);
    const_cast<QLoggingCategory &>(lcStudio3D()).setEnabled(QtDebugMsg, enabled);
    const_cast<QLoggingCategory &>(lc3DSSurface()).setEnabled(QtDebugMsg, enabled);
    const_cast<QLoggingCategory &>(lc3DSWidget()).setEnabled(QtDebugMsg, enabled);
}
}

QT_END_NAMESPACE
