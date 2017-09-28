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
#include <Qt3DStudioRuntime2/q3dspresentation.h>
#include <Qt3DStudioRuntime2/q3dsgraphicslimits.h>
#include <Qt3DRender/QShaderProgram>

QT_BEGIN_NAMESPACE

class Q3DSShaderManager
{
public:
    static Q3DSShaderManager& instance();
    Q3DSShaderManager(Q3DSShaderManager const&) = delete;
    void operator=(Q3DSShaderManager const&) = delete;

    void invalidate();

    Q3DSDefaultMaterialShaderGenerator *defaultMaterialShaderGenerator();

    Qt3DRender::QShaderProgram *generateShaderProgram(Q3DSDefaultMaterial &material,
                                                      const QVector<Q3DSLightNode*> &lights,
                                                      bool hasTransparency,
                                                      const Q3DSShaderFeatureSet &featureSet);

    Qt3DRender::QShaderProgram* getCubeDepthNoTessShader();

    Qt3DRender::QShaderProgram* getOrthographicDepthNoTessShader();

    Qt3DRender::QShaderProgram* getDepthPrepassShader(bool displaced);

    Qt3DRender::QShaderProgram *getOrthoShadowBlurXShader();
    Qt3DRender::QShaderProgram *getOrthoShadowBlurYShader();

    Qt3DRender::QShaderProgram *getCubeShadowBlurXShader(const Q3DSGraphicsLimits &limits);
    Qt3DRender::QShaderProgram *getCubeShadowBlurYShader(const Q3DSGraphicsLimits &limits);

    Qt3DRender::QShaderProgram *getSsaoTextureShader();

    Qt3DRender::QShaderProgram *getBsdfMipPreFilterShader();

#if 0
    Qt3DRender::QShaderProgram* getParaboloidDepthShader(TessModeValues::Enum tessMode);
    Qt3DRender::QShaderProgram* getParaboloidDepthNoTessShader();
    Qt3DRender::QShaderProgram* getParaboloidDepthTessLinearShader();
    Qt3DRender::QShaderProgram* getParaboloidDepthTessPhongShader();
    Qt3DRender::QShaderProgram* getParaboloidDepthTessNPatchShader();

    Qt3DRender::QShaderProgram* getCubeShadowDepthShader(TessModeValues::Enum tessMode);

    Qt3DRender::QShaderProgram* getCubeDepthTessLinearShader();
    Qt3DRender::QShaderProgram* getCubeDepthTessPhongShader();
    Qt3DRender::QShaderProgram* getCubeDepthTessNPatchShader();

    Qt3DRender::QShaderProgram* getOrthographicDepthShader(TessModeValues::Enum tessMode);
    Qt3DRender::QShaderProgram* getOrthographicDepthTessLinearShader();
    Qt3DRender::QShaderProgram* getOrthographicDepthTessPhongShader();
    Qt3DRender::QShaderProgram* getOrthographicDepthTessNPatchShader();

    Qt3DRender::QShaderProgram* getDepthTessPrepassShader(TessModeValues::Enum tessMode, bool displaced);
    Qt3DRender::QShaderProgram* getDepthTessLinearPrepassShader(bool displaced);
    Qt3DRender::QShaderProgram* getDepthTessPhongPrepassShader();
    Qt3DRender::QShaderProgram* getDepthTessNPatchPrepassShader();

    Qt3DRender::QShaderProgram* getDefaultAoPassShader(Q3DSShaderFeatureSet featureSet);
    Qt3DRender::QShaderProgram* getFakeCubeDepthShader(Q3DSShaderFeatureSet featureSet);
#endif

private:
    Q3DSShaderManager();

    Q3DSDefaultMaterialShaderGenerator *m_materialShaderGenerator;
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
};

QT_END_NAMESPACE

#endif // Q3DSSHADERMANAGER_H
