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

#include "q3dseffect.h"
#include "q3dsutils.h"

QT_BEGIN_NAMESPACE

Q3DSEffect::Q3DSEffect()
{
}

bool Q3DSEffect::isNull()
{
    // Effects have to have at least 1 shader
    return m_shaders.isEmpty();
}

const QMap<QString, Q3DSMaterial::PropertyElement> &Q3DSEffect::properties() const
{
    return m_properties;
}

const QMap<QString, Q3DSMaterial::Shader> &Q3DSEffect::shaders() const
{
    return m_shaders;
}

const QMap<QString, Q3DSMaterial::PassBuffer> &Q3DSEffect::buffers() const
{
    return m_buffers;
}

const QVector<Q3DSMaterial::Pass> &Q3DSEffect::passes() const
{
    return m_passes;
}

QString Q3DSEffect::addPropertyUniforms(const QString &shaderSrc) const
{
    QString s;

    auto addUniform = [&s](const char *type, const QString &name) {
        s += QLatin1String("uniform ") + QString::fromUtf8(type) + QLatin1Char(' ') + name + QLatin1String(";\n");
    };

    for (const Q3DSMaterial::PropertyElement &prop : m_properties) {
        switch (prop.type) {
        case Q3DS::Float:
            addUniform("float", prop.name);
            break;
        case Q3DS::Boolean:
            addUniform("bool", prop.name);
            break;
        case Q3DS::Long:
            addUniform("int", prop.name);
            break;
        case Q3DS::Float2:
            addUniform("vec2", prop.name);
            break;
        case Q3DS::Vector:
        case Q3DS::Scale:
        case Q3DS::Rotation:
        case Q3DS::Color:
            addUniform("vec3", prop.name);
            break;
        case Q3DS::Texture:
            addUniform("sampler2D", prop.name);
            // In addition we must add:
            //   - uniform vec4 <name>Info that contains (width, height, isPremultiplied, 0)
            //   - a texture2D_<name> function
            // The flag uniform from 3DS1 is dropped since its purpose was unknown.
            // The SNAPPER macros are not used, we insert the uniform/function code directly here.
            addUniform("vec4", prop.name + QLatin1String("Info"));
            s += QString(QLatin1String("vec4 texture2D_%1(vec2 uv) { return GetTextureValue(%1, uv, %1Info.z); }\n")).arg(prop.name);
            break;

        // ### there could be more that needs handling here (Buffer?)

        default:
            qWarning("Effect property %s has unsupported type; ignored", qPrintable(prop.name));
            break;
        }
    }

    s += shaderSrc;
    return s;
}

Q3DSEffect Q3DSEffectParser::parse(const QString &filename, bool *ok)
{
    if (!setSource(filename)) {
        if (ok != nullptr)
            *ok = false;
        return Q3DSEffect();
    }

    Q3DSEffect effect;

    QXmlStreamReader *r = reader();
    if (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Effect")) {
            while (r->readNextStartElement()) {
                if (r->name() == QStringLiteral("MetaData"))
                    parseProperties(effect);
                else if (r->name() == QStringLiteral("Shaders"))
                    parseShaders(effect);
                else if (r->name() == QStringLiteral("Passes"))
                    parsePasses(effect);
                else
                    r->skipCurrentElement();
            }
        } else {
            r->raiseError(QObject::tr("Not a Effect file"));
        }
    }

    if (r->hasError()) {
        Q3DSUtils::showMessage(readerErrorString());
        if (ok != nullptr)
            *ok = false;
        return Q3DSEffect();
    }

    if (ok != nullptr)
        *ok = true;

    return effect;
}

void Q3DSEffectParser::parseProperties(Q3DSEffect &effect)
{
    QXmlStreamReader *r = reader();
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Property")) {
            Q3DSMaterial::PropertyElement property = Q3DSMaterial::parsePropertyElement(r);

            if (property.name.isEmpty() || !isPropertyNameUnique(property.name, effect)) {
                // name can not be empty
                r->raiseError(QObject::tr("Property elements must have a unique name"));
            }

            if (property.formalName.isEmpty())
                property.formalName = property.name;

            effect.m_properties.insert(property.name, property);

            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else {
            r->skipCurrentElement();
        }
    }
}

