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
    : m_hasTransparency(false)
    , m_hasRefraction(false)
    , m_alwaysDirty(false)
    , m_shaderKey(0)
    , m_layerCount(0)
{
}

Q3DSCustomMaterial::~Q3DSCustomMaterial()
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

QString Q3DSCustomMaterial::shadersSharedCode() const
{
    return m_shadersSharedCode;
}

QVector<Q3DSMaterial::Shader> Q3DSCustomMaterial::shaders() const
{
    return m_shaders;
}

Qt3DRender::QMaterial *Q3DSCustomMaterial::generateMaterial()
{
    auto material = new Qt3DRender::QMaterial();
    auto *effect = new Qt3DRender::QEffect();
    auto *technique = new Qt3DRender::QTechnique();
    QVector<Qt3DRender::QRenderPass *> renderPasses;
    QVector<Qt3DRender::QShaderProgram *> shaderPrograms;

    QString shaderPrefix = QStringLiteral("#include \"customMaterial.glsllib\"\n");

    // Generate Parameters (before shader program, to also generate complete shader prefix)
    auto parameters = generateParameters(shaderPrefix);

    // Generate completed Shader Code (resolve #includes)
    // Add Shaders to shader program
    for (auto shader: m_shaders) {
        shaderPrograms.append(generateShaderProgram(shader, m_shadersSharedCode, shaderPrefix));
    }

    // TODO: Create A RenderPasses for each pass

    // TODO: Add Shader program to each Render pass as needed (1 : 1)


    // Add Renderpasses to technique
    for (auto renderpass : renderPasses) {
        technique->addRenderPass(renderpass);
    }

    // Add Technique to Effect
    effect->addTechnique(technique);

    // Add parameters to effect
    for (auto parameter : parameters)
        effect->addParameter(parameter);

    // Set the Effect to be active for this material
    material->setEffect(effect);

    return material;
}

Qt3DRender::QShaderProgram* Q3DSCustomMaterial::generateShaderProgram(const Q3DSMaterial::Shader &shader, const QString &globalSharedCode, const QString &shaderPrefixCode) const
{
    QString shaderPrefix = resolveShaderIncludes(shaderPrefixCode);
    QString globalShared = resolveShaderIncludes(globalSharedCode);
    QString shared = resolveShaderIncludes(shader.shared);
    QString vertexShader = resolveShaderIncludes(shader.vertexShader);
    QString fragmentShader = resolveShaderIncludes(shader.fragmentShader);

    // Initialize
    if (vertexShader.isEmpty())
        vertexShader.append(QLatin1String("void vert(){}"));

    if (fragmentShader.isEmpty())
        fragmentShader.append(QLatin1String("void frag(){}"));

    // Assemble
    QByteArray vertexShaderCode;
    QByteArray fragmentShaderCode;
    // Vertex
    vertexShaderCode.append(shaderPrefix.toUtf8());
    vertexShaderCode.append(globalShared.toUtf8());
    vertexShaderCode.append(shared.toUtf8());
    vertexShaderCode.append(QByteArrayLiteral("\n#ifdef VERTEX_SHADER\n"));
    vertexShaderCode.append(vertexShader.toUtf8());
    vertexShaderCode.append(QByteArrayLiteral("\n#endif\n"));

    // Fragment
    fragmentShaderCode.append(shaderPrefix.toUtf8());
    fragmentShaderCode.append(globalShared.toUtf8());
    fragmentShaderCode.append(shared.toUtf8());
    fragmentShaderCode.append(QByteArrayLiteral("\n#ifdef FRAGMENT_SHADER\n"));
    fragmentShaderCode.append(vertexShader.toUtf8());
    fragmentShaderCode.append(QByteArrayLiteral("\n#endif\n"));

    auto shaderProgram = new Qt3DRender::QShaderProgram();
    shaderProgram->setVertexShaderCode(vertexShaderCode);
    shaderProgram->setFragmentShaderCode(fragmentShaderCode);
    return shaderProgram;
}

