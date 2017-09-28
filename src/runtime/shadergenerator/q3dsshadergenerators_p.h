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

#ifndef Q3DSSHADERGENERATORS_H
#define Q3DSSHADERGENERATORS_H

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

#include "q3dsdefaultvertexpipeline_p.h"

QT_BEGIN_NAMESPACE

// Helper implements the vertex pipeline for mesh subsets when bound to the default material.
// Should be completely possible to use for custom materials with a bit of refactoring.
struct Q3DSSubsetMaterialVertexPipeline : public Q3DSVertexPipelineImpl
{
    TessModeValues::Enum m_TessMode;

    Q3DSSubsetMaterialVertexPipeline(Q3DSDefaultMaterialShaderGenerator &inMaterial,
                                     Q3DSAbstractShaderProgramGenerator &inProgram,
                                     bool inWireframe);

    void initializeTessControlShader();
    void initializeTessEvaluationShader();
    void finalizeTessControlShader();
    void finalizeTessEvaluationShader();

    void beginVertexGeneration(Q3DSImage *displacementImage) override;
    void beginFragmentGeneration() override;

    void assignOutput(const char *inVarName, const char *inVarValue) override;
    void doGenerateUVCoords(quint32 inUVSet = 0) override;

    // fragment shader expects varying vertex normal
    // lighting in vertex pipeline expects world_normal
    void doGenerateWorldNormal() override;
    void doGenerateObjectNormal() override;
    void doGenerateWorldPosition() override;

    void doGenerateVarTangentAndBinormal() override;

    void endVertexGeneration() override;

    void endFragmentGeneration() override;

    void addInterpolationParameter(const char *inName, const char *inType) override;

    Q3DSAbstractShaderStageGenerator &activeStage() override;
};

QT_END_NAMESPACE

#endif // Q3DSSHADERGENERATORS_H
