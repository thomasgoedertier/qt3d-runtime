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

#ifndef Q3DSSHADERMANAGER_H
#define Q3DSSHADERMANAGER_H

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
#include "q3dsdefaultvertexpipeline_p.h"
#include "q3dscustommaterialvertexpipeline_p.h"
#include "q3dsuippresentation_p.h"
#include <Qt3DRender/QShaderProgram>
#include <QMap>

QT_BEGIN_NAMESPACE

class Q3DSShaderManager
{
public:
    static Q3DSShaderManager& instance();
    Q3DSShaderManager(Q3DSShaderManager const&) = delete;
    void operator=(Q3DSShaderManager const&) = delete;

    void invalidate();
    void setProfiler(Q3DSProfiler *profiler);

    Q3DSDefaultMaterialShaderGenerator *defaultMaterialShaderGenerator();
    Q3DSCustomMaterialShaderGenerator *customMaterialShaderGenerator();

    Qt3DRender::QShaderProgram *generateShaderProgram(Q3DSDefaultMaterial &material,
                                                      Q3DSReferencedMaterial *referencedMaterial,
                                                      const QVector<Q3DSLightNode*> &lights,
                                                      bool hasTransparency,
                                                      const Q3DSShaderFeatureSet &featureSet);
    Qt3DRender::QShaderProgram *generateShaderProgram(Q3DSCustomMaterialInstance &material,
                                                      Q3DSReferencedMaterial *referencedMaterial,
                                                      const QVector<Q3DSLightNode*> &lights,
                                                      bool hasTransparency,
                                                      const Q3DSShaderFeatureSet &featureSet,
                                                      const QString &shaderName = QString());

    Qt3DRender::QShaderProgram* getCubeDepthNoTessShader(Qt3DCore::QNode *parent);

    Qt3DRender::QShaderProgram* getOrthographicDepthNoTessShader(Qt3DCore::QNode *parent);

    Qt3DRender::QShaderProgram* getDepthPrepassShader(Qt3DCore::QNode *parent, bool displaced);

    Qt3DRender::QShaderProgram *getOrthoShadowBlurXShader(Qt3DCore::QNode *parent);
    Qt3DRender::QShaderProgram *getOrthoShadowBlurYShader(Qt3DCore::QNode *parent);

    Qt3DRender::QShaderProgram *getCubeShadowBlurXShader(Qt3DCore::QNode *parent, const Q3DSGraphicsLimits &limits);
    Qt3DRender::QShaderProgram *getCubeShadowBlurYShader(Qt3DCore::QNode *parent, const Q3DSGraphicsLimits &limits);

    Qt3DRender::QShaderProgram *getSsaoTextureShader(Qt3DCore::QNode *parent);

    Qt3DRender::QShaderProgram *getBsdfMipPreFilterShader(Qt3DCore::QNode *parent);

    Qt3DRender::QShaderProgram *getProgAABlendShader(Qt3DCore::QNode *parent);

    Qt3DRender::QShaderProgram *getBlendOverlayShader(Qt3DCore::QNode *parent, int msaaSampleCount);
    Qt3DRender::QShaderProgram *getBlendColorBurnShader(Qt3DCore::QNode *parent, int msaaSampleCount);
    Qt3DRender::QShaderProgram *getBlendColorDodgeShader(Qt3DCore::QNode *parent, int msaaSampleCount);

    Qt3DRender::QShaderProgram *getEffectShader(Qt3DCore::QNode *parent,
                                                const QString &name,
                                                const QString &vertexShaderSrc,
                                                const QString &fragmentShaderSrc);

private:
    Q3DSShaderManager();

    Q3DSDefaultMaterialShaderGenerator *m_materialShaderGenerator;
    Q3DSCustomMaterialShaderGenerator *m_customMaterialShaderGenerator;
    Q3DSAbstractShaderProgramGenerator *m_shaderProgramGenerator;
    Qt3DRender::QShaderProgram *m_depthPrePassShader = nullptr;
    Qt3DRender::QShaderProgram *m_orthoDepthShader = nullptr;
    Qt3DRender::QShaderProgram *m_cubeDepthShader = nullptr;
    Qt3DRender::QShaderProgram *m_orthoShadowBlurXShader = nullptr;
    Qt3DRender::QShaderProgram *m_orthoShadowBlurYShader = nullptr;
    Qt3DRender::QShaderProgram *m_cubeShadowBlurXShader = nullptr;
    Qt3DRender::QShaderProgram *m_cubeShadowBlurYShader = nullptr;
    Qt3DRender::QShaderProgram *m_ssaoTextureShader = nullptr;
    Qt3DRender::QShaderProgram *m_bsdfMipPreFilterShader = nullptr;
    Qt3DRender::QShaderProgram *m_progAABlendShader = nullptr;
    QMap<int, Qt3DRender::QShaderProgram *> m_blendOverlayShader;
    QMap<int, Qt3DRender::QShaderProgram *> m_blendColorBurnShader;
    QMap<int, Qt3DRender::QShaderProgram *> m_blendColorDodgeShader;
};

QT_END_NAMESPACE

#endif // Q3DSSHADERMANAGER_H
