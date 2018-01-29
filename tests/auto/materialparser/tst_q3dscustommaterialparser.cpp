/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
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
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QString>
#include <QtTest>
#include <private/q3dsutils_p.h>
#include "q3dscustommaterial.h"

class tst_Q3DSCustomMaterialParser : public QObject
{
    Q_OBJECT

public:
    tst_Q3DSCustomMaterialParser();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testEmpty();
    void testInvalid();
    void testRepeatedLoad();
    void testValidateData();
    void testCommands();
    void testBuffers();
};

tst_Q3DSCustomMaterialParser::tst_Q3DSCustomMaterialParser()
{
}

void tst_Q3DSCustomMaterialParser::initTestCase()
{
    Q3DSUtils::setDialogsEnabled(false);
}

void tst_Q3DSCustomMaterialParser::cleanupTestCase()
{
}

void tst_Q3DSCustomMaterialParser::testEmpty()
{
    {
        Q3DSCustomMaterialParser parser;
    }

    {
        Q3DSCustomMaterialParser parser;
        bool ok = false;
        Q3DSCustomMaterial material = parser.parse(QStringLiteral("does_not_exist"), &ok);
        QVERIFY(!ok);
        QVERIFY(material.isNull());
    }
}

void tst_Q3DSCustomMaterialParser::testInvalid()
{
    Q3DSCustomMaterialParser parser;
    bool ok = false;
    Q3DSCustomMaterial material = parser.parse(QStringLiteral(":/data/invalid.material"), &ok);
    QVERIFY(!ok);
    QVERIFY(material.isNull());
    material = parser.parse(QStringLiteral(":/data/bad_version.material"), &ok);
    QVERIFY(!ok);
    QVERIFY(material.isNull());
    material = parser.parse(QStringLiteral(":/data/invalid_property.material"), &ok);
    QVERIFY(!ok);
    QVERIFY(material.isNull());
    material = parser.parse(QStringLiteral(":/data/missing_shader.material"), &ok);
    QVERIFY(!ok);
    QVERIFY(material.isNull());
    material = parser.parse(QStringLiteral(":/data/no_shaders.material"), &ok);
    QVERIFY(!ok);
    QVERIFY(material.isNull());
}

void tst_Q3DSCustomMaterialParser::testRepeatedLoad()
{
    Q3DSCustomMaterialParser parser;
    bool ok = false;
    Q3DSCustomMaterial material = parser.parse(QStringLiteral(":/data/carbon_fiber.material"), &ok);
    QVERIFY(ok);
    QVERIFY(!material.isNull());

    material = parser.parse(QStringLiteral(":/data/powder_coat.material"), &ok);
    QVERIFY(ok);
    QVERIFY(!material.isNull());

    material = parser.parse(QStringLiteral(":/data/thin_glass_frosted.material"), &ok);
    QVERIFY(ok);
    QVERIFY(!material.isNull());

    material = parser.parse(QStringLiteral(":/data/aluminum.material"), &ok);
    QVERIFY(ok);
    QVERIFY(!material.isNull());
    QVERIFY(!material.materialHasRefraction());
    QVERIFY(!material.materialHasTransparency());
}

void tst_Q3DSCustomMaterialParser::testValidateData()
{
    Q3DSCustomMaterialParser parser;
    bool ok = false;
    auto material = parser.parse(QStringLiteral(":/data/carbon_fiber.material"), &ok);
    QVERIFY(ok);

    QVERIFY(material.name() == QStringLiteral("carbon_fiber"));
    QVERIFY(material.properties().count() == 17);

    auto props = material.properties();
    auto bumpMapProperty = props[QStringLiteral("bump_texture")];
    QVERIFY(bumpMapProperty.type == Q3DS::Texture);
    QVERIFY(bumpMapProperty.clampType == Q3DSMaterial::Repeat);
    QVERIFY(bumpMapProperty.usageType == Q3DSMaterial::Bump);
    QVERIFY(bumpMapProperty.defaultValue == QStringLiteral("./maps/materials/carbon_fiber_bump.png"));

    QVERIFY(material.shaderType() == QStringLiteral("GLSL"));
    QVERIFY(material.shadersVersion() == QStringLiteral("330"));
    QVERIFY(!material.shaders().first().vertexShader.isEmpty());
    QVERIFY(material.shaders().first().vertexShader.startsWith(QStringLiteral("\n#ifdef VERTEX_SHADER\nvoid vert(){}")));
    QVERIFY(!material.shaders().first().fragmentShader.isEmpty());

    // Pass Data
    QVERIFY(material.layerCount() == 3);

    // shaderKey of 5 means Diffuse + Glossy
    QVERIFY(material.shaderIsDielectric());
    QVERIFY(material.shaderIsGlossy());
}

