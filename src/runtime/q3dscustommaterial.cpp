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

#include "q3dscustommaterial.h"
#include "q3dsutils.h"

#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QShaderProgram>

#include <QDebug>

QT_BEGIN_NAMESPACE

Q3DSCustomMaterial::Q3DSCustomMaterial()
    : m_alwaysDirty(false)
    , m_layerCount(0)
    , m_hasTransparency(false)
    , m_hasRefraction(false)
    , m_shaderKey(0)
{
}

QString Q3DSCustomMaterial::name() const
{
    return m_name;
}

QString Q3DSCustomMaterial::description() const
{
    return m_description;
}

bool Q3DSCustomMaterial::isNull() const
{
    return m_name.isEmpty();
}

const QMap<QString, Q3DSMaterial::PropertyElement> &Q3DSCustomMaterial::properties() const
{
    return m_properties;
}

QString Q3DSCustomMaterial::shaderType() const
{
    return m_shaderType;
}

QString Q3DSCustomMaterial::shadersVersion() const
{
    return m_shadersVersion;
}

const QVector<Q3DSMaterial::Shader> &Q3DSCustomMaterial::shaders() const
{
    return m_shaders;
}

const QVector<Q3DSMaterial::Pass> &Q3DSCustomMaterial::passes() const
{
    return m_passes;
}

const QHash<QString, Q3DSMaterial::Buffer> &Q3DSCustomMaterial::buffers() const
{
    return m_buffers;
}

int Q3DSCustomMaterial::layerCount() const
{
    return m_layerCount;
}

bool Q3DSCustomMaterial::isAlwaysDirty() const
{
    return m_alwaysDirty;
}

bool Q3DSCustomMaterial::shaderIsDielectric() const
{
    return m_shaderKey & Diffuse;
}

bool Q3DSCustomMaterial::shaderIsSpecular() const
{
    return m_shaderKey & Specular;
}

bool Q3DSCustomMaterial::shaderIsGlossy() const
{
    return m_shaderKey & Glossy;
}

bool Q3DSCustomMaterial::shaderIsCutoutEnabled() const
{
    return m_shaderKey & Cutout;
}

bool Q3DSCustomMaterial::shaderHasRefraction() const
{
    return m_shaderKey & Refraction;
}

bool Q3DSCustomMaterial::shaderHasTransparency() const
{
    return m_shaderKey & Transparent;
}

bool Q3DSCustomMaterial::shaderIsDisplaced() const
{
    return m_shaderKey & Displace;
}

bool Q3DSCustomMaterial::shaderIsVolumetric() const
{
    return m_shaderKey & Volumetric;
}

bool Q3DSCustomMaterial::shaderIsTransmissive() const
{
    return m_shaderKey & Transmissive;
}

QString Q3DSCustomMaterial::emissiveMaskMapName() const
{
    for (auto property : m_properties) {
        if (property.usageType == Q3DSMaterial::EmissiveMask)
            return property.name;
    }
    return QString();
}

bool Q3DSCustomMaterial::materialHasTransparency() const
{
    return m_hasTransparency;
}

bool Q3DSCustomMaterial::materialHasRefraction() const
{
    return m_hasRefraction;
}

Q3DSCustomMaterial Q3DSCustomMaterialParser::parse(const QString &filename, bool *ok)
{
    if (!setSource(filename)) {
        if (ok != nullptr)
            *ok = false;
        return Q3DSCustomMaterial();
    }

    m_material = Q3DSCustomMaterial();

    QXmlStreamReader *r = reader();
    if (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Material") && r->attributes().value(QLatin1String("version")) == QStringLiteral("1.0"))
            parseMaterial();
        else
            r->raiseError(QObject::tr("Not a valid version 1.0 .material file"));
    }

    if (r->hasError()) {
        Q3DSUtils::showMessage(readerErrorString());
        if (ok != nullptr)
            *ok = false;
        return Q3DSCustomMaterial();
    }

    if (ok != nullptr)
        *ok = true;

    return m_material;
}

