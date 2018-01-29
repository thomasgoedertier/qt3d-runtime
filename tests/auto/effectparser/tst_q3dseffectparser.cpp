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
#include "q3dseffect.h"

class tst_Q3DSEffectParser : public QObject
{
    Q_OBJECT

public:
    tst_Q3DSEffectParser();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testEmpty();
    void testRepeatedLoad();
    void testValidateData();
    void testGlobalSharedShaderSnippet();
};

tst_Q3DSEffectParser::tst_Q3DSEffectParser()
{
}

void tst_Q3DSEffectParser::initTestCase()
{
    Q3DSUtils::setDialogsEnabled(false);
}

void tst_Q3DSEffectParser::cleanupTestCase()
{
}

void tst_Q3DSEffectParser::testEmpty()
{
    {
        Q3DSEffectParser parser;
    }

    {
        Q3DSEffectParser parser;
        bool ok = false;
        parser.parse(QStringLiteral("does_not_exist"), &ok);
        QVERIFY(!ok);
    }
}

void tst_Q3DSEffectParser::testRepeatedLoad()
{
    Q3DSEffectParser parser;
    bool ok = false;
    auto effect = parser.parse(QStringLiteral(":/data/Bloom.effect"), &ok);
    QVERIFY(ok);
    QVERIFY(!effect.isNull());

    effect = parser.parse(QStringLiteral(":/data/Depth Of Field Bokeh.effect"), &ok);
    QVERIFY(ok);
    QVERIFY(!effect.isNull());

    effect = parser.parse(QStringLiteral(":/data/Sepia.effect"), &ok);
    QVERIFY(ok);
    QVERIFY(!effect.isNull());
}

void tst_Q3DSEffectParser::testValidateData()
{
    Q3DSEffectParser parser;
    bool ok = false;

    auto dofEffect = parser.parse(QStringLiteral(":/data/Depth Of Field Bokeh.effect"), &ok);
    QVERIFY(ok);
    QVERIFY(!dofEffect.isNull());

    // Properties
    {
        QVERIFY(dofEffect.properties().count() == 18);
        auto focusDistance = dofEffect.properties().value("FocusDistance");
        QVERIFY(focusDistance.type == Q3DS::Float);
        QVERIFY(focusDistance.defaultValue == "600");
        auto minBokehThreshold = dofEffect.properties().value("MinBokehThreshold");
        QVERIFY(minBokehThreshold.type == Q3DS::Float);
        QVERIFY(minBokehThreshold.defaultValue == "1.0");
        auto depthDebug = dofEffect.properties().value("DepthDebug");
        QVERIFY(depthDebug.type == Q3DS::Boolean);
        QVERIFY(depthDebug.defaultValue == "False");
        auto depthSampler = dofEffect.properties().value("DepthSampler");
        QVERIFY(depthSampler.type == Q3DS::Texture);
        QVERIFY(depthSampler.magFilterType == Q3DSMaterial::Nearest);
        QVERIFY(depthSampler.clampType == Q3DSMaterial::Clamp);
        auto bokehColorSampler = dofEffect.properties().value("BokehColorSampler");
        QVERIFY(bokehColorSampler.type == Q3DS::Image2D);
        QVERIFY(bokehColorSampler.format == "rgba32f");
        QVERIFY(bokehColorSampler.binding == "2");
        auto bokehCounter = dofEffect.properties().value("BokehCounter");
        QVERIFY(bokehCounter.type == Q3DS::Buffer);
        QVERIFY(bokehCounter.format == "uvec4");
        QVERIFY(bokehCounter.usageType == Q3DSMaterial::Storage);
        QVERIFY(bokehCounter.binding == "1");
        QVERIFY(bokehCounter.align == "std140");
    }

    // Shaders
    QVERIFY(dofEffect.shaders().count() == 6);
    QVERIFY(!dofEffect.shaders().value("BOKEH_RENDER").vertexShader.isEmpty());

    // Buffers
    QVERIFY(dofEffect.buffers().count() == 7);
    auto bokehBuffer = dofEffect.buffers().value("bokeh_buffer");
    QVERIFY(bokehBuffer.type() == "ubyte");
    QVERIFY(bokehBuffer.format() == "rgba");
    QVERIFY(bokehBuffer.size() == 1.0);
    QVERIFY(bokehBuffer.hasSceneLifetime());


    // Passes
    QVERIFY(dofEffect.passes().count() == 6);
    auto bokehRenderPass = dofEffect.passes().at(2); // Passes should have expected order
    QVERIFY(bokehRenderPass.shaderName == "BOKEH_RENDER");
    QVERIFY(bokehRenderPass.input == "depthblur_buffer");
    QVERIFY(bokehRenderPass.output == "bokeh_buffer");
    // Pass commands
    auto cmd = bokehRenderPass.commands.at(0);
    QVERIFY(cmd.type() == Q3DSMaterial::PassCommand::ImageInputType);
    Q3DSMaterial::PassCommand imageInputCmd = cmd;
    QVERIFY(imageInputCmd.data()->value == "bokeh_color_image");
    QVERIFY(imageInputCmd.data()->param == "BokehSampler");
    QVERIFY(imageInputCmd.data()->usage == "texture");
    QVERIFY(imageInputCmd.data()->sync);
}

void tst_Q3DSEffectParser::testGlobalSharedShaderSnippet()
{
    Q3DSEffectParser parser;
    bool ok = false;
    Q3DSEffect effect = parser.parse(QStringLiteral(":/data/Bloom.effect"), &ok);
    QVERIFY(ok);
    QVERIFY(!effect.isNull());

    QCOMPARE(effect.shaders().count(), 5);

    const QString globalSharedCode = QStringLiteral("#include \"blur.glsllib\"\nvarying float range;\n");
    for (const Q3DSMaterial::Shader &shader : effect.shaders()) {
        if (!shader.vertexShader.isEmpty())
            QVERIFY(shader.vertexShader.startsWith(globalSharedCode));
        if (!shader.fragmentShader.isEmpty())
            QVERIFY(shader.fragmentShader.startsWith(globalSharedCode));
    }
}

QTEST_APPLESS_MAIN(tst_Q3DSEffectParser)

#include "tst_q3dseffectparser.moc"
