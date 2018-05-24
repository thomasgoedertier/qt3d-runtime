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

#include "q3dsshaderprogramgenerator_p.h"
#include "q3dsutils_p.h"
#include "q3dsprofiler_p.h"
#include "q3dsgraphicslimits_p.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QCache>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

namespace {

bool saveShaderFile(const QByteArray &shaderData, const QString &filename)
{
    QFile shaderFile(filename);
    if (!shaderFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    shaderFile.write(shaderData);
    return true;
}

typedef QPair<QString, QString> ParamPair;
typedef QPair<QString, ParamPair> ConstantBuferParamPair;
class StageGeneratorBase : public Q3DSAbstractShaderStageGenerator
{
public:
    struct ShaderBinding {
        QString type;
        QString name;
        ShaderBinding() {}
        ShaderBinding(const QString &t, const QString &n)
            : type(t)
            , name(n)
        {}
        ShaderBinding(const ShaderBinding &data)
            : type(data.type)
            , name(data.name)
        {}
        bool operator ==(const ShaderBinding &other) const {
            return (type == other.type) && (name == other.name);
        }
    };
    StageGeneratorBase(Q3DSShaderGeneratorStages::Enum inStage)
        : m_outgoing(nullptr)
        , m_stage(inStage)
    {

    }

    virtual void begin(Q3DSShaderGeneratorStageFlags enableStages)
    {
        m_includes.clear();
        m_outgoing = nullptr;
        m_incoming.clear();
        m_uniforms.clear();
        m_constantBuffers.clear();
        m_constantBufferParams.clear();
        m_codeBuilder.clear();
        m_finalBuilder.clear();
        m_enabledStages = enableStages;
        m_addedFunctions.clear();
    }

    void addIncoming(const char *name, const char *type) override
    {
        auto incoming = ShaderBinding(QString::fromUtf8(type), QString::fromUtf8(name));
        if (!m_incoming.contains(incoming))
            m_incoming.append(incoming);
    }

    virtual QString getIncomingVariableName()
    {
        return QStringLiteral("in");
    }

    void addOutgoing(const char *name, const char *type) override
    {
        if (m_outgoing == nullptr) {
            Q_ASSERT(false);
            return;
        }

        m_outgoing->append(ShaderBinding(QString::fromUtf8(type), QString::fromUtf8(name)));
    }

    void addUniform(const char *name, const char *type) override
    {
        auto uniform = ShaderBinding(QString::fromUtf8(type), QString::fromUtf8(name));
        if (!m_uniforms.contains(uniform))
            m_uniforms.append(uniform);
    }

    void addInclude(const char *name) override
    {
        const QString nameStr = QString::fromUtf8(name);
        if (!m_includes.contains(nameStr))
            m_includes.append(nameStr);
    }

    void addFunction(const char *functionName) override
    {
        const QString funcStr = QString::fromUtf8(functionName);
        if (!m_addedFunctions.contains(funcStr)) {
            m_addedFunctions.append(funcStr);
            QByteArray includeName;
            QTextStream stream(&includeName);
            stream << "func" << functionName << ".glsllib";
            stream.flush();
            addInclude(includeName.constData());
        }
    }

    void addConstantBuffer(const char *name, const char *layout) override
    {
        auto constantBuffer = ShaderBinding(QString::fromUtf8(layout), QString::fromUtf8(name));
        if (!m_constantBuffers.contains(constantBuffer))
            m_constantBuffers.append(constantBuffer);
    }

    void addConstantBufferParam(const char *cbName, const char *paramName, const char *type) override
    {
        ParamPair paramPair(QString::fromUtf8(paramName), QString::fromUtf8(type));
        ConstantBuferParamPair cbParamPair(QString::fromUtf8(cbName), paramPair);
        m_constantBufferParams.append(cbParamPair);
    }

    Q3DSAbstractShaderStageGenerator &operator <<(const char *data) override
    {
        m_codeBuilder.append(QString::fromUtf8(data));
        return *this;
    }

    void append(const char *data) override
    {
        m_codeBuilder.append(QString::fromUtf8(data));
        m_codeBuilder.append(QLatin1String("\n"));
    }

    void appendPartial(const char *data) override
    {
        m_codeBuilder.append(QString::fromUtf8(data));
    }

    Q3DSShaderGeneratorStages::Enum stage() const override
    {
        return m_stage;
    }

    virtual void addShaderItemMap(const QString &itemType, const QVector<ShaderBinding> &itemMap, const QString &itemSuffix = QString())
    {
        m_finalBuilder.append(QLatin1String("\n"));
        for (const auto &item : itemMap) {
            m_finalBuilder.append(itemType);
            m_finalBuilder.append(QLatin1String(" "));
            m_finalBuilder.append(item.type);
            m_finalBuilder.append(QLatin1String(" "));
            m_finalBuilder.append(item.name);
            m_finalBuilder.append(itemSuffix);
            m_finalBuilder.append(QLatin1String(";\n"));
        }
    }

    virtual void addShaderIncomingMap()
    {
        addShaderItemMap(getIncomingVariableName(), m_incoming);
    }

    virtual void addShaderUniformMap()
    {
        addShaderItemMap(QLatin1String("uniform"), m_uniforms);
    }

    virtual void addShaderOutgoingMap()
    {
        if (m_outgoing)
            addShaderItemMap(QLatin1String("varying"), *m_outgoing);
    }

    virtual void addShaderConstantBufferItemMap(const QString &itemType,
                                                const QVector<ShaderBinding> &cbMap,
                                                const QVector<ConstantBuferParamPair> &cbParamsArray)
    {
        m_finalBuilder.append(QLatin1String("\n"));
        for (const auto &entry : cbMap) {
            m_finalBuilder.append(entry.type);
            m_finalBuilder.append(QLatin1String(" "));
            m_finalBuilder.append(itemType);
            m_finalBuilder.append(QLatin1String(" "));
            m_finalBuilder.append(entry.name);
            m_finalBuilder.append(QLatin1String(" {\n"));

            for (const auto &param : cbParamsArray) {
                if (entry.name == param.first) {
                    m_finalBuilder.append(param.second.second);
                    m_finalBuilder.append(QLatin1String(" "));
                    m_finalBuilder.append(param.second.first);
                    m_finalBuilder.append(QLatin1String(";\n"));
                }
            }

            m_finalBuilder.append(QLatin1String(";\n"));
        }
    }

    virtual void appendShaderCode()
    {
        m_finalBuilder.append(m_codeBuilder);
    }

    virtual QString buildShaderSource(const Q3DSShaderFeatureSet &inFeatureSet)
    {
        addShaderPreprocessor(m_finalBuilder, inFeatureSet);
        for (const QString &include : m_includes) {
            m_finalBuilder.append(QLatin1String("#include \""));
            m_finalBuilder.append(include);
            m_finalBuilder.append(QLatin1String("\"\n"));
        }
        addShaderIncomingMap();
        addShaderUniformMap();
        addShaderConstantBufferItemMap(QLatin1String("uniform"), m_constantBuffers, m_constantBufferParams);
        addShaderOutgoingMap();
        m_finalBuilder.append(QLatin1String("\n"));
        appendShaderCode();
        return m_finalBuilder;
    }

    void addShaderPreprocessor(QString &output, const Q3DSShaderFeatureSet &inFeatureSet)
    {
        Q3DSGraphicsLimits gfxLimits = Q3DS::graphicsLimits();
        const bool isOpenGLES = gfxLimits.format.renderableType() == QSurfaceFormat::OpenGLES;

        // Version String
        output.append(getVersionString(gfxLimits.format));

        if (isOpenGLES) {
            // TODO: check if this is portable:
            output.append(QLatin1String("#extension GL_OES_standard_derivatives : enable\n"));
            if (gfxLimits.format.majorVersion() == 2) {
                // ES2
                output.append(QLatin1String("#define GLSL_100 1\n"));
                output.append(QLatin1String("#define GLSL_130 0\n"));
                if (m_stage == Q3DSShaderGeneratorStages::Enum::Fragment) {
                    output.append(QLatin1String("#define fragOutput gl_FragData[0]\n"));

                    if (gfxLimits.shaderTextureLodSupported)
                        output.append(QLatin1String("#define textureLod texture2DLodEXT\n"));
                    else
                        output.append(QLatin1String("#define textureLod(s, co, lod) texture2D(s, co)\n"));

                    output.append(QLatin1String("#define texture texture2D\n"));
                }
            } else if (gfxLimits.format.majorVersion() == 3) {
                // ES3
                output.append(QLatin1String("#define GLSL_100 0\n"));
                output.append(QLatin1String("#define GLSL_130 1\n"));
                output.append(QLatin1String("#define texture2D texture\n"));
            }

            // Extensions
            output.append(addShaderExtensionStrings(gfxLimits.format));

            // Precision qualifier
            if (gfxLimits.format.majorVersion() == 3) {
                // ES3
                output.append(QLatin1String("precision highp float;\n"));
                output.append(QLatin1String("precision highp int;\n"));

                // Add backwards compatibility
                output.append(addBackwardCompatibilityDefines(gfxLimits.format, m_stage, inFeatureSet));

            } else {
                // ES2
                output.append(QLatin1String("precision mediump float;\n"));
                output.append(QLatin1String("precision mediump int;\n"));
                output.append(QLatin1String("precision mediump sampler2D;\n"));
                // ### These are not in GLSL 1.00
                // output.append("precision mediump sampler2DArray;\n");
                // output.append("precision mediump sampler2DShadow;\n");
            }
        } else {
            // OpenGL
            if (gfxLimits.format.majorVersion() == 2) {
                // GL2
                output.append(QLatin1String("#define GLSL_100 1\n"));
                output.append(QLatin1String("#define GLSL_130 0\n"));
            } else if (gfxLimits.format.majorVersion() >= 3) {
                // GL3 +
                output.append(QLatin1String("#define GLSL_100 0\n"));
                output.append(QLatin1String("#define GLSL_130 1\n"));
                output.append(QLatin1String("#define texture2D texture\n"));

                // Extensions
                output.append(addShaderExtensionStrings(gfxLimits.format));

                // Add backwards compatibility
                output.append(QLatin1String("#if __VERSION__ >= 330\n"));
                output.append(addBackwardCompatibilityDefines(gfxLimits.format, m_stage, inFeatureSet));
                output.append(QLatin1String("#else\n"));
                if (m_stage == Q3DSShaderGeneratorStages::Enum::Fragment)
                    output.append(QLatin1String("#define fragOutput gl_FragData[0]\n"));
                output.append(QLatin1String("#endif\n"));
            }
        }

        if (m_stage == Q3DSShaderGeneratorStages::Enum::TessControl) {
            output.append(QLatin1String("#define TESSELLATION_CONTROL_SHADER 1\n"));
            output.append(QLatin1String("#define TESSELLATION_EVALUATION_SHADER 0\n"));
        } else if (m_stage == Q3DSShaderGeneratorStages::Enum::TessEval) {
            output.append(QLatin1String("#define TESSELLATION_CONTROL_SHADER 0\n"));
            output.append(QLatin1String("#define TESSELLATION_EVALUATION_SHADER 1\n"));
        }

        for (const Q3DSShaderPreprocessorFeature &ppFeature : inFeatureSet)
            output.append(QLatin1String("#define ") + ppFeature.name + QLatin1String(" ")
                          + (ppFeature.enabled ? QLatin1String("1\n") : QLatin1String("0\n")));
    }

    QString getVersionString(const QSurfaceFormat &format) {
        if (format.renderableType() == QSurfaceFormat::OpenGL) {
            // Too Old
            if (format.majorVersion() < 2)
                return QLatin1String("\n");
            // GL2
            if (format.majorVersion() == 2) {
                return QLatin1String("#version 110\n");
            } else {
                // sane version numbers
                QString version = QString(QLatin1String("#version %1%2")).arg(format.majorVersion()).arg(format.minorVersion());
                version += QLatin1String("0");
                if (format.profile() == QSurfaceFormat::CoreProfile) {
                    version += QLatin1String(" core");
                }
                return version + QLatin1String("\n");
            }

        } else {
            // versions only matter after ES3
            if (format.majorVersion() < 3)
                return QLatin1String("\n");

            return QString(QLatin1String("#version %1%2%3 es\n")).arg(format.majorVersion()).arg(format.minorVersion()).arg(0);
        }
    }

    QString addShaderExtensionStrings(const QSurfaceFormat &format)
    {
        Q_UNUSED(format)
        QString output;
        Q3DSGraphicsLimits limits = Q3DS::graphicsLimits();

        if (limits.format.renderableType() == QSurfaceFormat::OpenGLES)
            if (limits.format.majorVersion() == 2)
                if (limits.shaderTextureLodSupported)
                    output.append(QStringLiteral("#extension GL_EXT_shader_texture_lod : enable\n"));

        // TODO: Find a way to query extensions (without creating lots of contexts)
#if 0
        output.append(QLatin1String("#extension GL_ARB_gpu_shader5 : enable\n"));
        output.append(QLatin1String("#extension GL_ARB_shader_image_load_store : enable\n"));
        output.append(QLatin1String("#extension GL_ARB_shading_language_420pack : enable\n"));
        output.append(QLatin1String("#extension GL_ARB_shader_image_load_store : enable\n"));
        output.append(QLatin1String("#extension GL_ARB_shader_atomic_counters : enable\n"));
        output.append(QLatin1String("#extension GL_ARB_shader_storage_buffer_object : enable\n"));
        output.append(QLatin1String("#extension GL_KHR_blend_equation_advanced : enable\n"));
#endif
        return output;
    }

    QString addBackwardCompatibilityDefines(const QSurfaceFormat &format,
                                            Q3DSShaderGeneratorStages::Enum stage,
                                            const Q3DSShaderFeatureSet &inFeatureSet)
    {
        Q_UNUSED(format)
        QString output;
        if (stage == Q3DSShaderGeneratorStages::Enum::Vertex || stage == Q3DSShaderGeneratorStages::Enum::TessControl
                || stage == Q3DSShaderGeneratorStages::Enum::TessEval || stage == Q3DSShaderGeneratorStages::Enum::Geometry) {
            output += QLatin1String("#define attribute in\n");
            output += QLatin1String("#define varying out\n");
        } else if (stage == Q3DSShaderGeneratorStages::Enum::Fragment) {
            bool needsFragOutput = true;
            for (auto f : inFeatureSet) {
                if (f.enabled && f.name == QLatin1String("Q3DS_NO_FRAGOUTPUT")) {
                    needsFragOutput = false;
                    break;
                }
            }
            output += QLatin1String("#define varying in\n");
            output += QLatin1String("#define texture2D texture\n");
            if (needsFragOutput)
                output += QLatin1String("#define gl_FragColor fragOutput\n");

            // TODO: check if we have support for advanceBlendSupport
            //            if (m_RenderContext.IsAdvancedBlendSupportedKHR())
#if 0
            output += QLatin1String("layout(blend_support_all_equations) out;\n ");
#endif
            if (needsFragOutput)
                output += QLatin1String("out vec4 fragOutput;\n");
        }
        return output;
    }

    QVector<ShaderBinding> m_incoming;
    QVector<ShaderBinding> *m_outgoing;
    QVector<QString> m_includes;
    QVector<ShaderBinding> m_uniforms;
    QVector<ShaderBinding> m_constantBuffers;
    QVector<ConstantBuferParamPair> m_constantBufferParams;
    QString m_codeBuilder;
    QString m_finalBuilder;
    Q3DSShaderGeneratorStages::Enum m_stage;
    Q3DSShaderGeneratorStageFlags m_enabledStages;
    QStringList m_addedFunctions;
};

class VertexShaderGenerator : public StageGeneratorBase
{
public:
    VertexShaderGenerator()
        : StageGeneratorBase(Q3DSShaderGeneratorStages::Enum::Vertex)
    {}
    virtual QString getIncomingVariableName() { return QStringLiteral("attribute"); }
    virtual void addIncomingInterpolatedMap() {}

    virtual QString aetInterpolatedIncomingSuffix() const { return QStringLiteral("_attr"); }
    virtual QString getInterpolatedOutgoingSuffix() const { return QString(); }
};

class TessControlShaderGenerator : public StageGeneratorBase
{
public:
    TessControlShaderGenerator()
        : StageGeneratorBase(Q3DSShaderGeneratorStages::Enum::TessControl)
    {}
    void addShaderIncomingMap() override
    {
        addShaderItemMap(QLatin1String("attribute"), m_incoming, QLatin1String("[]"));
    }

    void addShaderOutgoingMap() override
    {
        if (m_outgoing)
            addShaderItemMap(QLatin1String("varying"), *m_outgoing, QLatin1String("[]"));
    }
};

class TessEvalShaderGenerator : public StageGeneratorBase
{
public:
    TessEvalShaderGenerator()
        : StageGeneratorBase(Q3DSShaderGeneratorStages::Enum::TessEval)
    {}
    void addShaderIncomingMap() override
    {
        addShaderItemMap(QLatin1String("attribute"), m_incoming, QLatin1String("[]"));
    }
};

class GeometryShaderGenerator : public StageGeneratorBase
{
public:
    GeometryShaderGenerator()
        : StageGeneratorBase(Q3DSShaderGeneratorStages::Enum::Geometry)
    {}
    void addShaderIncomingMap() override
    {
        addShaderItemMap(QLatin1String("attribute"), m_incoming, QLatin1String("[]"));
    }

    void addShaderOutgoingMap() override
    {
        if (m_outgoing)
            addShaderItemMap(QLatin1String("varying"), *m_outgoing);
    }
};

class FragmentShaderGenerator : public StageGeneratorBase
{
public:
    FragmentShaderGenerator()
        : StageGeneratorBase(Q3DSShaderGeneratorStages::Enum::Fragment)
    {}
    QString getIncomingVariableName() override
    {
        Q3DSGraphicsLimits limits = Q3DS::graphicsLimits();
        if (limits.format.renderableType() == QSurfaceFormat::OpenGLES
                && limits.format.majorVersion() == 2)
            return QStringLiteral("varying");
        return QStringLiteral("in");
    }
    void addShaderOutgoingMap() override
    {
        // Do nothing
    }
};

struct ShaderGeneratedProgramOutput
{
    QString m_vertexShader;
    QString m_tessControlShader;
    QString m_tessEvalShader;
    QString m_geometryShader;
    QString m_fragmentShader;

    ShaderGeneratedProgramOutput()
    {
    }

    ShaderGeneratedProgramOutput(const QString &vs, const QString &tc, const QString &te,
                                  const QString &gs, const QString &fs)
        : m_vertexShader(vs)
        , m_tessControlShader(tc)
        , m_tessEvalShader(te)
        , m_geometryShader(gs)
        , m_fragmentShader(fs)
    {
    }
};

const QString CopyrightHeaderStart = QStringLiteral("/****************************************************************************");
const QString CopyrightHeaderEnd = QStringLiteral("****************************************************************************/");

QStringList generateShaderLocationPrefixes(const QString &includeName, const QString &shaderLibraryVersion)
{
    return {
        (QString(Q3DSUtils::resourcePrefix() + QLatin1String("res/effectlib/")) + shaderLibraryVersion + QLatin1String("/") + includeName),
        (QString(Q3DSUtils::resourcePrefix() + QLatin1String("res/effectlib/")) + includeName)
    };
}

QString resolveShaderIncludes(const QString &shaderCode, const QString &shaderLibraryVersion)
{
    QString output;
    QTextStream inputStream(const_cast<QString*>(&shaderCode), QIODevice::ReadOnly);
    QString currentLine;
    while (inputStream.readLineInto(&currentLine)) {
        // Check if starts with #include
        auto trimmedLine = currentLine.trimmed();
        if (trimmedLine.startsWith(QStringLiteral("#include"))) {
            QString includeName = currentLine.split('"', QString::SkipEmptyParts).last();
            QStringList fileNames = generateShaderLocationPrefixes(includeName, shaderLibraryVersion);
            bool isResolved = false;
            for (QString fileName : fileNames) {
                QFile file(fileName);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    // strip copyright header
                    QString content = QString::fromUtf8(file.readAll());
                    if (content.startsWith(CopyrightHeaderStart)) {
                        int clipPos = content.indexOf(CopyrightHeaderEnd) ;
                        if (clipPos >= 0)
                            content.remove(0, clipPos + CopyrightHeaderEnd.count());
                    }
                    output.append(QStringLiteral("\n// begin \"") + includeName + QStringLiteral("\"\n"));
                    output.append(resolveShaderIncludes(content, shaderLibraryVersion));
                    output.append(QStringLiteral("\n// end \"" ) + includeName + QStringLiteral("\"\n"));
                    isResolved = true;
                    file.close();
                    break;
                }
            }
            if (!isResolved)
                qWarning() << QStringLiteral("Could not find any glsllib includes for: ") << fileNames;
            output.append(QLatin1String("\n"));
        } else {
            output.append(currentLine);
            output.append(QLatin1String("\n"));
        }
    }
    return output;
}


class ShaderProgramGenerator : public Q3DSAbstractShaderProgramGenerator
{
public:
    ShaderProgramGenerator()
        : m_cache(1000)
    {
    }

