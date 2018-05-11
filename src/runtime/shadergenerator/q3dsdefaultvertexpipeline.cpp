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

#include "q3dsdefaultvertexpipeline_p.h"

QT_BEGIN_NAMESPACE

namespace {
struct ShaderGenerator : public Q3DSDefaultMaterialShaderGenerator
{
    Q3DSAbstractShaderProgramGenerator *m_ProgramGenerator;

    Q3DSDefaultMaterial *m_CurrentMaterial;
    Q3DSReferencedMaterial *m_referencedMaterial;

    Q3DSDefaultVertexPipeline *m_CurrentPipeline;
    Q3DSShaderFeatureSet m_CurrentFeatureSet;
    QVector<Q3DSLightNode*> m_Lights;
    Q3DSImage *m_FirstImage;
    bool m_HasTransparency;

    // ### Change these to QByteArray some day. Using QString only generates
    // lots of unnecessary conversions.

    QString m_ImageStem;
    QString m_ImageSampler;
    QString m_ImageOffsets;
    QString m_ImageRotations;
    QString m_ImageFragCoords;
    QString m_ImageTemp;
    QString m_ImageSize;

    QString m_TexCoordTemp;

    QString m_LightStem;
    QString m_LightColor;
    QString m_LightSpecularColor;
    QString m_lightAttenuation;
    QString m_LightConstantAttenuation;
    QString m_LightLinearAttenuation;
    QString m_LightQuadraticAttenuation;
    QString m_NormalizedDirection;
    QString m_LightDirection;
    QString m_LightPos;
    QString m_LightUp;
    QString m_LightRt;
    QString m_RelativeDistance;
    QString m_RelativeDirection;

    QString m_ShadowMapStem;
    QString m_ShadowCubeStem;
    QString m_ShadowMatrixStem;
    QString m_ShadowCoordStem;
    QString m_ShadowControlStem;

    QString m_TempStr;

    QString m_GeneratedShaderString;

    ShaderGenerator()
        : m_ProgramGenerator(nullptr)
        , m_CurrentMaterial(nullptr)
        , m_CurrentPipeline(nullptr)
        , m_FirstImage(nullptr)
    {
    }

    Q3DSAbstractShaderProgramGenerator *programGenerator() { return m_ProgramGenerator; }
    Q3DSDefaultVertexPipeline &vertexGenerator() { return *m_CurrentPipeline; }
    Q3DSAbstractShaderStageGenerator &fragmentGenerator()
    {
        return *m_ProgramGenerator->getStage(Q3DSShaderGeneratorStages::Enum::Fragment);
    }
    const Q3DSDefaultMaterial &material() { return *m_CurrentMaterial; }
    Q3DSShaderFeatureSet featureSet() { return m_CurrentFeatureSet; }
    bool hasTransparency() { return m_HasTransparency; }

    void setupImageVariableNames(const QString &name)
    {
        m_ImageStem = name;
        m_ImageStem.append(QLatin1String("_"));

        m_ImageSampler = m_ImageStem;
        m_ImageSampler.append(QLatin1String("sampler"));
        m_ImageOffsets = m_ImageStem;
        m_ImageOffsets.append(QLatin1String("offsets"));
        m_ImageRotations = m_ImageStem;
        m_ImageRotations.append(QLatin1String("rotations"));
        m_ImageFragCoords = m_ImageStem;
        m_ImageFragCoords.append(QLatin1String("uv_coords"));
        m_ImageSize = m_ImageStem;
        m_ImageSize.append(QLatin1String("size"));
    }

    void setupTexCoordVariableName(size_t uvSet)
    {
        m_TexCoordTemp = QLatin1String("varTexCoord");
        m_TexCoordTemp.append(QString::number(uvSet));
    }

    ImageVariableNames getImageVariableNames(const QString &name) override
    {
        setupImageVariableNames(name);
        ImageVariableNames retval;
        retval.imageSampler = m_ImageSampler;
        retval.imageFragCoords = m_ImageFragCoords;
        return retval;
    }

    void addLocalVariable(Q3DSAbstractShaderStageGenerator &inGenerator, const char *inName,
                          const char *inType)
    {
        inGenerator << "\t" << inType << " " << inName << ";" << "\n";
    }

    void addFunction(Q3DSAbstractShaderStageGenerator &generator, const char *functionName)
    {
        generator.addFunction(functionName);
    }

    void generateImageUVCoordinates(Q3DSAbstractShaderStageGenerator &inVertexPipeline, const QString &name, quint32 uvSet,
                                    Q3DSImage &image) override
    {
        Q3DSDefaultVertexPipeline &vertexShader(
                    static_cast<Q3DSDefaultVertexPipeline &>(inVertexPipeline));
        Q3DSAbstractShaderStageGenerator &fragmentShader(fragmentGenerator());
        setupImageVariableNames(name);
        setupTexCoordVariableName(uvSet);
        const QByteArray imageSampler = m_ImageSampler.toUtf8();
        const QByteArray imageOffsets = m_ImageOffsets.toUtf8();
        const QByteArray imageRotations = m_ImageRotations.toUtf8();
        fragmentShader.addUniform(imageSampler.constData(), "sampler2D");
        vertexShader.addUniform(imageOffsets.constData(), "vec3");
        fragmentShader.addUniform(imageOffsets.constData(), "vec3");
        vertexShader.addUniform(imageRotations.constData(), "vec4");
        fragmentShader.addUniform(imageRotations.constData(), "vec4");

        if (image.mappingMode() == Q3DSImage::MappingMode::UVMapping) { // aka Normal
            vertexShader << "\tuTransform = vec3( " << imageRotations.constData() << ".x, "
                         << imageRotations.constData() << ".y, " << imageOffsets.constData() << ".x );" << "\n";
            vertexShader << "\tvTransform = vec3( " << imageRotations.constData() << ".z, "
                         << imageRotations.constData() << ".w, " << imageOffsets.constData() << ".y );" << "\n";
            const QByteArray imageFragCoords = m_ImageFragCoords.toUtf8();
            vertexShader.addOutgoing(imageFragCoords.constData(), "vec2");
            addFunction(vertexShader, "getTransformedUVCoords");
            vertexShader.generateUVCoords(uvSet);
            m_ImageTemp = m_ImageFragCoords;
            m_ImageTemp.append(QLatin1String("temp"));
            const QByteArray imageTemp = m_ImageTemp.toUtf8();
            const QByteArray texCoordTemp = m_TexCoordTemp.toUtf8();
            vertexShader << "\tvec2 " << imageTemp.constData() << " = getTransformedUVCoords( vec3( "
                         << texCoordTemp.constData() << ", 1.0), uTransform, vTransform );" << "\n";
            // TODO Support checking for inverted UV Coordinates
            //            if (image.m_Image.m_TextureData.m_TextureFlags.IsInvertUVCoords())
            //                vertexShader << "\t" << m_ImageTemp << ".y = 1.0 - " << m_ImageFragCoords << ".y;"
            //                             << "\n";

            vertexShader.assignOutput(imageFragCoords.constData(), imageTemp.constData());
        } else {
            fragmentShader << "\tuTransform = vec3( " << imageRotations.constData() << ".x, "
                           << imageRotations.constData() << ".y, " << imageOffsets.constData() << ".x );" << "\n";
            fragmentShader << "\tvTransform = vec3( " << imageRotations.constData() << ".z, "
                           << imageRotations.constData() << ".w, " << imageOffsets.constData() << ".y );" << "\n";
            vertexShader.generateEnvMapReflection();
            addFunction(fragmentShader, "getTransformedUVCoords");
            const QByteArray imageFragCoords = m_ImageFragCoords.toUtf8();
            fragmentShader << "\tvec2 " << imageFragCoords.constData()
                           << " = getTransformedUVCoords( environment_map_reflection, uTransform, "
                              "vTransform );"
                           << "\n";
            // TODO Support checking for inverted UV Coordinates
            //            if (image.m_Image.m_TextureData.m_TextureFlags.IsInvertUVCoords())
            //                fragmentShader << "\t" << m_ImageFragCoords << ".y = 1.0 - " << m_ImageFragCoords
            //                               << ".y;" << "\n";
        }
    }

    void generateImageUVCoordinates(const QString &name, Q3DSImage &image, quint32 uvSet = 0)
    {
        generateImageUVCoordinates(vertexGenerator(), name, uvSet, image);
    }

