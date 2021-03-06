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

#ifndef Q3DSCUSTOMMATERIAL_P_H
#define Q3DSCUSTOMMATERIAL_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/QString>
#include <QtCore/QMap>
#include <Qt3DRender/QMaterial>

#include "q3dsabstractxmlparser_p.h"
#include "q3dsmaterial_p.h"

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
class QShaderProgram;
}

class Q3DSV_PRIVATE_EXPORT Q3DSCustomMaterial
{
public:
    Q3DSCustomMaterial();

    QString name() const;
    QString description() const;

    bool isNull() const;

    const QMap<QString, Q3DSMaterial::PropertyElement> &properties() const;

    QString shaderType() const;
    QString shadersVersion() const;
    const QVector<Q3DSMaterial::Shader> &shaders() const;
    const QVector<Q3DSMaterial::Pass> &passes() const;
    const QHash<QString, Q3DSMaterial::Buffer> &buffers() const;

    bool isAlwaysDirty() const;

    int layerCount() const; // nothing to do with normal layers

    bool shaderIsDielectric() const;
    bool shaderIsSpecular() const;
    bool shaderIsGlossy() const;
    bool shaderIsCutoutEnabled() const;
    bool shaderHasRefraction() const;
    bool shaderHasTransparency() const;
    bool shaderIsDisplaced() const;
    bool shaderIsVolumetric() const;
    bool shaderIsTransmissive() const;

    QString emissiveMaskMapName() const;

    bool materialHasTransparency() const;
    bool materialHasRefraction() const;

private:
    // MaterialElement
    QString m_name;
    QString m_description;
    bool m_alwaysDirty;

    // MetaData
    QString m_author;
    QString m_created;
    QString m_modified;
    QMap<QString,Q3DSMaterial::PropertyElement> m_properties;

    //Shaders
    QString m_shaderType;
    QString m_shadersVersion;
    QVector<Q3DSMaterial::Shader> m_shaders;

    // Passes
    QVector<Q3DSMaterial::Pass> m_passes;
    QHash<QString, Q3DSMaterial::Buffer> m_buffers;
    int m_layerCount;
    // these two are set based on Blending and Buffer stuff, not related to the shader key
    bool m_hasTransparency;
    bool m_hasRefraction;
    // metadata for the shader generator
    enum ShaderKeyFlag {
        Diffuse = 1 << 0,
        Specular = 1 << 1,
        Glossy = 1 << 2,
        Cutout = 1 << 3,
        Refraction = 1 << 4,
        Transparent = 1 << 5,
        Displace = 1 << 6,
        Volumetric = 1 << 7,
        Transmissive = 1 << 8,
    };
    quint32 m_shaderKey;

    friend class Q3DSCustomMaterialParser;
};

Q_DECLARE_TYPEINFO(Q3DSCustomMaterial, Q_MOVABLE_TYPE);

class Q3DSV_PRIVATE_EXPORT Q3DSCustomMaterialParser : public Q3DSAbstractXmlParser
{
public:
    Q3DSCustomMaterial parse(const QString &filename, bool *ok = nullptr);

private:
    void parseMaterial();
    void parseMetaData();
    void parseShaders();
    void parseProperty();
    void parseShader();
    void parsePasses();
    void parsePass();
    void parseBuffer();

    bool isPropertyNameUnique(const QString &name);

    Q3DSCustomMaterial m_material;
};

QT_END_NAMESPACE

#endif // Q3DSCUSTOMMATERIAL_P_H
