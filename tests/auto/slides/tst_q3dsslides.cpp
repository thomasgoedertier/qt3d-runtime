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

#include <QtTest>

#include <private/q3dsengine_p.h>
#include <private/q3dswindow_p.h>
#include <private/q3dsutils_p.h>
#include <private/q3dsuippresentation_p.h>
#include <private/q3dsscenemanager_p.h>
#include <private/q3dsslideplayer_p.h>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QLayer>

#include "../shared/shared.h"

class tst_Q3DSSlides : public QObject
{
    Q_OBJECT

public:
    tst_Q3DSSlides();
    ~tst_Q3DSSlides();
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void setPresentationSlides();
    void presentationRollback();
    void setComponentSlides();
    void componentRollback();
    void setDeepComponentSlides();
    void deepComponentRollback();
    void setNonVisibleComponentSlides();
    void testTimeLineVisibility();

private:
    Q3DSModelNode *getModelWithName(const QString &name, Q3DSGraphObject *parent);
    Q3DSComponentNode *getComponentWithName(const QString &name, Q3DSGraphObject *parent);
    Q3DSTextNode *getTextNodeWithName(const QString &name, Q3DSGraphObject *parent);

    bool isNodeVisible(Q3DSNode *node);

    Q3DSEngine *m_engine = nullptr;
    Q3DSWindow *m_view = nullptr;
    Q3DSUipPresentation *m_presentation = nullptr;
    Q3DSSceneManager *m_sceneManager = nullptr;
    Q3DSScene *m_scene = nullptr;

    // Slides
    // Presentation Slides
    Q3DSSlide *m_presentationMasterSlide = nullptr;
    Q3DSSlide *m_presentationSlide1 = nullptr;
    Q3DSSlide *m_presentationSlide2 = nullptr;
    Q3DSSlide *m_presentationSlide3 = nullptr;
    Q3DSSlide *m_presentationSlide4 = nullptr;
    Q3DSSlide *m_presentationSlide5 = nullptr;

    // Slide 3 ComponentSlides
    Q3DSSlide *m_componentMasterSlide = nullptr;
    Q3DSSlide *m_componentSlide1 = nullptr;
    Q3DSSlide *m_componentSlide2 = nullptr;
    Q3DSSlide *m_componentSlide3 = nullptr;

    // Slide 5 ComponentSlides
    Q3DSSlide *m_componentSlide5MasterSlide = nullptr;
    Q3DSSlide *m_componentSlide51 = nullptr;
    Q3DSSlide *m_componentSlide52 = nullptr;
    Q3DSSlide *m_componentSlide53 = nullptr;

    // Deep Component Slides
    Q3DSSlide *m_deepComponentMasterSlide = nullptr;
    Q3DSSlide *m_deepComponentSlide1 = nullptr;
    Q3DSSlide *m_deepComponentSlide2 = nullptr;

    // Objects
    // Presentation Objects
    Q3DSModelNode *m_masterCylinder = nullptr;
    Q3DSModelNode *m_dynamicSphere = nullptr;
    Q3DSModelNode *m_slide1Rect = nullptr;
    Q3DSModelNode *m_slide2Sphere = nullptr;
    Q3DSComponentNode *m_slide3Component = nullptr;
    Q3DSModelNode *m_slide4Cone = nullptr;
    Q3DSModelNode *m_slide5Rect = nullptr;
    Q3DSModelNode *m_slide5Sphere = nullptr;
    Q3DSComponentNode *m_slide5Component = nullptr;

    // Component Objects
    Q3DSModelNode *m_componentMasterCube = nullptr;
    Q3DSModelNode *m_componentSlide1Cone = nullptr;
    Q3DSModelNode *m_componentMasterCubeSlide5 = nullptr;
    Q3DSModelNode *m_componentSlide5Slide1Cone = nullptr;
    Q3DSTextNode *m_componentSlide2Text = nullptr;
    Q3DSComponentNode *m_componentSlide3Component = nullptr;

