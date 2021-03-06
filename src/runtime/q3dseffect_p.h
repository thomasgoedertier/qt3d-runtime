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

#ifndef Q3DSEFFECT_P_H
#define Q3DSEFFECT_P_H

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

#include "q3dsabstractxmlparser_p.h"
#include "q3dsmaterial_p.h"

QT_BEGIN_NAMESPACE

class Q3DSV_PRIVATE_EXPORT Q3DSEffect
{
public:
    enum Flag {
        ReliesOnTime = 0x01
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    Q3DSEffect();

    bool isNull();

    const QMap<QString, Q3DSMaterial::PropertyElement>& properties() const;
    const QMap<QString, Q3DSMaterial::Shader>& shaders() const;
    const QMap<QString, Q3DSMaterial::PassBuffer>& buffers() const;
    const QVector<Q3DSMaterial::Pass> &passes() const;

    QString addPropertyUniforms(const QString &shaderSrc) const;

    Flags flags() const;

private:
    QMap<QString, Q3DSMaterial::PropertyElement> m_properties;

    QMap<QString, Q3DSMaterial::Shader> m_shaders;
    QMap<QString, Q3DSMaterial::PassBuffer> m_buffers; // value type is the base class, subclasses have no data
    QVector<Q3DSMaterial::Pass> m_passes;

    Flags m_flags;

    friend class Q3DSEffectParser;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSEffect::Flags)
Q_DECLARE_TYPEINFO(Q3DSEffect, Q_MOVABLE_TYPE);

class Q3DSV_PRIVATE_EXPORT Q3DSEffectParser : public Q3DSAbstractXmlParser
{
public:
    Q3DSEffect parse(const QString &filename, bool *ok = nullptr);

private:
    void parseProperties(Q3DSEffect &effect);
    void parseShaders(Q3DSEffect &effect);
    void parsePasses(Q3DSEffect &effect);
    void parseShader(Q3DSEffect &effect);
    void parsePass(Q3DSEffect &effect);
    void parseBuffer(Q3DSEffect &effect);
    void parseImage(Q3DSEffect &effect);
    void parseDataBuffer(Q3DSEffect &effect);

    bool isPropertyNameUnique(const QString &name, const Q3DSEffect &effect);
};

QT_END_NAMESPACE

#endif // Q3DSEFFECT_P_H