void tst_Q3DSCustomMaterialParser::testCommands()
{
    Q3DSCustomMaterialParser parser;
    Q3DSCustomMaterial customMaterial;
    bool ok = false;

    customMaterial = parser.parse(QStringLiteral(":/data/thin_glass_frosted.material"), &ok);
    QVERIFY(ok);
    QVERIFY(!customMaterial.isNull());

    QCOMPARE(customMaterial.shaders().count(), 5);
    QCOMPARE(customMaterial.passes().count(), 5);

    QCOMPARE(customMaterial.layerCount(), 1);

    // ShaderKey == 20
    QVERIFY(customMaterial.shaderIsGlossy());
    QVERIFY(customMaterial.shaderHasRefraction());

    // Due to BufferBlit and Blending
    QVERIFY(customMaterial.materialHasRefraction());
    QVERIFY(customMaterial.materialHasTransparency());

    auto passes = customMaterial.passes();
    QCOMPARE(passes[0].commands.count(), 1);
    QCOMPARE(passes[1].commands.count(), 1);
    QCOMPARE(passes[2].commands.count(), 1);
    QCOMPARE(passes[3].commands.count(), 2);
    QCOMPARE(passes[4].commands.count(), 2);

    QCOMPARE(passes[0].shaderName, QLatin1String("NOOP"));
    QCOMPARE(passes[0].input, QLatin1String("[source]"));
    QCOMPARE(passes[0].output, QLatin1String("dummy_buffer"));
    QCOMPARE(passes[0].outputFormat, Q3DSMaterial::RGBA8);
    QCOMPARE(passes[0].needsClear, false);
    auto cmd = passes[0].commands[0];
    QCOMPARE(cmd.type(), Q3DSMaterial::PassCommand::BufferBlitType);
    QCOMPARE(cmd.data()->source, QString());
    QCOMPARE(cmd.data()->destination, QLatin1String("frame_buffer"));

    QCOMPARE(passes[1].shaderName, QLatin1String("PREBLUR"));
    QCOMPARE(passes[1].input, QLatin1String("[source]"));
    QCOMPARE(passes[1].output, QLatin1String("temp_buffer"));
    QCOMPARE(passes[1].outputFormat, Q3DSMaterial::RGBA8);
    QCOMPARE(passes[1].needsClear, false);
    cmd = passes[1].commands[0];
    QCOMPARE(cmd.type(), Q3DSMaterial::PassCommand::BufferInputType);
    QCOMPARE(cmd.data()->value, QLatin1String("frame_buffer"));
    QCOMPARE(cmd.data()->param, QLatin1String("OriginBuffer"));

    QCOMPARE(passes[2].shaderName, QLatin1String("BLURX"));
    QCOMPARE(passes[2].input, QLatin1String("[source]"));
    QCOMPARE(passes[2].output, QLatin1String("temp_blurX"));
    QCOMPARE(passes[2].outputFormat, Q3DSMaterial::RGBA8);
    QCOMPARE(passes[2].needsClear, false);
    cmd = passes[2].commands[0];
    QCOMPARE(cmd.type(), Q3DSMaterial::PassCommand::BufferInputType);
    QCOMPARE(cmd.data()->value, QLatin1String("temp_buffer"));
    QCOMPARE(cmd.data()->param, QLatin1String("BlurBuffer"));

    QCOMPARE(passes[3].shaderName, QLatin1String("BLURY"));
    QCOMPARE(passes[3].input, QLatin1String("[source]"));
    QCOMPARE(passes[3].output, QLatin1String("temp_blurY"));
    QCOMPARE(passes[3].outputFormat, Q3DSMaterial::RGBA8);
    QCOMPARE(passes[3].needsClear, false);
    cmd = passes[3].commands[0];
    QCOMPARE(cmd.type(), Q3DSMaterial::PassCommand::BufferInputType);
    QCOMPARE(cmd.data()->value, QLatin1String("temp_blurX"));
    QCOMPARE(cmd.data()->param, QLatin1String("BlurBuffer"));
    cmd = passes[3].commands[1];
    QCOMPARE(cmd.type(), Q3DSMaterial::PassCommand::BufferInputType);
    QCOMPARE(cmd.data()->value, QLatin1String("temp_buffer"));
    QCOMPARE(cmd.data()->param, QLatin1String("OriginBuffer"));

    QCOMPARE(passes[4].shaderName, QLatin1String("MAIN"));
    QCOMPARE(passes[4].input, QLatin1String("[source]"));
    QCOMPARE(passes[4].output, QLatin1String("[dest]"));
    QCOMPARE(passes[4].outputFormat, Q3DSMaterial::RGBA8);
    QCOMPARE(passes[4].needsClear, false);
    cmd = passes[4].commands[0];
    QCOMPARE(cmd.type(), Q3DSMaterial::PassCommand::BufferInputType);
    QCOMPARE(cmd.data()->value, QLatin1String("temp_blurY"));
    QCOMPARE(cmd.data()->param, QLatin1String("refractiveTexture"));
    cmd = passes[4].commands[1];
    QCOMPARE(cmd.type(), Q3DSMaterial::PassCommand::BlendingType);
    QCOMPARE(cmd.data()->blendSource, Q3DSMaterial::SrcAlpha);
    QCOMPARE(cmd.data()->blendDestination, Q3DSMaterial::OneMinusSrcAlpha);
}