    // Deep Component Objects
    Q3DSModelNode *m_deepComponentSlide1Cylinder = nullptr;
    Q3DSModelNode *m_deepComponentSlide2Sphere = nullptr;
    Q3DSModelNode *m_deepComponentSlide2Moon = nullptr;
    Q3DSTextNode *m_deepComponentMasterText = nullptr;

};

tst_Q3DSSlides::tst_Q3DSSlides()
{
}

tst_Q3DSSlides::~tst_Q3DSSlides()
{
    delete m_engine;
    delete m_view;
}

void tst_Q3DSSlides::initTestCase()
{
    if (!isOpenGLGoodEnough())
        QSKIP("This platform does not support OpenGL proper");

    QSurfaceFormat::setDefaultFormat(Q3DS::surfaceFormat());
    m_engine = new Q3DSEngine;
    m_view = new Q3DSWindow;
    m_view->setEngine(m_engine);
    m_view->forceResize(640, 480);

    Q3DSUtils::setDialogsEnabled(false);
    // This tests the basic Presentation (top level) slide change
    m_view->engine()->setSource(QLatin1String(":/test3.uip"));
    QVERIFY(m_view->engine()->presentation());
    QVERIFY(m_view->engine()->sceneManager());

    m_presentation = m_view->engine()->presentation();
    m_sceneManager = m_view->engine()->sceneManager();
    m_scene = m_presentation->scene();

    // Presentation Slides
    m_presentationMasterSlide = m_presentation->masterSlide();
    QVERIFY(m_presentationMasterSlide->childCount() == 5);
    m_presentationSlide1 = static_cast<Q3DSSlide *>(m_presentationMasterSlide->firstChild());
    QVERIFY(m_presentationSlide1);
    m_presentationSlide2 = static_cast<Q3DSSlide *>(m_presentationSlide1->nextSibling());
    QVERIFY(m_presentationSlide2);
    m_presentationSlide3 = static_cast<Q3DSSlide *>(m_presentationSlide2->nextSibling());
    QVERIFY(m_presentationSlide3);
    m_presentationSlide4 = static_cast<Q3DSSlide *>(m_presentationSlide3->nextSibling());
    QVERIFY(m_presentationSlide4);
    m_presentationSlide5 = static_cast<Q3DSSlide *>(m_presentationSlide4->nextSibling());
    QVERIFY(m_presentationSlide5);

    // Presentation Objects
    auto sceneLayer = m_scene->firstChild();
    QVERIFY(sceneLayer->childCount() == 11);
    m_masterCylinder = getModelWithName(QStringLiteral("MasterCylinder"), sceneLayer);
    QVERIFY(m_masterCylinder);
    m_dynamicSphere = getModelWithName(QStringLiteral("DynamicSphere"), sceneLayer);
    QVERIFY(m_dynamicSphere);
    m_slide1Rect = getModelWithName(QStringLiteral("Slide1Rect"), sceneLayer);
    QVERIFY(m_slide1Rect);
    m_slide2Sphere = getModelWithName(QStringLiteral("Slide2Sphere"), sceneLayer);
    QVERIFY(m_slide2Sphere);
    m_slide3Component = getComponentWithName(QStringLiteral("Slide3Component"), sceneLayer);
    QVERIFY(m_slide3Component);
    m_slide4Cone = getModelWithName(QStringLiteral("Slide4Cone"), sceneLayer);
    QVERIFY(m_slide4Cone);
    m_slide5Rect = getModelWithName(QStringLiteral("Slide5Rect"), sceneLayer);
    QVERIFY(m_slide5Rect);
    m_slide5Sphere = getModelWithName(QStringLiteral("Slide5Sphere"), sceneLayer);
    QVERIFY(m_slide5Sphere);
    m_slide5Component = getComponentWithName(QStringLiteral("Slide5Component"), sceneLayer);
    QVERIFY(m_slide5Component);

    // Component Slides
    m_componentMasterSlide = m_slide3Component->masterSlide();
    QVERIFY(m_componentMasterSlide);
    m_componentSlide1 = static_cast<Q3DSSlide *>(m_componentMasterSlide->firstChild());
    QVERIFY(m_componentSlide1);
    m_componentSlide2 = static_cast<Q3DSSlide *>(m_componentSlide1->nextSibling());
    QVERIFY(m_componentSlide2);
    m_componentSlide3 = static_cast<Q3DSSlide *>(m_componentSlide2->nextSibling());
    QVERIFY(m_componentSlide3);

    m_componentSlide5MasterSlide = m_slide5Component->masterSlide();
    QVERIFY(m_componentSlide5MasterSlide);
    m_componentSlide51 = static_cast<Q3DSSlide *>(m_componentSlide5MasterSlide->firstChild());
    QVERIFY(m_componentSlide51);
    m_componentSlide52 = static_cast<Q3DSSlide *>(m_componentSlide51->nextSibling());
    QVERIFY(m_componentSlide52);
    m_componentSlide53 = static_cast<Q3DSSlide *>(m_componentSlide52->nextSibling());
    QVERIFY(m_componentSlide53);

    // Component Objects
    m_componentMasterCube = getModelWithName(QStringLiteral("ComponentMasterCube"), m_slide3Component);
    QVERIFY(m_componentMasterCube);
    m_componentSlide1Cone = getModelWithName(QStringLiteral("ComponentSlide1Cone"), m_componentMasterCube);
    QVERIFY(m_componentSlide1Cone);
    m_componentSlide2Text = getTextNodeWithName(QStringLiteral("ComponentSlide2Text"), m_slide3Component);
    QVERIFY(m_componentSlide2Text);
    m_componentSlide3Component = getComponentWithName(QStringLiteral("ComponentSlide3Component"), m_slide3Component);
    QVERIFY(m_componentSlide3Component);
    m_componentMasterCubeSlide5 = getModelWithName(QStringLiteral("ComponentMasterCube"), m_slide5Component);
    QVERIFY(m_componentMasterCubeSlide5);
    m_componentSlide5Slide1Cone = getModelWithName(QStringLiteral("ComponentSlide1Cone"), m_componentMasterCubeSlide5);
    QVERIFY(m_componentSlide5Slide1Cone);

    // Deep Component Slides
    m_deepComponentMasterSlide = m_componentSlide3Component->masterSlide();
    QVERIFY(m_deepComponentMasterSlide);
    m_deepComponentSlide1 = static_cast<Q3DSSlide *>(m_deepComponentMasterSlide->firstChild());
    QVERIFY(m_deepComponentSlide1);
    m_deepComponentSlide2 = static_cast<Q3DSSlide *>(m_deepComponentSlide1->nextSibling());
    QVERIFY(m_deepComponentSlide2);

    // Deep Component Objects
    m_deepComponentSlide1Cylinder = getModelWithName(QStringLiteral("DeepComponentSlide1Cylinder"), m_componentSlide3Component);
    QVERIFY(m_deepComponentSlide1Cylinder);
    m_deepComponentSlide2Sphere = getModelWithName(QStringLiteral("DeepComponentSlide2Sphere"), m_componentSlide3Component);
    QVERIFY(m_deepComponentSlide2Sphere);
    m_deepComponentSlide2Moon = getModelWithName(QStringLiteral("DeepComponentSlide2Moon"), m_deepComponentSlide2Sphere);
    QVERIFY(m_deepComponentSlide2Moon);
    m_deepComponentMasterText = getTextNodeWithName(QStringLiteral("DeepComponentMasterText"), m_componentSlide3Component);
    QVERIFY(m_deepComponentMasterText);

    m_view->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_view));
}