    void generateImageUVCoordinates(const QString &name, Q3DSImage &image,
                                    Q3DSDefaultVertexPipeline &inShader)
    {
        if (image.mappingMode() == Q3DSImage::MappingMode::UVMapping) { // aka Normal
            setupImageVariableNames(name);
            const QByteArray imageSampler = m_ImageSampler.toUtf8();
            const QByteArray imageOffsets = m_ImageOffsets.toUtf8();
            const QByteArray imageRotations = m_ImageRotations.toUtf8();
            inShader.addUniform(imageSampler.constData(), "sampler2D");
            inShader.addUniform(imageOffsets.constData(), "vec3");
            inShader.addUniform(imageRotations.constData(), "vec4");

            const QByteArray imageFragCoords = m_ImageFragCoords.toUtf8();
            inShader << "\tuTransform = vec3( " << imageRotations.constData() << ".x, " << imageRotations.constData()
                     << ".y, " << imageOffsets.constData() << ".x );" << "\n";
            inShader << "\tvTransform = vec3( " << imageRotations.constData() << ".z, " << imageRotations.constData()
                     << ".w, " << imageOffsets.constData() << ".y );" << "\n";
            inShader << "\tvec2 " << imageFragCoords.constData() << ";" << "\n";
            addFunction(inShader, "getTransformedUVCoords");
            inShader.generateUVCoords();
            inShader
                    << "\t" << imageFragCoords.constData()
                    << " = getTransformedUVCoords( vec3( varTexCoord0, 1.0), uTransform, vTransform );"
                    << "\n";
            // TODO Support checking for intverted UV Coordinates
            //            if (image.m_Image.m_TextureData.m_TextureFlags.IsInvertUVCoords())
            //                inShader << "\t" << m_ImageFragCoords << ".y = 1.0 - " << m_ImageFragCoords << ".y;"
            //                         << "\n";
        }
    }

    void outputSpecularEquation(Q3DSDefaultMaterial::SpecularModel inSpecularModel,
                                Q3DSAbstractShaderStageGenerator &fragmentShader, const QString &inLightDir,
                                const QString &inLightSpecColor)
    {
        const QByteArray lightDir = inLightDir.toUtf8();
        const QByteArray lightSpecColor = inLightSpecColor.toUtf8();
        switch (inSpecularModel) {
        case Q3DSDefaultMaterial::SpecularModel::KGGX: {
            fragmentShader.addInclude("defaultMaterialPhysGlossyBSDF.glsllib");
            fragmentShader.addUniform("material_specular", "vec4");
            fragmentShader << "\tglobal_specular_light.rgb += lightAttenuation * specularAmount * "
                              "specularColor * kggxGlossyDefaultMtl( "
                           << "world_normal, tangent, -" << lightDir.constData() << ".xyz, view_vector, "
                           << lightSpecColor.constData()
                           << ".rgb, vec3(material_specular.xyz), roughnessAmount, "
                              "roughnessAmount).rgb;"
                           << "\n";
        } break;
        case Q3DSDefaultMaterial::SpecularModel::KWard: {
            fragmentShader.addInclude("defaultMaterialPhysGlossyBSDF.glsllib");
            fragmentShader.addUniform("material_specular", "vec4");
            fragmentShader << "\tglobal_specular_light.rgb += lightAttenuation * specularAmount * "
                              "specularColor * wardGlossyDefaultMtl( "
                           << "world_normal, tangent, -" << lightDir.constData() << ".xyz, view_vector, "
                           << lightSpecColor.constData()
                           << ".rgb, vec3(material_specular.xyz), roughnessAmount, "
                              "roughnessAmount ).rgb;"
                           << "\n";
        } break;
        default:
            addFunction(fragmentShader, "specularBSDF");
            fragmentShader << "\tglobal_specular_light.rgb += lightAttenuation * specularAmount * "
                              "specularColor * specularBSDF( "
                           << "world_normal, -" << lightDir.constData() << ".xyz, view_vector, "
                           << lightSpecColor.constData() << ".rgb, 1.0, 2.56 / (roughnessAmount + "
                                                  "0.01), vec3(1.0), scatter_reflect ).rgb;"
                           << "\n";
            break;
        }
    }

    void outputDiffuseAreaLighting(Q3DSAbstractShaderStageGenerator &infragmentShader, const QString &inPos,
                                   const QString &inLightPrefix)
    {
        m_lightAttenuation = inLightPrefix + QLatin1String("_attenuation");
        m_NormalizedDirection = inLightPrefix + QLatin1String("_areaDir");
        const QByteArray normalizedDirection = m_NormalizedDirection.toUtf8();
        addLocalVariable(infragmentShader, normalizedDirection.constData(), "vec3");
        const QByteArray lightDirection = m_LightDirection.toUtf8();
        const QByteArray lightPos = m_LightPos.toUtf8();
        const QByteArray lightUp = m_LightUp.toUtf8();
        const QByteArray lightRt = m_LightRt.toUtf8();
        const QByteArray pos = inPos.toUtf8();
        infragmentShader << "\tlightAttenuation = calculateDiffuseAreaOld( " << lightDirection.constData()
                         << ".xyz, " << lightPos.constData() << ".xyz, " << lightUp.constData() << ", " << lightRt.constData()
                         << ", " << pos.constData() << ", " << normalizedDirection.constData() << " );" << "\n";
    }

    void outputSpecularAreaLighting(Q3DSAbstractShaderStageGenerator &infragmentShader, const QString &inPos,
                                    const QString &inView, const QString &inLightSpecColor)
    {
        const QByteArray lightSpecColor = inLightSpecColor.toUtf8();
        addFunction(infragmentShader, "sampleAreaGlossyDefault");
        infragmentShader.addUniform("material_specular", "vec4");
        const QByteArray normalizedDirection = m_NormalizedDirection.toUtf8();
        const QByteArray lightPos = m_LightPos.toUtf8();
        const QByteArray lightUp = m_LightUp.toUtf8();
        const QByteArray lightRt = m_LightRt.toUtf8();
        const QByteArray pos = inPos.toUtf8();
        const QByteArray view = inView.toUtf8();
        infragmentShader << "global_specular_light.rgb += " << lightSpecColor.constData()
                         << ".rgb * lightAttenuation * shadowFac * material_specular.rgb * "
                            "specularColor * specularAmount * sampleAreaGlossyDefault( tanFrame, "
                         << pos.constData() << ", " << normalizedDirection.constData() << ", " << lightPos.constData() << ".xyz, "
                         << lightRt.constData() << ".w, " << lightUp.constData() << ".w, " << view.constData()
                         << ", roughnessAmount, roughnessAmount ).rgb;" << "\n";
    }

    void addTranslucencyIrradiance(Q3DSAbstractShaderStageGenerator &infragmentShader, Q3DSImage *image,
                                   const QString &inLightPrefix, bool areaLight)
    {
        Q_UNUSED(inLightPrefix);

        if (image == nullptr)
            return;

        addFunction(infragmentShader, "diffuseReflectionWrapBSDF");
        const QByteArray normalizedDirection = m_NormalizedDirection.toUtf8();
        const QByteArray lightColor = m_LightColor.toUtf8();
        if (areaLight) {
            infragmentShader << "\tglobal_diffuse_light.rgb += lightAttenuation * "
                                "translucent_thickness_exp * diffuseReflectionWrapBSDF( "
                                "-world_normal, "
                             << normalizedDirection.constData() << ", " << lightColor.constData()
                             << ".rgb, diffuseLightWrap ).rgb;" << "\n";
        } else {
            infragmentShader << "\tglobal_diffuse_light.rgb += lightAttenuation * "
                                "translucent_thickness_exp * diffuseReflectionWrapBSDF( "
                                "-world_normal, "
                             << "-" << normalizedDirection.constData() << ", " << lightColor.constData()
                             << ".rgb, diffuseLightWrap ).rgb;" << "\n";
        }
    }

    void setupShadowMapVariableNames(qint32 lightIdx)
    {
        Q_ASSERT(lightIdx > -1);
        m_ShadowMapStem = QLatin1String("shadowmap");
        m_ShadowCubeStem = QLatin1String("shadowcube");
        m_ShadowMapStem.append(QString::number(lightIdx));
        m_ShadowCubeStem.append(QString::number(lightIdx));
        m_ShadowMatrixStem = m_ShadowMapStem;
        m_ShadowMatrixStem.append(QLatin1String("_matrix"));
        m_ShadowCoordStem = m_ShadowMapStem;
        m_ShadowCoordStem.append(QLatin1String("_coord"));
        m_ShadowControlStem = m_ShadowMapStem;
        m_ShadowControlStem.append(QLatin1String("_control"));
    }

