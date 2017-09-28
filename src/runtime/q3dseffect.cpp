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
    //TODO: Reminder to append shader prefix "#include \"effect.glsllib\"\n" to all effects shaders
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

const QString &Q3DSEffect::sharedShaderCode() const
{
    return m_sharedShaderCode;
}

const QMap<QString, Q3DSMaterial::Shader> &Q3DSEffect::shaders() const
{
    return m_shaders;
}

const QMap<QString, Q3DSMaterial::PassBuffers> &Q3DSEffect::buffers() const
{
    return m_buffers;
}

const QVector<Q3DSMaterial::Pass> &Q3DSEffect::passes() const
{
    return m_passes;
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
            Q3DSMaterial::PropertyElement property = Q3DSMaterial::parserPropertyElement(r);

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
    int shaderCount = 0;
    QXmlStreamReader *r = reader();
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Shared")) {
            if ( r->readNext() == QXmlStreamReader::Characters) {
                effect.m_sharedShaderCode = r->text().toString().trimmed();
                r->skipCurrentElement();
            }
        } else if (r->name() == QStringLiteral("Shader")) {
            parseShader(effect);
            shaderCount++;
        } else {
            r->skipCurrentElement();
        }
    }
    if (shaderCount == 0)
        r->raiseError(QObject::tr("At least one Shader is required for a valid Material"));
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
    Q3DSMaterial::Pass pass;
    QXmlStreamReader *r = reader();
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("shader"))
            pass.shaderName = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("input"))
            pass.input = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("output"))
            pass.output = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("format"))
            pass.outputFormat = attribute.value().toString();
    }

    // Parse Pass Buffer Parameters
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("BufferInput")) {
            Q3DSMaterial::PassState bufferInput(Q3DSMaterial::PassState::BufferInputType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("param"))
                    bufferInput.state.param = attribute.value().toString();
                else if (attribute.name() == QStringLiteral("value"))
                    bufferInput.state.value = attribute.value().toString();
            }
            pass.states.append(bufferInput);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("DepthInput")) {
            Q3DSMaterial::PassState depthInput(Q3DSMaterial::PassState::DepthInputType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("param"))
                    depthInput.state.param = attribute.value().toString();
            }
            pass.states.append(depthInput);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("ImageInput")) {
            //Q3DSMaterial::ImageInput imageInput;
            Q3DSMaterial::PassState imageInput(Q3DSMaterial::PassState::ImageInputType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("param"))
                    imageInput.state.param = (attribute.value().toString());
                else if (attribute.name() == QStringLiteral("value"))
                    imageInput.state.value = (attribute.value().toString());
                else if (attribute.name() == QStringLiteral("usage"))
                    imageInput.state.usage = (attribute.value().toString());
                else if (attribute.name() == QStringLiteral("sync")) {
                    bool sync = false;
                    if (Q3DS::convertToBool(attribute.value(), &sync, "ImageInput sync", r))
                        imageInput.state.sync = sync;
                }
            }
            pass.states.append(imageInput);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("DataBufferInput")) {
            Q3DSMaterial::PassState dataBufferInput(Q3DSMaterial::PassState::DataBufferInputType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("param"))
                    dataBufferInput.state.param = (attribute.value().toString());
                else if (attribute.name() == QStringLiteral("usage"))
                    dataBufferInput.state.usage = (attribute.value().toString());
            }
            pass.states.append(dataBufferInput);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("SetParam")) {
            Q3DSMaterial::PassState setParam(Q3DSMaterial::PassState::SetParamType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("name"))
                    setParam.state.name = (attribute.value().toString());
                else if (attribute.name() == QStringLiteral("value"))
                    setParam.state.value = (attribute.value().toString());
            }
            pass.states.append(setParam);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("Blending")) {
            Q3DSMaterial::PassState blending(Q3DSMaterial::PassState::BlendingType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("source"))
                    blending.state.source = (attribute.value().toString());
                else if (attribute.name() == QStringLiteral("dest"))
                    blending.state.destination = (attribute.value().toString());
            }
            pass.states.append(blending);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("RenderState")) {
            Q3DSMaterial::PassState renderState(Q3DSMaterial::PassState::RenderStateType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("name"))
                    renderState.state.name = (attribute.value().toString());
                else if (attribute.name() == QStringLiteral("value"))
                    renderState.state.value = (attribute.value().toString());
            }
            pass.states.append(renderState);
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("DepthStencil")) {
            Q3DSMaterial::PassState depthStencil(Q3DSMaterial::PassState::DepthStencilType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("buffer")) {
                    depthStencil.state.bufferName = (attribute.value().toString());
                } else if (attribute.name() == QStringLiteral("reference")) {
                    qint32 stencilValue = 0;
                    if (Q3DS::convertToInt32(attribute.value(), &stencilValue, "stencil value", r))
                        depthStencil.state.stencilValue = stencilValue;
                } else if (attribute.name() == QStringLiteral("flags")) {
                    depthStencil.state.flags = (attribute.value().toString());
                } else if (attribute.name() == QStringLiteral("mask")) {
                    qint32 mask;
                    if (Q3DS::convertToInt32(attribute.value(), &mask, "stencil mask", r))
                        depthStencil.state.mask = mask;
                } else if (attribute.name() == QStringLiteral("stencil-fail")) {
                    Q3DSMaterial::StencilOp stencilOp;
                    if (Q3DSMaterial::convertToStencilOp(attribute.value(), &stencilOp, "stencil fail", r))
                        depthStencil.state.stencilFail = stencilOp;
                } else if (attribute.name() == QStringLiteral("depth-pass")) {
                    Q3DSMaterial::StencilOp stencilOp;
                    if (Q3DSMaterial::convertToStencilOp(attribute.value(), &stencilOp, "depth pass", r))
                        depthStencil.state.depthPass = stencilOp;
                } else if (attribute.name() == QStringLiteral("depth-fail")) {
                    Q3DSMaterial::StencilOp stencilOp;
                    if (Q3DSMaterial::convertToStencilOp(attribute.value(), &stencilOp, "depth fail", r))
                        depthStencil.state.depthFail = stencilOp;
                } else if (attribute.name() == QStringLiteral("stencil-function")) {
                    Q3DSMaterial::BoolOp boolOp;
                    if (Q3DSMaterial::convertToBoolOp(attribute.value(), &boolOp, "stencil function", r))
                        depthStencil.state.stencilFunction = boolOp;
                }
            }
            pass.states.append(depthStencil);
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
    Q3DSMaterial::Buffer buffer;
    QXmlStreamReader *r = reader();
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
    // Try and resolve TextureFormat
    Q3DSMaterial::TextureFormat format;
    const QString formatStr = buffer.format();
    if (Q3DSMaterial::convertToTextureFormat(&formatStr, buffer.type(), &format, "texture format", r)) {
        buffer.setTextureFormat(format);
    }
    effect.m_buffers.insert(buffer.name(), buffer);

    while (r->readNextStartElement())
        r->skipCurrentElement();
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
    if (dataBuffer.size() < 0)
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
