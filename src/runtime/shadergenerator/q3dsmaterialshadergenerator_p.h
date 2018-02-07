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

#ifndef Q3DSRENDERMATERIALSHADERGENERATOR_H
#define Q3DSRENDERMATERIALSHADERGENERATOR_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "q3dsshaderprogramgenerator_p.h"
#include "q3dsuippresentation_p.h"
#include "q3dsscenemanager_p.h"

QT_BEGIN_NAMESPACE

class Q3DSAbstractMaterialGenerator
{
public:
    struct ImageVariableNames
    {
        QString imageSampler;
        QString imageFragCoords;
    };

    virtual ~Q3DSAbstractMaterialGenerator() { }
    virtual ImageVariableNames getImageVariableNames(const QString &name) = 0;
    virtual void generateImageUVCoordinates(Q3DSAbstractShaderStageGenerator &vertexPipeline,
                                            const QString &name,
                                            quint32 uvSet,
                                            Q3DSImage &image) = 0;

    virtual Qt3DRender::QShaderProgram *generateShader(Q3DSGraphObject &material,
                                                       Q3DSAbstractShaderStageGenerator &vertexPipeline,
                                                       const Q3DSShaderFeatureSet &featureSet,
                                                       const QVector<Q3DSLightNode*> &lights,
                                                       bool hasTransparency,
                                                       const QString &description) = 0;
};

QT_END_NAMESPACE

#endif // Q3DSRENDERMATERIALSHADERGENERATOR_H
