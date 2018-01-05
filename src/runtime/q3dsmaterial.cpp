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

#include "q3dsmaterial.h"
#include <QtCore/QXmlStreamReader>

QT_BEGIN_NAMESPACE

namespace Q3DSMaterial {

bool convertToUsageType(const QStringRef &value, UsageType *type, const char *desc, QXmlStreamReader *reader)
{
    bool ok = false;
    if (value == QStringLiteral("diffuse")) {
        ok = true;
        *type = Q3DSMaterial::Diffuse;
    } else if (value == QStringLiteral("specular")) {
        ok = true;
        *type = Q3DSMaterial::Specular;
    } else if (value == QStringLiteral("bump")) {
        ok = true;
        *type = Q3DSMaterial::Bump;
    } else if (value == QStringLiteral("roughness")) {
        ok = true;
        *type = Q3DSMaterial::Roughness;
    } else if (value == QStringLiteral("environment")) {
        ok = true;
        *type = Q3DSMaterial::Environment;
    } else if (value == QStringLiteral("shadow")) {
        ok = true;
        *type = Q3DSMaterial::Shadow;
    } else if (value == QStringLiteral("displacement")) {
        ok = true;
        *type = Q3DSMaterial::Displacement;
    } else if (value == QStringLiteral("emissive")) {
        ok = true;
        *type = Q3DSMaterial::Emissive;
    } else if (value == QStringLiteral("emissive_mask")) {
        ok = true;
        *type = Q3DSMaterial::EmissiveMask;
    } else if (value == QStringLiteral("anisotropy")) {
        ok = true;
        *type = Q3DSMaterial::Anisotropy;
    } else if (value == QStringLiteral("gradient")) {
        ok = true;
        *type = Q3DSMaterial::Gradient;
    } else if (value == QStringLiteral("storage")) {
        ok = true;
        *type = Q3DSMaterial::Storage;
    } else {
        if (reader)
            reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
    }
    return ok;
}

bool convertToFilterType(const QStringRef &value, FilterType *type, const char *desc, QXmlStreamReader *reader)
{
    bool ok = false;
    if (value == QStringLiteral("nearest")) {
        ok = true;
        *type = Q3DSMaterial::Nearest;
    } else if (value == QStringLiteral("linear")) {
        ok = true;
        *type = Q3DSMaterial::Linear;
    } else if (value == QStringLiteral("linearMipmapLinear")) {
        ok = true;
        *type = Q3DSMaterial::LinearMipmapLinear;
    } else if (value == QStringLiteral("nearestMipmapNearest")) {
        ok = true;
        *type = Q3DSMaterial::NearestMipmapNearest;
    } else if (value == QStringLiteral("nearestMipmapLinear")) {
        ok = true;
        *type = Q3DSMaterial::NearestMipmapLinear;
    } else if (value == QStringLiteral("linearMipmapNearest")) {
        ok = true;
        *type = Q3DSMaterial::LinearMipmapNearest;
    } else {
        if (reader)
            reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
    }
    return ok;
}

bool convertToClampType(const QStringRef &value, ClampType *type, const char *desc, QXmlStreamReader *reader)
{
    bool ok = false;
    if (value == QStringLiteral("clamp")) {
        ok = true;
        *type = Q3DSMaterial::Clamp;
    } else if (value == QStringLiteral("repeat")) {
        ok = true;
        *type = Q3DSMaterial::Repeat;
    } else {
        if (reader)
            reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
    }
    return ok;
}

QString PassBuffer::name() const
{
    return m_name;
}

void PassBuffer::setName(const QString &name)
{
    m_name = name;
}

QString PassBuffer::type() const
{
    return m_type;
}

void PassBuffer::setType(const QString &type)
{
    m_type = type;
}

QString PassBuffer::format() const
{
    return m_format;
}

void PassBuffer::setFormat(const QString &format)
{
    m_format = format;
}

float PassBuffer::size() const
{
    return m_size;
}

void PassBuffer::setSize(float size)
{
    m_size = size;
}

bool PassBuffer::hasSceneLifetime() const
{
    return m_hasSceneLifetime;
}

void PassBuffer::setHasSceneLifetime(bool hasSceneLifetime)
{
    m_hasSceneLifetime = hasSceneLifetime;
}

PassBufferType PassBuffer::passBufferType() const
{
    return m_passBufferType;
}

FilterType PassBuffer::filter() const
{
    return m_filter;
}

void PassBuffer::setFilter(const FilterType &filter)
{
    m_filter = filter;
}

ClampType PassBuffer::wrap() const
{
    return m_wrap;
}

void PassBuffer::setWrap(const ClampType &wrap)
{
    m_wrap = wrap;
}

TextureFormat PassBuffer::textureFormat() const
{
    return m_textureFormat;
}

void PassBuffer::setTextureFormat(const TextureFormat &textureFormat)
{
    m_textureFormat = textureFormat;
}

ImageAccess PassBuffer::access() const
{
    return m_access;
}

void PassBuffer::setAccess(ImageAccess access)
{
    m_access = access;
}

QString PassBuffer::wrapName() const
{
    return m_wrapName;
}

void PassBuffer::setWrapName(const QString &wrapName)
{
    m_wrapName = wrapName;
}

QString PassBuffer::wrapType() const
{
    return m_wrapType;
}

void PassBuffer::setWrapType(const QString &wrapType)
{
    m_wrapType = wrapType;
}

PassCommand::PassCommand(Type type)
    : m_type(type)
{
}

PassCommand::Type PassCommand::type() const
{
    return m_type;
}

const PassCommand::Data *PassCommand::data() const
{
    return &m_data;
}

PassCommand::Data *PassCommand::data()
{
    return &m_data;
}

PropertyElement parsePropertyElement(QXmlStreamReader *r)
{
    Q3DSMaterial::PropertyElement property;
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("name")) {
            property.name = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("description")) {
            property.description = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("formalName")) {
            property.formalName = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("type")) {
            Q3DS::PropertyType type;
            int componentCount;
            if (Q3DS::convertToPropertyType(attribute.value(), &type, &componentCount, "property type", r)) {
                property.type = type;
                property.componentCount = componentCount;
            }
        } else if (attribute.name() == QStringLiteral("min")) {
            float min;
            if (Q3DS::convertToFloat(attribute.value(), &min, "min value", r))
                property.min = min;
        } else if (attribute.name() == QStringLiteral("max")) {
            float max;
            if (Q3DS::convertToFloat(attribute.value(), &max, "min value", r))
                property.max = max;
        } else if (attribute.name() == QStringLiteral("default")) {
            property.defaultValue = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("usage")) {
            Q3DSMaterial::UsageType type;
            if (Q3DSMaterial::convertToUsageType(attribute.value(), &type, "usage type", r))
                property.usageType = type;
        } else if (attribute.name() == QStringLiteral("filter")) {
            Q3DSMaterial::FilterType type;
            if (Q3DSMaterial::convertToFilterType(attribute.value(), &type, "magfilter type", r))
                property.magFilterType = type;
        } else if (attribute.name() == QStringLiteral("minfilter")) {
            Q3DSMaterial::FilterType type;
            if (Q3DSMaterial::convertToFilterType(attribute.value(), &type, "minfilter type", r))
                property.minFilterType = type;
        } else if (attribute.name() == QStringLiteral("clamp")) {
            Q3DSMaterial::ClampType type;
            if (Q3DSMaterial::convertToClampType(attribute.value(), &type, "clamp type", r))
                property.clampType = type;
        } else if (attribute.name() == QStringLiteral("format")) {
            property.format = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("binding")) {
            property.binding = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("align")) {
            property.align = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("access")) {
            property.access = attribute.value().toString();
        }
    }
    return property;
}

