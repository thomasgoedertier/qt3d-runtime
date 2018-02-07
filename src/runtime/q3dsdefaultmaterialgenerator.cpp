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

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QGraphicsApiFilter>

#include <QtCore/QLoggingCategory>
#include <QtGui/QOpenGLContext>

#include "q3dsdefaultmaterialgenerator_p.h"
#include "q3dsutils_p.h"

#include "shadergenerator/q3dsshadermanager_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcScene)

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
                                                                      Q3DSReferencedMaterial *referencedMaterial,
                                                                      const QVector<Qt3DRender::QParameter *> &params,
                                                                      const QVector<Q3DSLightNode*> &lights,
                                                                      Q3DSLayerNode *layer3DS)
{
    Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial;
    Qt3DRender::QEffect *effect = new Qt3DRender::QEffect;
    Qt3DRender::QTechnique *technique = new Qt3DRender::QTechnique;
    Q3DSSceneManager::markAsMainTechnique(technique);

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(layerData);

    Q3DSShaderFeatureSet features;
    fillFeatureSet(&features, layer3DS, defaultMaterial, referencedMaterial);

    Qt3DRender::QShaderProgram *shaderProgram = Q3DSShaderManager::instance().generateShaderProgram(*defaultMaterial,
                                                                                                    referencedMaterial,
                                                                                                    lights,
                                                                                                    false,
                                                                                                    features);

    // The returned program may be a cached instance, ensure it is not parented
    // to the QMaterial by parenting it to something else now.
    shaderProgram->setParent(layerData->entity);

    for (Qt3DRender::QRenderPass *pass : Q3DSSceneManager::standardRenderPasses(shaderProgram,
                                                                                layer3DS,
                                                                                defaultMaterial->blendMode(),
                                                                                defaultMaterial->displacementmap() != nullptr))
    {
        technique->addRenderPass(pass);
    }

    addDefaultApiFilter(technique);
    effect->addTechnique(technique);

    if (hasCompute()) {
        for (Qt3DRender::QTechnique *computeTechnique : Q3DSSceneManager::computeTechniques(layer3DS))
            effect->addTechnique(computeTechnique);
    }

    static const bool paramDebug = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 2;
    if (paramDebug)
        qCDebug(lcScene) << effect << "for default material" << defaultMaterial->id() << "has parameters:";
    for (Qt3DRender::QParameter *param : params) {
        effect->addParameter(param);
        if (paramDebug)
            qCDebug(lcScene) << "  " << param << param->name() << param->value();
    }
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

void Q3DSDefaultMaterialGenerator::fillFeatureSet(Q3DSShaderFeatureSet *features,
                                                  Q3DSLayerNode *layer3DS,
                                                  Q3DSDefaultMaterial *material,
                                                  Q3DSReferencedMaterial *referencedMaterial)
{
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(layerData);

    // Figure out if IBL data has been set
    bool enableLightProbe = false;
    bool enableLightProbe2 = false;
    bool enableIblFov = false;

    // Check Layer
    if (layer3DS->lightProbe())
        enableLightProbe = true;
    if (layer3DS->lightProbe2())
        enableLightProbe2 = true;
    if (layer3DS->probefov() < 180.0f && enableLightProbe)
        enableIblFov = true;

    // Check for Override in material or referenced material
    if (material->lightProbe() || (referencedMaterial && referencedMaterial->lightProbe()))
        enableLightProbe = true;

    features->append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_CG_LIGHTING"), true));
    features->append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_IBL_FOV"), enableIblFov));
    features->append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_LIGHT_PROBE"), enableLightProbe));
    features->append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_LIGHT_PROBE_2"), enableLightProbe2));
    features->append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_SSDO"), false));
    features->append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_SSM"), !layerData->shadowMapData.shadowCasters.isEmpty()));
    features->append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_SSAO"), layerData->ssaoTextureData.enabled));
}

QT_END_NAMESPACE