    void beginProgram(Q3DSShaderGeneratorStageFlags inEnabledStages) override
    {
        m_vs.begin(inEnabledStages);
        m_tc.begin(inEnabledStages);
        m_te.begin(inEnabledStages);
        m_gs.begin(inEnabledStages);
        m_fs.begin(inEnabledStages);
        m_enabledStages = inEnabledStages;
        linkStages();
    }
    Q3DSShaderGeneratorStageFlags getEnabledStages() const override
    {
        return m_enabledStages;
    }

    Q3DSAbstractShaderStageGenerator* getStage(Q3DSShaderGeneratorStages::Enum stage) override
    {
        if (stage > 0 || stage < Q3DSShaderGeneratorStages::Enum::StageCount) {
            if ((m_enabledStages & stage))
                return &internalGetStage(stage);
        } else {
            Q_ASSERT(false);
        }
        return nullptr;
    }

    Qt3DRender::QShaderProgram *compileGeneratedShader(const QString &inShaderName,
                                                       const Q3DSShaderFeatureSet &inFeatureSet,
                                                       bool separableProgram) override
    {
        Q_UNUSED(inShaderName)
        Q_UNUSED(separableProgram)

        // No stages enabled
        if (((quint32)m_enabledStages) == 0) {
            Q_ASSERT(false);
            return nullptr;
        }

        for (int stageIdx = 0, stageEnd = Q3DSShaderGeneratorStages::StageCount; stageIdx < stageEnd; ++stageIdx) {
            Q3DSShaderGeneratorStages::Enum stageName = static_cast<Q3DSShaderGeneratorStages::Enum>(1 << stageIdx);
            if (m_enabledStages & stageName) {
                StageGeneratorBase &theStage(internalGetStage(stageName));
                theStage.buildShaderSource(inFeatureSet);
            }
        }

        if (m_shaderContextLibraryVersion.isEmpty())
            resolveShaderLibraryVersion();

        QByteArray vertexShaderSource = resolveShaderIncludes(m_vs.m_finalBuilder, m_shaderContextLibraryVersion).toLocal8Bit();
        QByteArray tcShaderSource = resolveShaderIncludes(m_tc.m_finalBuilder, m_shaderContextLibraryVersion).toLocal8Bit();
        QByteArray teShaderSource = resolveShaderIncludes(m_te.m_finalBuilder, m_shaderContextLibraryVersion).toLocal8Bit();
        QByteArray geometryShaderSource = resolveShaderIncludes(m_gs.m_finalBuilder, m_shaderContextLibraryVersion).toLocal8Bit();
        QByteArray fragmentShaderSource = resolveShaderIncludes(m_fs.m_finalBuilder, m_shaderContextLibraryVersion).toLocal8Bit();

        // Debug
        static bool debug = qEnvironmentVariableIntValue("Q3DS_DEBUG") != 0;
        if (debug) {
            static int counter = 0;
            saveShaderFile(vertexShaderSource, QLatin1String("vertex") + QString::number(counter) + QLatin1String(".txt"));
            if (!tcShaderSource.isEmpty())
                saveShaderFile(tcShaderSource, QLatin1String("tc") + QString::number(counter) + QLatin1String(".txt"));
            if (!teShaderSource.isEmpty())
                saveShaderFile(teShaderSource, QLatin1String("te") + QString::number(counter) + QLatin1String(".txt"));
            if (!geometryShaderSource.isEmpty())
                saveShaderFile(geometryShaderSource, QLatin1String("geometry") + QString::number(counter) + QLatin1String(".txt"));
            saveShaderFile(fragmentShaderSource, QLatin1String("fragment") + QString::number(counter) + QLatin1String(".txt"));
            counter++;
        }

        // ### very naive caching, but even this gives huge memory savings
        // maybe replace with something else later on
        const QByteArray cacheKey = vertexShaderSource + tcShaderSource + teShaderSource + geometryShaderSource + fragmentShaderSource;
        if (m_cache.contains(cacheKey))
            return m_cache[cacheKey];

        auto shaderProgram = new Qt3DRender::QShaderProgram();
        shaderProgram->setVertexShaderCode(vertexShaderSource);
        shaderProgram->setTessellationControlShaderCode(tcShaderSource);
        shaderProgram->setTessellationEvaluationShaderCode(teShaderSource);
        shaderProgram->setGeometryShaderCode(geometryShaderSource);
        shaderProgram->setFragmentShaderCode(fragmentShaderSource);

        m_cache.insert(cacheKey, shaderProgram);

        if (m_profiler)
            m_profiler->trackNewObject(shaderProgram, Q3DSProfiler::ShaderProgramObject, "Shader program %s", qPrintable(inShaderName));

        return shaderProgram;
    }

