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

#include "q3dsshadergenerators_p.h"
#include "q3dsshadermanager_p.h"

QT_BEGIN_NAMESPACE

#define UIC_RENDER_SUPPORT_DEPTH_TEXTURE 1

Q3DSSubsetMaterialVertexPipeline::Q3DSSubsetMaterialVertexPipeline(Q3DSDefaultMaterialShaderGenerator &inMaterial,
                                                                   Q3DSAbstractShaderProgramGenerator &inProgram,
                                                                   bool inWireframe)
    : Q3DSVertexPipelineImpl(inMaterial, inProgram, inWireframe),
      m_TessMode(TessModeValues::NoTess)
{
#if 0
    if (m_Renderer.GetContext().IsTessellationSupported())
        m_TessMode = renderable.m_TessellationMode;

    if (m_Renderer.GetContext().IsGeometryStageSupported()
            && m_TessMode != TessModeValues::NoTess)
    {
        m_Wireframe = inWireframeRequested;
    }
#endif
}

void Q3DSSubsetMaterialVertexPipeline::initializeTessControlShader()
{
    if (m_TessMode == TessModeValues::NoTess || programGenerator().getStage(Q3DSShaderGeneratorStages::TessControl) == nullptr)
        return;

    Q3DSAbstractShaderStageGenerator &tessCtrlShader(*programGenerator().getStage(Q3DSShaderGeneratorStages::TessControl));

    tessCtrlShader.addUniform("tessLevelInner", "float");
    tessCtrlShader.addUniform("tessLevelOuter", "float");

    setupTessIncludes(Q3DSShaderGeneratorStages::TessControl, m_TessMode);

    tessCtrlShader.append("void main() {\n");

    tessCtrlShader.append("\tctWorldPos[0] = varWorldPos[0];");
    tessCtrlShader.append("\tctWorldPos[1] = varWorldPos[1];");
    tessCtrlShader.append("\tctWorldPos[2] = varWorldPos[2];");

    if (m_TessMode == TessModeValues::TessPhong || m_TessMode == TessModeValues::TessNPatch) {
        tessCtrlShader.append("\tctNorm[0] = varObjectNormal[0];");
        tessCtrlShader.append("\tctNorm[1] = varObjectNormal[1];");
        tessCtrlShader.append("\tctNorm[2] = varObjectNormal[2];");
    }
    if (m_TessMode == TessModeValues::TessNPatch) {
        tessCtrlShader.append("\tctTangent[0] = varTangent[0];");
        tessCtrlShader.append("\tctTangent[1] = varTangent[1];");
        tessCtrlShader.append("\tctTangent[2] = varTangent[2];");
    }

    tessCtrlShader.append(
                "\tgl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;");
    tessCtrlShader.append("\ttessShader( tessLevelOuter, tessLevelInner);\n");
}

void Q3DSSubsetMaterialVertexPipeline::initializeTessEvaluationShader()
{
    if (m_TessMode == TessModeValues::NoTess || programGenerator().getStage(Q3DSShaderGeneratorStages::TessEval) == nullptr)
        return;

    Q3DSAbstractShaderStageGenerator &tessEvalShader(*programGenerator().getStage(Q3DSShaderGeneratorStages::TessEval));

    setupTessIncludes(Q3DSShaderGeneratorStages::TessEval, m_TessMode);

    if (m_TessMode == TessModeValues::TessLinear)
        Q3DSShaderManager::instance().defaultMaterialShaderGenerator()->addDisplacementImageUniforms(
                    tessEvalShader, QLatin1String("displacementMap"), m_DisplacementImage);

    tessEvalShader.addUniform("modelViewProjection", "mat4");
    tessEvalShader.addUniform("modelNormalMatrix", "mat3");

    tessEvalShader.append("void main() {");

    if (m_TessMode == TessModeValues::TessNPatch) {
        tessEvalShader.append("\tctNorm[0] = varObjectNormalTC[0];");
        tessEvalShader.append("\tctNorm[1] = varObjectNormalTC[1];");
        tessEvalShader.append("\tctNorm[2] = varObjectNormalTC[2];");

        tessEvalShader.append("\tctTangent[0] = varTangentTC[0];");
        tessEvalShader.append("\tctTangent[1] = varTangentTC[1];");
        tessEvalShader.append("\tctTangent[2] = varTangentTC[2];");
    }

    tessEvalShader.append("\tvec4 pos = tessShader( );\n");
}