void tst_Q3DSSlides::cleanupTestCase()
{
    if (m_view)
        m_view->close();
}

void tst_Q3DSSlides::setPresentationSlides()
{
    QVERIFY(m_sceneManager->currentSlide() == m_presentationSlide1);
    // For the first check we have to wait from the first frame update
    QSignalSpy m_updateSpy(m_engine, SIGNAL(nextFrameStarting()));
    m_updateSpy.wait(30);

    Q3DSSlidePlayer *player = m_sceneManager->slidePlayer();
    player->stop();
    QVERIFY(player);

    // Check starting state
    QCOMPARE(player->duration(), 10000);
    // MasterCylinder should be visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    // "Slide 1 Rect" should be visible
    QVERIFY(isNodeVisible(m_slide1Rect));
    QVERIFY(!isNodeVisible(m_slide4Cone));

    // Set second slide
    m_sceneManager->setCurrentSlide(m_presentationSlide2, true);
    // Verify second slide state
    QCOMPARE(player->duration(), 10000);
    // MasterCylinder should be visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    // "Slide 2 Sphere" should be visible
    QVERIFY(isNodeVisible(m_slide2Sphere));
    // "Slide 1 Rect" should not be visible
    QVERIFY(!isNodeVisible(m_slide1Rect));

    // Go back to first slide
    m_sceneManager->setCurrentSlide(m_presentationSlide1, true);
    // Check first slide state
    QCOMPARE(player->duration(), 10000);
    // MasterCylinder should be visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    // "Slide 1 Rect" should be visible
    QVERIFY(isNodeVisible(m_slide1Rect));
    QVERIFY(!isNodeVisible(m_slide4Cone));
    // "Slide 2 Sphere" should not be visible
    QVERIFY(!isNodeVisible(m_slide2Sphere));

    // Set 3rd Slide (with Component)
    m_sceneManager->setCurrentSlide(m_presentationSlide3, true);
    QCOMPARE(player->duration(), 10000);
    // MasterCylinder should be visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    // Component Should be visible
    QVERIFY(isNodeVisible(m_slide3Component));
    // "Slide 2 Sphere" should not be visible
    QVERIFY(!isNodeVisible(m_slide2Sphere));
    // "Slide 1 Rect" should not be visible
    QVERIFY(!isNodeVisible(m_slide1Rect));

    // Set 4th Slide (from Component)
    m_sceneManager->setCurrentSlide(m_presentationSlide4, true);
    QCOMPARE(player->duration(), 12028);
    // MasterCylinder should be visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    // Component Should not be visible
    QVERIFY(!isNodeVisible(m_slide3Component));
    // "Slide 2 Sphere" should not be visible
    QVERIFY(!isNodeVisible(m_slide2Sphere));
    // "Slide 1 Rect" should not be visible
    QVERIFY(!isNodeVisible(m_slide1Rect));
    // Slide 4 Cone should be visible
    QVERIFY(isNodeVisible(m_slide4Cone));

    // Set the same slide again
    m_sceneManager->setCurrentSlide(m_presentationSlide4, true);
    QCOMPARE(player->duration(), 12028);
    // MasterCylinder should be visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    // Component Should not be visible
    QVERIFY(!isNodeVisible(m_slide3Component));
    // "Slide 2 Sphere" should not be visible
    QVERIFY(!isNodeVisible(m_slide2Sphere));
    // "Slide 1 Rect" should not be visible
    QVERIFY(!isNodeVisible(m_slide1Rect));
    // Slide 4 Cone should be visible
    QVERIFY(isNodeVisible(m_slide4Cone));
}