    void addShadowMapContribution(Q3DSAbstractShaderStageGenerator &inLightShader, qint32 lightIndex,
                                  Q3DSLightNode::LightType inType)
    {
        Q_ASSERT(lightIndex > -1);
        setupShadowMapVariableNames(lightIndex);

        inLightShader.addInclude("shadowMapping.glsllib");

        const QByteArray shadowMapStem = m_ShadowMapStem.toUtf8();
        const QByteArray shadowCubeStem = m_ShadowCubeStem.toUtf8();
        const QByteArray shadowControlStem = m_ShadowControlStem.toUtf8();
        const QByteArray shadowMatrixStem = m_ShadowMatrixStem.toUtf8();
        const QByteArray lightPos = m_LightPos.toUtf8();

        if (inType == Q3DSLightNode::LightType::Directional) {
            inLightShader.addUniform(shadowMapStem.constData(), "sampler2D");
        } else {
            inLightShader.addUniform(shadowCubeStem.constData(), "samplerCube");
        }
        inLightShader.addUniform(shadowControlStem.constData(), "vec4");
        inLightShader.addUniform(shadowMatrixStem.constData(), "mat4");

        if (inType != Q3DSLightNode::LightType::Directional) {
            inLightShader << "\tshadow_map_occl = sampleCubemap( " << shadowCubeStem.constData() << ", "
                          << shadowControlStem.constData() << ", " << shadowMatrixStem.constData() << ", " << lightPos.constData()
                          << ".xyz, varWorldPos, vec2(1.0, " << shadowControlStem.constData() << ".z) );"
                          << "\n";
        } else
            inLightShader << "\tshadow_map_occl = sampleOrthographic( " << shadowMapStem.constData() << ", "
                          << shadowControlStem.constData() << ", " << shadowMatrixStem.constData()
                          << ", varWorldPos, vec2(1.0, " << shadowControlStem.constData() << ".z) );" << "\n";
    }

    void addDisplacementImageUniforms(Q3DSAbstractShaderStageGenerator &inGenerator,
                                      const QString &displacementImageName,
                                      Q3DSImage *displacementImage) override
    {
        if (displacementImage) {
            setupImageVariableNames(displacementImageName);
            const QByteArray imageSampler = m_ImageSampler.toUtf8();
            inGenerator.addInclude("defaultMaterialFileDisplacementTexture.glsllib");
            inGenerator.addUniform("modelMatrix", "mat4");
            inGenerator.addUniform("eyePosition", "vec3");
            inGenerator.addUniform("displaceAmount", "float");
            inGenerator.addUniform(imageSampler.constData(), "sampler2D");
        }
    }

    bool maybeAddMaterialFresnel(Q3DSAbstractShaderStageGenerator &fragmentShader, bool inFragmentHasSpecularAmount)
    {
        if (inFragmentHasSpecularAmount == false)
            fragmentShader << "\tfloat specularAmount = 1.0;" << "\n";
        inFragmentHasSpecularAmount = true;
        fragmentShader.addInclude("defaultMaterialFresnel.glsllib");
        fragmentShader.addUniform("fresnelPower", "float");
        fragmentShader.addUniform("material_specular", "vec4");
        fragmentShader << "\tfloat fresnelRatio = defaultMaterialSimpleFresnel( world_normal, "
                          "view_vector, material_specular.w, fresnelPower );"
                       << "\n";
        fragmentShader << "\tspecularAmount *= fresnelRatio;" << "\n";

        return inFragmentHasSpecularAmount;
    }

    void setupLightVariableNames(qint32 lightIdx, Q3DSLightNode &inLight)
    {
        Q_ASSERT(lightIdx > -1);
        m_LightStem = QLatin1String("qLights");
        m_LightStem.append(QLatin1String("["));
        m_LightStem.append(QString::number(lightIdx));
        m_LightStem.append(QLatin1String("]."));

        m_LightColor = m_LightStem;
        m_LightColor.append(QLatin1String("diffuse"));
        m_LightDirection = m_LightStem;
        m_LightDirection.append(QLatin1String("direction"));
        m_LightSpecularColor = m_LightStem;
        m_LightSpecularColor.append(QLatin1String("specular"));
        if (inLight.lightType() == Q3DSLightNode::LightType::Point) {
            m_LightPos = m_LightStem;
            m_LightPos.append(QLatin1String("position"));
            m_LightConstantAttenuation = m_LightStem;
            m_LightConstantAttenuation.append(QLatin1String("constantAttenuation"));
            m_LightLinearAttenuation = m_LightStem;
            m_LightLinearAttenuation.append(QLatin1String("linearAttenuation"));
            m_LightQuadraticAttenuation = m_LightStem;
            m_LightQuadraticAttenuation.append(QLatin1String("quadraticAttenuation"));
        } else if (inLight.lightType() == Q3DSLightNode::LightType::Area) {
            m_LightPos = m_LightStem;
            m_LightPos.append(QLatin1String("position"));
            m_LightUp = m_LightStem;
            m_LightUp.append(QLatin1String("up"));
            m_LightRt = m_LightStem;
            m_LightRt.append(QLatin1String("right"));
        }
    }

    void addDisplacementMapping(Q3DSDefaultVertexPipeline &inShader)
    {
        inShader.addIncoming("attr_uv0", "vec2");
        inShader.addIncoming("attr_norm", "vec3");
        inShader.addUniform("displacementSampler", "sampler2D");
        inShader.addUniform("displaceAmount", "float");
        inShader.addUniform("displacementMap_rot", "vec4");
        inShader.addUniform("displacementMap_offset", "vec3");
        inShader.addInclude("defaultMaterialFileDisplacementTexture.glsllib");

        inShader.append("\tvec3 uTransform = vec3( displacementMap_rot.x, displacementMap_rot.y, "
                        "displacementMap_offset.x );");
        inShader.append("\tvec3 vTransform = vec3( displacementMap_rot.z, displacementMap_rot.w, "
                        "displacementMap_offset.y );");
        addFunction(inShader, "getTransformedUVCoords");
        inShader.generateUVCoords();
        inShader << "\tvarTexCoord0 = getTransformedUVCoords( vec3( varTexCoord0, 1.0), "
                    "uTransform, vTransform );\n";
        inShader << "\tvec3 displacedPos = defaultMaterialFileDisplacementTexture( "
                    "displacementSampler , displaceAmount, varTexCoord0 , attr_norm, attr_pos );"
                 << "\n";
        inShader.append("\tgl_Position = modelViewProjection * vec4(displacedPos, 1.0);");
    }

    void generateTextureSwizzle(Q3DSTextureSwizzleMode::Enum swizzleMode,
                                QByteArray &texSwizzle,
                                QByteArray &lookupSwizzle)
    {
        switch (swizzleMode) {
        case Q3DSTextureSwizzleMode::L8toR8:
        case Q3DSTextureSwizzleMode::L16toR16:
            texSwizzle.append(QByteArrayLiteral(".rgb"));
            lookupSwizzle.append(QByteArrayLiteral(".rrr"));
            break;
        case Q3DSTextureSwizzleMode::L8A8toRG8:
            texSwizzle.append(QByteArrayLiteral(".rgba"));
            lookupSwizzle.append(QByteArrayLiteral(".rrrg"));
            break;
        case Q3DSTextureSwizzleMode::A8toR8:
            texSwizzle.append(QByteArrayLiteral(".a"));
            lookupSwizzle.append(QByteArrayLiteral(".r"));
            break;
        default:
            break;
        }
    }

    void generateShadowMapOcclusion(qint32 lightIdx, bool inShadowEnabled,
                                    Q3DSLightNode::LightType inType)
    {
        Q_ASSERT(lightIdx > -1);
        if (inShadowEnabled) {
            vertexGenerator().generateWorldPosition();
            addShadowMapContribution(fragmentGenerator(), lightIdx, inType);
        } else {
            fragmentGenerator() << "\tshadow_map_occl = 1.0;" << "\n";
        }
    }

    void generateVertexShader()
    {
        // the pipeline opens/closes up the shaders stages
        vertexGenerator().beginVertexGeneration(m_CurrentMaterial->displacementmap());
    }

