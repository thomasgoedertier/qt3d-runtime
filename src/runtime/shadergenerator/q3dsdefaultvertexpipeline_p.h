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

#ifndef Q3DSDEFAULTVERTEXPIPELINE_H
#define Q3DSDEFAULTVERTEXPIPELINE_H

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

#include "q3dsmaterialshadergenerator_p.h"

QT_BEGIN_NAMESPACE

struct TessModeValues
{
    enum Enum {
        NoTess = 0,
        TessLinear = 1,
        TessPhong = 2,
        TessNPatch = 3,
    };

    static const char *toString(Enum value)
    {
        switch (value) {
        case NoTess:
            return "NoTess";
            break;
        case TessLinear:
            return "TessLinear";
            break;
        case TessPhong:
            return "TessPhong";
            break;
        case TessNPatch:
            return "TessNPatch";
            break;
        default:
            return "NoTess";
            break;
        }
    }
};

struct Q3DSTextureSwizzleMode
{
    enum Enum {
        NoSwizzle = 0,
        L8toR8,
        A8toR8,
        L8A8toRG8,
        L16toR16
    };
};


class Q3DSDefaultVertexPipeline : public Q3DSAbstractShaderStageGenerator
{
protected:
    virtual ~Q3DSDefaultVertexPipeline() {}
public:
    // Responsible for beginning all vertex and fragment generation (void main() { etc).
    virtual void beginVertexGeneration(Q3DSImage *displacementImage) = 0;
    // The fragment shader expects a floating point constant, object_opacity to be defined
    // post this method.
    virtual void beginFragmentGeneration() = 0;
    // Output variables may be mangled in some circumstances so the shader generation system
    // needs an abstraction
    // mechanism around this.
    virtual void assignOutput(const char *varName, const char *varValueExpr) = 0;

    virtual void generateUVCoords(quint32 inUVSet = 0) = 0;

    virtual void generateEnvMapReflection() = 0;
    virtual void generateViewVector() = 0;

    // fragment shader expects varying vertex normal
    // lighting in vertex pipeline expects world_normal
    virtual void generateWorldNormal() = 0; // world_normal in both vert and frag shader
    virtual void generateObjectNormal() = 0; // object_normal in both vert and frag shader
    virtual void generateWorldPosition() = 0; // model_world_position in both vert and frag shader
    virtual void generateVarTangentAndBinormal() = 0;
    virtual void generateVertexColor() = 0;

    virtual bool hasActiveWireframe() = 0; // varEdgeDistance is a valid entity

    // responsible for closing all vertex and fragment generation
    virtual void endVertexGeneration() = 0;
    virtual void endFragmentGeneration() = 0;
};

class Q3DSDefaultMaterialShaderGenerator : public Q3DSAbstractMaterialGenerator
{
public:
    virtual void addDisplacementImageUniforms(Q3DSAbstractShaderStageGenerator &inGenerator,
                                              const QString &displacementImageName,
                                              Q3DSImage *displacementImage) = 0;

    static Q3DSDefaultMaterialShaderGenerator &createDefaultMaterialShaderGenerator();
};

struct Q3DSVertexPipelineImpl : public Q3DSDefaultVertexPipeline
{
    struct GenerationFlagValues
    {
        enum Enum {
            UVCoords = 1,
            EnvMapReflection = 1 << 1,
            ViewVector = 1 << 2,
            WorldNormal = 1 << 3,
            ObjectNormal = 1 << 4,
            WorldPosition = 1 << 5,
            TangentBinormal = 1 << 6,
            UVCoords1 = 1 << 7,
            VertexColor = 1 << 8,
        };
    };

    Q_DECLARE_FLAGS(GenerationFlags, GenerationFlagValues::Enum)

    Q3DSDefaultMaterialShaderGenerator &m_MaterialGenerator;
    Q3DSAbstractShaderProgramGenerator &m_ProgramGenerator;

    GenerationFlags m_GenerationFlags;
    QMap<QString, QString> m_InterpolationParameters;
    bool m_Wireframe;
    Q3DSImage *m_DisplacementImage;
    QStringList m_addedFunctions;

    Q3DSVertexPipelineImpl(Q3DSDefaultMaterialShaderGenerator &inMaterial,
                                                   Q3DSAbstractShaderProgramGenerator &inProgram,
                                                   bool inWireframe // only works if tessellation is true
                                                   );

    // Trues true if the code was *not* set.
    bool setCode(GenerationFlagValues::Enum inCode);
    bool hasCode(GenerationFlagValues::Enum inCode);
    Q3DSAbstractShaderProgramGenerator &programGenerator();
    Q3DSAbstractShaderStageGenerator &vertex();
    Q3DSAbstractShaderStageGenerator &tessControl();

    Q3DSAbstractShaderStageGenerator &tessEval();

    Q3DSAbstractShaderStageGenerator &geometry();

    Q3DSAbstractShaderStageGenerator &fragment();

    Q3DSDefaultMaterialShaderGenerator &materialGenerator();

    bool hasTessellation() const;
    bool hasGeometryStage() const;
    bool hasDisplacment() const;

    void initializeWireframeGeometryShader();
    void finalizeWireframeGeometryShader();

    virtual void setupTessIncludes(Q3DSShaderGeneratorStages::Enum inStage, TessModeValues::Enum inTessMode);
    void generateUVCoords(quint32 inUVSet = 0) override;

    void generateEnvMapReflection() override;

    void generateViewVector() override;


    // fragment shader expects varying vertex normal
    // lighting in vertex pipeline expects world_normal
    void generateWorldNormal() override;

    void generateObjectNormal() override;

    void generateWorldPosition() override;

    void generateVarTangentAndBinormal() override;

    void generateVertexColor() override;

    bool hasActiveWireframe() override;

    // IShaderStageGenerator interface
    void addIncoming(const char *name, const char *type) override;


    void addOutgoing(const char *name, const char *type) override;


    void addUniform(const char *name, const char *type) override;


    void addInclude(const char *name) override;
    void addFunction(const char *functionName) override;

    void addConstantBuffer(const char *name, const char *layout) override;
    void addConstantBufferParam(const char *cbName,
                                const char *paramName,
                                const char *type) override;

    Q3DSAbstractShaderStageGenerator &operator<<(const char *data) override;

    void append(const char *data) override;
    void appendPartial(const char *data) override;

    Q3DSShaderGeneratorStages::Enum stage() const override;

    virtual Q3DSAbstractShaderStageGenerator &activeStage() = 0;
    virtual void addInterpolationParameter(const char *inParamName,
                                           const char *inParamType) = 0;

    virtual void doGenerateUVCoords(quint32 inUVSet) = 0;
    virtual void doGenerateWorldNormal() = 0;
    virtual void doGenerateObjectNormal() = 0;
    virtual void doGenerateWorldPosition() = 0;
    virtual void doGenerateVarTangentAndBinormal() = 0;
    virtual void doGenerateVertexColor() = 0;
};

QT_END_NAMESPACE

#endif // Q3DSDEFAULTVERTEXPIPELINE_H