void Q3DSEffectParser::parseShaders(Q3DSEffect &effect)
{
    QString sharedShaderCode;
    QString sharedVertexShaderCode;
    QString sharedFragmentShaderCode;

    int shaderCount = 0;
    QXmlStreamReader *r = reader();
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Shared")) {
            if (r->readNext() == QXmlStreamReader::Characters) {
                sharedShaderCode = r->text().toString().trimmed();
                if (!sharedShaderCode.isEmpty())
                    sharedShaderCode.append(QLatin1Char('\n'));
                r->skipCurrentElement();
            }
        } else if (r->name() == QStringLiteral("VertexShaderShared")) {
            if (r->readNext() == QXmlStreamReader::Characters) {
                sharedVertexShaderCode = r->text().toString().trimmed();
                if (!sharedVertexShaderCode.isEmpty())
                    sharedVertexShaderCode.append(QLatin1Char('\n'));
                r->skipCurrentElement();
            }
        } else if (r->name() == QStringLiteral("FragmentShaderShared")) {
            if (r->readNext() == QXmlStreamReader::Characters) {
                sharedFragmentShaderCode = r->text().toString().trimmed();
                if (!sharedFragmentShaderCode.isEmpty())
                    sharedFragmentShaderCode.append(QLatin1Char('\n'));
                r->skipCurrentElement();
            }
        } else if (r->name() == QStringLiteral("Shader")) {
            parseShader(effect);
            shaderCount++;
        } else if (r->name() == QStringLiteral("ComputeShader")) {
            qWarning("Effects: Compute shaders not supported; ignored");
            r->skipCurrentElement();
        } else {
            r->skipCurrentElement();
        }
    }

    if (shaderCount == 0)
        r->raiseError(QObject::tr("At least one Shader is required for a valid Material"));

    for (Q3DSMaterial::Shader &shader : effect.m_shaders)
        combineShaderCode(&shader, sharedShaderCode, sharedVertexShaderCode, sharedFragmentShaderCode);
}

void Q3DSEffectParser::parsePasses(Q3DSEffect &effect)
{
    QXmlStreamReader *r = reader();
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Pass")) {
            parsePass(effect);
        } else if (r->name() == QStringLiteral("Buffer")) {
            parseBuffer(effect);
        } else if (r->name() == QStringLiteral("Image")) {
            parseImage(effect);
        } else if (r->name() == QStringLiteral("DataBuffer")) {
            parseDataBuffer(effect);
        } else {
            r->skipCurrentElement();
        }
    }
}

void Q3DSEffectParser::parseShader(Q3DSEffect &effect)
{
    QXmlStreamReader *r = reader();
    Q3DSMaterial::Shader shader = Q3DSMaterial::parserShaderElement(r);
    // Shaders without names should be given index based names
    if (shader.name.isEmpty())
        shader.name = QString::number(effect.m_shaders.count() + 1);
    effect.m_shaders.insert(shader.name, shader);
}

