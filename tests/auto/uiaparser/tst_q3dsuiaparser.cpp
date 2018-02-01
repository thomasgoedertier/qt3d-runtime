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

#include <QtTest/QtTest>
#include <private/q3dsutils_p.h>
#include <private/q3dsuiaparser_p.h>

class tst_Q3DSUiaParser : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void testEmpty();
    void testInvalid_data();
    void testInvalid();
    void testMixed();
};

void tst_Q3DSUiaParser::initTestCase()
{
    Q3DSUtils::setDialogsEnabled(false);
}

void tst_Q3DSUiaParser::cleanup()
{
}

void tst_Q3DSUiaParser::testEmpty()
{
    {
        Q3DSUiaParser parser;
    }

    {
        Q3DSUiaParser parser;
        Q3DSUiaParser::Uia data = parser.parse(QLatin1String("invalid_file"));
        QVERIFY(!data.isValid());
    }
}

void tst_Q3DSUiaParser::testInvalid_data()
{
    QTest::addColumn<QString>("fn");
    QTest::newRow("1") << QString(QLatin1String(":/data/invalid1.uia"));
    QTest::newRow("2") << QString(QLatin1String(":/data/invalid2.uia"));
}

void tst_Q3DSUiaParser::testInvalid()
{
    QFETCH(QString, fn);
    Q3DSUiaParser parser;
    QVERIFY(QFile::exists(fn));
    Q3DSUiaParser::Uia data = parser.parse(fn);
    QVERIFY(!data.isValid());
    QVERIFY(!parser.readerErrorString().isEmpty());
}

void tst_Q3DSUiaParser::testMixed()
{
    Q3DSUiaParser parser;
    Q3DSUiaParser::Uia data = parser.parse(QLatin1String(":/data/test1.uia"));
    QVERIFY(data.isValid());

    QCOMPARE(data.initialPresentationId, QStringLiteral("main"));
    QCOMPARE(data.presentations.count(), 4);

    QCOMPARE(data.presentations[0].type, Q3DSUiaParser::Uia::Presentation::Uip);
    QCOMPARE(data.presentations[0].id, QStringLiteral("main"));
    QCOMPARE(data.presentations[0].source, QStringLiteral("main.uip"));

    QCOMPARE(data.presentations[1].type, Q3DSUiaParser::Uia::Presentation::Uip);
    QCOMPARE(data.presentations[1].id, QStringLiteral("subpres1"));
    QCOMPARE(data.presentations[1].source, QStringLiteral("subpres1.uip"));

    QCOMPARE(data.presentations[3].type, Q3DSUiaParser::Uia::Presentation::Qml);
    QCOMPARE(data.presentations[3].id, QStringLiteral("quick"));
    QCOMPARE(data.presentations[3].source, QStringLiteral("quick-preview.qml"));
}

#include <tst_q3dsuiaparser.moc>
QTEST_MAIN(tst_Q3DSUiaParser)
