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

#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QGraphicsApiFilter>

#include <QtGui/QOpenGLContext>

#include "q3dsdefaultmaterialgenerator.h"
#include "q3dsutils.h"

#include "shadergenerator/q3dsshadermanager_p.h"

QT_BEGIN_NAMESPACE

// List of Qt3D builtin Uniforms: (from RenderView)
//
// mat4 modelMatrix
// mat4 viewMatrix
// mat4 projectionMatrix
// mat4 modelView
// mat4 viewProjectionMatrix
// mat4 modelViewProjection or mvp
// mat4 inverseModelMatrix
// mat4 inverseViewMatrix
// mat4 inverseProjectionMatrix
// mat4 inverseModelView
// mat4 inverseViewProjectionMatrix
// mat4 inverseModelViewProjection
// mat3 modelNormalMatrix  <- normal_matrix
// mat3 modelViewNormal
// mat4 viewportMatrix
// mat4 inverseViewportMatrix;
// float exposure
// float gamma
// float time
// vec3 eyePosition <- "eyePosition"

// List of Qt3D Default Attribute Names
//
// vec3 vertexPosition
// vec3 vertexNormal
// vec3 vertexColor
// vec2 vertexTexCoord
// vec3 vertexTangent

// List of Q3DS Default Attribute Names
//
// vec3 attr_textan;
// vec3 attr_binormal;
// vec3 attr_pos;
// vec3 attr_norm;
// vec2 attr_uv0

// Caller takes ownership everything generated in the method
Qt3DRender::QMaterial *Q3DSDefaultMaterialGenerator::generateMaterial(Q3DSDefaultMaterial *defaultMaterial,
                                                                      const QVector<Qt3DRender::QParameter *> &params,
                                                                      const QVector<Q3DSLightNode*> &lights,
                                                                      Q3DSLayerNode *layer3DS)
{
    Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial;
    Qt3DRender::QEffect *effect = new Qt3DRender::QEffect;
    Qt3DRender::QTechnique *technique = new Qt3DRender::QTechnique;
    Q3DSSceneBuilder::markAsMainTechnique(technique);

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(layerData);

    Q3DSShaderFeatureSet features;

    if (!layerData->shadowMapData.shadowCasters.isEmpty())
        features.append(Q3DSShaderPreprocessorFeature(QLatin1String("UIC_ENABLE_SSM"), true));

    if (layerData->ssaoTextureData.enabled)
        features.append(Q3DSShaderPreprocessorFeature(QLatin1String("UIC_ENABLE_SSAO"), true));

    Qt3DRender::QShaderProgram *shaderProgram = Q3DSShaderManager::instance().generateShaderProgram(*defaultMaterial,
                                                                                                    lights,
                                                                                                    false,
                                                                                                    features);
    for (Qt3DRender::QRenderPass *pass : Q3DSSceneBuilder::standardRenderPasses(shaderProgram,
                                                                                layer3DS,
                                                                                defaultMaterial->blendMode(),
                                                                                defaultMaterial->displacementmap() != nullptr))
    {
        technique->addRenderPass(pass);
    }

    addDefaultApiFilter(technique);
    effect->addTechnique(technique);

    if (hasCompute()) {
        for (Qt3DRender::QTechnique *computeTechnique : Q3DSSceneBuilder::computeTechniques())
            effect->addTechnique(computeTechnique);
    }

    for (Qt3DRender::QParameter *param : params)
        effect->addParameter(param);
    material->setEffect(effect);

    return material;
}

void Q3DSDefaultMaterialGenerator::addDefaultApiFilter(Qt3DRender::QTechnique *technique, bool *isGLES)
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    bool isOpenGLES = QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
    if (isGLES)
        *isGLES = isOpenGLES;
    if (isOpenGLES)
        technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGLES);
    else
        technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    technique->graphicsApiFilter()->setMajorVersion(format.majorVersion());
    technique->graphicsApiFilter()->setMinorVersion(format.minorVersion());
    if (format.profile() == QSurfaceFormat::CoreProfile)
        technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);
}

bool Q3DSDefaultMaterialGenerator::hasCompute()
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES)
        return format.version() >= qMakePair(3, 1);
    else
        return format.version() >= qMakePair(4, 3);
}

QT_END_NAMESPACE
