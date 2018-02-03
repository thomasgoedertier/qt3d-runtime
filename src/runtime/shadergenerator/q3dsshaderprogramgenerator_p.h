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

#ifndef Q3DSSHADERPROGRAMGENERATOR_H
#define Q3DSSHADERPROGRAMGENERATOR_H

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

#include <Qt3DRender/QShaderProgram>
#include <QtCore/QString>

QT_BEGIN_NAMESPACE

class Q3DSProfiler;

// So far the generator is only useful for graphics stages,
// it doesn't seem useful for compute stages.
struct Q3DSShaderGeneratorStages
{
    enum Enum {
        Vertex = 1,
        TessControl = 1 << 1,
        TessEval = 1 << 2,
        Geometry = 1 << 3,
        Fragment = 1 << 4,
        StageCount = 5,
    };
};

Q_DECLARE_FLAGS(Q3DSShaderGeneratorStageFlags, Q3DSShaderGeneratorStages::Enum)

class Q3DSAbstractShaderStageGenerator
{
public:
    virtual ~Q3DSAbstractShaderStageGenerator();

    virtual void addIncoming(const char *name, const char *type) = 0;
    virtual void addOutgoing(const char *name, const char *type) = 0;
    virtual void addUniform(const char *name, const char *type) = 0;
    virtual void addInclude(const char *name) = 0;
    virtual void addFunction(const char *functionName) = 0;

    virtual void addConstantBuffer(const char *name, const char *layout) = 0;
    virtual void addConstantBufferParam(const char *cbName, const char *paramName,
                                        const char *type) = 0;

    virtual Q3DSAbstractShaderStageGenerator &operator<<(const char *data) = 0;
    virtual void append(const char *data) = 0;
    virtual void appendPartial(const char *data) = 0;

    virtual Q3DSShaderGeneratorStages::Enum stage() const = 0;
};

struct Q3DSShaderPreprocessorFeature
{
    QString m_name;
    bool m_enabled;
    Q3DSShaderPreprocessorFeature()
        : m_enabled(false)
    {
    }
    Q3DSShaderPreprocessorFeature(const QString &name, bool val)
        : m_name(name)
        , m_enabled(val)
    {
    }
    bool operator<(const Q3DSShaderPreprocessorFeature &other) const;
    bool operator==(const Q3DSShaderPreprocessorFeature &other) const;
};

typedef QVector<Q3DSShaderPreprocessorFeature> Q3DSShaderFeatureSet;

class Q3DSAbstractShaderProgramGenerator
{
public:
    virtual ~Q3DSAbstractShaderProgramGenerator();
    static Q3DSShaderGeneratorStageFlags defaultFlags()
    {
        return Q3DSShaderGeneratorStageFlags(Q3DSShaderGeneratorStages::Vertex
                                          | Q3DSShaderGeneratorStages::Fragment);
    }
    virtual void beginProgram(Q3DSShaderGeneratorStageFlags inEnabledStages = defaultFlags()) = 0;

    virtual Q3DSShaderGeneratorStageFlags getEnabledStages() const = 0;

    // get the stage or NULL if it has not been created.
    virtual Q3DSAbstractShaderStageGenerator *getStage(Q3DSShaderGeneratorStages::Enum inStage) = 0;

    // Implicit call to end program.
    virtual Qt3DRender::QShaderProgram *compileGeneratedShader(const QString &inShaderName,
                                                               const Q3DSShaderFeatureSet &inFeatureSet,
                                                               bool separableProgram = false) = 0;

    Qt3DRender::QShaderProgram *compileGeneratedShader(const QString &inShaderName,
                                                       bool separableProgram = false)
    {
        return compileGeneratedShader(inShaderName, Q3DSShaderFeatureSet(), separableProgram);
    }

    virtual void invalidate() = 0;

    static Q3DSAbstractShaderProgramGenerator *createProgramGenerator();
    void setProfiler(Q3DSProfiler *profiler) { m_profiler = profiler; }

#if 0
    static void outputParaboloidDepthVertex(Q3DSAbstractShaderStageGenerator &vertexShader);
    // By convention, the local space result of the TE is stored in vec4 pos local variable.
    // This function expects such state.
    static void outputParaboloidDepthTessEval(Q3DSAbstractShaderStageGenerator &tessEvalShader);
    // Utilities shared among the various different systems.
    static void outputParaboloidDepthFragment(Q3DSAbstractShaderStageGenerator &fragmentShader);

    static void outputCubeFaceDepthVertex(Q3DSAbstractShaderStageGenerator &vertexShader);
    static void outputCubeFaceDepthGeometry(Q3DSAbstractShaderStageGenerator &geometryShader);
    static void outputCubeFaceDepthFragment(Q3DSAbstractShaderStageGenerator &fragmentShader);
#endif

protected:
    Q3DSProfiler *m_profiler = nullptr;
};

QT_END_NAMESPACE

#endif // Q3DSSHADERPROGRAMGENERATOR_H
