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

#include "q3dscustommaterialvertexpipeline_p.h"
#include "q3dsshadermanager_p.h"

QT_BEGIN_NAMESPACE

namespace {
struct ShaderGenerator : public Q3DSCustomMaterialShaderGenerator
{
    Q3DSAbstractShaderProgramGenerator *m_programGenerator = nullptr;
    Q3DSCustomMaterial *m_currentMaterial = nullptr;
    Q3DSDefaultVertexPipeline *m_currentPipeline = nullptr;
    Q3DSShaderFeatureSet m_currentFeatureSet;
    QVector<Q3DSLightNode*> m_lights;
    bool m_hasTransparency;

    QString m_imageStem;
    QString m_imageSampler;
    QString m_imageFragCoords;
    QString m_imageRotScale;
    QString m_imageOffset;

    QString m_GeneratedShaderString;

    ShaderGenerator()
    {

    }

    Q3DSAbstractShaderProgramGenerator *programGenerator() { return m_programGenerator; }
    Q3DSDefaultVertexPipeline &vertexGenerator() { return *m_currentPipeline; }
    Q3DSAbstractShaderStageGenerator &fragmentGenerator()
    {
        return *m_programGenerator->getStage(Q3DSShaderGeneratorStages::Enum::Fragment);
    }
    const Q3DSCustomMaterial &material() { return *m_currentMaterial; }
    Q3DSShaderFeatureSet featureSet() { return m_currentFeatureSet; }
    bool hasTransparency() { return m_hasTransparency; }

    void setupImageVariableNames(const QString &name)
    {
        m_imageStem = name;
        m_imageStem.append(QLatin1String("_"));

        m_imageSampler = m_imageStem;
        m_imageSampler.append(QLatin1String("sampler"));
        m_imageOffset = m_imageStem;
        m_imageOffset.append(QLatin1String("offsets"));
        m_imageRotScale = m_imageStem;
        m_imageRotScale.append(QLatin1String("rot_scale"));
        m_imageFragCoords = m_imageStem;
        m_imageFragCoords.append(QLatin1String("uv_coords"));
    }

    ImageVariableNames getImageVariableNames(const QString &name) override
    {
        setupImageVariableNames(name);
        ImageVariableNames retval;
        retval.imageSampler = m_imageSampler;
        retval.imageFragCoords = m_imageFragCoords;
        return retval;
    }
    void generateImageUVCoordinates(Q3DSAbstractShaderStageGenerator &vertexPipeline, const QString &name, quint32 uvSet, Q3DSImage &image) override
    {
        // XXX TODO ImageUVCoordinates
        Q_UNUSED(vertexPipeline)
        Q_UNUSED(name)
        Q_UNUSED(uvSet)
        Q_UNUSED(image)
    }

    void generateVertexShader(const QString &materialName)
    {
        Q_UNUSED(materialName)
        // XXX TODO include vertex data (I dont think 3DS even supports this)

        // the pipeline opens/closes up the shaders stages
        // XXX add displacement map
        vertexGenerator().beginVertexGeneration(nullptr);
    }

