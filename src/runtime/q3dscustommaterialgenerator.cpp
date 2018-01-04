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

#include "q3dscustommaterialgenerator.h"
#include "q3dsdefaultmaterialgenerator.h"

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

#include "q3dsutils.h"

#include "shadergenerator/q3dsshadermanager_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcScene)

Qt3DRender::QMaterial *Q3DSCustomMaterialGenerator::generateMaterial(Q3DSCustomMaterialInstance *customMaterial, const QVector<Qt3DRender::QParameter *> &params, const QVector<Q3DSLightNode *> &lights, Q3DSLayerNode *layer3DS, const Q3DSMaterial::Pass &pass)
{
    Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial;
    Qt3DRender::QEffect *effect = new Qt3DRender::QEffect;
    Qt3DRender::QTechnique *technique = new Qt3DRender::QTechnique;
    Q3DSSceneManager::markAsMainTechnique(technique);

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(layerData);

    Q3DSShaderFeatureSet features;
    features.append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_CG_LIGHTING"), true));
    features.append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_IBL_FOV"), false));
    features.append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_LIGHT_PROBE"), false));
    features.append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_LIGHT_PROBE_2"), false));
    features.append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_SSDO"), false));
    features.append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_SSM"), !layerData->shadowMapData.shadowCasters.isEmpty()));
    features.append(Q3DSShaderPreprocessorFeature(QLatin1String("QT3DS_ENABLE_SSAO"), layerData->ssaoTextureData.enabled));

    Qt3DRender::QShaderProgram *shaderProgram = Q3DSShaderManager::instance().generateShaderProgram(*customMaterial,
                                                                                                    lights,
                                                                                                    false,
                                                                                                    features,
                                                                                                    pass.shaderName);

    // The returned program may be a cached instance, ensure it is not parented
    // to the QMaterial by parenting it to something else now.
    shaderProgram->setParent(layerData->entity);
    // ### TODO This is does not take into consideration anything specified by the custom material pass
    for (Qt3DRender::QRenderPass *pass : Q3DSSceneManager::standardRenderPasses(shaderProgram,
                                                                                layer3DS,
                                                                                Q3DSDefaultMaterial::Normal,
                                                                                customMaterial->material()->shaderIsDisplaced()))
    {
        technique->addRenderPass(pass);
    }

    Q3DSDefaultMaterialGenerator::addDefaultApiFilter(technique);
    effect->addTechnique(technique);

    if (Q3DSDefaultMaterialGenerator::hasCompute()) {
        for (Qt3DRender::QTechnique *computeTechnique : Q3DSSceneManager::computeTechniques(layer3DS))
            effect->addTechnique(computeTechnique);
    }

    static const bool paramDebug = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 2;
    if (paramDebug)
        qCDebug(lcScene) << effect << "for default material" << customMaterial->id() << "has parameters:";
    for (Qt3DRender::QParameter *param : params) {
        effect->addParameter(param);
        if (paramDebug)
            qCDebug(lcScene) << "  " << param << param->name() << param->value();
    }
    material->setEffect(effect);

    return material;
}

QT_END_NAMESPACE