    void generateFragmentShader()
    {
        bool specularEnabled = m_CurrentMaterial->specularAmount() > 0.01f;
        bool fresnelEnabled = m_CurrentMaterial->fresnelPower() > 0.0f;
        bool vertexColorsEnabled = m_CurrentMaterial->vertexColors();
        bool hasLighting = m_CurrentMaterial->shaderLighting() != Q3DSDefaultMaterial::NoShaderLighting;
        bool hasLightmaps = false;

        Q3DSImage *bumpImage = m_CurrentMaterial->bumpMap();
        // beware of the differences between specularAmount and specularReflection maps
        Q3DSImage *specularAmountImage = m_CurrentMaterial->specularMap();
        Q3DSImage *specularReflectionImage = m_CurrentMaterial->specularReflection();
        Q3DSImage *roughnessImage = m_CurrentMaterial->roughnessMap();
        Q3DSImage *normalImage = m_CurrentMaterial->normalMap();
        Q3DSImage *translucencyImage = m_CurrentMaterial->translucencyMap();
        // lightmaps
        Q3DSImage *lightmapIndirectImage = nullptr;
        if (m_referencedMaterial && m_referencedMaterial->lightmapIndirectMap())
            lightmapIndirectImage = m_referencedMaterial->lightmapIndirectMap();
        else if (m_CurrentMaterial->lightmapIndirectMap())
            lightmapIndirectImage = m_CurrentMaterial->lightmapIndirectMap();

        Q3DSImage *lightmapRadiosityImage = nullptr;
        if (m_referencedMaterial && m_referencedMaterial->lightmapRadiosityMap())
            lightmapRadiosityImage = m_referencedMaterial->lightmapRadiosityMap();
        else if (m_CurrentMaterial->lightmapRadiosityMap())
            lightmapRadiosityImage = m_CurrentMaterial->lightmapRadiosityMap();

        Q3DSImage *lightmapShadowImage = nullptr;
        if (m_referencedMaterial && m_referencedMaterial->lightmapShadowMap())
            lightmapShadowImage = m_referencedMaterial->lightmapShadowMap();
        else if (m_CurrentMaterial->lightmapShadowMap())
            lightmapShadowImage = m_CurrentMaterial->lightmapShadowMap();

        if (lightmapIndirectImage || lightmapRadiosityImage || lightmapShadowImage)
            hasLightmaps = true;

        const bool hasImage = m_CurrentMaterial->diffuseMap()
                || m_CurrentMaterial->specularReflection()
                || m_CurrentMaterial->specularMap()
                || m_CurrentMaterial->roughnessMap()
                || m_CurrentMaterial->bumpMap()
                || m_CurrentMaterial->normalMap()
                || m_CurrentMaterial->displacementmap()
                || m_CurrentMaterial->opacityMap()
                || m_CurrentMaterial->emissiveMap()
                || lightmapIndirectImage
                || lightmapRadiosityImage
                || lightmapShadowImage;

        // Environment mapping is present if any of the images have the mapping mode set to Environment
        // except for bump, specularamount, normal
        auto mapHasEnvMap = [](Q3DSImage *img) { return img && img->mappingMode() == Q3DSImage::EnvironmentalMapping; };
        // ### do all of the below checks make sense?
        const bool hasEnvMap = mapHasEnvMap(m_CurrentMaterial->diffuseMap())
                || mapHasEnvMap(m_CurrentMaterial->specularReflection()) // the common case
                || mapHasEnvMap(m_CurrentMaterial->opacityMap())
                || mapHasEnvMap(m_CurrentMaterial->displacementmap())
                || mapHasEnvMap(m_CurrentMaterial->emissiveMap());

        bool enableSSAO = false;
        bool enableSSDO = false;
        bool enableShadowMaps = false;
        bool hasIblProbe = false;

        // specify how to build lights
        bool isgles2 = Q3DS::graphicsLimits().format.renderableType() == QSurfaceFormat::OpenGLES &&
                       Q3DS::graphicsLimits().format.majorVersion() == 2;

        auto features = featureSet();
        for (const auto &feature : features) {
            if (!feature.enabled)
                continue;
            if (feature.name == QStringLiteral("QT3DS_ENABLE_SSAO"))
                enableSSAO = true;
            else if (feature.name == QStringLiteral("QT3DS_ENABLE_SSDO"))
                enableSSDO = true;
            else if (feature.name == QStringLiteral("QT3DS_ENABLE_SSM"))
                enableShadowMaps = true;
            else if (feature.name == QStringLiteral("QT3DS_ENABLE_LIGHT_PROBE")
                     || feature.name == QStringLiteral("QT3DS_ENABLE_LIGHT_PROBE_2"))
                hasIblProbe = true;
        }

        bool includeSSAOSSDOVars = enableSSAO || enableSSDO || enableShadowMaps || bumpImage != nullptr || normalImage != nullptr;

        vertexGenerator().beginFragmentGeneration();
        Q3DSAbstractShaderStageGenerator &fragmentShader(fragmentGenerator());
        Q3DSDefaultVertexPipeline &vertexShader(vertexGenerator());

        // The fragment or vertex shaders may not use the material_properties or diffuse
        // uniforms in all cases but it is simpler to just add them and let the linker strip them.
        fragmentShader.addUniform("material_diffuse", "vec4");
        fragmentShader.addUniform("diffuse_color", "vec3");
        fragmentShader.addUniform("material_properties", "vec4");

        // All these are needed for SSAO
        if (includeSSAOSSDOVars)
            fragmentShader.addInclude("SSAOCustomMaterial.glsllib");

        if (hasIblProbe && hasLighting)
            fragmentShader.addInclude("sampleProbe.glsllib");

        if (hasLighting) {
            addFunction(fragmentShader, "sampleLightVars");
            addFunction(fragmentShader, "diffuseReflectionBSDF");
        }

        if (hasLighting && hasLightmaps)
            fragmentShader.addInclude("evalLightmaps.glsllib");

        // view_vector, varWorldPos, world_normal are all used if there is a specular map
        // in addition to if there is specular lighting.  So they are lifted up here, always
        // generated.
        // we rely on the linker to strip out what isn't necessary instead of explicitly stripping
        // it for code simplicity.
        if (hasImage) {
            fragmentShader.append("\tvec3 uTransform;");
            fragmentShader.append("\tvec3 vTransform;");
        }

        if (includeSSAOSSDOVars || specularReflectionImage || hasLighting || hasEnvMap || fresnelEnabled || hasIblProbe) {
            vertexShader.generateViewVector();
            vertexShader.generateWorldNormal();
            vertexShader.generateWorldPosition();
        }

        if (includeSSAOSSDOVars || specularEnabled || hasIblProbe)
            vertexShader.generateVarTangentAndBinormal();

        if (vertexColorsEnabled)
            vertexShader.generateVertexColor();
        else
            fragmentShader.append("\tvec3 vertColor = vec3(1.0);");

        if (includeSSAOSSDOVars) {
            // You do bump or normal mapping but not both
            if (bumpImage != nullptr) {
                generateImageUVCoordinates(QLatin1String("bumpMap"), *bumpImage);
                const QByteArray imageSampler = m_ImageSampler.toUtf8();
                const QByteArray imageFragCoords = m_ImageFragCoords.toUtf8();

                fragmentShader.addUniform("bumpAmount", "float");

                if (isgles2) {
                    const QByteArray imageSamplerSize = m_ImageSize.toUtf8();
                    fragmentShader.addUniform(m_ImageSize.toLatin1(), "vec2");
                    fragmentShader.addInclude("defaultMaterialBumpNoLod.glsllib");
                    fragmentShader << "\tworld_normal = defaultMaterialBumpNoLod( " << imageSampler.constData()
                                   << ", bumpAmount, " << imageFragCoords.constData()
                                   << ", tangent, binormal, world_normal, "
                                   << imageSamplerSize.constData() << ");" << "\n";
                } else {
                    fragmentShader.addInclude("defaultMaterialFileBumpTexture.glsllib");
                    // vec3 simplerFileBumpTexture( in sampler2D sampler, in float factor, vec2
                    // texCoord, vec3 tangent, vec3 binormal, vec3 normal )

                    fragmentShader << "\tworld_normal = simplerFileBumpTexture( " << imageSampler.constData()
                                   << ", bumpAmount, " << imageFragCoords.constData()
                                   << ", tangent, binormal, world_normal );" << "\n";
                }
                // Do gram schmidt
                fragmentShader << "\tbinormal = normalize(cross(world_normal, tangent) );\n";
                fragmentShader << "\ttangent = normalize(cross(binormal, world_normal) );\n";

            } else if (normalImage != nullptr) {
                generateImageUVCoordinates(QLatin1String("normalMap"), *normalImage);
                const QByteArray imageSampler = m_ImageSampler.toUtf8();
                const QByteArray imageFragCoords = m_ImageFragCoords.toUtf8();

                fragmentShader.addInclude("defaultMaterialFileNormalTexture.glsllib");
                fragmentShader.addUniform("bumpAmount", "float");

                fragmentShader << "\tworld_normal = defaultMaterialFileNormalTexture( "
                               << imageSampler.constData() << ", bumpAmount, " << imageFragCoords.constData()
                               << ", tangent, binormal );" << "\n";
            }
        }

        if (includeSSAOSSDOVars || specularEnabled || hasIblProbe)
            fragmentShader << "\tmat3 tanFrame = mat3(tangent, binormal, world_normal);" << "\n";

        bool fragmentHasSpecularAmount = false;

        if (m_CurrentMaterial->emissiveMap())
            fragmentShader.append("\tvec3 global_emission = material_diffuse.rgb;");

        if (hasLighting) {
            fragmentShader.addUniform("light_ambient_total", "vec3");

            fragmentShader.append(
                        "\tvec4 global_diffuse_light = vec4(light_ambient_total.xyz, 1.0);");
            fragmentShader.append("\tvec3 global_specular_light = vec3(0.0, 0.0, 0.0);");
            fragmentShader.append("\tfloat shadow_map_occl = 1.0;");

            if (specularEnabled) {
                vertexShader.generateViewVector();
                fragmentShader.addUniform("material_properties", "vec4");
            }

            if (lightmapIndirectImage != nullptr) {
                generateImageUVCoordinates(QLatin1String("lightmapIndirect"), *lightmapIndirectImage, 1);
                const QByteArray imageSampler = m_ImageSampler.toUtf8();
                const QByteArray imageFragCoords = m_ImageFragCoords.toUtf8();

                fragmentShader << "\tvec4 indirect_light = texture2D( " << imageSampler.constData() << ", "
                               << imageFragCoords.constData() << ");" << "\n";
                fragmentShader << "\tglobal_diffuse_light += indirect_light;" << "\n";
                if (specularEnabled) {
                    fragmentShader
                            << "\tglobal_specular_light += indirect_light.rgb * material_properties.x;"
                            << "\n";
                }
            }

            if (lightmapRadiosityImage != nullptr) {
                generateImageUVCoordinates(QLatin1String("lightmapRadiosity"), *lightmapRadiosityImage, 1);
                const QByteArray imageSampler = m_ImageSampler.toUtf8();
                const QByteArray imageFragCoords = m_ImageFragCoords.toUtf8();

                fragmentShader << "\tvec4 direct_light = texture2D( " << imageSampler.constData() << ", "
                               << imageFragCoords.constData() << ");" << "\n";
                fragmentShader << "\tglobal_diffuse_light += direct_light;" << "\n";
                if (specularEnabled) {
                    fragmentShader
                            << "\tglobal_specular_light += direct_light.rgb * material_properties.x;"
                            << "\n";
                }
            }

            if (translucencyImage != nullptr) {
                fragmentShader.addUniform("translucentFalloff", "float");
                fragmentShader.addUniform("diffuseLightWrap", "float");

                generateImageUVCoordinates(QLatin1String("translucencyMap"), *translucencyImage);
                const QByteArray imageSampler = m_ImageSampler.toUtf8();
                const QByteArray imageFragCoords = m_ImageFragCoords.toUtf8();

                fragmentShader << "\tvec4 translucent_depth_range = texture2D( " << imageSampler.constData()
                               << ", " << imageFragCoords.constData() << ");" << "\n";
                fragmentShader << "\tfloat translucent_thickness = translucent_depth_range.r * "
                                  "translucent_depth_range.r;"
                               << "\n";
                fragmentShader << "\tfloat translucent_thickness_exp = exp( translucent_thickness "
                                  "* translucentFalloff);"
                               << "\n";
            }

            fragmentShader.append("\tfloat lightAttenuation = 1.0;");

            addLocalVariable(fragmentShader, "aoFactor", "float");

            if (hasLighting && enableSSAO)
                fragmentShader.append("\taoFactor = customMaterialAO();");
            else
                fragmentShader.append("\taoFactor = 1.0;");

            addLocalVariable(fragmentShader, "shadowFac", "float");

            if (specularEnabled) {
                fragmentShader << "\tfloat specularAmount = material_properties.x;" << "\n";
                fragmentHasSpecularAmount = true;
            }
            // Fragment lighting means we can perhaps attenuate the specular amount by a texture
            // lookup.
            fragmentShader << "\tvec3 specularColor = vec3(1.0);" << "\n";
            if (specularAmountImage) {
                if (!specularEnabled)
                    fragmentShader << "\tfloat specularAmount = 1.0;" << "\n";
                generateImageUVCoordinates(QLatin1String("specularMap"), *specularAmountImage);
                const QByteArray imageSampler = m_ImageSampler.toUtf8();
                const QByteArray imageFragCoords = m_ImageFragCoords.toUtf8();

                fragmentShader << "\tspecularColor = texture2D( "
                               << imageSampler.constData() << ", " << imageFragCoords.constData() << " ).xyz;" << "\n";
                fragmentHasSpecularAmount = true;
            }
            if (fresnelEnabled)
                fragmentHasSpecularAmount = maybeAddMaterialFresnel(fragmentShader, fragmentHasSpecularAmount);

            fragmentShader << "\tfloat roughnessAmount = material_properties.y;" << "\n";
            if (roughnessImage) {
                generateImageUVCoordinates(QLatin1String("roughnessMap"), *roughnessImage);
                const QByteArray imageSampler = m_ImageSampler.toUtf8();
                const QByteArray imageFragCoords = m_ImageFragCoords.toUtf8();

                fragmentShader << "\tfloat sampledRoughness = texture2D( "
                               << imageSampler.constData() << ", " << imageFragCoords.constData() << " ).x;" << "\n";
                //The roughness sampled from roughness textures is Disney roughness
                //which has to be squared to get the proper value
                fragmentShader << "\troughnessAmount = roughnessAmount * sampledRoughness * sampledRoughness;" << "\n";
            }

            // Iterate through all lights. Note that this is suitable for
            // default materials only, where all lights are provided in a
            // single constant buffer (cbBufferLights). Custom materials need
            // separated non-area (cbBufferLights) and area
            // (cbBufferAreaLights) buffers. That is not handled in here.
            for (qint32 lightIdx = 0; lightIdx < m_Lights.size(); ++lightIdx) {
                Q3DSLightNode *lightNode = m_Lights[lightIdx];
                setupLightVariableNames(lightIdx, *lightNode);
                bool isDirectional = lightNode->lightType() == Q3DSLightNode::Directional;
                bool isArea = lightNode->lightType() == Q3DSLightNode::Area;
                bool isShadow = enableShadowMaps && lightNode->castShadow();

                fragmentShader.append("");

                m_TempStr = QLatin1String("light");
                m_TempStr.append(QString::number(lightIdx));

                const QByteArray tempStr = m_TempStr.toUtf8();
                fragmentShader << "\t//Light " << tempStr.constData() << "\n";
                fragmentShader << "\tlightAttenuation = 1.0;" << "\n";
                if (isDirectional) {
                    if (enableSSDO) {
                        const QByteArray lightDirection = m_LightDirection.toUtf8();
                        fragmentShader << "\tshadowFac = customMaterialShadow( " << lightDirection.constData()
                                       << ".xyz, varWorldPos );" << "\n";
                    } else {
                        fragmentShader << "\tshadowFac = 1.0;" << "\n";
                    }

                    generateShadowMapOcclusion(lightIdx, enableShadowMaps && isShadow,
                                               lightNode->lightType());

                    if (specularEnabled && enableShadowMaps && isShadow)
                        fragmentShader << "\tlightAttenuation *= shadow_map_occl;" << "\n";

                    const QByteArray lightColor = m_LightColor.toUtf8();
                    const QByteArray lightDirection = m_LightDirection.toUtf8();
                    fragmentShader << "\tglobal_diffuse_light.rgb += shadowFac * shadow_map_occl * "
                                      "diffuseReflectionBSDF( world_normal, "
                                   << "-" << lightDirection.constData() << ".xyz, view_vector, "
                                   << lightColor.constData() << ".rgb, 0.0 ).rgb;" << "\n";

                    if (specularEnabled) {
                        outputSpecularEquation(material().specularModel(), fragmentShader,
                                               m_LightDirection,
                                               m_LightSpecularColor);
                    }
                } else if (isArea) {
                    addFunction(fragmentShader, "areaLightVars");
                    addFunction(fragmentShader, "calculateDiffuseAreaOld");
                    vertexShader.generateWorldPosition();
                    generateShadowMapOcclusion(lightIdx, enableShadowMaps && isShadow,
                                               lightNode->lightType());

                    // Debug measure to make sure paraboloid sampling was projecting to the right
                    // location
                    // fragmentShader << "\tglobal_diffuse_light.rg += " << m_ShadowCoordStem << ";"
                    // << "\n";
                    m_NormalizedDirection = m_TempStr;
                    m_NormalizedDirection.append(QLatin1String("_Frame"));

                    QByteArray normalizedDirection = m_NormalizedDirection.toUtf8();
                    const QByteArray lightDirection = m_LightDirection.toUtf8();
                    const QByteArray lightUp = m_LightUp.toUtf8();
                    const QByteArray lightRt = m_LightRt.toUtf8();
                    const QByteArray lightColor = m_LightColor.toUtf8();

                    addLocalVariable(fragmentShader, normalizedDirection.constData(), "mat3");
                    fragmentShader << normalizedDirection.constData() << " = mat3( " << lightRt.constData() << ".xyz, "
                                   << lightUp.constData() << ".xyz, -" << lightDirection.constData() << ".xyz );"
                                   << "\n";

                    if (enableSSDO) {
                        fragmentShader << "\tshadowFac = shadow_map_occl * customMaterialShadow( "
                                       << lightDirection.constData() << ".xyz, varWorldPos );" << "\n";
                    } else {
                        fragmentShader << "\tshadowFac = shadow_map_occl;" << "\n";
                    }

                    if (specularEnabled) {
                        vertexShader.generateViewVector();
                        outputSpecularAreaLighting(fragmentShader, QLatin1String("varWorldPos"), QLatin1String("view_vector"),
                                                   m_LightSpecularColor);
                    }

                    outputDiffuseAreaLighting(fragmentShader, QLatin1String("varWorldPos"), m_TempStr);
                    normalizedDirection = m_NormalizedDirection.toUtf8();
                    fragmentShader << "\tlightAttenuation *= shadowFac;" << "\n";

                    addTranslucencyIrradiance(fragmentShader, translucencyImage, m_TempStr, true);

                    fragmentShader << "\tglobal_diffuse_light.rgb += lightAttenuation * "
                                      "diffuseReflectionBSDF( world_normal, "
                                   << normalizedDirection.constData() << ", view_vector, " << lightColor.constData()
                                   << ".rgb, 0.0 ).rgb;" << "\n";
                } else {

                    vertexShader.generateWorldPosition();
                    generateShadowMapOcclusion(lightIdx, enableShadowMaps && isShadow,
                                               lightNode->lightType());

                    m_RelativeDirection = m_TempStr;
                    m_RelativeDirection.append(QLatin1String("_relativeDirection"));

                    m_NormalizedDirection = m_RelativeDirection;
                    m_NormalizedDirection.append(QLatin1String("_normalized"));

                    m_RelativeDistance = m_TempStr;
                    m_RelativeDistance.append(QLatin1String("_distance"));

                    const QByteArray relativeDirection = m_RelativeDirection.toUtf8();
                    const QByteArray relativeDistance = m_RelativeDistance.toUtf8();
                    const QByteArray normalizedDirection = m_NormalizedDirection.toUtf8();
                    const QByteArray lightPos = m_LightPos.toUtf8();

                    fragmentShader << "\tvec3 " << relativeDirection.constData() << " = varWorldPos - "
                                   << lightPos.constData() << ".xyz;" << "\n";
                    fragmentShader << "\tfloat " << relativeDistance.constData() << " = length( "
                                   << relativeDirection.constData() << " );" << "\n";
                    fragmentShader << "\tvec3 " << normalizedDirection.constData() << " = "
                                   << relativeDirection.constData() << " / " << relativeDistance.constData() << ";"
                                   << "\n";
                    if (enableSSDO) {
                        fragmentShader << "\tshadowFac = shadow_map_occl * customMaterialShadow( "
                                       << normalizedDirection.constData() << ", varWorldPos );" << "\n";
                    } else {
                        fragmentShader << "\tshadowFac = shadow_map_occl;" << "\n";
                    }

                    addFunction(fragmentShader, "calculatePointLightAttenuation");

                    const QByteArray lightConstantAttenuation = m_LightConstantAttenuation.toUtf8();
                    const QByteArray lightLinearAttenuation = m_LightLinearAttenuation.toUtf8();
                    const QByteArray lightQuadraticAttenuation = m_LightQuadraticAttenuation.toUtf8();

                    fragmentShader
                            << "\tlightAttenuation = shadowFac * calculatePointLightAttenuation( vec3( "
                            << lightConstantAttenuation.constData() << ", " << lightLinearAttenuation.constData() << ", "
                            << lightQuadraticAttenuation.constData() << "), " << relativeDistance.constData() << ");"
                            << "\n";

                    addTranslucencyIrradiance(fragmentShader, translucencyImage, m_TempStr, false);

                    const QByteArray lightColor = m_LightColor.toUtf8();
                    fragmentShader << "\tglobal_diffuse_light.rgb += lightAttenuation * "
                                      "diffuseReflectionBSDF( world_normal, "
                                   << "-" << normalizedDirection.constData() << ", view_vector, "
                                   << lightColor.constData() << ".rgb, 0.0 ).rgb;" << "\n";

                    if (specularEnabled) {
                        outputSpecularEquation(material().specularModel(), fragmentShader,
                                               m_NormalizedDirection,
                                               m_LightSpecularColor);
                    }
                }
            }

            // This may be damn confusing but the light colors are already modulated by the base
            // material color.
            // Thus material color is the base material color * material emissive.
            // Except material_color.a *is* the actual opacity factor.
            // Furthermore object_opacity is something that may come from the vertex pipeline or
            // somewhere else.
            // We leave it up to the vertex pipeline to figure it out.
            fragmentShader << "\tglobal_diffuse_light = vec4(global_diffuse_light.xyz * aoFactor, "
                              "object_opacity);"
                           << "\n" << "\tglobal_specular_light = vec3(global_specular_light.xyz);"
                           << "\n";
        } else // no lighting.
        {
            fragmentShader << "\tvec4 global_diffuse_light = vec4(0.0, 0.0, 0.0, object_opacity);"
                           << "\n" << "\tvec3 global_specular_light = vec3(0.0, 0.0, 0.0);" << "\n";

            if (fresnelEnabled) {
                // We still have specular maps and such that could potentially use the fresnel variable.
                fragmentHasSpecularAmount = maybeAddMaterialFresnel(fragmentShader, fragmentHasSpecularAmount);
                fragmentShader << "\tglobal_diffuse_light.a = mix( global_diffuse_light.a, 1.0, fresnelRatio );"
                               << "\n";
            }
        }

        fragmentShader
                << "\tglobal_diffuse_light.rgb *= diffuse_color.rgb;" // because unlike 3DS1 this is not done up-front in the uni.buf.
                << "\n";

        if (!m_CurrentMaterial->emissiveMap()) {
            fragmentShader
                    << "\tglobal_diffuse_light.rgb += diffuse_color.rgb * material_diffuse.rgb;"
                    << "\n";
        }

        // since we already modulate our material diffuse color
        // into the light color we will miss it entirely if no IBL
        // or light is used
        if (hasLightmaps && !(m_Lights.size() || hasIblProbe))
            fragmentShader << "\tglobal_diffuse_light.rgb *= diffuse_color.rgb;" << "\n";

        if (hasLighting && hasIblProbe) {
            vertexShader.generateWorldNormal();

            fragmentShader << "\tglobal_diffuse_light.rgb += diffuse_color.rgb * aoFactor * "
                              "sampleDiffuse( tanFrame ).xyz;"
                           << "\n";

            if (specularEnabled) {
                fragmentShader.addUniform("material_specular", "vec4");

                fragmentShader << "\tglobal_specular_light.xyz += specularColor * specularAmount * "
                                  "vec3(material_specular.xyz) * sampleGlossy( tanFrame, "
                                  "view_vector, roughnessAmount ).xyz;"
                               << "\n";
            }
        }

        auto addTexColor = [&](const QString &name, Q3DSImage *img) {
            QByteArray texSwizzle;
            QByteArray lookupSwizzle;
            QString texLodStr;

            generateImageUVCoordinates(name, *img, 0);
            const QByteArray imageSampler = m_ImageSampler.toUtf8();
            const QByteArray imageFragCoords = m_ImageFragCoords.toUtf8();
            const QByteArray texLod = texLodStr.toUtf8();

            generateTextureSwizzle(Q3DSTextureSwizzleMode::Enum::NoSwizzle,
                                   texSwizzle,
                                   lookupSwizzle);

            if (texLod.isEmpty()) {
                fragmentShader << "\ttexture_color" << texSwizzle << " = texture2D( "
                               << imageSampler.constData() << ", " << imageFragCoords.constData() << ")"
                               << lookupSwizzle.constData() << ";" << "\n";
            } else {
                fragmentShader << "\ttexture_color" << texSwizzle.constData() << "= textureLod( "
                               << imageSampler.constData() << ", " << imageFragCoords.constData() << ", "
                               << texLod.constData() << " )" << lookupSwizzle.constData() << ";"
                               << "\n";
                fragmentShader << "\ttexture_color.xyz *= fresnelColor;" << "\n";
            }

            //TODO determine if the texture is premultipled by checking its format
            if (true) //if premultiplied
                fragmentShader << "\ttexture_color.rgb = texture_color.a > 0.0 ? "
                                  "texture_color.rgb / texture_color.a : vec3( 0, 0, 0 );"
                               << "\n";
        };

        if (hasImage) {
            fragmentShader.append("\tvec4 texture_color;");
            addLocalVariable(fragmentShader, "fresnelColor", "vec3");

            // Instead of looping and ignoring the non-interesting ones, check
            // if the interesting image types are present, and add the texture
            // lookup for each one.

            if (m_CurrentMaterial->diffuseMap()) {
                addTexColor(QLatin1String("diffuseMap"), m_CurrentMaterial->diffuseMap());
                // Assume premultiplied
                fragmentShader.append("\tglobal_diffuse_light *= texture_color;");
            }

            if (lightmapShadowImage) {
                addTexColor(QLatin1String("lightmapShadow"), lightmapShadowImage);
                fragmentShader.append("\tglobal_diffuse_light *= texture_color;");
            }

            if (m_CurrentMaterial->specularReflection()) {
                addTexColor(QLatin1String("specularreflection"), m_CurrentMaterial->specularReflection());
                fragmentShader.addUniform("material_specular", "vec4");
                if (fragmentHasSpecularAmount) {
                    fragmentShader.append("\tglobal_specular_light.xyz += specularColor * specularAmount * "
                                          "texture_color.xyz * material_specular.xyz;");
                } else {
                    fragmentShader.append("\tglobal_specular_light.xyz += texture_color.xyz * "
                                          "material_specular.xyz;");
                }
                fragmentShader.append("\tglobal_diffuse_light.a *= texture_color.a;");
            }

            if (m_CurrentMaterial->emissiveMap()) {
                addTexColor(QLatin1String("emissiveMap"), m_CurrentMaterial->emissiveMap());
                fragmentShader.append("\tglobal_emission *= texture_color.xyz * texture_color.a;");
            }

            if (m_CurrentMaterial->opacityMap()) {
                addTexColor(QLatin1String("opacityMap"), m_CurrentMaterial->opacityMap());
                fragmentShader.append("\tglobal_diffuse_light.a *= texture_color.a;");
            }
        }

        if (m_CurrentMaterial->emissiveMap())
            fragmentShader.append("\tglobal_diffuse_light.rgb += global_emission.rgb;");

        // Ensure the rgb colors are in range.
        fragmentShader.append("\tfragOutput = vec4( clamp( vertColor * global_diffuse_light.xyz + "
                              "global_specular_light.xyz, 0.0, 65519.0 ), global_diffuse_light.a "
                              ");");

        if (vertexGenerator().hasActiveWireframe()) {
            fragmentShader.append("vec3 edgeDistance = varEdgeDistance * gl_FragCoord.w;");
            fragmentShader.append(
                        "\tfloat d = min(min(edgeDistance.x, edgeDistance.y), edgeDistance.z);");
            fragmentShader.append("\tfloat mixVal = smoothstep(0.0, 1.0, d);"); // line width 1.0

            fragmentShader.append(
                        "\tfragOutput = mix( vec4(0.0, 1.0, 0.0, 1.0), fragOutput, mixVal);");
        }
    }

