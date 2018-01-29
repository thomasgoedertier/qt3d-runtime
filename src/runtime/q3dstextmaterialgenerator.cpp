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

#include "q3dstextmaterialgenerator_p.h"
#include "q3dsdefaultmaterialgenerator.h"

#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QFilterKey>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QDepthTest>
#include <Qt3DRender/QNoDepthMask>
#include <Qt3DRender/QBlendEquation>
#include <Qt3DRender/QBlendEquationArguments>

#include <QtCore/QUrl>
#include <QtGui/QOpenGLContext>

QT_BEGIN_NAMESPACE

Qt3DRender::QMaterial *Q3DSTextMaterialGenerator::generateMaterial(const QVector<Qt3DRender::QParameter *> &params)
{
    Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial;
    Qt3DRender::QEffect *effect = new Qt3DRender::QEffect;
    Qt3DRender::QTechnique *technique = new Qt3DRender::QTechnique;
    Q3DSSceneManager::markAsMainTechnique(technique);

    bool isGLES = false;
    Q3DSDefaultMaterialGenerator::addDefaultApiFilter(technique, &isGLES);

    Qt3DRender::QShaderProgram *shaderProgram = new Qt3DRender::QShaderProgram;
    if (isGLES) {
        shaderProgram->setVertexShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl(QLatin1String("qrc:/q3ds/shaders/text.vert"))));
        shaderProgram->setFragmentShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl(QLatin1String("qrc:/q3ds/shaders/text.frag"))));
    } else {
        shaderProgram->setVertexShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl(QLatin1String("qrc:/q3ds/shaders/text_core.vert"))));
        shaderProgram->setFragmentShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl(QLatin1String("qrc:/q3ds/shaders/text_core.frag"))));
    }

    Qt3DRender::QRenderPass *renderPass = new Qt3DRender::QRenderPass;
    Qt3DRender::QFilterKey *transFilterKey = new Qt3DRender::QFilterKey;
    transFilterKey->setName(QLatin1String("pass"));
    transFilterKey->setValue(QLatin1String("transparent"));
    renderPass->addFilterKey(transFilterKey);

    Qt3DRender::QDepthTest *depthTest = new Qt3DRender::QDepthTest;
    depthTest->setDepthFunction(Qt3DRender::QDepthTest::LessOrEqual);
    renderPass->addRenderState(depthTest);
    Qt3DRender::QNoDepthMask *noDepthWrite = new Qt3DRender::QNoDepthMask;
    renderPass->addRenderState(noDepthWrite);

    Qt3DRender::QBlendEquation *blendFunc = new Qt3DRender::QBlendEquation;
    blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
    Qt3DRender::QBlendEquationArguments *blendArgs = new Qt3DRender::QBlendEquationArguments;
    blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::One);
    blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
    blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::One);
    blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
    renderPass->addRenderState(blendFunc);
    renderPass->addRenderState(blendArgs);

    renderPass->setShaderProgram(shaderProgram);

    technique->addRenderPass(renderPass);
    effect->addTechnique(technique);

    for (Qt3DRender::QParameter *param : params)
        effect->addParameter(param);
    material->setEffect(effect);

    return material;
}

QT_END_NAMESPACE