    void generateFragmentShader(const QString &materialName)
    {
        // Get the shader source from the Q3DSCustomMaterial based
        // on the name provided.  If there is only 1 shader then
        // the name will usually be empty
        QString fragSource;
        for (const auto &shader : m_currentMaterial->shaders()) {
            if (shader.name == materialName)
                fragSource = shader.shared + shader.fragmentShader;
        }
        if (fragSource.isEmpty() &&
            materialName.isEmpty() &&
            m_currentMaterial->shaders().count() == 1) {
            // fallback to first shader
            auto shader = m_currentMaterial->shaders().first();
            fragSource = shader.shared + shader.fragmentShader;
        }
        Q_ASSERT(!fragSource.isEmpty());

        // By default all custom materials have lighting enabled
        bool hasLighting = true;

        // XXX TODO handle light maps

//        bool hasLightmaps = false;
//        SRenderableImage *lightmapShadowImage = NULL;
//        SRenderableImage *lightmapIndirectImage = NULL;
//        SRenderableImage *lightmapRadisoityImage = NULL;

//        for (SRenderableImage *img = m_FirstImage; img != NULL; img = img->m_NextImage) {
//            if (img->m_MapType == ImageMapTypes::LightmapIndirect) {
//                lightmapIndirectImage = img;
//                hasLightmaps = true;
//            } else if (img->m_MapType == ImageMapTypes::LightmapRadiosity) {
//                lightmapRadisoityImage = img;
//                hasLightmaps = true;
//            } else if (img->m_MapType == ImageMapTypes::LightmapShadow) {
//                lightmapShadowImage = img;
//            }
//        }

        vertexGenerator().generateUVCoords(0);
        // XXX for lightmaps we expect a second set of uv coordinates
//        if (hasLightmaps)
//            vertexGenerator().generateUVCoords(1);

        Q3DSDefaultVertexPipeline &vertexShader(vertexGenerator());
        Q3DSAbstractShaderStageGenerator &fragmentShader(fragmentGenerator());

        QString srcString(fragSource);

        fragmentShader << "#define FRAGMENT_SHADER\n\n";

        if (!srcString.contains(QStringLiteral("void main()")))
            fragmentShader.addInclude("evalLightmaps.glsllib");

        // check dielectric materials
        if (!m_currentMaterial->shaderIsDielectric())
            fragmentShader << "#define MATERIAL_IS_NON_DIELECTRIC 1\n\n";
        else
            fragmentShader << "#define MATERIAL_IS_NON_DIELECTRIC 0\n\n";

        fragmentShader << "#define QT3DS_ENABLE_RNM 0\n\n";

        fragmentShader << fragSource.toLocal8Bit() << QStringLiteral("\n").toLocal8Bit();

        // If a "main()" is already
        // written, we'll assume that the
        // shader pass is already written out
        // and we don't need to add anything.
        if (srcString.contains(QStringLiteral("void main()")))
        {
            // Nothing beyond the basics, anyway
            vertexShader.generateWorldNormal();
            vertexShader.generateVarTangentAndBinormal();
            vertexShader.generateWorldPosition();

            vertexShader.generateViewVector();
            return;
        }

        // XXX TODO special lighting
//        if (m_currentMaterial->shaderHasLighting() && lightmapIndirectImage) {
//            GenerateLightmapIndirectFunc(fragmentShader, &lightmapIndirectImage->m_Image);
//        }
//        if (Material().HasLighting() && lightmapRadisoityImage) {
//            GenerateLightmapRadiosityFunc(fragmentShader, &lightmapRadisoityImage->m_Image);
//        }
//        if (Material().HasLighting() && lightmapShadowImage) {
//            GenerateLightmapShadowFunc(fragmentShader, &lightmapShadowImage->m_Image);
//        }

//        if (Material().HasLighting() && (lightmapIndirectImage || lightmapRadisoityImage))
//            GenerateLightmapIndirectSetupCode(fragmentShader, lightmapIndirectImage,
//                                              lightmapRadisoityImage);

//        if (Material().HasLighting()) {
//            ApplyEmissiveMask(fragmentShader, Material().m_EmissiveMap2);
//        }

        // setup main
        vertexGenerator().beginFragmentGeneration();

        // since we do pixel lighting we always need this if lighting is enabled
        // We write this here because the functions below may also write to
        // the fragment shader
        if (hasLighting) {
            vertexShader.generateWorldNormal();
            vertexShader.generateVarTangentAndBinormal();
            vertexShader.generateWorldPosition();

            if (m_currentMaterial->shaderIsSpecular())
                vertexShader.generateViewVector();
        }

        fragmentShader << "  initializeBaseFragmentVariables();\n";
        fragmentShader << "  computeTemporaries();\n";
        fragmentShader << "  normal = normalize( computeNormal() );\n";
        fragmentShader << "  initializeLayerVariables();\n";
        fragmentShader << "  float alpha = clamp( evalCutout(), 0.0, 1.0 );\n";

        if (m_currentMaterial->shaderIsCutoutEnabled()) {
            fragmentShader << "  if ( alpha <= 0.0f )\n";
            fragmentShader << "    discard;\n";
        }

//        // indirect / direct lightmap init
//        if (hasLighting && (lightmapIndirectImage || lightmapRadisoityImage))
//            fragmentShader << "  initializeLayerVariablesWithLightmap();" << Endl;

//        // shadow map
//        GenerateLightmapShadowCode(fragmentShader, lightmapShadowImage);

        // main Body
        fragmentShader << "#include \"customMaterialFragBodyAO.glsllib\"\n";

        // for us right now transparency means we render a glass style material
        if (m_currentMaterial->shaderHasTransparency() && !m_currentMaterial->shaderIsTransmissive())
            fragmentShader << " rgba = computeGlass( normal, materialIOR, alpha, rgba );\n";
        if (m_currentMaterial->shaderIsTransmissive())
            fragmentShader << " rgba = computeOpacity( rgba );\n";

        if (vertexGenerator().hasActiveWireframe()) {
            fragmentShader.append("vec3 edgeDistance = varEdgeDistance * gl_FragCoord.w;");
            fragmentShader.append(
                "\tfloat d = min(min(edgeDistance.x, edgeDistance.y), edgeDistance.z);");
            fragmentShader.append("\tfloat mixVal = smoothstep(0.0, 1.0, d);"); // line width 1.0

            fragmentShader.append("\trgba = mix( vec4(0.0, 1.0, 0.0, 1.0), rgba, mixVal);");
        }

        fragmentShader << "  fragColor = rgba;\n";
    }