void tst_Q3DSSlides::presentationRollback()
{
    // Go back to first slide
    m_sceneManager->setCurrentSlide(m_presentationSlide1, true);

    // DynamicSphere exists on the master slide (visible by default)
    // DynamicSphere has eyeball set to false on Slides 2 and 4
    QVERIFY(isNodeVisible(m_dynamicSphere));

    // Go to Second Slide
    m_sceneManager->setCurrentSlide(m_presentationSlide2, true);
    QVERIFY(!isNodeVisible(m_dynamicSphere));

    // Go to Third Slide (requires rollback)
    m_sceneManager->setCurrentSlide(m_presentationSlide3, true);
    QVERIFY(isNodeVisible(m_dynamicSphere));

    // Go to Forth Slide
    m_sceneManager->setCurrentSlide(m_presentationSlide4, true);
    QVERIFY(!isNodeVisible(m_dynamicSphere));

    // Go to another slide with eyeball false (no rollback)
    m_sceneManager->setCurrentSlide(m_presentationSlide2, true);
    QVERIFY(!isNodeVisible(m_dynamicSphere));

    // Make sure rollback still works
    m_sceneManager->setCurrentSlide(m_presentationSlide1, true);
    QVERIFY(isNodeVisible(m_dynamicSphere));

}

void tst_Q3DSSlides::setComponentSlides()
{
    // Set the presentation slide to Slide3 (contains component)
    m_sceneManager->setCurrentSlide(m_presentationSlide3, true);
    QVERIFY(isNodeVisible(m_slide3Component));

    // Initial slide should be m_componentSlide1
    QVERIFY(isNodeVisible(m_componentMasterCube));
    QVERIFY(isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));

    // Switch to Component Slide 2 (text)
    m_sceneManager->setComponentCurrentSlide(m_componentSlide2, true);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));

    // Switch back to Component Slide 1
    m_sceneManager->setComponentCurrentSlide(m_componentSlide1, true);
    QVERIFY(isNodeVisible(m_componentMasterCube));
    QVERIFY(isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));

    // Switch to Component Slide 3 (deep component)
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3, true);
    QVERIFY(isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(isNodeVisible(m_componentSlide3Component));

    // Switch to same slide
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3, true);
    QVERIFY(isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(isNodeVisible(m_componentSlide3Component));
}

