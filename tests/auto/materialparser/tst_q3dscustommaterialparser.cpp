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
#include "q3dsutils.h"
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
    void testMaterialGeneration();
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
    QVERIFY(bumpMapProperty.defaultValue == QStringLiteral(".\\maps\\materials\\carbon_fiber_bump.png"));

    QVERIFY(material.shaderType() == QStringLiteral("GLSL"));
    QVERIFY(material.shadersVersion() == QStringLiteral("330"));
    QVERIFY(material.shaders().first().vertexShader.isEmpty());
    QVERIFY(!material.shaders().first().fragmentShader.isEmpty());

    // Pass Data
    QVERIFY(material.shaderKey() == 5);
    QVERIFY(material.layerCount() == 3);
}

void tst_Q3DSCustomMaterialParser::testMaterialGeneration()
{
    Q3DSCustomMaterialParser parser;
    bool ok;
    auto customMaterial = parser.parse(QStringLiteral(":/data/carbon_fiber.material"), &ok);
    QVERIFY(ok);

    auto material = customMaterial.generateMaterial();
    QVERIFY(material != nullptr);

    delete material;

}

QTEST_MAIN(tst_Q3DSCustomMaterialParser)

#include "tst_q3dscustommaterialparser.moc"