Shader parserShaderElement(QXmlStreamReader *r)
{
    Q3DSMaterial::Shader shader;
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("name"))
            shader.name = attribute.value().toString();
    }
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Shared")) {
            if ( r->readNext() == QXmlStreamReader::Characters) {
                shader.shared = r->text().toString().trimmed();
                r->skipCurrentElement();
            }
        } else if (r->name() == QStringLiteral("VertexShader")) {
            if ( r->readNext() == QXmlStreamReader::Characters) {
                shader.vertexShader = r->text().toString().trimmed();
                if (shader.vertexShader.isEmpty())
                    shader.vertexShader = QLatin1String("void vert(){}\n");
                r->skipCurrentElement();
            }
        } else if (r->name() == QStringLiteral("FragmentShader")) {
            if ( r->readNext() == QXmlStreamReader::Characters) {
                shader.fragmentShader = r->text().toString().trimmed();
                if (shader.fragmentShader.isEmpty())
                    shader.fragmentShader = QLatin1String("void frag(){}\n");
                r->skipCurrentElement();
            }
        } else if (r->name() == QStringLiteral("GeometryShader")) {
            if (r->readNext() == QXmlStreamReader::Characters) {
                qWarning("Custom material/effect: Geometry shaders are not supported; ignored");
                r->skipCurrentElement();
            }
        } else {
            r->skipCurrentElement();
        }
    }
    return shader;
}

