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

#include "q3dsstudio3dnode_p.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>

QT_BEGIN_NAMESPACE

static QSGMaterialType materialType;

const char * const *Q3DSStudio3DMaterialShader::attributeNames() const
{
    static char const *const attr[] = { "qt_VertexPosition", "qt_VertexTexCoord", 0 };
    return attr;
}

const char *Q3DSStudio3DMaterialShader::vertexShader() const
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx->format().version() >= qMakePair(3, 2) && ctx->format().profile() == QSurfaceFormat::CoreProfile) {
        return ""
               "#version 150 core                                   \n"
               "uniform mat4 qt_Matrix;                             \n"
               "in vec4 qt_VertexPosition;                          \n"
               "in vec2 qt_VertexTexCoord;                          \n"
               "out vec2 qt_TexCoord;                               \n"
               "void main() {                                       \n"
               "   qt_TexCoord = qt_VertexTexCoord;                 \n"
               "   gl_Position = qt_Matrix * qt_VertexPosition;     \n"
               "}";
    } else {
        return ""
               "uniform highp mat4 qt_Matrix;                       \n"
               "attribute highp vec4 qt_VertexPosition;             \n"
               "attribute highp vec2 qt_VertexTexCoord;             \n"
               "varying highp vec2 qt_TexCoord;                     \n"
               "void main() {                                       \n"
               "   qt_TexCoord = qt_VertexTexCoord;                 \n"
               "   gl_Position = qt_Matrix * qt_VertexPosition;     \n"
               "}";
    }
}

const char *Q3DSStudio3DMaterialShader::fragmentShader() const
{
    // Make the result have pre-multiplied alpha.
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx->format().version() >= qMakePair(3, 2) && ctx->format().profile() == QSurfaceFormat::CoreProfile) {
        return ""
               "#version 150 core                                   \n"
               "uniform sampler2D source;                           \n"
               "uniform float qt_Opacity;                           \n"
               "in vec2 qt_TexCoord;                                \n"
               "out vec4 fragColor;                                 \n"
               "void main() {                                       \n"
               "   vec4 p = texture(source, qt_TexCoord);           \n"
               "   float a = qt_Opacity * p.a;                      \n"
               "   fragColor = vec4(p.rgb * a, a);                  \n"
               "}";
    } else {
        return ""
               "uniform highp sampler2D source;                         \n"
               "uniform highp float qt_Opacity;                         \n"
               "varying highp vec2 qt_TexCoord;                         \n"
               "void main() {                                           \n"
               "   highp vec4 p = texture2D(source, qt_TexCoord);       \n"
               "   highp float a = qt_Opacity * p.a;                    \n"
               "   gl_FragColor = vec4(p.rgb * a, a);                   \n"
               "}";
    }
}

void Q3DSStudio3DMaterialShader::initialize()
{
    m_matrixId = program()->uniformLocation("qt_Matrix");
    m_opacityId = program()->uniformLocation("qt_Opacity");
}

static inline bool isPowerOfTwo(int x)
{
    // Assumption: x >= 1
    return x == (x & -x);
}

void Q3DSStudio3DMaterialShader::updateState(const RenderState &state, QSGMaterial *newEffect, QSGMaterial *oldEffect)
{
    Q_ASSERT(!oldEffect || newEffect->type() == oldEffect->type());
    Q3DSStudio3DMaterial *tx = static_cast<Q3DSStudio3DMaterial *>(newEffect);
    Q3DSStudio3DMaterial *oldTx = static_cast<Q3DSStudio3DMaterial *>(oldEffect);

    QSGTexture *t = tx->texture();
    if (t) {
        QOpenGLContext *ctx = const_cast<QOpenGLContext *>(state.context());
        if (!ctx->functions()->hasOpenGLFeature(QOpenGLFunctions::NPOTTextureRepeat)) {
            const QSize size = t->textureSize();
            const bool isNpot = !isPowerOfTwo(size.width()) || !isPowerOfTwo(size.height());
            if (isNpot) {
                t->setHorizontalWrapMode(QSGTexture::ClampToEdge);
                t->setVerticalWrapMode(QSGTexture::ClampToEdge);
            }
        }
        if (!oldTx || oldTx->texture()->textureId() != t->textureId())
            t->bind();
        else
            t->updateBindOptions();
    }

    if (state.isMatrixDirty())
        program()->setUniformValue(m_matrixId, state.combinedMatrix());

    if (state.isOpacityDirty())
        program()->setUniformValue(m_opacityId, state.opacity());
}

void Q3DSStudio3DMaterial::setTexture(QSGTexture *texture)
{
    m_texture = texture;
    setFlag(Blending, m_texture ? m_texture->hasAlphaChannel() : false);
}

QSGTexture *Q3DSStudio3DMaterial::texture() const
{
    return m_texture;
}

QSGMaterialType *Q3DSStudio3DMaterial::type() const
{
    return &materialType;
}

QSGMaterialShader *Q3DSStudio3DMaterial::createShader() const
{
    return new Q3DSStudio3DMaterialShader;
}

Q3DSStudio3DNode::Q3DSStudio3DNode()
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    setMaterial(&m_material);
    setGeometry(&m_geometry);
}

void Q3DSStudio3DNode::setTexture(QSGTexture *texture)
{
    m_material.setTexture(texture);
    markDirty(DirtyMaterial);
}

QSGTexture *Q3DSStudio3DNode::texture() const
{
    return m_material.texture();
}

void Q3DSStudio3DNode::setRect(const QRectF &rect)
{
    if (rect == m_rect)
        return;

    m_rect = rect;

    // Map the item's bounding rect to normalized texture coordinates
    const QRectF sourceRect(0.0f, 1.0f, 1.0f, -1.0f);
    QSGGeometry::updateTexturedRectGeometry(&m_geometry, m_rect, sourceRect);

    markDirty(DirtyGeometry);
}

QT_END_NAMESPACE