    Qt3DRender::QShaderProgram *generateMaterialShader(const QString &description)
    {
        generateVertexShader();
        generateFragmentShader();

        vertexGenerator().endVertexGeneration();
        vertexGenerator().endFragmentGeneration();

        return programGenerator()->compileGeneratedShader(description, m_CurrentFeatureSet);
    }

    Qt3DRender::QShaderProgram *generateShader(Q3DSGraphObject &defaultMaterial,
                                               Q3DSReferencedMaterial *referencedMaterial,
                                               Q3DSAbstractShaderStageGenerator &vertexPipeline,
                                               const Q3DSShaderFeatureSet &featureSet,
                                               const QVector<Q3DSLightNode*> &lights,
                                               bool hasTransparency,
                                               const QString &description) override
    {
        if (defaultMaterial.type() != Q3DSGraphObject::DefaultMaterial)
            return nullptr;

        m_CurrentMaterial = static_cast<Q3DSDefaultMaterial*>(&defaultMaterial);
        m_referencedMaterial = referencedMaterial;
        m_CurrentPipeline = static_cast<Q3DSDefaultVertexPipeline*>(&vertexPipeline);
        m_ProgramGenerator = &static_cast<Q3DSVertexPipelineImpl*>(&vertexPipeline)->programGenerator();
        m_CurrentFeatureSet = featureSet;
        m_Lights = lights;
        m_HasTransparency = hasTransparency;

        return generateMaterialShader(description);
    }
};

} // namespace