void combineShaderCode(Shader *shader,
                       const QString &sharedShaderCode,
                       const QString &sharedVertexShaderCode,
                       const QString &sharedFragmentShaderCode)
{
    // Prepare the final vertex and fragment shader strings
    // The order is: global shared, local shared, ifdef, global type-specific shared, local code, endif
    // The "shaderPrefix" of 3DS1 is not handled in the parser, it is added later on.

    if (!shader->vertexShader.isEmpty()) {
        shader->vertexShader.append(QLatin1String("\n#endif\n"));
        shader->vertexShader.prepend(sharedVertexShaderCode);
        shader->vertexShader.prepend(QLatin1String("\n#ifdef VERTEX_SHADER\n"));
        shader->vertexShader.prepend(shader->shared);
        shader->vertexShader.prepend(sharedShaderCode);
    }

    if (!shader->fragmentShader.isEmpty()) {
        shader->fragmentShader.append(QLatin1String("\n#endif\n"));
        shader->fragmentShader.prepend(sharedFragmentShaderCode);
        shader->fragmentShader.prepend(QLatin1String("\n#ifdef FRAGMENT_SHADER\n"));
        shader->fragmentShader.prepend(shader->shared);
        shader->fragmentShader.prepend(sharedShaderCode);
    }
}

Buffer parseBuffer(QXmlStreamReader *r)
{
    Q3DSMaterial::Buffer buffer;

    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("name")) {
            buffer.setName(attribute.value().toString());
        } else if (attribute.name() == QStringLiteral("lifetime")) {
            buffer.setHasSceneLifetime(attribute.value().toString() == QStringLiteral("scene"));
        } else if (attribute.name() == QStringLiteral("type")) {
            buffer.setType(attribute.value().toString());
        } else if (attribute.name() == QStringLiteral("format")) {
            buffer.setFormat(attribute.value().toString());
        } else if (attribute.name() == QStringLiteral("filter")) {
            Q3DSMaterial::FilterType filter;
            if (Q3DSMaterial::convertToFilterType(attribute.value(), &filter, "filter", r))
                buffer.setFilter(filter);
        } else if (attribute.name() == QStringLiteral("wrap")) {
            Q3DSMaterial::ClampType wrap;
            if (Q3DSMaterial::convertToClampType(attribute.value(), &wrap, "wrap", r))
                buffer.setWrap(wrap);
        } else if (attribute.name() == QStringLiteral("size")) {
            float size;
            if (Q3DS::convertToFloat(attribute.value(), &size, "size", r))
                buffer.setSize(size);
        }
    }

    // Try to resolve the texture format. "source" is handled specially and resolves to Unknown.
    Q3DSMaterial::TextureFormat format;
    const QString formatStr = buffer.format();
    if (Q3DSMaterial::convertToTextureFormat(&formatStr, buffer.type(), &format, "texture format", r))
        buffer.setTextureFormat(format);

    while (r->readNextStartElement())
        r->skipCurrentElement();

    return buffer;
}

QString filterTypeToString(FilterType type)
{
    QString filterString;
    switch (type) {
    case Nearest:
        filterString = QStringLiteral("nearest");
        break;
    case Linear:
        filterString = QStringLiteral("linear");
        break;
    case LinearMipmapLinear:
        filterString = QStringLiteral("linearMipmapLinear");
        break;
    case NearestMipmapNearest:
        filterString = QStringLiteral("nearestMipmapNearest");
        break;
    case NearestMipmapLinear:
        filterString = QStringLiteral("nearestMipmapLinear");
        break;
    case LinearMipmapNearest:
        filterString = QStringLiteral("linearMipmapNearest");
        break;
    }
    return filterString;
}

QString clampTypeToString(ClampType type)
{
    QString clampType;
    switch (type) {
    case Clamp:
        clampType = QStringLiteral("clamp");
        break;
    case Repeat:
        clampType = QStringLiteral("repeat");
        break;
    }
    return clampType;
}

bool convertToBoolOp(const QStringRef &value, BoolOp *type, const char *desc, QXmlStreamReader *reader)
{
    bool ok = false;
    if (value == QStringLiteral("never")) {
        ok = true;
        *type = Q3DSMaterial::Never;
    } else if (value == QStringLiteral("less")) {
        ok = true;
        *type = Q3DSMaterial::Less;
    } else if (value == QStringLiteral("less-than-or-equal")) {
        ok = true;
        *type = Q3DSMaterial::LessThanOrEqual;
    } else if (value == QStringLiteral("equal")) {
        ok = true;
        *type = Q3DSMaterial::Equal;
    } else if (value == QStringLiteral("not-equal")) {
        ok = true;
        *type = Q3DSMaterial::NotEqual;
    } else if (value == QStringLiteral("greater")) {
        ok = true;
        *type = Q3DSMaterial::Greater;
    } else if (value == QStringLiteral("greater-than-or-equal")) {
        ok = true;
        *type = Q3DSMaterial::GreaterThanOrEqual;
    } else if (value == QStringLiteral("always")) {
        ok = true;
        *type = Q3DSMaterial::AlwaysTrue;
    } else {
        if (reader)
            reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
    }
    return ok;
}