void tst_Q3DSSlides::componentRollback()
{
    // Set the presentation slide to Slide3 (contains component)
    m_sceneManager->setCurrentSlide(m_presentationSlide3, true);
    QVERIFY(isNodeVisible(m_slide3Component));

    // Set first slide which contains "Master Cube"
    m_sceneManager->setComponentCurrentSlide(m_componentSlide1, true);
    QVERIFY(isNodeVisible(m_componentMasterCube));

    // Second slide sets Master Cube eyeball to false
    m_sceneManager->setComponentCurrentSlide(m_componentSlide2, true);
    QVERIFY(!isNodeVisible(m_componentMasterCube));

    // Switch back to first slide (requires rollback)
    m_sceneManager->setComponentCurrentSlide(m_componentSlide1, true);
    QVERIFY(isNodeVisible(m_componentMasterCube));

    // Move to slide 3 (no change)
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3, true);
    QVERIFY(isNodeVisible(m_componentMasterCube));
}

void tst_Q3DSSlides::setDeepComponentSlides()
{
    // Deep Component is a nested component Slide
    // PresentationSlide3 -> ComponentSlide3 -> DeepComponentSlide[n]
    // Set the presentation slide to Slide3 (contains component)
    m_sceneManager->setCurrentSlide(m_presentationSlide3, true);
    QVERIFY(isNodeVisible(m_slide3Component));
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3, true);
    QVERIFY(isNodeVisible(m_componentSlide3Component));

    // At this point the first slide of deep component should be active
    QVERIFY(m_componentSlide3Component->currentSlide() == m_deepComponentSlide1);
    QVERIFY(isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));

    // Switch to second deep component slide
    m_sceneManager->setComponentCurrentSlide(m_deepComponentSlide2, true);
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(isNodeVisible(m_deepComponentSlide2Moon));

    // Switch back to deep component first slide
    m_sceneManager->setComponentCurrentSlide(m_deepComponentSlide1, true);
    QVERIFY(isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));

    // Set same slide again
    m_sceneManager->setComponentCurrentSlide(m_deepComponentSlide1, true);
    QVERIFY(isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
}

void tst_Q3DSSlides::deepComponentRollback()
{
    // Deep Component is a nested component Slide
    // PresentationSlide3 -> ComponentSlide3 -> DeepComponentSlide[n]
    // Set the presentation slide to Slide3 (contains component)
    m_sceneManager->setCurrentSlide(m_presentationSlide3, true);
    QVERIFY(isNodeVisible(m_slide3Component));
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3, true);
    QVERIFY(isNodeVisible(m_componentSlide3Component));

    // First slide has "master text" active
    m_sceneManager->setComponentCurrentSlide(m_deepComponentSlide1, true);
    QVERIFY(isNodeVisible(m_deepComponentMasterText));

    // Second slide has eyeball set to false for master text
    m_sceneManager->setComponentCurrentSlide(m_deepComponentSlide2, true);
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    // Switch back to first slide to perform rollback
    m_sceneManager->setComponentCurrentSlide(m_deepComponentSlide1, true);
    QVERIFY(isNodeVisible(m_deepComponentMasterText));
}

