/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef Q3DSDEFAULTMATERIALGENERATOR_H
#define Q3DSDEFAULTMATERIALGENERATOR_H

#include <Qt3DStudioRuntime2/q3dsscenemanager.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
class QParameter;
class QMaterial;
class QTechnique;
}

class Q3DSDefaultMaterialGenerator
{
public:
    Qt3DRender::QMaterial *generateMaterial(Q3DSDefaultMaterial *defaultMaterial,
                                            const QVector<Qt3DRender::QParameter *> &params,
                                            const QVector<Q3DSLightNode *> &lights,
                                            Q3DSLayerNode *layer3DS);

    static void addDefaultApiFilter(Qt3DRender::QTechnique *technique, bool *isGLES = nullptr);
    static bool hasCompute();
};

QT_END_NAMESPACE

#endif // Q3DSDEFAULTMATERIALGENERATOR_H