    Qt3DRender::QShaderProgram *generateCustomMaterialShader(const QString &shaderPrefix, const QString &customMaterialName)
    {
        m_GeneratedShaderString = shaderPrefix;
        generateVertexShader(customMaterialName);
        generateFragmentShader(customMaterialName);

        vertexGenerator().endVertexGeneration();
        vertexGenerator().endFragmentGeneration();

        return programGenerator()->compileGeneratedShader(m_GeneratedShaderString, m_currentFeatureSet);
    }

    Qt3DRender::QShaderProgram *generateShader(Q3DSGraphObject &material,
                                               Q3DSAbstractShaderStageGenerator &vertexPipeline,
                                               const Q3DSShaderFeatureSet &featureSet,
                                               const QVector<Q3DSLightNode *> &lights,
                                               bool hasTransparency,
                                               const QString &shaderPrefix,
                                               const QString &materialName) override
    {
        if (material.type() != Q3DSGraphObject::CustomMaterial)
            return nullptr;

        m_currentMaterial = const_cast<Q3DSCustomMaterial*>(static_cast<Q3DSCustomMaterialInstance*>(&material)->material());
        m_currentPipeline = static_cast<Q3DSCustomMaterialVertexPipeline*>(&vertexPipeline);
        m_programGenerator = &static_cast<Q3DSVertexPipelineImpl*>(&vertexPipeline)->programGenerator();
        m_currentFeatureSet = featureSet;
        m_lights = lights;
        m_hasTransparency = hasTransparency;

        return generateCustomMaterialShader(shaderPrefix, materialName);
    }
};
}

Q3DSCustomMaterialVertexPipeline::Q3DSCustomMaterialVertexPipeline(Q3DSDefaultMaterialShaderGenerator &inMaterial,
                                                                   Q3DSAbstractShaderProgramGenerator &inProgram,
                                                                   bool inWireframe)
    : Q3DSSubsetMaterialVertexPipeline(inMaterial, inProgram, inWireframe)
{

}