void Q3DSEffectParser::parsePass(Q3DSEffect &effect)
{
    Q3DSMaterial::Pass pass; // sets defaults like [source], [dest], RGBA8, ...
    QXmlStreamReader *r = reader();
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("shader")) {
            pass.shaderName = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("input")) {
            pass.input = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("output")) {
            pass.output = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("format")) {
            Q3DSMaterial::TextureFormat fmt;
            if (Q3DSMaterial::convertToTextureFormat(attribute.value(), QLatin1String("ubyte"), &fmt, "texture format", r))
                pass.outputFormat = fmt;
        }
    }

    // Parse pass commands
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("BufferInput")) {
            Q3DSMaterial::PassCommand bufferInput(Q3DSMaterial::PassCommand::BufferInputType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("param"))
                    bufferInput.data()->param = attribute.value().toString();
                else if (attribute.name() == QStringLiteral("value"))
                    bufferInput.data()->value = attribute.value().toString();
            }
            pass.commands.append(bufferInput);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("DepthInput")) {
            Q3DSMaterial::PassCommand depthInput(Q3DSMaterial::PassCommand::DepthInputType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("param"))
                    depthInput.data()->param = attribute.value().toString();
            }
            pass.commands.append(depthInput);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("ImageInput")) {
            //Q3DSMaterial::ImageInput imageInput;
            Q3DSMaterial::PassCommand imageInput(Q3DSMaterial::PassCommand::ImageInputType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("param"))
                    imageInput.data()->param = attribute.value().toString();
                else if (attribute.name() == QStringLiteral("value"))
                    imageInput.data()->value = attribute.value().toString();
                else if (attribute.name() == QStringLiteral("usage"))
                    imageInput.data()->usage = attribute.value().toString();
                else if (attribute.name() == QStringLiteral("sync")) {
                    bool sync = false;
                    if (Q3DS::convertToBool(attribute.value(), &sync, "ImageInput sync", r))
                        imageInput.data()->sync = sync;
                }
            }
            pass.commands.append(imageInput);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("DataBufferInput")) {
            Q3DSMaterial::PassCommand dataBufferInput(Q3DSMaterial::PassCommand::DataBufferInputType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("param"))
                    dataBufferInput.data()->param = attribute.value().toString();
                else if (attribute.name() == QStringLiteral("usage"))
                    dataBufferInput.data()->usage = attribute.value().toString();
            }
            pass.commands.append(dataBufferInput);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("SetParam")) {
            Q3DSMaterial::PassCommand setParam(Q3DSMaterial::PassCommand::SetParamType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("name"))
                    setParam.data()->name = attribute.value().toString();
                else if (attribute.name() == QStringLiteral("value"))
                    setParam.data()->value = attribute.value().toString();
            }
            pass.commands.append(setParam);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("Blending")) {
            Q3DSMaterial::PassCommand blending(Q3DSMaterial::PassCommand::BlendingType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("source"))
                    blending.data()->source = attribute.value().toString();
                else if (attribute.name() == QStringLiteral("dest"))
                    blending.data()->destination = attribute.value().toString();
            }
            pass.commands.append(blending);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("RenderState")) {
            Q3DSMaterial::PassCommand renderState(Q3DSMaterial::PassCommand::RenderStateType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("name"))
                    renderState.data()->name = attribute.value().toString();
                else if (attribute.name() == QStringLiteral("value"))
                    renderState.data()->value = attribute.value().toString();
            }
            pass.commands.append(renderState);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("DepthStencil")) {
            Q3DSMaterial::PassCommand depthStencil(Q3DSMaterial::PassCommand::DepthStencilType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("buffer")) {
                    depthStencil.data()->bufferName = attribute.value().toString();
                } else if (attribute.name() == QStringLiteral("reference")) {
                    qint32 stencilValue = 0;
                    if (Q3DS::convertToInt32(attribute.value(), &stencilValue, "stencil value", r))
                        depthStencil.data()->stencilValue = stencilValue;
                } else if (attribute.name() == QStringLiteral("flags")) {
                    depthStencil.data()->flags = attribute.value().toString();
                } else if (attribute.name() == QStringLiteral("mask")) {
                    qint32 mask;
                    if (Q3DS::convertToInt32(attribute.value(), &mask, "stencil mask", r))
                        depthStencil.data()->mask = mask;
                } else if (attribute.name() == QStringLiteral("stencil-fail")) {
                    Q3DSMaterial::StencilOp stencilOp;
                    if (Q3DSMaterial::convertToStencilOp(attribute.value(), &stencilOp, "stencil fail", r))
                        depthStencil.data()->stencilFail = stencilOp;
                } else if (attribute.name() == QStringLiteral("depth-pass")) {
                    Q3DSMaterial::StencilOp stencilOp;
                    if (Q3DSMaterial::convertToStencilOp(attribute.value(), &stencilOp, "depth pass", r))
                        depthStencil.data()->depthPass = stencilOp;
                } else if (attribute.name() == QStringLiteral("depth-fail")) {
                    Q3DSMaterial::StencilOp stencilOp;
                    if (Q3DSMaterial::convertToStencilOp(attribute.value(), &stencilOp, "depth fail", r))
                        depthStencil.data()->depthFail = stencilOp;
                } else if (attribute.name() == QStringLiteral("stencil-function")) {
                    Q3DSMaterial::BoolOp boolOp;
                    if (Q3DSMaterial::convertToBoolOp(attribute.value(), &boolOp, "stencil function", r))
                        depthStencil.data()->stencilFunction = boolOp;
                }
            }
            pass.commands.append(depthStencil);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else {
            r->skipCurrentElement();
        }
    }

    effect.m_passes.append(pass);
}

void Q3DSEffectParser::parseBuffer(Q3DSEffect &effect)
{
    QXmlStreamReader *r = reader();
    Q3DSMaterial::Buffer buffer = Q3DSMaterial::parseBuffer(r);
    effect.m_buffers.insert(buffer.name(), buffer);
}