    void invalidate() override
    {
        // clear() would also delete the objects in the cache - that is not
        // wanted since with Qt3D everything gets parented somewhere, and by
        // this stage the shader programs may have been already deleted.
        for (auto k : m_cache.keys())
            m_cache.take(k);
    }

private:
    void linkStages()
    {
        // link stages incming to outgoing variables
        StageGeneratorBase *previous = nullptr;
        int theStageId = 1;
        for (int idx = 0, end = (int)Q3DSShaderGeneratorStages::StageCount; idx < end;
             ++idx, theStageId = theStageId << 1) {
            StageGeneratorBase *thisStage = NULL;
            Q3DSShaderGeneratorStages::Enum theStageEnum =
                static_cast<Q3DSShaderGeneratorStages::Enum>(theStageId);
            if ((m_enabledStages & theStageEnum)) {
                thisStage = &internalGetStage(theStageEnum);
                if (previous)
                    previous->m_outgoing = &thisStage->m_incoming;
                previous = thisStage;
            }
        }
    }


    StageGeneratorBase &internalGetStage(Q3DSShaderGeneratorStages::Enum stage)
    {
        switch (stage) {
        case Q3DSShaderGeneratorStages::Enum::Vertex:
            return m_vs;
        case Q3DSShaderGeneratorStages::Enum::TessControl:
            return m_tc;
        case Q3DSShaderGeneratorStages::Enum::TessEval:
            return m_te;
        case Q3DSShaderGeneratorStages::Enum::Geometry:
            return m_gs;
        case Q3DSShaderGeneratorStages::Enum::Fragment:
            return m_fs;
        default:
            Q_ASSERT(false);
            break;
        }
        return m_vs;
    }