bool convertToStencilOp(const QStringRef &value, StencilOp *type, const char *desc, QXmlStreamReader *reader)
{
    bool ok = false;
    if (value == QStringLiteral("keep")) {
        ok = true;
        *type = Q3DSMaterial::Keep;
    } else if (value == QStringLiteral("zero")) {
        ok = true;
        *type = Q3DSMaterial::Zero;
    } else if (value == QStringLiteral("replace")) {
        ok = true;
        *type = Q3DSMaterial::Replace;
    } else if (value == QStringLiteral("increment")) {
        ok = true;
        *type = Q3DSMaterial::Increment;
    } else if (value == QStringLiteral("increment-wrap")) {
        ok = true;
        *type = Q3DSMaterial::IncrementWrap;
    } else if (value == QStringLiteral("decrement")) {
        ok = true;
        *type = Q3DSMaterial::Decrement;
    } else if (value == QStringLiteral("decrement-wrap")) {
        ok = true;
        *type = Q3DSMaterial::DecrementWrap;
    } else if (value == QStringLiteral("invert")) {
        ok = true;
        *type = Q3DSMaterial::Invert;
    } else {
        if (reader)
            reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
    }
    return ok;
}

bool convertToTextureFormat(const QStringRef &value, const QString &typeValue, TextureFormat *type, const char *desc, QXmlStreamReader *reader)
{
    bool ok = false;
    if (value == QStringLiteral("source")) {
        ok = true;
        *type = UnknownTextureFormat;
    } else if (value == QStringLiteral("depth24stencil8")) {
        ok = true;
        *type = Depth24Stencil8;
    } else {
        // These depend on type
        if (typeValue == QStringLiteral("ubyte")) {
            if (value == QStringLiteral("rgb")) {
                ok = true;
                *type = RGB8;
            } else if (value == QStringLiteral("rgba")) {
                ok = true;
                *type = RGBA8;
            } else if (value == QStringLiteral("alpha")) {
                ok = true;
                *type = Alpha8;
            } else if (value == QStringLiteral("lum")) {
                ok = true;
                *type = Luminance8;
            } else if (value == QStringLiteral("lum_alpha")) {
                ok = true;
                *type = LuminanceAlpha8;
            } else if (value == QStringLiteral("rg")) {
                ok = true;
                *type = RG8;
            }
        } else if (typeValue == QStringLiteral("ushort")) {
            if (value == QStringLiteral("rgb")) {
                ok = true;
                *type = RGB565;
            } else if (value == QStringLiteral("rgba")) {
                ok = true;
                *type = RGBA5551;
            }
        } else if (typeValue == QStringLiteral("fp16")) {
            if (value == QStringLiteral("rgba")) {
                ok = true;
                *type = RGBA16F;
            } else if (value == QStringLiteral("rg")) {
                ok = true;
                *type = RG16F;
            }
        } else if (typeValue == QStringLiteral("fp32")) {
            if (value == QStringLiteral("rgba")) {
                ok = true;
                *type = RGBA32F;
            } else if (value == QStringLiteral("rg")) {
                ok = true;
                *type = RG32F;
            }
        } else {
            if (reader)
                reader->raiseError(QObject::tr("Invalid %1 \"%2\" of type \"%3\"").arg(QString::fromUtf8(desc)).arg(value.toString()).arg(typeValue));
        }
    }
    return ok;
}

bool convertToBlendFunc(const QStringRef &value, BlendFunc *type, const char *desc, QXmlStreamReader *reader)
{
    bool ok = false;
    if (value == QStringLiteral("SrcAlpha")) {
        ok = true;
        *type = Q3DSMaterial::SrcAlpha;
    } else if (value == QStringLiteral("OneMinusSrcAlpha")) {
        ok = true;
        *type = Q3DSMaterial::OneMinusSrcAlpha;
    } else if (value == QStringLiteral("One")) {
        ok = true;
        *type = Q3DSMaterial::One;
    } else {
        if (reader)
            reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
        *type = Q3DSMaterial::One;
    }
    return ok;
}

bool convertToImageAccess(const QStringRef &value, ImageAccess *type, const char *desc, QXmlStreamReader *reader)
{
    bool ok = false;
    if (value == QStringLiteral("read")) {
        ok = true;
        *type = Q3DSMaterial::Read;
    } else if (value == QStringLiteral("write")) {
        ok = true;
        *type = Q3DSMaterial::Write;
    } else if (value == QStringLiteral("readwrite")) {
        ok = true;
        *type = Q3DSMaterial::ReadWrite;
    } else {
        if (reader)
            reader->raiseError(QObject::tr("Invalid %1 \"%2\"").arg(QString::fromUtf8(desc)).arg(value.toString()));
        *type = Q3DSMaterial::ReadWrite;
    }
    return ok;
}

}

QT_END_NAMESPACE