void Q3DSSubsetMaterialVertexPipeline::finalizeTessControlShader()
{
    Q3DSAbstractShaderStageGenerator &tessCtrlShader(*programGenerator().getStage(Q3DSShaderGeneratorStages::TessControl));
    // add varyings we must pass through
    for (const QString &key : m_InterpolationParameters.keys()) {
        const QByteArray ba = key.toUtf8();
        tessCtrlShader << "\t" << ba.constData()
                       << "TC[gl_InvocationID] = " << ba.constData()
                       << "[gl_InvocationID];\n";
    }
}

void Q3DSSubsetMaterialVertexPipeline::finalizeTessEvaluationShader()
{
    Q3DSAbstractShaderStageGenerator &tessEvalShader(
                *programGenerator().getStage(Q3DSShaderGeneratorStages::TessEval));

    const char *outExt = "";
    if (programGenerator().getEnabledStages() & Q3DSShaderGeneratorStages::Geometry)
        outExt = "TE";

    // add varyings we must pass through
    if (m_TessMode == TessModeValues::TessNPatch) {
        for (const QString &key : m_InterpolationParameters.keys()) {
            const QByteArray ba = key.toUtf8();
            tessEvalShader << "\t" << ba.constData() << outExt
                           << " = gl_TessCoord.z * " << ba.constData() << "TC[0] + ";
            tessEvalShader << "gl_TessCoord.x * " << ba.constData() << "TC[1] + ";
            tessEvalShader << "gl_TessCoord.y * " << ba.constData() << "TC[2];\n";
        }

        // transform the normal
        if (m_GenerationFlags & GenerationFlagValues::WorldNormal)
            tessEvalShader << "\n\tvarNormal" << outExt
                           << " = normalize(modelNormalMatrix * teNorm);\n";
        // transform the tangent
        if (m_GenerationFlags & GenerationFlagValues::TangentBinormal) {
            tessEvalShader << "\n\tvarTangent" << outExt
                           << " = normalize(modelNormalMatrix * teTangent);\n";
            // transform the binormal
            tessEvalShader << "\n\tvarBinormal" << outExt
                           << " = normalize(modelNormalMatrix * teBinormal);\n";
        }
    } else {
        for (const QString &key : m_InterpolationParameters.keys()) {
            const QByteArray ba = key.toUtf8();
            tessEvalShader << "\t" << ba.constData() << outExt
                           << " = gl_TessCoord.x * " << ba.constData() << "TC[0] + ";
            tessEvalShader << "gl_TessCoord.y * " << ba.constData() << "TC[1] + ";
            tessEvalShader << "gl_TessCoord.z * " << ba.constData() << "TC[2];\n";
        }

        // displacement mapping makes only sense with linear tessellation
        if (m_TessMode == TessModeValues::TessLinear && m_DisplacementImage) {
            Q3DSDefaultMaterialShaderGenerator::ImageVariableNames theNames =
                    Q3DSShaderManager::instance().defaultMaterialShaderGenerator()->getImageVariableNames(QLatin1String("displacementMap"));
            const QByteArray imageSampler = theNames.imageSampler.toUtf8();
            const QByteArray imageFragCoords = theNames.imageFragCoords.toUtf8();
            tessEvalShader << "\tpos.xyz = uicDefaultMaterialFileDisplacementTexture( "
                           << imageSampler.constData() << ", displaceAmount, "
                           << imageFragCoords.constData() << outExt;
            tessEvalShader << ", varObjectNormal" << outExt << ", pos.xyz );"
                           << "\n";
            tessEvalShader << "\tvarWorldPos" << outExt
                           << "= (modelMatrix * pos).xyz;" << "\n";
            tessEvalShader << "\tvarViewVector" << outExt
                           << "= normalize(eyePosition - "
                           << "varWorldPos" << outExt << ");" << "\n";
        }

        // transform the normal
        tessEvalShader << "\n\tvarNormal" << outExt
                       << " = normalize(modelNormalMatrix * varObjectNormal" << outExt
                       << ");\n";
    }

    tessEvalShader.append("\tgl_Position = modelViewProjection * pos;\n");
}