void Q3DSCustomMaterialVertexPipeline::beginVertexGeneration(Q3DSImage *displacementImage)
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

    vertexShader.addInclude("viewProperties.glsllib");
    vertexShader.addInclude("customMaterial.glsllib");

    vertexShader.addIncoming("attr_pos", "vec3");
    vertexShader << "void main()" << "\n" << "{" << "\n";

    if (displacementImage) {
        generateUVCoords(0);
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

void Q3DSCustomMaterialVertexPipeline::beginFragmentGeneration()
{
    // Add "customMaterial.glsllib" because we dont apply it on load
    // like Qt3DStudio does.
    fragment().addInclude("customMaterial.glsllib");

    fragment() << "void main()" << "\n" << "{" << "\n";
}

Q3DSCustomMaterialShaderGenerator &Q3DSCustomMaterialShaderGenerator::createCustomMaterialShaderGenerator()
{
    static ShaderGenerator *shaderGenerator = nullptr;
    if (!shaderGenerator)
        shaderGenerator = new ShaderGenerator();
    return *shaderGenerator;
}

void Q3DSCustomMaterialVertexPipeline::generateUVCoords(quint32 inUVSet)
{
    if (inUVSet == 0 && setCode(GenerationFlagValues::UVCoords))
        return;
    if (inUVSet == 1 && setCode(GenerationFlagValues::UVCoords1))
        return;

    Q_ASSERT(inUVSet == 0 || inUVSet == 1);

    if (inUVSet == 0)
        addInterpolationParameter("varTexCoord0", "vec2");
    else if (inUVSet == 1)
        addInterpolationParameter("varTexCoord1", "vec2");

    doGenerateUVCoords(inUVSet);
}

void Q3DSCustomMaterialVertexPipeline::generateEnvMapReflection()
{
    // Do Nothing
}

void Q3DSCustomMaterialVertexPipeline::generateViewVector()
{
    // Do Nothing
}

void Q3DSCustomMaterialVertexPipeline::generateWorldNormal()
{
    if (setCode(GenerationFlagValues::WorldNormal))
        return;
    addInterpolationParameter("varNormal", "vec3");
    doGenerateWorldNormal();
}

void Q3DSCustomMaterialVertexPipeline::generateObjectNormal()
{
    if (setCode(GenerationFlagValues::ObjectNormal))
        return;
    doGenerateObjectNormal();
}

void Q3DSCustomMaterialVertexPipeline::generateWorldPosition()
{
    if (setCode(GenerationFlagValues::WorldPosition))
        return;

    activeStage().addUniform("modelMatrix", "mat4");
    addInterpolationParameter("varWorldPos", "vec3");
    addInterpolationParameter("varObjPos", "vec3");
    doGenerateWorldPosition();
}

void Q3DSCustomMaterialVertexPipeline::generateVarTangentAndBinormal()
{
    if (setCode(GenerationFlagValues::TangentBinormal))
        return;
    addInterpolationParameter("varTangent", "vec3");
    addInterpolationParameter("varBinormal", "vec3");
    addInterpolationParameter("varObjTangent", "vec3");
    addInterpolationParameter("varObjBinormal", "vec3");
    doGenerateVarTangentAndBinormal();
}

void Q3DSCustomMaterialVertexPipeline::doGenerateUVCoords(quint32 inUVSet)
{
    Q_ASSERT(inUVSet == 0 || inUVSet == 1);

    if (inUVSet == 0) {
        vertex().addIncoming("attr_uv0", "vec2");
        vertex() << "\tvec3 texCoord0 = vec3( attr_uv0, 0.0 );" << "\n";
        assignOutput("varTexCoord0", "texCoord0");
    } else if (inUVSet == 1) {
        vertex().addIncoming("attr_uv1", "vec2");
        vertex() << "\tvec3 texCoord1 = vec3( attr_uv1, 1.0 );" << "\n";
        assignOutput("varTexCoord1", "texCoord1");
    }
}

void Q3DSCustomMaterialVertexPipeline::doGenerateWorldNormal()
{
    Q3DSAbstractShaderStageGenerator &vertexGenerator(vertex());
    vertexGenerator.addIncoming("attr_norm", "vec3");
    vertexGenerator.addUniform("modelNormalMatrix", "mat3");
    if (hasTessellation() == false) {
        vertexGenerator.append(
                    "\tvarNormal = normalize(modelNormalMatrix * attr_norm );");
    }
}

void Q3DSCustomMaterialVertexPipeline::doGenerateObjectNormal()
{
    addInterpolationParameter("varObjectNormal", "vec3");
    vertex().append("\tvarObjectNormal = attr_norm;");
}

void Q3DSCustomMaterialVertexPipeline::doGenerateWorldPosition()
{
    vertex().append("\tvarObjPos = attr_pos;");
    vertex().append("\tvec4 worldPos = (model_matrix * vec4(attr_pos, 1.0));");
    assignOutput("varWorldPos", "worldPos.xyz");
}

void Q3DSCustomMaterialVertexPipeline::doGenerateVarTangentAndBinormal()
{
    vertex().addIncoming("attr_textan", "vec3");
    vertex().addIncoming("attr_binormal", "vec3");

    vertex() << "\tvarTangent = normal_matrix * attr_textan;" << "\n"
             << "\tvarBinormal = normal_matrix * attr_binormal;" << "\n";

    vertex() << "\tvarObjTangent = attr_textan;\n" <<
                "\tvarObjBinormal = attr_binormal;\n";
}

QT_END_NAMESPACE