namespace {
void appendShaderUniform(const QString &type, const QString &name, QString &shaderPrefix)
{
    shaderPrefix.append(QLatin1String("uniform "));
    shaderPrefix.append(type);
    shaderPrefix.append(QLatin1String(" "));
    shaderPrefix.append(name);
    shaderPrefix.append(QLatin1String(";\n"));
}
}

QMap<QString, Qt3DRender::QParameter *> Q3DSCustomMaterial::generateParameters(QString &shaderPrefix) const
{
    // We need to Generate Parameters for the effect
    // and also generate prefixes for the shaders
    QMap<QString, Qt3DRender::QParameter *> parameters;

    for (auto property : m_properties.values()) {
        switch (property.type) {
        case Q3DS::Boolean:
            appendShaderUniform(QLatin1String("bool"), property.name, shaderPrefix);
            bool boolValue;
            Q3DS::convertToBool(&property.defaultValue, &boolValue, "bool");
            parameters.insert(property.name, new Qt3DRender::QParameter(property.name, boolValue));
            break;
        case Q3DS::Long:
            appendShaderUniform(QLatin1String("int"), property.name, shaderPrefix);
            int intValue;
            if (!Q3DS::convertToInt(&property.defaultValue, &intValue, "long value"))
                intValue = 0;
            parameters.insert(property.name, new Qt3DRender::QParameter(property.name, intValue));
            break;
        case Q3DS::FloatRange:
        case Q3DS::Float:
        case Q3DS::FontSize:
            appendShaderUniform(QLatin1String("float"), property.name, shaderPrefix);
            float floatValue;
            if (!Q3DS::convertToFloat(&property.defaultValue, &floatValue, "float value"))
                floatValue = 0.0f;
            parameters.insert(property.name, new Qt3DRender::QParameter(property.name, floatValue));
            break;
        case Q3DS::Float2:
        {
            appendShaderUniform(QLatin1String("vec2"), property.name, shaderPrefix);
            QVector2D vec2Value;
            Q3DS::convertToVector2D(&property.defaultValue, &vec2Value, "Vec2 value");
            parameters.insert(property.name, new Qt3DRender::QParameter(property.name, vec2Value));
        }
            break;
        case Q3DS::Vector:
        case Q3DS::Scale:
        case Q3DS::Rotation:
        case Q3DS::Color:
        {
            appendShaderUniform(QLatin1String("vec3"), property.name, shaderPrefix);
            QVector3D vec3Value;
            Q3DS::convertToVector3D(&property.defaultValue, &vec3Value, "Vec3 value");
            parameters.insert(property.name, new Qt3DRender::QParameter(property.name, vec3Value));
        }
            break;
        case Q3DS::Texture:
            // String takes extra work
            shaderPrefix.append(QLatin1String("SNAPPER_SAMPLER2D("));
            shaderPrefix.append(property.name);
            shaderPrefix.append(QLatin1String(", "));
            shaderPrefix.append(property.name);
            shaderPrefix.append(QLatin1String(", "));
            shaderPrefix.append(Q3DSMaterial::filterTypeToString(property.minFilterType));
            shaderPrefix.append(QLatin1String(", "));
            shaderPrefix.append(Q3DSMaterial::clampTypeToString(property.clampType));
            shaderPrefix.append(QLatin1String(", "));
            shaderPrefix.append(QLatin1String("false )\n"));
            break;
        case Q3DS::StringList:
            // I dont really understand why, but apparently this is a int uniform ?
            appendShaderUniform(QLatin1String("int"), property.name, shaderPrefix);
            break;
        case Q3DS::Image2D:
            shaderPrefix.append(QLatin1String("layout("));
            shaderPrefix.append(property.format);
            if (!property.binding.isEmpty()) {
                shaderPrefix.append(QLatin1String(", binding = "));
                shaderPrefix.append(property.binding);
            }
            shaderPrefix.append(QLatin1String(") "));
            // if we have format layout we cannot set an additional access qualifier
            if (!property.access.isEmpty() && property.format.isEmpty())
                shaderPrefix.append(property.access);
            shaderPrefix.append(QLatin1String(" uniform image2D "));
            shaderPrefix.append(property.name);
            shaderPrefix.append(QLatin1String(";\n"));
            break;
        case Q3DS::Buffer:
            // Only storage counters
            if (property.usageType == Q3DSMaterial::Storage) {
                shaderPrefix.append(QLatin1String("layout("));
                shaderPrefix.append(property.align);
                if (!property.binding.isEmpty()) {
                    shaderPrefix.append(QLatin1String(", binding = "));
                    shaderPrefix.append(property.binding);
                }
                shaderPrefix.append(QLatin1String(") "));

                shaderPrefix.append(QLatin1String("buffer "));
                shaderPrefix.append(property.name);
                shaderPrefix.append(QLatin1String("\n{ \n"));
                shaderPrefix.append(property.format);
                shaderPrefix.append(QLatin1String(" "));
                shaderPrefix.append(property.name);
                shaderPrefix.append(QLatin1String("_data[]; \n};\n"));
            }
            break;
        default:
            // Don't know how to handle anything else...yet
            Q_ASSERT(false);
        }
    }
    return parameters;
}