void tst_Q3DSSlides::setNonVisibleComponentSlides()
{
    // It is possible to change component slides even though components
    // are not visible.  This is useful because when you do switch to a
    // slide where components contents are shown they will be in the
    // correct state. We have to make sure that changing component slides
    // does not have un-intended side effects for the active slides.

    // Select a presentation slide that does not have a component
    m_sceneManager->setCurrentSlide(m_presentationSlide1, true);
    QVERIFY(!isNodeVisible(m_slide3Component));

    // m_slide3Component is only visible on m_presentationSlide3
    // change m_slide3Component's slides and make sure the contents
    // do not become visible.
    m_sceneManager->setComponentCurrentSlide(m_componentSlide1, true);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    m_sceneManager->setComponentCurrentSlide(m_componentSlide2, true);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    m_sceneManager->setComponentCurrentSlide(m_componentSlide3, true);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    // also change deep compoents slides (now active in component slide3)
    m_sceneManager->setComponentCurrentSlide(m_deepComponentSlide1, true);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    m_sceneManager->setComponentCurrentSlide(m_deepComponentSlide2, true);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    // Now make component visible by switching to slide 3
    m_sceneManager->setCurrentSlide(m_presentationSlide3, true);
    QVERIFY(isNodeVisible(m_slide3Component));
    QVERIFY(isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    // make sure that they go away again
    m_sceneManager->setCurrentSlide(m_presentationSlide1, true);
    QVERIFY(!isNodeVisible(m_slide3Component));
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));
}

