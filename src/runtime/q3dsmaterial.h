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

#ifndef Q3DSMATERIAL_H
#define Q3DSMATERIAL_H

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include <Qt3DStudioRuntime2/q3dsruntimeglobal.h>
#include <Qt3DStudioRuntime2/q3dspresentationcommon.h> // not q3dspresentation.h, that would be circular ref

QT_BEGIN_NAMESPACE

class QXmlStreamReader;

// The data structures here are used by both custom materials and effects.
// Some are only applicable to effects however since those have a wider set of features.
namespace Q3DSMaterial {

enum UsageType {
    Diffuse,
    Specular,
    Roughness,
    Bump,
    Environment,
    Shadow,
    Displacement,
    Emissive,
    EmissiveMask,
    Anisotropy,
    Gradient,
    Storage,
    Brush,
    Cutout,
    Transmission
};

enum FilterType {
    Nearest,                // MagOp || MinOp
    Linear,                 // MagOp || MinOp
    LinearMipmapLinear,     // MinOp
    NearestMipmapNearest,   // MinOp
    NearestMipmapLinear,    // MinOp
    LinearMipmapNearest     // MinOp
};

QString filterTypeToString(FilterType type);

enum ClampType { // TextureCoordOp
    Clamp,
    Repeat
};

enum TextureFormat {
    UnknownTextureFormat,
    R8,
    R16,
    R16F,
    R32I,
    R32UI,
    R32F,
    RG8,
    RGBA8,
    RGB8,
    SRGB8,
    SRGB8A8,
    RGB565,
    RGBA5551,
    Alpha8,
    Luminance8,
    Luminance16,
    LuminanceAlpha8,
    RGBA16F,
    RG16F,
    RG32F,
    RGB32F,
    RGBA32F,
    R11G11B10,
    RGB9E5,
    RGBA_DXT1,
    RGB_DXT1,
    RGBA_DXT3,
    RGBA_DXT5,
    Depth16,
    Depth24,
    Depth32,
    Depth24Stencil8
};

enum BlendFunc { // both Source and Dest
    SrcAlpha,
    OneMinusSrcAlpha,
    One
};

enum ImageAccess {
    Read,
    Write,
    ReadWrite
};

QString clampTypeToString(ClampType type);

struct Q3DSV_EXPORT PropertyElement
{
    QString name;
    QString description;
    QString formalName;
    Q3DS::PropertyType type;
    int componentCount;
    float min;
    float max;
    QString defaultValue;
    UsageType usageType;
    FilterType magFilterType;
    FilterType minFilterType;
    ClampType clampType;

    // Buffers
    QString format;
    QString binding;
    QString align;
    QString access;

    PropertyElement()
        : type(Q3DS::Float)
        , componentCount(1)
        , min(0.0f)
        , max(0.0f)
        , usageType(Diffuse)
        , magFilterType(Linear)
        , clampType(Clamp)
    {}
};

struct Q3DSV_EXPORT Shader // or rather, program
{
    QString name;
    QString shared;
    // the vertex/fragment shaders already include both 'shared' and the global preambles
    QString vertexShader;
    QString fragmentShader;
};

enum PassBufferType {
    UnknownBufferType,
    BufferType,
    ImageType,
    DataBufferType
};

class Q3DSV_EXPORT PassBuffer
{
public:
    PassBuffer(PassBufferType type_ = UnknownBufferType)
        : m_passBufferType(type_)
    {}

    QString name() const;
    void setName(const QString &name);

    PassBufferType passBufferType() const;

    QString type() const;
    void setType(const QString &type);

    QString format() const; // "source" or a texture format
    void setFormat(const QString &format);

    float size() const;
    void setSize(float size);

    bool hasSceneLifetime() const;
    void setHasSceneLifetime(bool hasSceneLifetime);

    // Buffer + Image

    FilterType filter() const;
    void setFilter(const FilterType &filter);

    ClampType wrap() const;
    void setWrap(const ClampType &wrap);

    TextureFormat textureFormat() const; // unknown when format() == "source", a valid format otherwise
    void setTextureFormat(const TextureFormat &textureFormat);

    // Image

    ImageAccess access() const;
    void setAccess(ImageAccess access);

    // DataBuffer

    QString wrapName() const;
    void setWrapName(const QString &wrapName);

    QString wrapType() const;
    void setWrapType(const QString &wrapType);

private:
    QString m_name;
    PassBufferType m_passBufferType = UnknownBufferType;
    QString m_type;
    QString m_format;
    float m_size = 1.0f;
    bool m_hasSceneLifetime = false;