QString Q3DSCustomMaterial::resolveShaderIncludes(const QString &shaderCode) const
{
    QString output;
    QTextStream inputStream(const_cast<QString*>(&shaderCode), QIODevice::ReadOnly);
    QString currentLine;
    while (inputStream.readLineInto(&currentLine)) {
        // Check if starts with #include
        currentLine = currentLine.trimmed();
        if (currentLine.startsWith(QStringLiteral("#include"))) {
            QString fileName = currentLine.split('"', QString::SkipEmptyParts).last();
            fileName.prepend(Q3DSUtils::resourcePrefix() + QLatin1String("res/effectlib/"));
            QFile file(fileName);
            if (!file.open(QIODevice::ReadOnly))
                qWarning() << QObject::tr("Could not open glsllib '%1'").arg(fileName);
            else
                output.append(QString::fromUtf8(file.readAll()));
            file.close();
            output.append(QLatin1String("\n"));
        } else {
            output.append(currentLine);
            output.append(QLatin1String("\n"));
        }
    }
    return output;
}

quint32 Q3DSCustomMaterial::layerCount() const
{
    return m_layerCount;
}

quint32 Q3DSCustomMaterial::shaderKey() const
{
    return m_shaderKey;
}

bool Q3DSCustomMaterial::alwaysDirty() const
{
    return m_alwaysDirty;
}

bool Q3DSCustomMaterial::hasRefraction() const
{
    return m_hasRefraction;
}

bool Q3DSCustomMaterial::hasTransparency() const
{
    return m_hasTransparency;
}


Q3DSCustomMaterial Q3DSCustomMaterialParser::parse(const QString &filename, bool *ok)
{
    if (!setSource(filename)) {
        if (ok != nullptr)
            *ok = false;
        return Q3DSCustomMaterial();
    }

    Q3DSCustomMaterial customMaterial;

    QXmlStreamReader *r = reader();
    if (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Material") && r->attributes().value(QLatin1String("version")) == QStringLiteral("1.0"))
            parseMaterial(customMaterial);
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
    return customMaterial;
}

void Q3DSCustomMaterialParser::parseMaterial(Q3DSCustomMaterial &customMaterial)
{
    QXmlStreamReader *r = reader();
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("name"))
            customMaterial.m_name = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("description"))
            customMaterial.m_description = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("always-dirty"))
            customMaterial.m_alwaysDirty = true;
    }

    if (customMaterial.m_name.isEmpty()) // Use filename (minus extension)
        customMaterial.m_name = sourceInfo()->baseName();

    int shadersCount = 0;
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("MetaData")) {
            parseMetaData(customMaterial);
        } else if (r->name() == QStringLiteral("Shaders")) {
            parseShaders(customMaterial);
            shadersCount++;
        } else if (r->name() == QStringLiteral("Passes")) {
            parsePasses(customMaterial);
        } else {
            r->skipCurrentElement();
        }
    }
    // There must be a Shaders element for a valid Material
    if (shadersCount == 0 && !r->hasError())
        r->raiseError(QObject::tr("A Shaders element is required for a valid Material"));
}