Q3DSVertexPipelineImpl::Q3DSVertexPipelineImpl(Q3DSDefaultMaterialShaderGenerator &inMaterial,
                                               Q3DSAbstractShaderProgramGenerator &inProgram,
                                               bool inWireframe // only works if tessellation is true
                                               )
    : m_MaterialGenerator(inMaterial)
    , m_ProgramGenerator(inProgram)
    , m_Wireframe(inWireframe)
    , m_DisplacementImage(nullptr)
{
}

// Trues true if the code was *not* set.
bool Q3DSVertexPipelineImpl::setCode(GenerationFlagValues::Enum inCode)
{
    if (((quint32)m_GenerationFlags & inCode) != 0)
        return true;
    m_GenerationFlags |= inCode;
    return false;
}

bool Q3DSVertexPipelineImpl::hasCode(GenerationFlagValues::Enum inCode)
{
    return ((quint32)(m_GenerationFlags & inCode)) != 0;
}

Q3DSAbstractShaderProgramGenerator &Q3DSVertexPipelineImpl::programGenerator()
{
    return m_ProgramGenerator;
}

Q3DSAbstractShaderStageGenerator &Q3DSVertexPipelineImpl::vertex()
{
    return *programGenerator().getStage(Q3DSShaderGeneratorStages::Vertex);
}