void tst_Q3DSSlides::testTimeLineVisibility()
{
    // Select a presentation slide that has items with different starttimes
    // and endtimes than the slide they are on
    m_sceneManager->setCurrentSlide(m_presentationSlide5, true);

    QVERIFY(isNodeVisible(m_masterCylinder));
    QVERIFY(isNodeVisible(m_dynamicSphere));
    QVERIFY(!isNodeVisible(m_slide5Rect));
    QVERIFY(isNodeVisible(m_slide5Sphere));

    Q3DSSlidePlayer *player = m_sceneManager->slidePlayer();
    player->stop();
    QVERIFY(player);

    QSignalSpy updateSpy(m_engine, SIGNAL(nextFrameStarting()));
    QSignalSpy positionChangedSpy(player, &Q3DSSlidePlayer::positionChanged);

    const auto seekAndWait = [player, &updateSpy, &positionChangedSpy](int value) {
        const float t = value;
        player->seek(t);
        QVERIFY(positionChangedSpy.wait(5000));
        QVERIFY(updateSpy.wait(5000));
    };

    m_sceneManager->setComponentCurrentSlide(m_componentSlide51);
    Q_ASSERT(m_componentSlide5MasterSlide == m_slide5Component->masterSlide());
    Q3DSSlidePlayer *compPlayer = m_componentSlide5MasterSlide->attached<Q3DSSlideAttached>()->slidePlayer;
    compPlayer->stop();
    QVERIFY(compPlayer);

    QSignalSpy compPositionChangedSpy(compPlayer, &Q3DSSlidePlayer::positionChanged);

    const auto seekAndWaitForComp = [compPlayer, &updateSpy, &compPositionChangedSpy](int value) {
        const float t = value;
        compPlayer->seek(t);
        QVERIFY(compPositionChangedSpy.wait(5000));
        QVERIFY(updateSpy.wait(5000));
    };

    seekAndWait(999);

    // Still invisible
    QVERIFY(isNodeVisible(m_masterCylinder));
    QVERIFY(isNodeVisible(m_dynamicSphere));
    QVERIFY(!isNodeVisible(m_slide5Rect));
    QVERIFY(isNodeVisible(m_slide5Sphere));
    QVERIFY(!isNodeVisible(m_slide5Component));
    QVERIFY(!isNodeVisible(m_componentMasterCubeSlide5));

    seekAndWait(1000);

    // Slide 5 component becomes visible, but not components objects
    QVERIFY(isNodeVisible(m_masterCylinder));
    QVERIFY(isNodeVisible(m_dynamicSphere));
    QVERIFY(isNodeVisible(m_slide5Rect));
    QVERIFY(isNodeVisible(m_slide5Sphere));
    QVERIFY(isNodeVisible(m_slide5Component));
    QVERIFY(!isNodeVisible(m_componentMasterCubeSlide5));

    seekAndWait(1500);
    // Sync up the comp player
    seekAndWaitForComp(1500);

    // Everything now visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    QVERIFY(isNodeVisible(m_dynamicSphere));
    QVERIFY(isNodeVisible(m_slide5Rect));
    QVERIFY(isNodeVisible(m_slide5Sphere));
    QVERIFY(isNodeVisible(m_slide5Component));
    QVERIFY(isNodeVisible(m_componentMasterCubeSlide5));

    // Verify a positionChanged signal coming from component slide player
    seekAndWaitForComp(1501);

    seekAndWait(2000);
    // Sync up the comp player
    seekAndWaitForComp(2000);

    // Everything still visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    QVERIFY(isNodeVisible(m_dynamicSphere));
    QVERIFY(isNodeVisible(m_slide5Rect));
    QVERIFY(isNodeVisible(m_slide5Sphere));
    QVERIFY(isNodeVisible(m_slide5Component));
    QVERIFY(isNodeVisible(m_componentMasterCubeSlide5));

    seekAndWait(2001);
    // Sync up the comp player
    seekAndWaitForComp(2001);

    // Neither rect nor sphere are visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    QVERIFY(isNodeVisible(m_dynamicSphere));
    QVERIFY(!isNodeVisible(m_slide5Rect));
    QVERIFY(!isNodeVisible(m_slide5Sphere));
    QVERIFY(isNodeVisible(m_slide5Component));
    QVERIFY(isNodeVisible(m_componentMasterCubeSlide5));

    seekAndWait(2885);
    // Sync up the comp player
    seekAndWaitForComp(2885);

    // Neither rect nor sphere are visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    QVERIFY(isNodeVisible(m_dynamicSphere));
    QVERIFY(!isNodeVisible(m_slide5Rect));
    QVERIFY(!isNodeVisible(m_slide5Sphere));
    QVERIFY(!isNodeVisible(m_slide5Component));
    QVERIFY(!isNodeVisible(m_componentMasterCubeSlide5));

    // Now test time lines that are out of sync
    // Start by a know state with all visible
    seekAndWait(1500);
    // Sync up the comp player
    seekAndWaitForComp(1500);

    // Everything now visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    QVERIFY(isNodeVisible(m_dynamicSphere));
    QVERIFY(isNodeVisible(m_slide5Rect));
    QVERIFY(isNodeVisible(m_slide5Sphere));
    QVERIFY(isNodeVisible(m_slide5Component));
    QVERIFY(isNodeVisible(m_componentMasterCubeSlide5));

    seekAndWaitForComp(0);
    QVERIFY(isNodeVisible(m_slide5Component));
    QVERIFY(!isNodeVisible(m_componentMasterCubeSlide5));
    QVERIFY(!isNodeVisible(m_componentSlide5Slide1Cone));

    seekAndWaitForComp(1300);
    QVERIFY(isNodeVisible(m_slide5Component));
    QVERIFY(isNodeVisible(m_componentMasterCubeSlide5));
    QVERIFY(isNodeVisible(m_componentSlide5Slide1Cone));

    seekAndWait(3000);
    QVERIFY(!isNodeVisible(m_slide5Component));
    // Component nodes should be hidden by parent.
    QVERIFY(!isNodeVisible(m_componentMasterCubeSlide5));
    QVERIFY(!isNodeVisible(m_componentSlide5Slide1Cone));

    seekAndWait(1500);
    QVERIFY(isNodeVisible(m_slide5Component));
    // Component nodes should be made visible by parent.
    QVERIFY(isNodeVisible(m_componentMasterCubeSlide5));
    QVERIFY(isNodeVisible(m_componentSlide5Slide1Cone));

    seekAndWait(3000);
    QVERIFY(!isNodeVisible(m_slide5Component));
    // Make the parent make the component nodes hidden again
    QVERIFY(!isNodeVisible(m_componentMasterCubeSlide5));
    QVERIFY(!isNodeVisible(m_componentSlide5Slide1Cone));

    seekAndWaitForComp(1000);
    // Component should not make its nodes visible.
    QVERIFY(!isNodeVisible(m_componentMasterCubeSlide5));
    QVERIFY(!isNodeVisible(m_componentSlide5Slide1Cone));

    seekAndWaitForComp(100);
    // Hidden by component
    QVERIFY(!isNodeVisible(m_componentMasterCubeSlide5));
    QVERIFY(!isNodeVisible(m_componentSlide5Slide1Cone));

    seekAndWait(1500);
    // ... Now make sure the parent doesn't make the component nodes visible!
    QVERIFY(!isNodeVisible(m_componentMasterCubeSlide5));
    QVERIFY(!isNodeVisible(m_componentSlide5Slide1Cone));

    seekAndWait(0);
    // Sync up the comp player
    seekAndWaitForComp(0);

    // Back to the beginning
    QVERIFY(isNodeVisible(m_masterCylinder));
    QVERIFY(isNodeVisible(m_dynamicSphere));
    QVERIFY(!isNodeVisible(m_slide5Rect));
    QVERIFY(isNodeVisible(m_slide5Sphere));
    QVERIFY(!isNodeVisible(m_slide5Component));
    QVERIFY(!isNodeVisible(m_componentMasterCubeSlide5));
}

