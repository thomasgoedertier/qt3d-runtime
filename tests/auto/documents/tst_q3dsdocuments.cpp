/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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
#include <private/q3dsengine_p.h>
#include <private/q3dswindow_p.h>
#include <private/q3dsutils_p.h>
#include <private/q3dsscenemanager_p.h>
#include <private/q3dsuipdocument_p.h>
#include <private/q3dsuiadocument_p.h>
#include <private/q3dsqmldocument_p.h>

#include "../shared/shared.h"

class tst_Q3DSDocuments : public QObject
{
    Q_OBJECT
public:
    tst_Q3DSDocuments();

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void testEmptyUip();
    void testEmptyUia();
    void testInvalidUip();
    void testUipFromSource();
    void testUipFromSourceData();
    void testUiaWithUip();
    void testUiaWith2Uip();
    void testUiaWith2Uip2();
    void testUiaWithQml();
    void testUiaWithOnlyQml();

private:
    Q3DSEngine *m_engine = nullptr;
    Q3DSWindow *m_view = nullptr;
    QByteArray m_validUipDoc;
    QByteArray m_validQmlDoc;
};

tst_Q3DSDocuments::tst_Q3DSDocuments()
{
    m_validUipDoc = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?> \
        <UIP version=\"3\" > \
            <Project > \
                <ProjectSettings author=\"\" company=\"Qt\" presentationWidth=\"800\" presentationHeight=\"480\" maintainAspect=\"True\" /> \
                <Graph> \
                    <Scene id=\"Scene\" bgcolorenable=\"False\" backgroundcolor=\"0.27451 0.776471 0.52549\" > \
                        <Layer id=\"Layer\" > \
                            <Camera id=\"Camera\" /> \
                            <Light id=\"Light\" /> \
                            <Model id=\"Cube\" > \
                                <Material id=\"Material\" /> \
                            </Model> \
                        </Layer> \
                    </Scene> \
                </Graph> \
                <Logic > \
                    <State name=\"Master Slide\" component=\"#Scene\" > \
                        <Add ref=\"#Layer\" /> \
                        <Add ref=\"#Camera\" /> \
                        <Add ref=\"#Light\" /> \
                        <State id=\"Scene-Slide1\" name=\"Slide1\" > \
                            <Add ref=\"#Cube\" name=\"Cube\" rotation=\"-52.4901 -10.7851 -20.7213\" scale=\"2 2 2\" sourcepath=\"#Cube\" /> \
                            <Add ref=\"#Material\" /> \
                        </State> \
                    </State> \
                </Logic>  \
            </Project> \
        </UIP>";

    m_validQmlDoc = \
        "import QtQuick 2.7 \n \
        Rectangle { \n \
            id: rootItem \n \
            width: 512 \n \
            height: 512 \n \
            color: \"white\" \n \
        }";

}

void tst_Q3DSDocuments::initTestCase()
{
    if (!isOpenGLGoodEnough())
        QSKIP("This platform does not support OpenGL proper");

    QSurfaceFormat::setDefaultFormat(Q3DSEngine::surfaceFormat());

    Q3DSUtils::setDialogsEnabled(false);
}

void tst_Q3DSDocuments::cleanupTestCase()
{
}

void tst_Q3DSDocuments::init()
{
    m_engine = new Q3DSEngine;
    m_view = new Q3DSWindow;
    m_view->setEngine(m_engine);
}

void tst_Q3DSDocuments::cleanup()
{
    /*
    // Show briefly at end of each testcase
    m_view->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_view));
    m_view->close();
    */

    delete m_engine;
    delete m_view;
}

void tst_Q3DSDocuments::testEmptyUip()
{
    Q3DSUipDocument uipDoc;
    m_engine->setDocument(uipDoc);
    QVERIFY(!m_engine->uipDocument());
    QVERIFY(!m_engine->sceneManager());
}

void tst_Q3DSDocuments::testEmptyUia()
{
    Q3DSUiaDocument uiaDoc;
    m_engine->setDocument(uiaDoc);
    QVERIFY(!m_engine->uipDocument());
    QVERIFY(!m_engine->sceneManager());
}

void tst_Q3DSDocuments::testInvalidUip()
{
    Q3DSUipDocument uipDoc;
    uipDoc.setSourceData("INVALID DATA");
    m_engine->setDocument(uipDoc);
    QVERIFY(!m_engine->uipDocument());
    QVERIFY(!m_engine->sceneManager());
}