void Q3DSCustomMaterialParser::parseMaterial()
{
    QXmlStreamReader *r = reader();
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("name"))
            m_material.m_name = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("description"))
            m_material.m_description = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("always-dirty"))
            m_material.m_alwaysDirty = true;
    }

    if (m_material.m_name.isEmpty()) // Use filename (minus extension)
        m_material.m_name = sourceInfo()->baseName();

    int shadersCount = 0;
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("MetaData")) {
            parseMetaData();
        } else if (r->name() == QStringLiteral("Shaders")) {
            parseShaders();
            shadersCount++;
        } else if (r->name() == QStringLiteral("Passes")) {
            parsePasses();
        } else {
            r->skipCurrentElement();
        }
    }
    // There must be a Shaders element for a valid Material
    if (shadersCount == 0 && !r->hasError())
        r->raiseError(QObject::tr("A Shaders element is required for a valid Material"));
}

void Q3DSCustomMaterialParser::parseMetaData()
{
    QXmlStreamReader *r = reader();
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("author"))
            m_material.m_author = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("created"))
            m_material.m_created = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("modified"))
            m_material.m_modified = attribute.value().toString();
    }

    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Property"))
            parseProperty();
        else
            r->skipCurrentElement();
    }
}

void Q3DSCustomMaterialParser::parseShaders()
{
    QString sharedShaderCode;
    QString sharedVertexShaderCode;
    QString sharedFragmentShaderCode;

    QXmlStreamReader *r = reader();
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("type"))
            m_material.m_shaderType = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("version"))
            m_material.m_shadersVersion = attribute.value().toString();
    }

    int shaderCount = 0;
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
            parseShader();
            shaderCount++;
        } else {
            r->skipCurrentElement();
        }
    }
    if (shaderCount == 0)
        r->raiseError(QObject::tr("At least one Shader is required for a valid Material"));

    for (Q3DSMaterial::Shader &shader : m_material.m_shaders)
        combineShaderCode(&shader, sharedShaderCode, sharedVertexShaderCode, sharedFragmentShaderCode);
}

void Q3DSCustomMaterialParser::parseProperty()
{
    QXmlStreamReader *r = reader();
    Q3DSMaterial::PropertyElement property = Q3DSMaterial::parsePropertyElement(r);

    if (property.name.isEmpty() || !isPropertyNameUnique(property.name)) {
        // name can not be empty
        r->raiseError(QObject::tr("Property elements must have a unique name"));
    }

    if (property.formalName.isEmpty())
        property.formalName = property.name;

    m_material.m_properties.insert(property.name, property);

    while (r->readNextStartElement())
        r->skipCurrentElement();
}

void Q3DSCustomMaterialParser::parseShader()
{
    QXmlStreamReader *r = reader();
    Q3DSMaterial::Shader shader = Q3DSMaterial::parserShaderElement(r);

    m_material.m_shaders.append(shader);
}

void Q3DSCustomMaterialParser::parsePasses()
{
    QXmlStreamReader *r = reader();
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Pass")) {
            parsePass();
        } else if (r->name() == QStringLiteral("ShaderKey")) {
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("value")) {
                    qint32 value;
                    if (Q3DS::convertToInt32(attribute.value(), &value, "shaderkey value", r))
                        m_material.m_shaderKey = value;
                }
            }
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("LayerKey")) {
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("count")) {
                    qint32 count;
                    if (Q3DS::convertToInt32(attribute.value(), &count, "layerkey value", r))
                        m_material.m_layerCount = count;
                }
            }
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("Buffer")) {
            parseBuffer();
        } else {
            r->skipCurrentElement();
        }
    }
}