Q3DSModelNode *tst_Q3DSSlides::getModelWithName(const QString &name, Q3DSGraphObject *parent)
{
    Q3DSGraphObject *n = parent->firstChild();
    while (n) {
        if (n->type() == Q3DSGraphObject::Model) {
            auto model = static_cast<Q3DSModelNode *>(n);
            if (model->name() == name)
                return model;
        }
        n = n->nextSibling();
    }
    return nullptr;
}

Q3DSComponentNode *tst_Q3DSSlides::getComponentWithName(const QString &name, Q3DSGraphObject *parent)
{
    Q3DSGraphObject *n = parent->firstChild();
    while (n) {
        if (n->type() == Q3DSGraphObject::Component) {
            auto component = static_cast<Q3DSComponentNode *>(n);
            if (component->name() == name)
                return component;
        }
        n = n->nextSibling();
    }
    return nullptr;
}

Q3DSTextNode *tst_Q3DSSlides::getTextNodeWithName(const QString &name, Q3DSGraphObject *parent)
{
    Q3DSGraphObject *n = parent->firstChild();
    while (n) {
        if (n->type() == Q3DSGraphObject::Text) {
            auto text = static_cast<Q3DSTextNode *>(n);
            if (text->name() == name)
                return text;
        }
        n = n->nextSibling();
    }
    return nullptr;
}

bool tst_Q3DSSlides::isNodeVisible(Q3DSNode *node)
{
    if (!node)
        return false;

    auto nodeAttached = node->attached<Q3DSNodeAttached>();
    auto entity = nodeAttached->entity;
    if (!entity)
        return false;

    auto layerAttached = nodeAttached->layer3DS->attached<Q3DSLayerAttached>();
    Q_ASSERT(layerAttached->opaqueTag || layerAttached->transparentTag);

    const bool visible = (nodeAttached->visibilityTag == Q3DSGraphObjectAttached::Visible);
    // These should be in sync, or we're in trouble.
    Q_ASSERT(visible == (entity->components().contains(layerAttached->opaqueTag) || entity->components().contains(layerAttached->transparentTag)));

    return visible;
}

QTEST_MAIN(tst_Q3DSSlides);

#include "tst_q3dsslides.moc"