void tst_Q3DSDocuments::testUipFromSource()
{
    Q3DSUipDocument uipDoc;
    uipDoc.setSource(QLatin1String(":/data/modded_cube.uip"));
    m_engine->setDocument(uipDoc);
    QVERIFY(m_engine->presentation());
    QVERIFY(m_engine->sceneManager());
}

void tst_Q3DSDocuments::testUipFromSourceData()
{
    Q3DSUipDocument uipDoc;
    uipDoc.setSourceData(m_validUipDoc);
    m_engine->setDocument(uipDoc);
    QVERIFY(m_engine->presentation());
    QVERIFY(m_engine->sceneManager());
}

void tst_Q3DSDocuments::testUiaWithUip()
{
    Q3DSUiaDocument uiaDoc;
    Q3DSUipDocument uipDoc;
    uipDoc.setSourceData(m_validUipDoc);
    uiaDoc.addSubDocument(uipDoc);
    m_engine->setDocument(uiaDoc);
    QVERIFY(m_engine->presentation());
    QVERIFY(m_engine->sceneManager());
}

void tst_Q3DSDocuments::testUiaWith2Uip()
{
    Q3DSUiaDocument uiaDoc;
    Q3DSUipDocument uipDoc;
    uipDoc.setSourceData(m_validUipDoc);
    uipDoc.setId("uipSecond");
    uiaDoc.addSubDocument(uipDoc);
    Q3DSUipDocument uipDoc2;
    uipDoc2.setSourceData(m_validUipDoc);
    uipDoc2.setId("uipInitial");
    uiaDoc.setInitialDocumentId("uipInitial"); // Switch to first
    uiaDoc.addSubDocument(uipDoc2);
    m_engine->setDocument(uiaDoc);
    QVERIFY(m_engine->presentation());
    QVERIFY(m_engine->sceneManager());
    QVERIFY(m_engine->uipDocument(0)->id() == "uipInitial");
}

void tst_Q3DSDocuments::testUiaWith2Uip2()
{
    // One uip from data, other from file
    Q3DSUiaDocument uiaDoc;
    Q3DSUipDocument uipDoc;
    uipDoc.setSourceData(m_validUipDoc);
    uipDoc.setId("uipFromData");
    uiaDoc.addSubDocument(uipDoc);
    Q3DSUipDocument uipDoc2;
    uipDoc2.setSource(QLatin1String(":/data/modded_cube.uip"));
    uipDoc2.setId("uipFromFile");
    uiaDoc.addSubDocument(uipDoc2);
    m_engine->setDocument(uiaDoc);
    QVERIFY(m_engine->presentation());
    QVERIFY(m_engine->sceneManager());
    QVERIFY(m_engine->uipDocument(0)->id() == "uipFromData");
    QVERIFY(m_engine->uipDocument(1)->id() == "uipFromFile");
    QVERIFY(!m_engine->uipDocument(2));
}

void tst_Q3DSDocuments::testUiaWithQml()
{
    Q3DSUiaDocument uiaDoc;
    Q3DSUipDocument uipDoc;
    uipDoc.setSourceData(m_validUipDoc);
    uipDoc.setId("uipDoc");
    uiaDoc.addSubDocument(uipDoc);
    Q3DSQmlDocument qmlDoc;
    qmlDoc.setSourceData(m_validQmlDoc);
    qmlDoc.setId("qmlDoc");
    uiaDoc.addSubDocument(qmlDoc);
    QVERIFY(uiaDoc.qmlDocuments().size() == 1);
    QVERIFY(uiaDoc.uipDocuments().size() == 1);
    m_engine->setDocument(uiaDoc);
    QVERIFY(m_engine->presentation());
    QVERIFY(m_engine->sceneManager());
}

void tst_Q3DSDocuments::testUiaWithOnlyQml()
{
    Q3DSUiaDocument uiaDoc;
    Q3DSQmlDocument qmlDoc;
    qmlDoc.setSourceData(m_validQmlDoc);
    qmlDoc.setId("qmlDoc");
    uiaDoc.addSubDocument(qmlDoc);
    QVERIFY(uiaDoc.qmlDocuments().size() == 1);
    QVERIFY(uiaDoc.uipDocuments().size() == 0);
    m_engine->setDocument(uiaDoc);
    // Should fail, uia without uip doesn't work
    QVERIFY(!m_engine->uipDocument());
    QVERIFY(!m_engine->sceneManager());
}

#include <tst_q3dsdocuments.moc>
QTEST_MAIN(tst_Q3DSDocuments)