void Q3DSCustomMaterialParser::parseMetaData(Q3DSCustomMaterial &customMaterial)
{
    QXmlStreamReader *r = reader();
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("author"))
            customMaterial.m_author = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("created"))
            customMaterial.m_created = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("modified"))
            customMaterial.m_modified = attribute.value().toString();
    }

    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Property"))
            parseProperty(customMaterial);
        else
            r->skipCurrentElement();
    }
}

void Q3DSCustomMaterialParser::parseShaders(Q3DSCustomMaterial &customMaterial)
{
    QXmlStreamReader *r = reader();
    for (auto attribute : r->attributes()) {
        if (attribute.name() == QStringLiteral("type"))
            customMaterial.m_shaderType = attribute.value().toString();
        else if (attribute.name() == QStringLiteral("version"))
            customMaterial.m_shadersVersion = attribute.value().toString();
    }

    int shaderCount = 0;
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Shared")) {
            if ( r->readNext() == QXmlStreamReader::Characters)
                customMaterial.m_shadersSharedCode = r->text().toString().trimmed();
            r->skipCurrentElement();
        } else if (r->name() == QStringLiteral("Shader")) {
            parseShader(customMaterial);
            shaderCount++;
        } else {
            r->skipCurrentElement();
        }
    }
    if (shaderCount == 0)
        r->raiseError(QObject::tr("At least one Shader is required for a valid Material"));
}

void Q3DSCustomMaterialParser::parseProperty(Q3DSCustomMaterial &customMaterial)
{
    QXmlStreamReader *r = reader();
    Q3DSMaterial::PropertyElement property = Q3DSMaterial::parserPropertyElement(r);

    if (property.name.isEmpty() || !isPropertyNameUnique(property.name, customMaterial)) {
        // name can not be empty
        r->raiseError(QObject::tr("Property elements must have a unique name"));
    }

    if (property.formalName.isEmpty())
        property.formalName = property.name;

    customMaterial.m_properties.insert(property.name, property);

    while (r->readNextStartElement()) {
        r->skipCurrentElement();
    }
}

void Q3DSCustomMaterialParser::parseShader(Q3DSCustomMaterial &customMaterial)
{
    QXmlStreamReader *r = reader();
    Q3DSMaterial::Shader shader = Q3DSMaterial::parserShaderElement(r);

    customMaterial.m_shaders.append(shader);
}

void Q3DSCustomMaterialParser::parsePasses(Q3DSCustomMaterial &customMaterial)
{
    QXmlStreamReader *r = reader();
    while (r->readNextStartElement()) {
        if (r->name() == QStringLiteral("Pass")) {
            //TODO: Parse Pass (not used as far as I know) UICDMMetaData.cpp: LoadMaterialClassXML(...)
            while (r->readNextStartElement()) {
                r->skipCurrentElement();
            }
        } else if (r->name() == QStringLiteral("ShaderKey")) {
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("value")) {
                    qint32 value;
                    if (Q3DS::convertToInt32(attribute.value(), &value, "shaderkey value", r))
                        customMaterial.m_shaderKey = value;
                }
            }
            while (r->readNextStartElement()) {
                r->skipCurrentElement();
            }
        } else if (r->name() == QStringLiteral("LayerKey")) {
            for (auto attribute : r->attributes()) {
                if (attribute.name() == QStringLiteral("count")) {
                    qint32 count;
                    if (Q3DS::convertToInt32(attribute.value(), &count, "shaderkey value", r))
                        customMaterial.m_layerCount = count;
                }
            }
            while (r->readNextStartElement()) {
                r->skipCurrentElement();
            }
        } else if (r->name() == QStringLiteral("Buffer")) {
            // TODO: parse Buffer (not used as far as I know) UICDMMetaData.cpp: LoadMaterialClassXML(...)
            while (r->readNextStartElement()) {
                r->skipCurrentElement();
            }
        } else {
            r->skipCurrentElement();
        }
    }
}

bool Q3DSCustomMaterialParser::isPropertyNameUnique(const QString &name, const Q3DSCustomMaterial &customMaterial)
{
    for (auto property : customMaterial.m_properties) {
        if (property.name == name)
            return false;
    }
    return true;
}

QT_END_NAMESPACE