void tst_Q3DSCustomMaterialParser::testBuffers()
{
    Q3DSCustomMaterialParser parser;
    Q3DSCustomMaterial customMaterial;
    bool ok = false;

    customMaterial = parser.parse(QStringLiteral(":/data/thin_glass_frosted.material"), &ok);
    QVERIFY(ok);
    QVERIFY(!customMaterial.isNull());

    QCOMPARE(customMaterial.shaders().count(), 5);
    QCOMPARE(customMaterial.passes().count(), 5);
    QCOMPARE(customMaterial.buffers().count(), 5);

    auto buffers = customMaterial.buffers();
    QVERIFY(buffers.contains(QLatin1String("frame_buffer")));
    QVERIFY(buffers.contains(QLatin1String("dummy_buffer")));
    QVERIFY(buffers.contains(QLatin1String("temp_buffer")));
    QVERIFY(buffers.contains(QLatin1String("temp_blurX")));
    QVERIFY(buffers.contains(QLatin1String("temp_blurY")));

    Q3DSMaterial::Buffer b = buffers.value(QLatin1String("frame_buffer"));
    QCOMPARE(b.format(), QStringLiteral("source"));
    QCOMPARE(b.type(), QString());
    QCOMPARE(b.textureFormat(), Q3DSMaterial::UnknownTextureFormat);
    QCOMPARE(b.filter(), Q3DSMaterial::Linear);
    QCOMPARE(b.wrap(), Q3DSMaterial::Clamp);
    QCOMPARE(b.size(), 1.0f);
    QCOMPARE(b.hasSceneLifetime(), false);

    b = buffers.value(QLatin1String("dummy_buffer"));
    QCOMPARE(b.format(), QStringLiteral("rgba"));
    QCOMPARE(b.type(), QStringLiteral("ubyte"));
    QCOMPARE(b.textureFormat(), Q3DSMaterial::RGBA8);
    QCOMPARE(b.filter(), Q3DSMaterial::Nearest);
    QCOMPARE(b.wrap(), Q3DSMaterial::Clamp);
    QCOMPARE(b.size(), 1.0f);
    QCOMPARE(b.hasSceneLifetime(), false);

    b = buffers.value(QLatin1String("temp_buffer"));
    QCOMPARE(b.format(), QStringLiteral("rgba"));
    QCOMPARE(b.type(), QStringLiteral("fp16"));
    QCOMPARE(b.textureFormat(), Q3DSMaterial::RGBA16F);
    QCOMPARE(b.filter(), Q3DSMaterial::Linear);
    QCOMPARE(b.wrap(), Q3DSMaterial::Clamp);
    QCOMPARE(b.size(), 0.5f);
    QCOMPARE(b.hasSceneLifetime(), false);

    b = buffers.value(QLatin1String("temp_blurX"));
    QCOMPARE(b.format(), QStringLiteral("rgba"));
    QCOMPARE(b.type(), QStringLiteral("fp16"));
    QCOMPARE(b.textureFormat(), Q3DSMaterial::RGBA16F);
    QCOMPARE(b.filter(), Q3DSMaterial::Linear);
    QCOMPARE(b.wrap(), Q3DSMaterial::Clamp);
    QCOMPARE(b.size(), 0.5f);
    QCOMPARE(b.hasSceneLifetime(), false);

    b = buffers.value(QLatin1String("temp_blurY"));
    QCOMPARE(b.format(), QStringLiteral("rgba"));
    QCOMPARE(b.type(), QStringLiteral("fp16"));
    QCOMPARE(b.textureFormat(), Q3DSMaterial::RGBA16F);
    QCOMPARE(b.filter(), Q3DSMaterial::Linear);
    QCOMPARE(b.wrap(), Q3DSMaterial::Clamp);
    QCOMPARE(b.size(), 0.5f);
    QCOMPARE(b.hasSceneLifetime(), false);
}

QTEST_MAIN(tst_Q3DSCustomMaterialParser)

#include "tst_q3dscustommaterialparser.moc"