void Q3DSSubsetMaterialVertexPipeline::beginVertexGeneration(Q3DSImage *displacementImage)
{
    m_DisplacementImage = displacementImage;

    Q3DSShaderGeneratorStageFlags theStages(Q3DSAbstractShaderProgramGenerator::defaultFlags());
    if (m_TessMode != TessModeValues::NoTess) {
        theStages |= Q3DSShaderGeneratorStages::TessControl;
        theStages |= Q3DSShaderGeneratorStages::TessEval;
    }
    if (m_Wireframe) {
        theStages |= Q3DSShaderGeneratorStages::Geometry;
    }
    programGenerator().beginProgram(theStages);
    if (m_TessMode != TessModeValues::NoTess) {
        initializeTessControlShader();
        initializeTessEvaluationShader();
    }
    if (m_Wireframe) {
        initializeWireframeGeometryShader();
    }
    // Open up each stage.
    Q3DSAbstractShaderStageGenerator &vertexShader(vertex());
    vertexShader.addIncoming("attr_pos", "vec3");
    vertexShader << "void main()" << "\n" << "{" << "\n";
    vertexShader << "\tvec3 uTransform;" << "\n";
    vertexShader << "\tvec3 vTransform;" << "\n";

    if (displacementImage) {
        generateUVCoords();
        materialGenerator().generateImageUVCoordinates(*this, QLatin1String("displacementMap"), 0,
                                                       *displacementImage);
        if (!hasTessellation()) {
            vertexShader.addUniform("displaceAmount", "float");
            // we create the world position setup here
            // because it will be replaced with the displaced position
            setCode(GenerationFlagValues::WorldPosition);
            vertexShader.addUniform("modelMatrix", "mat4");

            vertexShader.addInclude("uicDefaultMaterialFileDisplacementTexture.glsllib");
            Q3DSDefaultMaterialShaderGenerator::ImageVariableNames theVarNames =
                    materialGenerator().getImageVariableNames(QLatin1String("displacementMap"));

            const QByteArray imageSampler = theVarNames.imageSampler.toUtf8();
            const QByteArray imageFragCoords = theVarNames.imageFragCoords.toUtf8();

            vertexShader.addUniform(imageSampler.constData(), "sampler2D");

            vertexShader
                    << "\tvec3 displacedPos = uicDefaultMaterialFileDisplacementTexture( "
                    << imageSampler.constData() << ", displaceAmount, "
                    << imageFragCoords.constData() << ", attr_norm, attr_pos );" << "\n";
            addInterpolationParameter("varWorldPos", "vec3");
            vertexShader.append("\tvec3 local_model_world_position = (modelMatrix * "
                                "vec4(displacedPos, 1.0)).xyz;");
            assignOutput("varWorldPos", "local_model_world_position");
        }
    }
    // for tessellation we pass on the position in object coordinates
    // Also note that gl_Position is written in the tess eval shader
    if (hasTessellation())
        vertexShader.append("\tgl_Position = vec4(attr_pos, 1.0);");
    else {
        vertexShader.addUniform("modelViewProjection", "mat4");
        if (displacementImage)
            vertexShader.append(
                        "\tgl_Position = modelViewProjection * vec4(displacedPos, 1.0);");
        else
            vertexShader.append(
                        "\tgl_Position = modelViewProjection * vec4(attr_pos, 1.0);");
    }

    if (hasTessellation()) {
        generateWorldPosition();
        generateWorldNormal();
        generateObjectNormal();
        generateVarTangentAndBinormal();
    }
}

void Q3DSSubsetMaterialVertexPipeline::beginFragmentGeneration()
{
    fragment().addUniform("material_diffuse", "vec4");
    fragment() << "void main()" << "\n" << "{" << "\n";
    // We do not pass object opacity through the pipeline.
    fragment() << "\tfloat object_opacity = material_diffuse.a;" << "\n";
}

