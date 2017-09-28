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

#ifndef Q3DSCUSTOMMATERIAL_H
#define Q3DSCUSTOMMATERIAL_H

#include <QtCore/QString>
#include <QtCore/QMap>
#include <Qt3DRender/QMaterial>

#include <Qt3DStudioRuntime2/q3dsabstractxmlparser.h>
#include <Qt3DStudioRuntime2/q3dsmaterial.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
class QEffect;
class QTechnique;
class QParameter;
class QRenderPass;
class QShaderProgram;
}

class Q3DSV_EXPORT Q3DSCustomMaterial
{
public:
    Q3DSCustomMaterial();
    ~Q3DSCustomMaterial();

    QString name() const;
    QString description() const;

    bool isNull() const;

    const QMap<QString, Q3DSMaterial::PropertyElement> &properties() const;

    QString shaderType() const;
    QString shadersVersion() const;
    QString shadersSharedCode() const;
    QVector<Q3DSMaterial::Shader> shaders() const;

    Qt3DRender::QMaterial *generateMaterial();

    bool hasTransparency() const;
    bool hasRefraction() const;
    bool alwaysDirty() const;
    quint32 shaderKey() const;
    quint32 layerCount() const;

private:
    Qt3DRender::QShaderProgram *generateShaderProgram(const Q3DSMaterial::Shader &shader,
                                                      const QString &globalSharedCode,
                                                      const QString &shaderPrefixCode) const;
    QMap<QString, Qt3DRender::QParameter *> generateParameters(QString &shaderPrefix) const;
    QString resolveShaderIncludes(const QString &shaderCode) const;

    // MaterialElement
    QString m_name;
    QString m_description;

    // MetaData
    QString m_author;
    QString m_created;
    QString m_modified;
    QMap<QString,Q3DSMaterial::PropertyElement> m_properties;

    //Shaders
    QString m_shaderType;
    QString m_shadersVersion;
    QString m_shadersSharedCode; //Shared between all shaders
    QVector<Q3DSMaterial::Shader> m_shaders;

    bool m_hasTransparency;
    bool m_hasRefraction;
    bool m_alwaysDirty;
    quint32 m_shaderKey;
    quint32 m_layerCount;

    friend class Q3DSCustomMaterialParser;
};

class Q3DSV_EXPORT Q3DSCustomMaterialParser : public Q3DSAbstractXmlParser
{
public:
    Q3DSCustomMaterial parse(const QString &filename, bool *ok = nullptr);

private:
    void parseMaterial(Q3DSCustomMaterial &customMaterial);
    void parseMetaData(Q3DSCustomMaterial &customMaterial);
    void parseShaders(Q3DSCustomMaterial &customMaterial);
    void parseProperty(Q3DSCustomMaterial &customMaterial);
    void parseShader(Q3DSCustomMaterial &customMaterial);
    void parsePasses(Q3DSCustomMaterial &customMaterial);

    bool isPropertyNameUnique(const QString &name, const Q3DSCustomMaterial &customMaterial);
};

QT_END_NAMESPACE

#endif // Q3DSCUSTOMMATERIAL_H