Q3DSAbstractShaderStageGenerator &Q3DSVertexPipelineImpl::tessControl()
{
    return *programGenerator().getStage(Q3DSShaderGeneratorStages::TessControl);
}

Q3DSAbstractShaderStageGenerator &Q3DSVertexPipelineImpl::tessEval()
{
    return *programGenerator().getStage(Q3DSShaderGeneratorStages::TessEval);
}

Q3DSAbstractShaderStageGenerator &Q3DSVertexPipelineImpl::geometry()
{
    return *programGenerator().getStage(Q3DSShaderGeneratorStages::Geometry);
}
Q3DSAbstractShaderStageGenerator &Q3DSVertexPipelineImpl::fragment()
{
    return *programGenerator().getStage(Q3DSShaderGeneratorStages::Fragment);
}

Q3DSDefaultMaterialShaderGenerator &Q3DSVertexPipelineImpl::materialGenerator()
{
    return m_MaterialGenerator;
}

bool Q3DSVertexPipelineImpl::hasTessellation() const
{
    return m_ProgramGenerator.getEnabledStages() & Q3DSShaderGeneratorStages::TessEval;
}

bool Q3DSVertexPipelineImpl::hasGeometryStage() const
{
    return m_ProgramGenerator.getEnabledStages() & Q3DSShaderGeneratorStages::Geometry;
}

bool Q3DSVertexPipelineImpl::hasDisplacment() const
{
    return m_DisplacementImage != nullptr;
}

void Q3DSVertexPipelineImpl::initializeWireframeGeometryShader()
{
    if (m_Wireframe && programGenerator().getStage(Q3DSShaderGeneratorStages::Geometry)
            && programGenerator().getStage(Q3DSShaderGeneratorStages::TessEval))
    {
        Q3DSAbstractShaderStageGenerator &geometryShader(
                    *programGenerator().getStage(Q3DSShaderGeneratorStages::Geometry));
        // currently geometry shader is only used for drawing wireframe
        if (m_Wireframe) {
            geometryShader.addUniform("viewport_matrix", "mat4");
            geometryShader.addOutgoing("varEdgeDistance", "vec3");
            geometryShader.append("layout (triangles) in;");
            geometryShader.append("layout (triangle_strip, max_vertices = 3) out;");
            geometryShader.append("void main() {");

            // how this all work see
            // http://developer.download.nvidia.com/SDK/10.5/direct3d/Source/SolidWireframe/Doc/SolidWireframe.pdf

            geometryShader.append(
                        "// project points to screen space\n"
                        "\tvec3 p0 = vec3(viewport_matrix * (gl_in[0].gl_Position / "
                        "gl_in[0].gl_Position.w));\n"
                        "\tvec3 p1 = vec3(viewport_matrix * (gl_in[1].gl_Position / "
                        "gl_in[1].gl_Position.w));\n"
                        "\tvec3 p2 = vec3(viewport_matrix * (gl_in[2].gl_Position / "
                        "gl_in[2].gl_Position.w));\n"
                        "// compute triangle heights\n"
                        "\tfloat e1 = length(p1 - p2);\n"
                        "\tfloat e2 = length(p2 - p0);\n"
                        "\tfloat e3 = length(p1 - p0);\n"
                        "\tfloat alpha = acos( (e2*e2 + e3*e3 - e1*e1) / (2.0*e2*e3) );\n"
                        "\tfloat beta = acos( (e1*e1 + e3*e3 - e2*e2) / (2.0*e1*e3) );\n"
                        "\tfloat ha = abs( e3 * sin( beta ) );\n"
                        "\tfloat hb = abs( e3 * sin( alpha ) );\n"
                        "\tfloat hc = abs( e2 * sin( alpha ) );\n");
        }
    }
}