    void resolveShaderLibraryVersion()
    {
        QString versionString;
        if (Q3DS::graphicsLimits().useGles2Path)
            versionString = QLatin1String("gles2");
        m_shaderContextLibraryVersion = versionString;
    }

    VertexShaderGenerator m_vs;
    TessControlShaderGenerator m_tc;
    TessEvalShaderGenerator m_te;
    GeometryShaderGenerator m_gs;
    FragmentShaderGenerator m_fs;

    Q3DSShaderGeneratorStageFlags m_enabledStages;

    QCache<QByteArray, Qt3DRender::QShaderProgram> m_cache;
    QString m_shaderContextLibraryVersion;
};

} // end namespace


bool Q3DSShaderPreprocessorFeature::operator<(const Q3DSShaderPreprocessorFeature &other) const
{
    //return strcmp(m_name.c_str(), other.m_name.c_str()) < 0;
    return name < other.name;
}

bool Q3DSShaderPreprocessorFeature::operator==(const Q3DSShaderPreprocessorFeature &other) const
{

    return name == other.name && enabled == other.enabled;
}

Q3DSAbstractShaderProgramGenerator::~Q3DSAbstractShaderProgramGenerator()
{

}

Q3DSAbstractShaderProgramGenerator *Q3DSAbstractShaderProgramGenerator::createProgramGenerator()
{
    return new ShaderProgramGenerator();
}

Q3DSAbstractShaderStageGenerator::~Q3DSAbstractShaderStageGenerator()
{
}

QT_END_NAMESPACE