void Q3DSCustomMaterialParser::parsePass()
{
    /* A complex example would be:
      <Passes>
        <ShaderKey value="20"/>
        <LayerKey count="1"/>
        <Buffer name="frame_buffer" format="source" filter="linear" wrap="clamp" size="1.0" lifetime="frame"/>
        <Buffer name="dummy_buffer" type="ubyte" format="rgba" wrap="clamp" size="1.0" lifetime="frame"/>
        <Buffer name="temp_buffer" type="fp16" format="rgba" filter="linear" wrap="clamp" size="0.5" lifetime="frame"/>
        <Buffer name="temp_blurX" type="fp16" format="rgba" filter="linear" wrap="clamp" size="0.5" lifetime="frame"/>
        <Buffer name="temp_blurY" type="fp16" format="rgba" filter="linear" wrap="clamp" size="0.5" lifetime="frame"/>
        <Pass shader="NOOP" output="dummy_buffer">
            <BufferBlit dest="frame_buffer"/>
        </Pass>
        <Pass shader="PREBLUR" output="temp_buffer">
            <BufferInput value="frame_buffer" param="OriginBuffer"/>
        </Pass>
        <Pass shader="BLURX" output="temp_blurX">
            <BufferInput value="temp_buffer" param="BlurBuffer"/>
        </Pass>
        <Pass shader="BLURY" output="temp_blurY">
            <BufferInput value="temp_blurX" param="BlurBuffer"/>
            <BufferInput value="temp_buffer" param="OriginBuffer"/>
        </Pass>
        <Pass shader="MAIN">
            <BufferInput value="temp_blurY" param="refractiveTexture"/>
            <Blending source="SrcAlpha" dest="OneMinusSrcAlpha"/>
        </Pass>
      </Passes>
    */

    QXmlStreamReader *r = reader();
    QXmlStreamAttributes passAttrs = r->attributes();

    // skip when type != "render"
    QStringRef passType = passAttrs.value(QLatin1String("type"));
    if (!passType.isEmpty() && passType != QStringLiteral("render")) {
        r->skipCurrentElement();
        return;
    }

    Q3DSMaterial::Pass pass; // sets defaults like [source], [dest], RGBA8, ...

    for (auto attribute : passAttrs) {
        if (attribute.name() == QStringLiteral("shader")) {
            // The old runtime prefixes shader names with the custom material
            // ID and " - ". This is skipped here since the shaders are stored
            // per Q3DSCustomMaterial anyway.
            pass.shaderName = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("input")) {
            pass.input = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("output")) {
            pass.output = attribute.value().toString();
        } else if (attribute.name() == QStringLiteral("clear")) {
            bool needsClear = false;
            if (Q3DS::convertToBool(attribute.value(), &needsClear, "bool", r))
                pass.needsClear = needsClear;
        } else if (attribute.name() == QStringLiteral("format")) {
            Q3DSMaterial::TextureFormat fmt;
            if (Q3DSMaterial::convertToTextureFormat(attribute.value(), QLatin1String("ubyte"), &fmt, "texture format", r))
                pass.outputFormat = fmt;
        }
    }

    while (r->readNextStartElement()) {
        // Commands with unspecified attributes leave the corresponding
        // PassCommand::Data fields unset (e.g. empty string). This is
        // different from Pass::input, output, etc. that do get a default value.
        if (r->name() == QStringLiteral("BufferBlit")) {
            Q3DSMaterial::PassCommand bufferBlit(Q3DSMaterial::PassCommand::BufferBlitType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("source"))
                    bufferBlit.data()->source = attribute.value().toString();
                else if (attribute.name() == QStringLiteral("dest"))
                    bufferBlit.data()->destination = attribute.value().toString();
            }
            pass.commands.append(bufferBlit);
            m_material.m_hasRefraction = true; // "We use buffer blits to simulate glass refraction"
            while (r->readNextStartElement())
                r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("BufferInput")) {
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
        } else if (r->name() == QStringLiteral("Blending")) {
            Q3DSMaterial::PassCommand blending(Q3DSMaterial::PassCommand::BlendingType);
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("source"))
                    blending.data()->source = attribute.value().toString();
                else if (attribute.name() == QStringLiteral("dest"))
                    blending.data()->destination = attribute.value().toString();
            }
            pass.commands.append(blending);
            m_material.m_hasTransparency = true; // if we have blending we have transparency
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
        } else {
            r->skipCurrentElement();
        }
    }

    m_material.m_passes.append(pass);
}

void Q3DSCustomMaterialParser::parseBuffer()
{
    QXmlStreamReader *r = reader();
    Q3DSMaterial::Buffer buffer = Q3DSMaterial::parseBuffer(r);
    m_material.m_buffers.insert(buffer.name(), buffer);
}

bool Q3DSCustomMaterialParser::isPropertyNameUnique(const QString &name)
{
    for (auto property : m_material.m_properties) {
        if (property.name == name)
            return false;
    }
    return true;
}

QT_END_NAMESPACE