void Q3DSVertexPipelineImpl::finalizeWireframeGeometryShader()
{
    Q3DSAbstractShaderStageGenerator &geometryShader(
                *programGenerator().getStage(Q3DSShaderGeneratorStages::Geometry));

    if (m_Wireframe == true && programGenerator().getStage(Q3DSShaderGeneratorStages::Geometry)
            && programGenerator().getStage(Q3DSShaderGeneratorStages::TessEval))
    {
        // we always assume triangles
        for (int i = 0; i < 3; i++) {
            const QByteArray n = QByteArray::number(i);
            for (const QString &key : m_InterpolationParameters.keys()) {
                const QByteArray ba = key.toUtf8();
                geometryShader << "\t" << ba.constData() << " = "
                               << ba.constData() << "TE[" << n.constData() << "];\n";
            }

            geometryShader << "\tgl_Position = gl_in[" << n.constData() << "].gl_Position;\n";
            // the triangle distance is interpolated through the shader stage
            if (i == 0)
                geometryShader << "\n\tvarEdgeDistance = vec3(ha*"
                               << "gl_in[" << n.constData() << "].gl_Position.w, 0.0, 0.0);\n";
            else if (i == 1)
                geometryShader << "\n\tvarEdgeDistance = vec3(0.0, hb*"
                               << "gl_in[" << n.constData() << "].gl_Position.w, 0.0);\n";
            else if (i == 2)
                geometryShader << "\n\tvarEdgeDistance = vec3(0.0, 0.0, hc*"
                               << "gl_in[" << n.constData() << "].gl_Position.w);\n";

            // submit vertex
            geometryShader << "\tEmitVertex();\n";
        }
        // end primitive
        geometryShader << "\tEndPrimitive();\n";
    }
}

void Q3DSVertexPipelineImpl::setupTessIncludes(Q3DSShaderGeneratorStages::Enum inStage,
                                               TessModeValues::Enum inTessMode)
{
    Q3DSAbstractShaderStageGenerator &tessShader(*programGenerator().getStage(inStage));

    // depending on the selected tessellation mode chose program
    switch (inTessMode) {
    case TessModeValues::TessPhong:
        tessShader.addInclude("tessellationPhong.glsllib");
        break;
    case TessModeValues::TessNPatch:
        tessShader.addInclude("tessellationNPatch.glsllib");
        break;
//    case TessModeValues::TessLinear:
    default:
        tessShader.addInclude("tessellationLinear.glsllib");
        break;
    }
}

void Q3DSVertexPipelineImpl::generateUVCoords(quint32 inUVSet)
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

void Q3DSVertexPipelineImpl::generateEnvMapReflection()
{
    if (setCode(GenerationFlagValues::EnvMapReflection))
        return;

    generateWorldPosition();
    generateWorldNormal();
    Q3DSAbstractShaderStageGenerator &activeGenerator(activeStage());
    activeGenerator.addInclude("viewProperties.glsllib");
    addInterpolationParameter("var_object_to_camera", "vec3");
    activeGenerator.append("\tvar_object_to_camera = normalize( local_model_world_position "
                           "- eyePosition );");
    // World normal cannot be relied upon in the vertex shader because of bump maps.
    fragment().append("\tvec3 environment_map_reflection = reflect( "
                      "normalize(var_object_to_camera), world_normal.xyz );");
    fragment().append("\tenvironment_map_reflection *= vec3( 0.5, 0.5, 0 );");
    fragment().append("\tenvironment_map_reflection += vec3( 0.5, 0.5, 1.0 );");
}

void Q3DSVertexPipelineImpl::generateViewVector()
{
    if (setCode(GenerationFlagValues::ViewVector))
        return;
    generateWorldPosition();
    Q3DSAbstractShaderStageGenerator &activeGenerator(activeStage());
    activeGenerator.addInclude("viewProperties.glsllib");
    addInterpolationParameter("varViewVector", "vec3");
    activeGenerator.append("\tvec3 local_view_vector = normalize(eyePosition - "
                           "local_model_world_position);");
    assignOutput("varViewVector", "local_view_vector");
    fragment() << "\tvec3 view_vector = normalize(varViewVector);" << "\n";
}

// fragment shader expects varying vertex normal
// lighting in vertex pipeline expects world_normal
void Q3DSVertexPipelineImpl::generateWorldNormal()
{
    if (setCode(GenerationFlagValues::WorldNormal))
        return;
    addInterpolationParameter("varNormal", "vec3");
    doGenerateWorldNormal();
    fragment().append("\tvec3 world_normal = normalize( varNormal );");
}

void Q3DSVertexPipelineImpl::generateObjectNormal()
{
    if (setCode(GenerationFlagValues::ObjectNormal))
        return;
    doGenerateObjectNormal();
    fragment().append("\tvec3 object_normal = normalize(varObjectNormal);");
}

void Q3DSVertexPipelineImpl::generateWorldPosition()
{
    if (setCode(GenerationFlagValues::WorldPosition))
        return;

    activeStage().addUniform("modelMatrix", "mat4");
    addInterpolationParameter("varWorldPos", "vec3");
    doGenerateWorldPosition();

    assignOutput("varWorldPos", "local_model_world_position");
}

void Q3DSVertexPipelineImpl::generateVarTangentAndBinormal()
{
    if (setCode(GenerationFlagValues::TangentBinormal))
        return;
    addInterpolationParameter("varTangent", "vec3");
    addInterpolationParameter("varBinormal", "vec3");
    doGenerateVarTangentAndBinormal();
    fragment() << "\tvec3 tangent = normalize(varTangent);" << "\n"
               << "\tvec3 binormal = normalize(varBinormal);" << "\n";
}

void Q3DSVertexPipelineImpl::generateVertexColor()
{
    if (setCode(GenerationFlagValues::VertexColor))
        return;
    addInterpolationParameter("varColor", "vec3");
    doGenerateVertexColor();
    fragment().append("\tvec3 vertColor = varColor;");
}

bool Q3DSVertexPipelineImpl::hasActiveWireframe()
{
    return m_Wireframe;
}

// IShaderStageGenerator interface
void Q3DSVertexPipelineImpl::addIncoming(const char *name, const char *type)
{
    activeStage().addIncoming(name, type);
}

void Q3DSVertexPipelineImpl::addOutgoing(const char *name, const char *type)
{
    addInterpolationParameter(name, type);
}

void Q3DSVertexPipelineImpl::addUniform(const char *name, const char *type)
{
    activeStage().addUniform(name, type);
}

void Q3DSVertexPipelineImpl::addInclude(const char *name)
{
    activeStage().addInclude(name);
}

void Q3DSVertexPipelineImpl::addFunction(const char *functionName)
{
    const QString func = QString::fromUtf8(functionName);
    if (!m_addedFunctions.contains(func)) {
        m_addedFunctions.append(func);
        QByteArray includeName;
        QTextStream stream(&includeName);
        stream << "func" << functionName << ".glsllib";
        stream.flush();
        addInclude(includeName.constData());
    }
}

void Q3DSVertexPipelineImpl::addConstantBuffer(const char *name, const char *layout)
{
    activeStage().addConstantBuffer(name, layout);
}

void Q3DSVertexPipelineImpl::addConstantBufferParam(const char *cbName, const char *paramName, const char *type)
{
    activeStage().addConstantBufferParam(cbName, paramName, type);
}

Q3DSAbstractShaderStageGenerator &Q3DSVertexPipelineImpl::operator<<(const char *data)
{
    activeStage() << data;
    return *this;
}

void Q3DSVertexPipelineImpl::append(const char *data)
{
    activeStage().append(data);
}

void Q3DSVertexPipelineImpl::appendPartial(const char *data)
{
    activeStage().append(data);
}

Q3DSShaderGeneratorStages::Enum Q3DSVertexPipelineImpl::stage() const
{
    return const_cast<Q3DSVertexPipelineImpl *>(this)->activeStage().stage();
}

Q3DSDefaultMaterialShaderGenerator &Q3DSDefaultMaterialShaderGenerator::createDefaultMaterialShaderGenerator()
{
    static ShaderGenerator *defaultGenerator = nullptr;
    if (!defaultGenerator)
        defaultGenerator = new ShaderGenerator();
    return *defaultGenerator;
}

QT_END_NAMESPACE