void Q3DSSubsetMaterialVertexPipeline::assignOutput(const char *inVarName, const char *inVarValue)
{
    vertex() << "\t" << inVarName << " = " << inVarValue << ";\n";
}

void Q3DSSubsetMaterialVertexPipeline::doGenerateUVCoords(quint32 inUVSet)
{
    Q_ASSERT(inUVSet == 0 || inUVSet == 1);

    if (inUVSet == 0) {
        vertex().addIncoming("attr_uv0", "vec2");
        vertex() << "\tvarTexCoord0 = attr_uv0;" << "\n";
    } else if (inUVSet == 1) {
        vertex().addIncoming("attr_uv1", "vec2");
        vertex() << "\tvarTexCoord1 = attr_uv1;" << "\n";
    }
}

void Q3DSSubsetMaterialVertexPipeline::doGenerateWorldNormal()
{
    Q3DSAbstractShaderStageGenerator &vertexGenerator(vertex());
    vertexGenerator.addIncoming("attr_norm", "vec3");
    vertexGenerator.addUniform("modelNormalMatrix", "mat3");
    if (hasTessellation() == false) {
        vertexGenerator.append(
                    "\tvec3 world_normal = normalize(modelNormalMatrix * attr_norm).xyz;");
        vertexGenerator.append("\tvarNormal = world_normal;");
    }
}

void Q3DSSubsetMaterialVertexPipeline::doGenerateObjectNormal()
{
    addInterpolationParameter("varObjectNormal", "vec3");
    vertex().append("\tvarObjectNormal = attr_norm;");
}

void Q3DSSubsetMaterialVertexPipeline::doGenerateWorldPosition()
{
    vertex().append(
                "\tvec3 local_model_world_position = (modelMatrix * vec4(attr_pos, 1.0)).xyz;");
    assignOutput("varWorldPos", "local_model_world_position");
}

void Q3DSSubsetMaterialVertexPipeline::doGenerateVarTangentAndBinormal()
{
    vertex().addIncoming("attr_textan", "vec3");
    vertex().addIncoming("attr_binormal", "vec3");

    bool hasNPatchTessellation = m_TessMode == TessModeValues::TessNPatch;

    if (!hasNPatchTessellation) {
        vertex() << "\tvarTangent = modelNormalMatrix * attr_textan;" << "\n"
                 << "\tvarBinormal = modelNormalMatrix * attr_binormal;" << "\n";
    } else {
        vertex() << "\tvarTangent = attr_textan;" << "\n"
                 << "\tvarBinormal = attr_binormal;" << "\n";
    }
}

void Q3DSSubsetMaterialVertexPipeline::endVertexGeneration()
{

    if (hasTessellation()) {
        // finalize tess control shader
        finalizeTessControlShader();
        // finalize tess evaluation shader
        finalizeTessEvaluationShader();

        tessControl().append("}");
        tessEval().append("}");
    }
    if (m_Wireframe) {
        // finalize geometry shader
        finalizeWireframeGeometryShader();
        geometry().append("}");
    }
    vertex().append("}");
}

void Q3DSSubsetMaterialVertexPipeline::endFragmentGeneration()
{
    fragment().append("}");
}

void Q3DSSubsetMaterialVertexPipeline::addInterpolationParameter(const char *inName, const char *inType)
{
    m_InterpolationParameters.insert(QString::fromUtf8(inName), QString::fromUtf8(inType));
    vertex().addOutgoing(inName, inType);
    fragment().addIncoming(inName, inType);
    if (hasTessellation()) {
        QByteArray nameBuilder(inName);
        nameBuilder.append("TC");
        tessControl().addOutgoing(nameBuilder, inType);

        nameBuilder = inName;
        if (programGenerator().getEnabledStages() & Q3DSShaderGeneratorStages::Geometry) {
            nameBuilder.append("TE");
            geometry().addOutgoing(inName, inType);
        }
        tessEval().addOutgoing(nameBuilder.constData(), inType);
    }
}

Q3DSAbstractShaderStageGenerator &Q3DSSubsetMaterialVertexPipeline::activeStage()
{
    return vertex();
}

QT_END_NAMESPACE