    FilterType m_filter = Nearest;
    ClampType m_wrap = Clamp;
    TextureFormat m_textureFormat = UnknownTextureFormat;

    ImageAccess m_access = Read;

    QString m_wrapName;
    QString m_wrapType;
};

// no data allowed in the PassBuffer subclasses in order to avoid object slicing

class Q3DSV_EXPORT Buffer : public PassBuffer
{
public:
    Buffer() : PassBuffer(PassBufferType::BufferType) {}
};

class Q3DSV_EXPORT ImageBuffer : public PassBuffer
{
public:
    ImageBuffer() : PassBuffer(PassBufferType::ImageType) {}
};

class Q3DSV_EXPORT DataBuffer : public PassBuffer
{
public:
    DataBuffer() : PassBuffer(PassBufferType::DataBufferType) {}
};

enum BoolOp {
    Never,
    Less,
    LessThanOrEqual,
    Equal,
    NotEqual,
    Greater,
    GreaterThanOrEqual,
    AlwaysTrue
};

enum StencilOp {
    Keep,
    Zero,
    Replace,
    Increment,
    IncrementWrap,
    Decrement,
    DecrementWrap,
    Invert
};

class Q3DSV_EXPORT PassCommand
{
public:
    enum Type {
        UnknownType,
        BufferBlitType,      // custom material only
        BufferInputType,     // both
        DataBufferInputType, // effect only
        ImageInputType,      // effect only
        DepthInputType,      // effect only
        BlendingType,        // both
        SetParamType,        // effect only
        RenderStateType,     // both
        DepthStencilType     // effect only
    };

    PassCommand(Type type = UnknownType);
    Type type() const;

    struct Data {
        QString value;
        QString param;
        QString usage;
        bool sync = false;
        QString source;
        QString destination;
        BlendFunc blendSource = One;
        BlendFunc blendDestination = One;
        QString name;
        QString bufferName;
        quint32 stencilValue = 0;
        QString flags;
        quint32 mask = 4294967295U; // 0xffffffff
        BoolOp stencilFunction = Equal;
        StencilOp stencilFail = Keep;
        StencilOp depthPass = Keep;
        StencilOp depthFail = Keep;
    };

    const Data *data() const;
    Data *data();

private:
    Type m_type;
    Data m_data;
};

struct Q3DSV_EXPORT Pass
{
    QString shaderName;
    QString input;
    QString output;
    Q3DSMaterial::TextureFormat outputFormat;
    QVector<PassCommand> commands;
    bool needsClear;

    Pass()
        : input(QLatin1String("[source]"))
        , output(QLatin1String("[dest]"))
        , outputFormat(Q3DSMaterial::RGBA8)
        , needsClear(false)
    {}
};

// parsing code common between custom materials and effects
PropertyElement parsePropertyElement(QXmlStreamReader *r);
Shader parserShaderElement(QXmlStreamReader *r);
void combineShaderCode(Shader *shader,
                       const QString &sharedShaderCode,
                       const QString &sharedVertexShaderCode,
                       const QString &sharedFragmentShaderCode);
Buffer parseBuffer(QXmlStreamReader *r);

bool convertToUsageType(const QStringRef &value, Q3DSMaterial::UsageType *type, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToFilterType(const QStringRef &value, Q3DSMaterial::FilterType *type, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToClampType(const QStringRef &value, Q3DSMaterial::ClampType *type, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToBoolOp(const QStringRef &value, Q3DSMaterial::BoolOp *type, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToStencilOp(const QStringRef &value, Q3DSMaterial::StencilOp *type, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToTextureFormat(const QStringRef &value, const QString &typeValue, Q3DSMaterial::TextureFormat *type, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToBlendFunc(const QStringRef &value, Q3DSMaterial::BlendFunc *type, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
bool convertToImageAccess(const QStringRef &value, Q3DSMaterial::ImageAccess *type, const char *desc = nullptr, QXmlStreamReader *reader = nullptr);
}

Q_DECLARE_TYPEINFO(Q3DSMaterial::PropertyElement, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSMaterial::Shader, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSMaterial::PassBuffer, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSMaterial::Buffer, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSMaterial::ImageBuffer, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSMaterial::DataBuffer, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSMaterial::PassCommand::Data, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSMaterial::PassCommand, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSMaterial::Pass, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif // Q3DSMATERIAL_H