void Q3DSEffectParser::parseImage(Q3DSEffect &effect)
{
    Q3DSMaterial::ImageBuffer imageBuffer;
    QXmlStreamReader *r = reader();
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("name")) {
            imageBuffer.setName(attribute.value().toString());
        } else if (attribute.name() == QStringLiteral("lifetime")) {
            imageBuffer.setHasSceneLifetime(attribute.value().toString() == QStringLiteral("scene"));
        } else if (attribute.name() == QStringLiteral("type")) {
            imageBuffer.setType(attribute.value().toString());
        } else if (attribute.name() == QStringLiteral("format")) {
            imageBuffer.setFormat(attribute.value().toString());
        } else if (attribute.name() == QStringLiteral("filter")) {
            Q3DSMaterial::FilterType filter;
            if (Q3DSMaterial::convertToFilterType(attribute.value(), &filter, "filter", r))
                imageBuffer.setFilter(filter);
        } else if (attribute.name() == QStringLiteral("wrap")) {
            Q3DSMaterial::ClampType wrap;
            if (Q3DSMaterial::convertToClampType(attribute.value(), &wrap, "wrap", r))
                imageBuffer.setWrap(wrap);
        } else if (attribute.name() == QStringLiteral("size")) {
            float size;
            if (Q3DS::convertToFloat(attribute.value(), &size, "size", r))
                imageBuffer.setSize(size);
        } else if (attribute.name() == QStringLiteral("access")) {
            Q3DSMaterial::ImageAccess imageAccess;
            if (Q3DSMaterial::convertToImageAccess(attribute.value(), &imageAccess, "access", r))
                imageBuffer.setAccess(imageAccess);
        }
    }
    // Try and resolve TextureFormat
    Q3DSMaterial::TextureFormat format;
    const QString formatStr = imageBuffer.format();
    if (Q3DSMaterial::convertToTextureFormat(&formatStr, imageBuffer.type(), &format, "texture format", r)) {
        imageBuffer.setTextureFormat(format);
    }
    effect.m_buffers.insert(imageBuffer.name(), imageBuffer);

    while (r->readNextStartElement())
        r->skipCurrentElement();
}

namespace {
    size_t getTypeSize(const QString &format)
    {
        if (format == QStringLiteral("uint"))
            return sizeof(quint32);
        if (format == QStringLiteral("int"))
            return sizeof(qint32);
        if (format == QStringLiteral("uvec4"))
            return sizeof(quint32) * 4;

        return 1;
    }
}

void Q3DSEffectParser::parseDataBuffer(Q3DSEffect &effect)
{
    Q3DSMaterial::DataBuffer dataBuffer;
    QXmlStreamReader *r = reader();
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("name")) {
            dataBuffer.setName(attribute.value().toString());
        } else if (attribute.name() == QStringLiteral("lifetime")) {
            dataBuffer.setHasSceneLifetime(attribute.value().toString() == QStringLiteral("scene"));
        } else if (attribute.name() == QStringLiteral("type")) {
            dataBuffer.setType(attribute.value().toString());
        } else if (attribute.name() == QStringLiteral("format")) {
            dataBuffer.setFormat(attribute.value().toString());
        } else if (attribute.name() == QStringLiteral("size")) {
            float size;
            if (Q3DS::convertToFloat(attribute.value(), &size, "size", r))
                dataBuffer.setSize(size);
        } else if (attribute.name() == QStringLiteral("wrapType")) {
            dataBuffer.setWrapType(attribute.value().toString());
        } else if (attribute.name() == QStringLiteral("wrapName")) {
            dataBuffer.setWrapName(attribute.value().toString());
        }
    }

    // Adjust Size
    dataBuffer.setSize(dataBuffer.size() * getTypeSize(dataBuffer.format()));
    if (dataBuffer.size() <= 0)
        dataBuffer.setSize(1.0f);

    // Make sure wrapName and name are not empty
    if (dataBuffer.wrapName().isEmpty() || dataBuffer.name().isEmpty())
        r->raiseError(QObject::tr("Effect DataBuffers require non-empty name and wrapName"));
    else
        effect.m_buffers.insert(dataBuffer.name(), dataBuffer);

    while (r->readNextStartElement())
        r->skipCurrentElement();
}

bool Q3DSEffectParser::isPropertyNameUnique(const QString &name, const Q3DSEffect &effect)
{
    for (auto property : effect.m_properties) {
        if (property.name == name)
            return false;
    }
    return true;
}

QT_END_NAMESPACE
