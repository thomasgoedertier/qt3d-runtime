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

#include "q3dsshadermanager_p.h"
#include "q3dsshadergenerators_p.h"
#include <QOpenGLContext>

QT_BEGIN_NAMESPACE

Q3DSShaderManager &Q3DSShaderManager::instance()
{
    static Q3DSShaderManager instance;

    return instance;
}

Q3DSDefaultMaterialShaderGenerator *Q3DSShaderManager::defaultMaterialShaderGenerator()
{
    return m_materialShaderGenerator;
}

Q3DSCustomMaterialShaderGenerator *Q3DSShaderManager::customMaterialShaderGenerator()
{
    return m_customMaterialShaderGenerator;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::generateShaderProgram(Q3DSDefaultMaterial &material,
                                                                     const QVector<Q3DSLightNode*> &lights,
                                                                     bool hasTransparency,
                                                                     const Q3DSShaderFeatureSet &featureSet)
{
    Q3DSSubsetMaterialVertexPipeline pipeline(*m_materialShaderGenerator, *m_shaderProgramGenerator, false);
    return m_materialShaderGenerator->generateShader(material, pipeline, featureSet, lights, hasTransparency,
                                                     QString(QLatin1String("default material %1")).arg(material.id().constData()));
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::generateShaderProgram(Q3DSCustomMaterialInstance &material,
                                                                     const QVector<Q3DSLightNode *> &lights,
                                                                     bool hasTransparency,
                                                                     const Q3DSShaderFeatureSet &featureSet,
                                                                     const QString &shaderName)
{
    Q3DSCustomMaterialVertexPipeline pipeline(*m_materialShaderGenerator, *m_shaderProgramGenerator, false);
    return m_customMaterialShaderGenerator->generateShader(material, pipeline, featureSet, lights, hasTransparency, shaderName);
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getCubeDepthNoTessShader(Qt3DCore::QNode *parent)
{
    if (!m_cubeDepthShader) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        vertexShader->addIncoming("attr_pos", "vec3");
        vertexShader->addUniform("modelMatrix", "mat4");
        vertexShader->addUniform("modelViewProjection", "mat4");
        vertexShader->addOutgoing("world_pos", "vec4");
        vertexShader->append("void main() {");
        vertexShader->append("    world_pos = modelMatrix * vec4( attr_pos, 1.0 );");
        vertexShader->append("    world_pos /= world_pos.w;");
        vertexShader->append("    gl_Position = modelViewProjection * vec4( attr_pos, 1.0 );");
        vertexShader->append("}");

        fragmentShader->addUniform("camera_position", "vec3");
        fragmentShader->addUniform("camera_properties", "vec2");
        fragmentShader->append("void main() {");
        fragmentShader->append("    vec3 camPos = vec3( camera_position.x, camera_position.y, -camera_position.z );");
        fragmentShader->append("    float dist = length( world_pos.xyz - camPos );");
        fragmentShader->append("    dist = (dist - camera_properties.x) / (camera_properties.y - camera_properties.x);"); // prop.x == nearPlane, prop.y == farPlane
        fragmentShader->append("    fragOutput = vec4(dist, dist, dist, 1.0);");
        fragmentShader->append("}");

        m_cubeDepthShader = m_shaderProgramGenerator->compileGeneratedShader(QLatin1String("cubemap face depth shader"), Q3DSShaderFeatureSet());
        m_cubeDepthShader->setParent(parent);
    }

    return m_cubeDepthShader;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getOrthographicDepthNoTessShader(Qt3DCore::QNode *parent)
{
    if (!m_orthoDepthShader) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        vertexShader->addIncoming("attr_pos", "vec3");
        vertexShader->addUniform("modelViewProjection", "mat4");
        vertexShader->addOutgoing("outDepth", "vec3");
        vertexShader->append("void main() {");
        vertexShader->append("    gl_Position = modelViewProjection * vec4( attr_pos, 1.0 );");
        vertexShader->append("    outDepth.x = gl_Position.z / gl_Position.w;");
        vertexShader->append("}");

        fragmentShader->append("void main() {");
        fragmentShader->append("    float depth = (outDepth.x + 1.0) * 0.5;");
        fragmentShader->append("    fragOutput = vec4(depth);");
        fragmentShader->append("}");

        m_orthoDepthShader = m_shaderProgramGenerator->compileGeneratedShader(QLatin1String("orthographic depth shader"), Q3DSShaderFeatureSet());
        m_orthoDepthShader->setParent(parent);
    }

    return m_orthoDepthShader;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getDepthPrepassShader(Qt3DCore::QNode *parent, bool displaced)
{
    if (!m_depthPrePassShader) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        vertexShader->addIncoming("attr_pos", "vec3");
        vertexShader->addUniform("modelViewProjection", "mat4");
        vertexShader->append("void main() {");

        if (displaced) {
            // NB some uniform names are modified compared to NDD in order to match the names used in the normal pass.
            vertexShader->addIncoming("attr_uv0", "vec2");
            vertexShader->addIncoming("attr_norm", "vec3");
            vertexShader->addUniform("displacementMap_sampler", "sampler2D");
            vertexShader->addUniform("displaceAmount", "float");
            vertexShader->addUniform("displacementMap_rotations", "vec4");
            vertexShader->addUniform("displacementMap_offsets", "vec3");
            vertexShader->addInclude("defaultMaterialFileDisplacementTexture.glsllib");

            vertexShader->append("    vec3 uTransform = vec3( displacementMap_rotations.x, displacementMap_rotations.y, "
                            "displacementMap_offsets.x );");
            vertexShader->append("    vec3 vTransform = vec3( displacementMap_rotations.z, displacementMap_rotations.w, "
                            "displacementMap_offsets.y );");
            vertexShader->addFunction("getTransformedUVCoords");
            vertexShader->append("    vec2 uv_coords = attr_uv0;");
            vertexShader->append("    uv_coords = getTransformedUVCoords( vec3( uv_coords, 1.0), uTransform, vTransform );");
            vertexShader->append("    vec3 displacedPos = defaultMaterialFileDisplacementTexture( displacementMap_sampler, displaceAmount, uv_coords, attr_norm, attr_pos );");
            vertexShader->append("    gl_Position = modelViewProjection * vec4(displacedPos, 1.0);");
        } else {
            vertexShader->append("    gl_Position = modelViewProjection * vec4(attr_pos, 1.0);");
        }
        vertexShader->append("}");

        fragmentShader->append("void main() {");
        fragmentShader->append("    fragOutput = vec4(0.0);");
        fragmentShader->append("}");

        m_depthPrePassShader = m_shaderProgramGenerator->compileGeneratedShader(displaced ? QLatin1String("depth prepass displaced") : QLatin1String("depth prepass"), Q3DSShaderFeatureSet());
        m_depthPrePassShader->setParent(parent);
    }

    return m_depthPrePassShader;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getOrthoShadowBlurXShader(Qt3DCore::QNode *parent)
{
    if (!m_orthoShadowBlurXShader) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        // NB! The vertex shader is modified compared to the original: It uses
        // Qt3D attribute names (vertexPosition instead of attr_pos) and takes
        // modelMatrix into account.
        vertexShader->addIncoming("vertexPosition", "vec3");
        vertexShader->addIncoming("vertexTexCoord", "vec2"); // texcoord behavior is not the same as in the cube blur shaders
        vertexShader->addUniform("modelMatrix", "mat4");
        vertexShader->addOutgoing("uv_coords", "vec2");
        vertexShader->append("void main() {");
        vertexShader->append("    vec4 pos = modelMatrix * vec4(vertexPosition, 1.0);");
        vertexShader->append("    gl_Position = pos;");
        vertexShader->append("    uv_coords = vertexTexCoord;");
        vertexShader->append("}");

        fragmentShader->addUniform("camera_properties", "vec2");
        fragmentShader->addUniform("depthSrc", "sampler2D");
        fragmentShader->append("void main() {");
        fragmentShader->append("    vec2 ofsScale = vec2( camera_properties.x / 7680.0, 0.0 );");
        fragmentShader->append("    float depth0 = texture(depthSrc, uv_coords).x;");
        fragmentShader->append("    float depth1 = texture(depthSrc, uv_coords + ofsScale).x;");
        fragmentShader->append("    depth1 += texture(depthSrc, uv_coords - ofsScale).x;");
        fragmentShader->append(
                    "    float depth2 = texture(depthSrc, uv_coords + 2.0 * ofsScale).x;");
        fragmentShader->append("    depth2 += texture(depthSrc, uv_coords - 2.0 * ofsScale).x;");
        fragmentShader->append(
                    "    float outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    fragOutput = vec4(outDepth);");
        fragmentShader->append("}");

        m_orthoShadowBlurXShader = m_shaderProgramGenerator->compileGeneratedShader(QLatin1String("shadow map blur X shader"), Q3DSShaderFeatureSet());
        m_orthoShadowBlurXShader->setParent(parent);
    }

    return m_orthoShadowBlurXShader;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getOrthoShadowBlurYShader(Qt3DCore::QNode *parent)
{
    if (!m_orthoShadowBlurYShader) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        // NB! The vertex shader is modified compared to the original: It uses
        // Qt3D attribute names (vertexPosition instead of attr_pos) and takes
        // modelMatrix into account.
        vertexShader->addIncoming("vertexPosition", "vec3");
        vertexShader->addIncoming("vertexTexCoord", "vec2"); // texcoord behavior is not the same as in the cube blur shaders
        vertexShader->addUniform("modelMatrix", "mat4");
        vertexShader->addOutgoing("uv_coords", "vec2");
        vertexShader->append("void main() {");
        vertexShader->append("    vec4 pos = modelMatrix * vec4(vertexPosition, 1.0);");
        vertexShader->append("    gl_Position = pos;");
        vertexShader->append("    uv_coords = vertexTexCoord;");
        vertexShader->append("}");

        fragmentShader->addUniform("camera_properties", "vec2");
        fragmentShader->addUniform("depthSrc", "sampler2D");
        fragmentShader->append("void main() {");
        fragmentShader->append("    vec2 ofsScale = vec2( 0.0, camera_properties.x / 7680.0 );");
        fragmentShader->append("    float depth0 = texture(depthSrc, uv_coords).x;");
        fragmentShader->append("    float depth1 = texture(depthSrc, uv_coords + ofsScale).x;");
        fragmentShader->append("    depth1 += texture(depthSrc, uv_coords - ofsScale).x;");
        fragmentShader->append(
                    "    float depth2 = texture(depthSrc, uv_coords + 2.0 * ofsScale).x;");
        fragmentShader->append("    depth2 += texture(depthSrc, uv_coords - 2.0 * ofsScale).x;");
        fragmentShader->append(
                    "    float outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    fragOutput = vec4(outDepth);");
        fragmentShader->append("}");

        m_orthoShadowBlurYShader = m_shaderProgramGenerator->compileGeneratedShader(QLatin1String("shadow map blur Y shader"), Q3DSShaderFeatureSet());
        m_orthoShadowBlurYShader->setParent(parent);
    }

    return m_orthoShadowBlurYShader;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getCubeShadowBlurXShader(Qt3DCore::QNode *parent, const Q3DSGraphicsLimits &)
{
    if (!m_cubeShadowBlurXShader) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        // NB! The vertex shader is modified compared to the original: It uses
        // Qt3D attribute names (vertexPosition instead of attr_pos) and takes
        // modelMatrix into account.
        vertexShader->addIncoming("vertexPosition", "vec3");
        vertexShader->addUniform("modelMatrix", "mat4");
        vertexShader->addOutgoing("uv_coords", "vec2");
        vertexShader->append("void main() {");
        vertexShader->append("    vec4 pos = modelMatrix * vec4(vertexPosition, 1.0);");
        vertexShader->append("    gl_Position = pos;");
        vertexShader->append("    uv_coords.xy = pos.xy;");
        vertexShader->append("}");

        // This with the ShadowBlurYShader design for a 2-pass 5x5 (sigma=1.0)
        // Weights computed using -- http://dev.theomader.com/gaussian-kernel-calculator/
        fragmentShader->addUniform("camera_properties", "vec2");
        fragmentShader->addUniform("depthCube", "samplerCube");
        fragmentShader->append("layout(location = 0) out vec4 frag0;");
        fragmentShader->append("layout(location = 1) out vec4 frag1;");
        fragmentShader->append("layout(location = 2) out vec4 frag2;");
        fragmentShader->append("layout(location = 3) out vec4 frag3;");
        fragmentShader->append("layout(location = 4) out vec4 frag4;");
        fragmentShader->append("layout(location = 5) out vec4 frag5;");
        fragmentShader->append("void main() {");
        fragmentShader->append("    float ofsScale = camera_properties.x / 2500.0;");
        fragmentShader->append("    vec3 dir0 = vec3(1.0, -uv_coords.y, -uv_coords.x);");
        fragmentShader->append("    vec3 dir1 = vec3(-1.0, -uv_coords.y, uv_coords.x);");
        fragmentShader->append("    vec3 dir2 = vec3(uv_coords.x, 1.0, uv_coords.y);");
        fragmentShader->append("    vec3 dir3 = vec3(uv_coords.x, -1.0, -uv_coords.y);");
        fragmentShader->append("    vec3 dir4 = vec3(uv_coords.x, -uv_coords.y, 1.0);");
        fragmentShader->append("    vec3 dir5 = vec3(-uv_coords.x, -uv_coords.y, -1.0);");
        fragmentShader->append("    float depth0;");
        fragmentShader->append("    float depth1;");
        fragmentShader->append("    float depth2;");
        fragmentShader->append("    float outDepth;");
        fragmentShader->append("    depth0 = texture(depthCube, dir0).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir0 + vec3(0.0, 0.0, -ofsScale)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir0 + vec3(0.0, 0.0, ofsScale)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir0 + vec3(0.0, 0.0, -2.0*ofsScale)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir0 + vec3(0.0, 0.0, 2.0*ofsScale)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag0 = vec4(outDepth);");

        fragmentShader->append("    depth0 = texture(depthCube, dir1).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir1 + vec3(0.0, 0.0, -ofsScale)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir1 + vec3(0.0, 0.0, ofsScale)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir1 + vec3(0.0, 0.0, -2.0*ofsScale)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir1 + vec3(0.0, 0.0, 2.0*ofsScale)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag1 = vec4(outDepth);");

        fragmentShader->append("    depth0 = texture(depthCube, dir2).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir2 + vec3(-ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir2 + vec3(ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir2 + vec3(-2.0*ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir2 + vec3(2.0*ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag2 = vec4(outDepth);");

        fragmentShader->append("    depth0 = texture(depthCube, dir3).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir3 + vec3(-ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir3 + vec3(ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir3 + vec3(-2.0*ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir3 + vec3(2.0*ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag3 = vec4(outDepth);");

        fragmentShader->append("    depth0 = texture(depthCube, dir4).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir4 + vec3(-ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir4 + vec3(ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir4 + vec3(-2.0*ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir4 + vec3(2.0*ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag4 = vec4(outDepth);");

        fragmentShader->append("    depth0 = texture(depthCube, dir5).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir5 + vec3(-ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir5 + vec3(ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir5 + vec3(-2.0*ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir5 + vec3(2.0*ofsScale, 0.0, 0.0)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag5 = vec4(outDepth);");

        fragmentShader->append("}");

        m_cubeShadowBlurXShader = m_shaderProgramGenerator->compileGeneratedShader(QLatin1String("cubemap shadow blur X shader"), Q3DSShaderFeatureSet());
        m_cubeShadowBlurXShader->setParent(parent);
    }

    return m_cubeShadowBlurXShader;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getCubeShadowBlurYShader(Qt3DCore::QNode *parent, const Q3DSGraphicsLimits &)
{
    if (!m_cubeShadowBlurYShader) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        // NB! The vertex shader is modified compared to the original: It uses
        // Qt3D attribute names (vertexPosition instead of attr_pos) and takes
        // modelMatrix into account.
        vertexShader->addIncoming("vertexPosition", "vec3");
        vertexShader->addUniform("modelMatrix", "mat4");
        vertexShader->addOutgoing("uv_coords", "vec2");
        vertexShader->append("void main() {");
        vertexShader->append("    vec4 pos = modelMatrix * vec4(vertexPosition, 1.0);");
        vertexShader->append("    gl_Position = pos;");
        vertexShader->append("    uv_coords.xy = pos.xy;");
        vertexShader->append("}");

        // This with the ShadowBlurXShader design for a 2-pass 5x5 (sigma=1.0)
        // Weights computed using -- http://dev.theomader.com/gaussian-kernel-calculator/
        fragmentShader->addUniform("camera_properties", "vec2");
        fragmentShader->addUniform("depthCube", "samplerCube");
        fragmentShader->append("layout(location = 0) out vec4 frag0;");
        fragmentShader->append("layout(location = 1) out vec4 frag1;");
        fragmentShader->append("layout(location = 2) out vec4 frag2;");
        fragmentShader->append("layout(location = 3) out vec4 frag3;");
        fragmentShader->append("layout(location = 4) out vec4 frag4;");
        fragmentShader->append("layout(location = 5) out vec4 frag5;");
        fragmentShader->append("void main() {");
        fragmentShader->append("    float ofsScale = camera_properties.x / 2500.0;");
        fragmentShader->append("    vec3 dir0 = vec3(1.0, -uv_coords.y, -uv_coords.x);");
        fragmentShader->append("    vec3 dir1 = vec3(-1.0, -uv_coords.y, uv_coords.x);");
        fragmentShader->append("    vec3 dir2 = vec3(uv_coords.x, 1.0, uv_coords.y);");
        fragmentShader->append("    vec3 dir3 = vec3(uv_coords.x, -1.0, -uv_coords.y);");
        fragmentShader->append("    vec3 dir4 = vec3(uv_coords.x, -uv_coords.y, 1.0);");
        fragmentShader->append("    vec3 dir5 = vec3(-uv_coords.x, -uv_coords.y, -1.0);");
        fragmentShader->append("    float depth0;");
        fragmentShader->append("    float depth1;");
        fragmentShader->append("    float depth2;");
        fragmentShader->append("    float outDepth;");
        fragmentShader->append("    depth0 = texture(depthCube, dir0).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir0 + vec3(0.0, -ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir0 + vec3(0.0, ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir0 + vec3(0.0, -2.0*ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir0 + vec3(0.0, 2.0*ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag0 = vec4(outDepth);");

        fragmentShader->append("    depth0 = texture(depthCube, dir1).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir1 + vec3(0.0, -ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir1 + vec3(0.0, ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir1 + vec3(0.0, -2.0*ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir1 + vec3(0.0, 2.0*ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag1 = vec4(outDepth);");

        fragmentShader->append("    depth0 = texture(depthCube, dir2).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir2 + vec3(0.0, 0.0, -ofsScale)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir2 + vec3(0.0, 0.0, ofsScale)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir2 + vec3(0.0, 0.0, -2.0*ofsScale)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir2 + vec3(0.0, 0.0, 2.0*ofsScale)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag2 = vec4(outDepth);");

        fragmentShader->append("    depth0 = texture(depthCube, dir3).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir3 + vec3(0.0, 0.0, -ofsScale)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir3 + vec3(0.0, 0.0, ofsScale)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir3 + vec3(0.0, 0.0, -2.0*ofsScale)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir3 + vec3(0.0, 0.0, 2.0*ofsScale)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag3 = vec4(outDepth);");

        fragmentShader->append("    depth0 = texture(depthCube, dir4).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir4 + vec3(0.0, -ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir4 + vec3(0.0, ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir4 + vec3(0.0, -2.0*ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir4 + vec3(0.0, 2.0*ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag4 = vec4(outDepth);");

        fragmentShader->append("    depth0 = texture(depthCube, dir5).x;");
        fragmentShader->append(
                    "    depth1 = texture(depthCube, dir5 + vec3(0.0, -ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth1 += texture(depthCube, dir5 + vec3(0.0, ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 = texture(depthCube, dir5 + vec3(0.0, -2.0*ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    depth2 += texture(depthCube, dir5 + vec3(0.0, 2.0*ofsScale, 0.0)).x;");
        fragmentShader->append(
                    "    outDepth = 0.38774 * depth0 + 0.24477 * depth1 + 0.06136 * depth2;");
        fragmentShader->append("    frag5 = vec4(outDepth);");

        fragmentShader->append("}");

        m_cubeShadowBlurYShader = m_shaderProgramGenerator->compileGeneratedShader(QLatin1String("cubemap shadow blur Y shader"), Q3DSShaderFeatureSet());
        m_cubeShadowBlurYShader->setParent(parent);
    }

    return m_cubeShadowBlurYShader;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getSsaoTextureShader(Qt3DCore::QNode *parent)
{
    if (!m_ssaoTextureShader) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        // use qt3d attribute names, use modelMatrix as well
        vertexShader->addIncoming("vertexPosition", "vec3");
        vertexShader->addIncoming("vertexTexCoord", "vec2");
        vertexShader->addUniform("modelMatrix", "mat4");
        vertexShader->addOutgoing("uv_coords", "vec2");
        vertexShader->append("void main() {");
        vertexShader->append("    vec4 pos = modelMatrix * vec4(vertexPosition, 1.0);");
        vertexShader->append("    gl_Position = vec4( pos.xy, 0.5, 1.0 );");
        vertexShader->append("    uv_coords = vertexTexCoord;");
        vertexShader->append("}");

        fragmentShader->addInclude("viewProperties.glsllib");
        fragmentShader->addInclude("screenSpaceAO.glsllib");

        (*fragmentShader) << "layout (std140) uniform cbAoShadow {\n"
                          << "    vec4 ao_properties;\n"
                          << "    vec4 ao_properties2;\n"
                          << "    vec4 shadow_properties;\n"
                          << "    vec4 aoScreenConst;\n"
                          << "    vec4 UvToEyeConst;\n"
                          << "};\n";

        fragmentShader->addUniform("depth_sampler", "sampler2D");
        fragmentShader->append("void main() {");
        (*fragmentShader) << "    float aoFactor;\n";
        (*fragmentShader) << "    vec3 screenNorm;\n";

        // We're taking multiple depth samples and getting the derivatives at
        // each of them to get a more accurate view space normal vector. When
        // we do only one, we tend to get bizarre values at the edges
        // surrounding objects, and this also ends up giving us weird AO
        // values. If we had a proper screen-space normal map, that would also
        // do the trick.
        fragmentShader->append("    ivec2 iCoords = ivec2( gl_FragCoord.xy );");
        fragmentShader->append("    float depth = getDepthValue( texelFetch(depth_sampler, iCoords, 0), camera_properties );");
        fragmentShader->append("    depth = depthValueToLinearDistance( depth, camera_properties );");
        fragmentShader->append("    depth = (depth - camera_properties.x) / (camera_properties.y - camera_properties.x);");
        fragmentShader->append("    float depth2 = getDepthValue( texelFetch(depth_sampler, iCoords+ivec2(1), 0), camera_properties );");
        fragmentShader->append("    depth2 = depthValueToLinearDistance( depth, camera_properties );");
        fragmentShader->append("    float depth3 = getDepthValue( texelFetch(depth_sampler, iCoords-ivec2(1), 0), camera_properties );");
        fragmentShader->append("    depth3 = depthValueToLinearDistance( depth, camera_properties );");
        fragmentShader->append("    vec3 tanU = vec3(10, 0, dFdx(depth));");
        fragmentShader->append("    vec3 tanV = vec3(0, 10, dFdy(depth));");
        fragmentShader->append("    screenNorm = normalize(cross(tanU, tanV));");
        fragmentShader->append("    tanU = vec3(10, 0, dFdx(depth2));");
        fragmentShader->append("    tanV = vec3(0, 10, dFdy(depth2));");
        fragmentShader->append("    screenNorm += normalize(cross(tanU, tanV));");
        fragmentShader->append("    tanU = vec3(10, 0, dFdx(depth3));");
        fragmentShader->append("    tanV = vec3(0, 10, dFdy(depth3));");
        fragmentShader->append("    screenNorm += normalize(cross(tanU, tanV));");
        fragmentShader->append("    screenNorm = -normalize(screenNorm);");

        fragmentShader->append("    aoFactor = SSambientOcclusion( depth_sampler, screenNorm, ao_properties, ao_properties2, camera_properties, aoScreenConst, UvToEyeConst );");

        fragmentShader->append("    fragOutput = vec4(aoFactor, aoFactor, aoFactor, 1.0);");
        fragmentShader->append("}");

        m_ssaoTextureShader = m_shaderProgramGenerator->compileGeneratedShader(QLatin1String("fullscreen AO pass shader"), Q3DSShaderFeatureSet());
        m_ssaoTextureShader->setParent(parent);
    }

    return m_ssaoTextureShader;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getBsdfMipPreFilterShader(Qt3DCore::QNode *parent)
{
    if (!m_bsdfMipPreFilterShader) {
        m_bsdfMipPreFilterShader = new Qt3DRender::QShaderProgram;
        bool isOpenGLES = QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES;
        QByteArray code;

        // NB changed to rgba32f as opposed to the half float (rgba16f) original

        if (isOpenGLES) {
            code += "#version 310 es\n"
                    "#extension GL_ARB_compute_shader : enable\n"
                    "precision highp float;\n"
                    "precision highp int;\n"
                    "precision mediump image2D;\n";
        } else {
            code += "#version 430\n"
                    "#extension GL_ARB_compute_shader : enable\n";
        }

        code += "int wrapMod( in int a, in int base )\n"
                "{\n"
                "  return ( a >= 0 ) ? a % base : -(a % base) + base;\n"
                "}\n";

        code += "void getWrappedCoords( inout int sX, inout int sY, in int width, in int height )\n"
                "{\n"
                "  if (sY < 0) { sX -= width >> 1; sY = -sY; }\n"
                "  if (sY >= height) { sX += width >> 1; sY = height - sY; }\n"
                "  sX = wrapMod( sX, width );\n"
                "}\n";

        code += "// Set workgroup layout;\n"
                "layout (local_size_x = 16, local_size_y = 16) in;\n\n"
                "layout (rgba32f, binding = 1) readonly uniform image2D inputImage;\n\n"
                "layout (rgba32f, binding = 2) writeonly uniform image2D outputImage;\n\n"
                "void main()\n"
                "{\n"
                "  int prevWidth = int(gl_NumWorkGroups.x) << 1;\n"
                "  int prevHeight = int(gl_NumWorkGroups.y) << 1;\n"
                "  if ( gl_GlobalInvocationID.x >= gl_NumWorkGroups.x || gl_GlobalInvocationID.y >= "
                "gl_NumWorkGroups.y )\n"
                "    return;\n"
                "  vec4 accumVal = vec4(0.0);\n"
                "  for ( int sy = -2; sy <= 2; ++sy )\n"
                "  {\n"
                "    for ( int sx = -2; sx <= 2; ++sx )\n"
                "    {\n"
                "      int sampleX = sx + (int(gl_GlobalInvocationID.x) << 1);\n"
                "      int sampleY = sy + (int(gl_GlobalInvocationID.y) << 1);\n"
                "      getWrappedCoords(sampleX, sampleY, prevWidth, prevHeight);\n"
                "      if ((sampleY * prevWidth + sampleX) < 0 )\n"
                "        sampleY = prevHeight + sampleY;\n"
                "      ivec2 pos = ivec2(sampleX, sampleY);\n"
                "      vec4 value = imageLoad(inputImage, pos);\n"
                "      float filterPdf = 1.0 / ( 1.0 + float(sx*sx + sy*sy)*2.0 );\n"
                "      filterPdf /= 4.71238898;\n"
                "      accumVal[0] += filterPdf * value.r;\n"
                "      accumVal[1] += filterPdf * value.g;\n"
                "      accumVal[2] += filterPdf * value.b;\n"
                "      accumVal[3] += filterPdf * value.a;\n"
                "    }\n"
                "  }\n"
                "  imageStore( outputImage, ivec2(gl_GlobalInvocationID.xy), accumVal );\n"
                "}\n";

        m_bsdfMipPreFilterShader->setComputeShaderCode(code);
        m_bsdfMipPreFilterShader->setParent(parent);
    }

    return m_bsdfMipPreFilterShader;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getProgAABlendShader(Qt3DCore::QNode *parent)
{
    if (!m_progAABlendShader) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        // Use Qt3D attribute names and take modelMatrix into account since the
        // quad entity may have a transform on it.
        vertexShader->addIncoming("vertexPosition", "vec3");
        vertexShader->addIncoming("vertexTexCoord", "vec2");
        vertexShader->addUniform("modelMatrix", "mat4");
        vertexShader->addOutgoing("uv_coords", "vec2");
        vertexShader->append("void main() {");
        vertexShader->append("    gl_Position = modelMatrix * vec4(vertexPosition, 1.0 );");
        vertexShader->append("    uv_coords = vertexTexCoord;");
        vertexShader->append("}");

        fragmentShader->addUniform("accumulator", "sampler2D");
        fragmentShader->addUniform("last_frame", "sampler2D");
        fragmentShader->addUniform("blend_factors", "vec2");
        fragmentShader->append("void main() {");
        fragmentShader->append("    vec4 accum = texture2D( accumulator, uv_coords );");
        fragmentShader->append("    vec4 lastFrame = texture2D( last_frame, uv_coords );");
        fragmentShader->append("    fragOutput = accum*blend_factors.y + lastFrame*blend_factors.x;");
        fragmentShader->append("}");

        m_progAABlendShader = m_shaderProgramGenerator->compileGeneratedShader(
                    QLatin1String("layer progressive/temporal AA blend shader"), Q3DSShaderFeatureSet());
        m_progAABlendShader->setParent(parent);
    }

    return m_progAABlendShader;
}

static void addBlendShaderPreamble(Q3DSAbstractShaderStageGenerator *fragmentShader, int msaaSampleCount)
{
    Q_UNUSED(msaaSampleCount);
    fragmentShader->addUniform("base_layer", "sampler2D");
    if (msaaSampleCount <= 1)
        fragmentShader->addUniform("blend_layer", "sampler2D");
    else
        fragmentShader->addUniform("blend_layer", "sampler2DMS");

    fragmentShader->append("void main() {");
    fragmentShader->append("    vec4 base = texture2D( base_layer, uv_coords );");
    fragmentShader->append("    if (base.a != 0.0) base.rgb /= base.a; else base = vec4(0.0);");
    if (msaaSampleCount <= 1) {
        fragmentShader->append("    vec4 blend = texture2D( blend_layer, uv_coords );");
    } else {
        fragmentShader->append("    ivec2 tc = ivec2(floor(textureSize(blend_layer) * uv_coords));");
        switch (msaaSampleCount) {
        case 2:
            fragmentShader->append("    vec4 blend = texelFetch(blend_layer, tc, 0) + texelFetch(blend_layer, tc, 1);");
            fragmentShader->append("    blend /= 2.0;");
            break;
        default:
            fragmentShader->append("    vec4 blend = texelFetch(blend_layer, tc, 0) + texelFetch(blend_layer, tc, 1) + texelFetch(blend_layer, tc, 2) + texelFetch(blend_layer, tc, 3);");
            fragmentShader->append("    blend /= 4.0;");
            break;
        }
    }
    fragmentShader->append("    if (blend.a != 0.0) blend.rgb /= blend.a; else blend = vec4(0.0);");
    fragmentShader->append("    vec4 res = vec4(0.0);");
    fragmentShader->append("    float p0 = base.a * blend.a; float p1 = base.a * (1 - blend.a); float p2 = blend.a * (1 - base.a);");
    fragmentShader->append("    res.a = p0 + p1 + p2;");
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getBlendOverlayShader(Qt3DCore::QNode *parent, int msaaSampleCount)
{
    Qt3DRender::QShaderProgram *&prog = m_blendOverlayShader[msaaSampleCount];
    if (!prog) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        // Use Qt3D attribute names and take modelMatrix into account since the
        // quad entity may have a transform on it.
        vertexShader->addIncoming("vertexPosition", "vec3");
        vertexShader->addIncoming("vertexTexCoord", "vec2");
        vertexShader->addUniform("modelMatrix", "mat4");
        vertexShader->addOutgoing("uv_coords", "vec2");
        vertexShader->append("void main() {");
        vertexShader->append("    gl_Position = modelMatrix * vec4(vertexPosition, 1.0 );");
        vertexShader->append("    uv_coords = vertexTexCoord;");
        vertexShader->append("}");

        // This deviates from the original 3DS1 shader.
        // Below is based on https://www.khronos.org/registry/OpenGL/extensions/KHR/KHR_blend_equation_advanced.txt
        // with proper handling of premultiplied alpha:
        addBlendShaderPreamble(fragmentShader, msaaSampleCount);
        // overlay = 2 * bottom * top if bottom < 0.5, 1 - 2 * (1 - bottom) * (1 - top) otherwise
        fragmentShader->append("    float f_rs_rd = (base.r < 0.5 ? (2.0 * base.r * blend.r) : (1.0 - 2.0 * (1.0 - base.r) * (1.0 - blend.r)));");
        fragmentShader->append("    float f_gs_gd = (base.g < 0.5 ? (2.0 * base.g * blend.g) : (1.0 - 2.0 * (1.0 - base.g) * (1.0 - blend.g)));");
        fragmentShader->append("    float f_bs_bd = (base.b < 0.5 ? (2.0 * base.b * blend.b) : (1.0 - 2.0 * (1.0 - base.b) * (1.0 - blend.b)));");
        fragmentShader->append("    res.r = f_rs_rd * p0 + base.r * p1 + blend.r * p2;");
        fragmentShader->append("    res.g = f_gs_gd * p0 + base.g * p1 + blend.g * p2;");
        fragmentShader->append("    res.b = f_bs_bd * p0 + base.b * p1 + blend.b * p2;");
        fragmentShader->append("    fragOutput = vec4(res.rgb * res.a, res.a);");
        fragmentShader->append("}");

        prog = m_shaderProgramGenerator->compileGeneratedShader(
                    QLatin1String("advanced overlay blend shader"), Q3DSShaderFeatureSet());
        prog->setParent(parent);
    }

    return prog;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getBlendColorBurnShader(Qt3DCore::QNode *parent, int msaaSampleCount)
{
    Qt3DRender::QShaderProgram *&prog = m_blendColorBurnShader[msaaSampleCount];
    if (!prog) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        // Use Qt3D attribute names and take modelMatrix into account since the
        // quad entity may have a transform on it.
        vertexShader->addIncoming("vertexPosition", "vec3");
        vertexShader->addIncoming("vertexTexCoord", "vec2");
        vertexShader->addUniform("modelMatrix", "mat4");
        vertexShader->addOutgoing("uv_coords", "vec2");
        vertexShader->append("void main() {");
        vertexShader->append("    gl_Position = modelMatrix * vec4(vertexPosition, 1.0 );");
        vertexShader->append("    uv_coords = vertexTexCoord;");
        vertexShader->append("}");

        // This deviates from the original 3DS1 shader.
        // Below is based on https://www.khronos.org/registry/OpenGL/extensions/KHR/KHR_blend_equation_advanced.txt
        // with proper handling of premultiplied alpha:
        addBlendShaderPreamble(fragmentShader, msaaSampleCount);
        // color burn = invert the bottom layer, divide it by the top layer, then invert
        fragmentShader->append("    float f_rs_rd = ((base.r == 1.0) ? 1.0 : (blend.r == 0.0) ? 0.0 : 1.0 - min(1.0, ((1.0 - base.r) / blend.r)));");
        fragmentShader->append("    float f_gs_gd = ((base.g == 1.0) ? 1.0 : (blend.g == 0.0) ? 0.0 : 1.0 - min(1.0, ((1.0 - base.g) / blend.g)));");
        fragmentShader->append("    float f_bs_bd = ((base.b == 1.0) ? 1.0 : (blend.b == 0.0) ? 0.0 : 1.0 - min(1.0, ((1.0 - base.b) / blend.b)));");
        fragmentShader->append("    res.r = f_rs_rd * p0 + base.r * p1 + blend.r * p2;");
        fragmentShader->append("    res.g = f_gs_gd * p0 + base.g * p1 + blend.g * p2;");
        fragmentShader->append("    res.b = f_bs_bd * p0 + base.b * p1 + blend.b * p2;");
        fragmentShader->append("    fragOutput = vec4(res.rgb * res.a, res.a);");
        fragmentShader->append("}");

        prog = m_shaderProgramGenerator->compileGeneratedShader(
                    QLatin1String("advanced color burn blend shader"), Q3DSShaderFeatureSet());
        prog->setParent(parent);
    }

    return prog;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getBlendColorDodgeShader(Qt3DCore::QNode *parent, int msaaSampleCount)
{
    Qt3DRender::QShaderProgram *&prog = m_blendColorDodgeShader[msaaSampleCount];
    if (!prog) {
        m_shaderProgramGenerator->beginProgram();
        Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
        Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

        // Use Qt3D attribute names and take modelMatrix into account since the
        // quad entity may have a transform on it.
        vertexShader->addIncoming("vertexPosition", "vec3");
        vertexShader->addIncoming("vertexTexCoord", "vec2");
        vertexShader->addUniform("modelMatrix", "mat4");
        vertexShader->addOutgoing("uv_coords", "vec2");
        vertexShader->append("void main() {");
        vertexShader->append("    gl_Position = modelMatrix * vec4(vertexPosition, 1.0 );");
        vertexShader->append("    uv_coords = vertexTexCoord;");
        vertexShader->append("}");

        // This deviates from the original 3DS1 shader.
        // Below is based on https://www.khronos.org/registry/OpenGL/extensions/KHR/KHR_blend_equation_advanced.txt
        // with proper handling of premultiplied alpha:
        addBlendShaderPreamble(fragmentShader, msaaSampleCount);
        // color dodge = divide bottom layer by inverted top layer
        fragmentShader->append("    float f_rs_rd = ((base.r == 0.0) ? 0.0 : (blend.r == 1.0) ? 1.0 : min(base.r / (1.0 - blend.r), 1.0));");
        fragmentShader->append("    float f_gs_gd = ((base.g == 0.0) ? 0.0 : (blend.g == 1.0) ? 1.0 : min(base.g / (1.0 - blend.g), 1.0));");
        fragmentShader->append("    float f_bs_bd = ((base.b == 0.0) ? 0.0 : (blend.b == 1.0) ? 1.0 : min(base.b / (1.0 - blend.b), 1.0));");
        fragmentShader->append("    res.r = f_rs_rd * p0 + base.r * p1 + blend.r * p2;");
        fragmentShader->append("    res.g = f_gs_gd * p0 + base.g * p1 + blend.g * p2;");
        fragmentShader->append("    res.b = f_bs_bd * p0 + base.b * p1 + blend.b * p2;");
        fragmentShader->append("    fragOutput = vec4(res.rgb * res.a, res.a);");
        fragmentShader->append("}");

        prog = m_shaderProgramGenerator->compileGeneratedShader(
                    QLatin1String("advanced color dodge blend shader"), Q3DSShaderFeatureSet());
        prog->setParent(parent);
    }

    return prog;
}

Qt3DRender::QShaderProgram *Q3DSShaderManager::getEffectShader(Qt3DCore::QNode *parent,
                                                               const QString &name,
                                                               const QString &vertexShaderSrc,
                                                               const QString &fragmentShaderSrc)
{
    // No cache on this level. m_shaderProgramGenerator has one.

    m_shaderProgramGenerator->beginProgram();
    Q3DSAbstractShaderStageGenerator *vertexShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Vertex);
    Q3DSAbstractShaderStageGenerator *fragmentShader = m_shaderProgramGenerator->getStage(Q3DSShaderGeneratorStages::Fragment);

    vertexShader->append("\n#define VERTEX_SHADER\n#include \"effect.glsllib\"\n");
    vertexShader->append(vertexShaderSrc.toUtf8().constData());

    fragmentShader->append("\n#define FRAGMENT_SHADER\n#include \"effect.glsllib\"\n");
    fragmentShader->append(fragmentShaderSrc.toUtf8().constData());

    Qt3DRender::QShaderProgram *prog = m_shaderProgramGenerator->compileGeneratedShader(name, Q3DSShaderFeatureSet());
    prog->setParent(parent);
    return prog;
}

Q3DSShaderManager::Q3DSShaderManager()
    : m_materialShaderGenerator(&Q3DSDefaultMaterialShaderGenerator::createDefaultMaterialShaderGenerator())
    , m_customMaterialShaderGenerator(&Q3DSCustomMaterialShaderGenerator::createCustomMaterialShaderGenerator())
    , m_shaderProgramGenerator(Q3DSAbstractShaderProgramGenerator::createProgramGenerator())
{
}

void Q3DSShaderManager::invalidate()
{
    m_depthPrePassShader = nullptr;
    m_orthoDepthShader = nullptr;
    m_cubeDepthShader = nullptr;
    m_orthoShadowBlurXShader = nullptr;
    m_orthoShadowBlurYShader = nullptr;
    m_cubeShadowBlurXShader = nullptr;
    m_cubeShadowBlurYShader = nullptr;
    m_ssaoTextureShader = nullptr;
    m_bsdfMipPreFilterShader = nullptr;
    m_progAABlendShader = nullptr;
    m_blendOverlayShader.clear();
    m_blendColorBurnShader.clear();
    m_blendColorDodgeShader.clear();

    m_shaderProgramGenerator->invalidate();
}

void Q3DSShaderManager::setProfiler(Q3DSProfiler *profiler)
{
    m_shaderProgramGenerator->setProfiler(profiler);
}

QT_END_NAMESPACE
