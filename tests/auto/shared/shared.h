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

#ifndef Q3DSAUTOSHARED_H
#define Q3DSAUTOSHARED_H

#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#include <Qt3DStudioRuntime2/private/q3dsgraphicslimits_p.h>

bool isOpenGLGoodEnough()
{
    static bool check = []() {
        if (!QGuiApplicationPrivate::instance()->platformIntegration()->hasCapability(QPlatformIntegration::OpenGL))
            return false;

        Q3DSGraphicsLimits gfxLimits = Q3DS::graphicsLimits();
        if (gfxLimits.versionedContextFailed)
            return false;

        if (gfxLimits.vendor.contains(QByteArrayLiteral("VMware, Inc.")))
            return false;

        if (gfxLimits.renderer.contains(QByteArrayLiteral("Apple Software Renderer")))
            return false;

        if (gfxLimits.renderer.contains(QByteArrayLiteral("ANGLE")))
            return false;

        return true;
    }();

    return check;
}

#endif
