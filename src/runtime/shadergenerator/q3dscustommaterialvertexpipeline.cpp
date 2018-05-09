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
    Q3DSCustomMaterialInstance *m_materialInstance = nullptr;
    Q3DSCustomMaterial *m_currentMaterial = nullptr;
    Q3DSReferencedMaterial *m_referencedMaterial = nullptr;
    Q3DSDefaultVertexPipeline *m_currentPipeline = nullptr;
    Q3DSShaderFeatureSet m_currentFeatureSet;
    QVector<Q3DSLightNode*> m_lights;
    bool m_hasTransparency;

    QString m_imageStem;
    QString m_imageSampler;
    QString m_imageFragCoords;
    QString m_imageRotations;
    QString m_imageOffset;

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
        m_imageRotations = m_imageStem;
        m_imageRotations.append(QLatin1String("rotations"));
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

    void generateLightmapIndirectFunc(Q3DSAbstractShaderStageGenerator &fragmentShader, Q3DSImage *indirectLightmap)
    {
        fragmentShader << "\n";
        fragmentShader << "vec3 computeMaterialLightmapIndirect()\n{\n";
        fragmentShader << "  vec4 indirect = vec4( 0.0, 0.0, 0.0, 0.0 );\n";
        if (indirectLightmap) {
            setupImageVariableNames(QStringLiteral("lightmapIndirect"));
            fragmentShader.addUniform(m_imageSampler.toLatin1(), "sampler2D");
            fragmentShader.addUniform(m_imageOffset.toLatin1(), "vec3");
            fragmentShader.addUniform(m_imageRotations.toLatin1(), "vec4");

            fragmentShader << "\n  indirect = evalIndirectLightmap( " << m_imageSampler.toLatin1()
                             << ", varTexCoord1, ";
            fragmentShader << m_imageRotations.toLatin1() << ", ";
            fragmentShader << m_imageOffset.toLatin1() << " );\n\n";
        }

        fragmentShader << "  return indirect.rgb;\n";
        fragmentShader << "}\n\n";
    }

    void generateLightmapRadiosityFunc(Q3DSAbstractShaderStageGenerator &fragmentShader, Q3DSImage *radiosityLightmap)
    {
        fragmentShader << "\n";
        fragmentShader << "vec3 computeMaterialLightmapRadiosity()\n{\n";
        fragmentShader << "  vec4 radiosity = vec4( 1.0, 1.0, 1.0, 1.0 );\n";
        if (radiosityLightmap) {
            setupImageVariableNames(QStringLiteral("lightmapRadiosity"));
            fragmentShader.addUniform(m_imageSampler.toLatin1(), "sampler2D");
            fragmentShader.addUniform(m_imageOffset.toLatin1(), "vec3");
            fragmentShader.addUniform(m_imageRotations.toLatin1(), "vec4");

            fragmentShader << "\n  radiosity = evalRadiosityLightmap( " << m_imageSampler.toLatin1()
                             << ", varTexCoord1, ";
            fragmentShader << m_imageRotations.toLatin1() << ", ";
            fragmentShader << m_imageOffset.toLatin1() << " );\n\n";
        }

        fragmentShader << "  return radiosity.rgb;\n";
        fragmentShader << "}\n\n";
    }

    void generateLightmapShadowFunc(Q3DSAbstractShaderStageGenerator &fragmentShader, Q3DSImage *shadowLightmap)
    {
        fragmentShader << "\n";
        fragmentShader << "vec4 computeMaterialLightmapShadow()\n{\n";
        fragmentShader << "  vec4 shadowMask = vec4( 1.0, 1.0, 1.0, 1.0 );\n";
        if (shadowLightmap) {
            setupImageVariableNames(QStringLiteral("lightmapShadow"));
            // Add uniforms
            fragmentShader.addUniform(m_imageSampler.toLatin1(), "sampler2D");
            fragmentShader.addUniform(m_imageOffset.toLatin1(), "vec3");
            fragmentShader.addUniform(m_imageRotations.toLatin1(), "vec4");

            fragmentShader << "\n  shadowMask = evalShadowLightmap( " << m_imageSampler.toLatin1()
                             << ", texCoord0, ";
            fragmentShader << m_imageRotations.toLatin1() << ", ";
            fragmentShader << m_imageOffset.toLatin1() << " );\n\n";
        }

        fragmentShader << "  return shadowMask;\n";
        fragmentShader << "}\n\n";
    }

    void generateLightmapIndirectSetupCode(Q3DSAbstractShaderStageGenerator &fragmentShader,
                                           Q3DSImage *indirectLightmap,
                                           Q3DSImage *radiosityLightmap)
    {
        if (!indirectLightmap && !radiosityLightmap)
            return;

        QString finalValue;

        fragmentShader << "\n";
        fragmentShader << "void initializeLayerVariablesWithLightmap(void)\n{\n";
        if (indirectLightmap) {
            fragmentShader
                << "  vec3 lightmapIndirectValue = computeMaterialLightmapIndirect( );\n";
            finalValue.append(QStringLiteral("vec4(lightmapIndirectValue, 1.0)"));
        }
        if (radiosityLightmap) {
            fragmentShader
                << "  vec3 lightmapRadisoityValue = computeMaterialLightmapRadiosity( );\n";
            if (finalValue.isEmpty())
                finalValue.append(QStringLiteral("vec4(lightmapRadisoityValue, 1.0)"));
            else
                finalValue.append(QStringLiteral(" + vec4(lightmapRadisoityValue, 1.0)"));
        }

        finalValue.append(QStringLiteral(";\n"));

        for (int idx = 0; idx < m_currentMaterial->layerCount(); idx++) {
            fragmentShader << "  layers" << QStringLiteral("[%1]").arg(idx).toLatin1() << ".base += " << finalValue.toLatin1();
            fragmentShader << "  layers" << QStringLiteral("[%1]").arg(idx).toLatin1() << ".layer += " << finalValue.toLatin1();
        }

        fragmentShader << "}\n\n";
    }

    void generateLightmapShadowCode(Q3DSAbstractShaderStageGenerator &fragmentShader, Q3DSImage *shadowMap)
    {
        if (shadowMap) {
            fragmentShader << " tmpShadowTerm *= computeMaterialLightmapShadow( );\n\n";
        }
    }

    void generateVertexShader(const QString &shaderName)
    {
        Q_UNUSED(shaderName);
        // XXX TODO include vertex data (I dont think 3DS even supports this)

        // the pipeline opens/closes up the shaders stages
        // XXX add displacement map
        vertexGenerator().beginVertexGeneration(nullptr);
    }

    void generateFragmentShader(const QString &shaderName)
    {
        // Get the shader source from the Q3DSCustomMaterial based
        // on the name provided.  If there is only 1 shader then
        // the name will usually be empty
        QString fragSource;
        for (const auto &shader : m_currentMaterial->shaders()) {
            if (shader.name == shaderName)
                fragSource = shader.shared + shader.fragmentShader;
        }
        if (fragSource.isEmpty() &&
            shaderName.isEmpty() &&
            m_currentMaterial->shaders().count() == 1) {
            // fallback to first shader
            auto shader = m_currentMaterial->shaders().first();
            fragSource = shader.fragmentShader;
        }
        Q_ASSERT(!fragSource.isEmpty());

        // By default all custom materials have lighting enabled
        bool hasLighting = true;
        bool hasLightmaps = false;
        Q3DSImage *lightmapIndirectImage = nullptr;
        if (m_referencedMaterial && m_referencedMaterial->lightmapIndirectMap())
            lightmapIndirectImage = m_referencedMaterial->lightmapIndirectMap();
        else if (m_materialInstance->lightmapIndirectMap())
            lightmapIndirectImage = m_materialInstance->lightmapIndirectMap();

        Q3DSImage *lightmapRadiosityImage = nullptr;
        if (m_referencedMaterial && m_referencedMaterial->lightmapRadiosityMap())
            lightmapRadiosityImage = m_referencedMaterial->lightmapRadiosityMap();
        else if (m_materialInstance->lightmapRadiosityMap())
            lightmapRadiosityImage = m_materialInstance->lightmapRadiosityMap();

        Q3DSImage *lightmapShadowImage = nullptr;
        if (m_referencedMaterial && m_referencedMaterial->lightmapShadowMap())
            lightmapShadowImage = m_referencedMaterial->lightmapShadowMap();
        else if (m_materialInstance->lightmapShadowMap())
            lightmapShadowImage = m_materialInstance->lightmapShadowMap();
        hasLightmaps = lightmapIndirectImage || lightmapRadiosityImage || lightmapShadowImage;

        vertexGenerator().generateUVCoords(0);
        if (hasLightmaps)
            vertexGenerator().generateUVCoords(1);

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

        if (hasLighting && lightmapIndirectImage) {
            generateLightmapIndirectFunc(fragmentShader, lightmapIndirectImage);
        }
        if (hasLighting && lightmapRadiosityImage) {
            generateLightmapRadiosityFunc(fragmentShader, lightmapRadiosityImage);
        }
        if (hasLighting && lightmapShadowImage) {
            generateLightmapShadowFunc(fragmentShader, lightmapShadowImage);
        }

        if (hasLighting && (lightmapIndirectImage || lightmapRadiosityImage))
            generateLightmapIndirectSetupCode(fragmentShader, lightmapIndirectImage, lightmapRadiosityImage);

        // applyEmmisiveMask
        if (hasLighting) {
            fragmentShader << "\n";
            fragmentShader << "vec3 computeMaterialEmissiveMask()\n{\n";
            fragmentShader << "  vec3 emissiveMask = vec3( 1.0, 1.0, 1.0 );\n";
            QString emissiveMaskMapName = m_currentMaterial->emissiveMaskMapName();
            if (!emissiveMaskMapName.isEmpty()) {
                fragmentShader << "  texture_coordinate_info tci;\n";
                fragmentShader << "  texture_coordinate_info transformed_tci;\n";
                fragmentShader << "  tci = textureCoordinateInfo( texCoord0, tangent, binormal );\n";
                fragmentShader << "  transformed_tci = transformCoordinate( "
                                    "rotationTranslationScale( vec3( 0.000000, 0.000000, 0.000000 ), ";
                fragmentShader << "vec3( 0.000000, 0.000000, 0.000000 ), vec3( 1.000000, 1.000000, "
                                    "1.000000 ) ), tci );\n";
                fragmentShader << "  emissiveMask = fileTexture( "
                               << emissiveMaskMapName.toLocal8Bit()
                               << ", vec3( 0, 0, 0 ), vec3( 1, 1, 1 ), mono_alpha, transformed_tci, ";
                fragmentShader << "vec2( 0.000000, 1.000000 ), vec2( 0.000000, 1.000000 ), "
                                    "wrap_repeat, wrap_repeat, gamma_default ).tint;\n";
            }

            fragmentShader << "  return emissiveMask;\n";
            fragmentShader << "}\n\n";
        }

        // Add Uniforms from material properties
        for (auto property : m_currentMaterial->properties()) {
            switch (property.type) {
            case Q3DS::Boolean:
                fragmentShader.addUniform(property.name.toLocal8Bit(), "bool");
                break;
            case Q3DS::Long:
                fragmentShader.addUniform(property.name.toLocal8Bit(), "int");
                break;
            case Q3DS::FloatRange:
            case Q3DS::Float:
            case Q3DS::FontSize:
                fragmentShader.addUniform(property.name.toLocal8Bit(), "float");
                break;
            case Q3DS::Float2:
                fragmentShader.addUniform(property.name.toLocal8Bit(), "vec2");
                break;
            case Q3DS::Vector:
            case Q3DS::Scale:
            case Q3DS::Rotation:
            case Q3DS::Color:
                fragmentShader.addUniform(property.name.toLocal8Bit(), "vec3");
                break;
            case Q3DS::Texture:
                fragmentShader.addUniform(property.name.toLocal8Bit(), "sampler2D");
                break;
            case Q3DS::StringList:
                fragmentShader.addUniform(property.name.toLocal8Bit(), "int");
                break;
            default:
                break;
            }
        }


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

        // indirect / direct lightmap init
        if (hasLighting && (lightmapIndirectImage || lightmapRadiosityImage))
            fragmentShader << "  initializeLayerVariablesWithLightmap();\n";

        // shadow map
        generateLightmapShadowCode(fragmentShader, lightmapShadowImage);

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

        fragmentShader << "  rgba.a *= object_opacity;\n";
        fragmentShader << "  fragColor = rgba;\n";
    }

    Qt3DRender::QShaderProgram *generateCustomMaterialShader(const QString &shaderName)
    {
        generateVertexShader(shaderName);
        generateFragmentShader(shaderName);

        vertexGenerator().endVertexGeneration();
        vertexGenerator().endFragmentGeneration();

        return programGenerator()->compileGeneratedShader(shaderName, m_currentFeatureSet);
    }

    Qt3DRender::QShaderProgram *generateShader(Q3DSGraphObject &material,
                                               Q3DSReferencedMaterial *referencedMaterial,
                                               Q3DSAbstractShaderStageGenerator &vertexPipeline,
                                               const Q3DSShaderFeatureSet &featureSet,
                                               const QVector<Q3DSLightNode *> &lights,
                                               bool hasTransparency,
                                               const QString &shaderName) override
    {
        if (material.type() != Q3DSGraphObject::CustomMaterial)
            return nullptr;

        m_materialInstance = static_cast<Q3DSCustomMaterialInstance*>(&material);
        m_currentMaterial = const_cast<Q3DSCustomMaterial*>(m_materialInstance->material());
        m_referencedMaterial = referencedMaterial;
        m_currentPipeline = static_cast<Q3DSCustomMaterialVertexPipeline*>(&vertexPipeline);
        m_programGenerator = &static_cast<Q3DSVertexPipelineImpl*>(&vertexPipeline)->programGenerator();
        m_currentFeatureSet = featureSet;
        m_lights = lights;
        m_hasTransparency = hasTransparency;

        return generateCustomMaterialShader(shaderName);
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

            vertexShader.addInclude("defaultMaterialFileDisplacementTexture.glsllib");
            Q3DSDefaultMaterialShaderGenerator::ImageVariableNames theVarNames =
                    materialGenerator().getImageVariableNames(QLatin1String("displacementMap"));

            const QByteArray imageSampler = theVarNames.imageSampler.toUtf8();
            const QByteArray imageFragCoords = theVarNames.imageFragCoords.toUtf8();

            vertexShader.addUniform(imageSampler.constData(), "sampler2D");

            vertexShader
                    << "\tvec3 displacedPos = defaultMaterialFileDisplacementTexture( "
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
    fragment().addUniform("object_opacity", "float");
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
        addInterpolationParameter("varTexCoord0", "vec3");
    else if (inUVSet == 1)
        addInterpolationParameter("varTexCoord1", "vec3");

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
    vertex().append("\tvec4 worldPos = (modelMatrix * vec4(attr_pos, 1.0));");
    assignOutput("varWorldPos", "worldPos.xyz");
}

void Q3DSCustomMaterialVertexPipeline::doGenerateVarTangentAndBinormal()
{
    vertex().addIncoming("attr_textan", "vec3");
    vertex().addIncoming("attr_binormal", "vec3");

    vertex() << "\tvarTangent = modelNormalMatrix * attr_textan;" << "\n"
             << "\tvarBinormal = modelNormalMatrix * attr_binormal;" << "\n";

    vertex() << "\tvarObjTangent = attr_textan;\n" <<
                "\tvarObjBinormal = attr_binormal;\n";
}

void Q3DSCustomMaterialVertexPipeline::doGenerateVertexColor()
{
    vertex().addIncoming("attr_color", "vec3");
    vertex() << "\tvarColor = attr_color;";
}

QT_END_NAMESPACE
