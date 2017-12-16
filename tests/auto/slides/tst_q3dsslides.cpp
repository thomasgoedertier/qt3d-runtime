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


#include <Qt3DStudioRuntime2/Q3DSUtils>
#include <Qt3DStudioRuntime2/Q3DStudioWindow>
#include <Qt3DStudioRuntime2/Q3DSPresentation>
#include <Qt3DStudioRuntime2/Q3DSSceneManager>
#include <Qt3DCore/QEntity>
#include <Qt3DRender/QLayer>

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

private:
    Q3DSModelNode *getModelWithName(const QString &name, Q3DSGraphObject *parent);
    Q3DSComponentNode *getComponentWithName(const QString &name, Q3DSGraphObject *parent);
    Q3DSTextNode *getTextNodeWithName(const QString &name, Q3DSGraphObject *parent);

    bool isNodeVisible(Q3DSNode *node);

    Q3DStudioWindow *m_view = nullptr;
    Q3DSPresentation *m_presentation = nullptr;
    Q3DSSceneManager *m_sceneManager = nullptr;
    Q3DSScene *m_scene = nullptr;

    // Slides
    // Presentation Slides
    Q3DSSlide *m_presentationMasterSlide = nullptr;
    Q3DSSlide *m_presentationSlide1 = nullptr;
    Q3DSSlide *m_presentationSlide2 = nullptr;
    Q3DSSlide *m_presentationSlide3 = nullptr;
    Q3DSSlide *m_presentationSlide4 = nullptr;

    // Slide 3 ComponentSlides
    Q3DSSlide *m_componentMasterSlide = nullptr;
    Q3DSSlide *m_componentSlide1 = nullptr;
    Q3DSSlide *m_componentSlide2 = nullptr;
    Q3DSSlide *m_componentSlide3 = nullptr;

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

    // Component Objects
    Q3DSModelNode *m_componentMasterCube = nullptr;
    Q3DSModelNode *m_componentSlide1Cone = nullptr;
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
    Q3DStudioWindow::initStaticPreApp();
    Q3DStudioWindow::initStaticPostApp();
    m_view = new Q3DStudioWindow();
    m_view->resize(640, 480);
}

tst_Q3DSSlides::~tst_Q3DSSlides()
{
    delete m_view;
}

void tst_Q3DSSlides::initTestCase()
{
    Q3DSUtils::setDialogsEnabled(false);
    // This tests the basic Presentation (top level) slide change
    m_view->setSource(QLatin1String(":/test3.uip"));
    QVERIFY(m_view->uipDocument()->presentation());
    QVERIFY(m_view->sceneManager());

    m_presentation = m_view->uipDocument()->presentation();
    m_sceneManager = m_view->sceneManager();
    m_scene = m_presentation->scene();

    // Presentation Slides
    m_presentationMasterSlide = m_presentation->masterSlide();
    QVERIFY(m_presentationMasterSlide->childCount() == 4);
    m_presentationSlide1 = static_cast<Q3DSSlide *>(m_presentationMasterSlide->firstChild());
    QVERIFY(m_presentationSlide1);
    m_presentationSlide2 = static_cast<Q3DSSlide *>(m_presentationSlide1->nextSibling());
    QVERIFY(m_presentationSlide2);
    m_presentationSlide3 = static_cast<Q3DSSlide *>(m_presentationSlide2->nextSibling());
    QVERIFY(m_presentationSlide3);
    m_presentationSlide4 = static_cast<Q3DSSlide *>(m_presentationSlide3->nextSibling());
    QVERIFY(m_presentationSlide4);

    // Presentation Objects
    auto sceneLayer = m_scene->firstChild();
    QVERIFY(sceneLayer->childCount() == 8);
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

    // Component Slides
    m_componentMasterSlide = m_slide3Component->masterSlide();
    QVERIFY(m_componentMasterSlide);
    m_componentSlide1 = static_cast<Q3DSSlide *>(m_componentMasterSlide->firstChild());
    QVERIFY(m_componentSlide1);
    m_componentSlide2 = static_cast<Q3DSSlide *>(m_componentSlide1->nextSibling());
    QVERIFY(m_componentSlide2);
    m_componentSlide3 = static_cast<Q3DSSlide *>(m_componentSlide2->nextSibling());
    QVERIFY(m_componentSlide3);

    // Component Objects
    m_componentMasterCube = getModelWithName(QStringLiteral("ComponentMasterCube"), m_slide3Component);
    QVERIFY(m_componentMasterCube);
    m_componentSlide1Cone = getModelWithName(QStringLiteral("ComponentSlide1Cone"), m_componentMasterCube);
    QVERIFY(m_componentSlide1Cone);
    m_componentSlide2Text = getTextNodeWithName(QStringLiteral("ComponentSlide2Text"), m_slide3Component);
    QVERIFY(m_componentSlide2Text);
    m_componentSlide3Component = getComponentWithName(QStringLiteral("ComponentSlide3Component"), m_slide3Component);
    QVERIFY(m_componentSlide3Component);

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
    m_view->close();
}

void tst_Q3DSSlides::setPresentationSlides()
{
    QVERIFY(m_sceneManager->currentSlide() == m_presentationSlide1);
    // For the first check we have to wait from the first frame update
    QSignalSpy m_updateSpy(m_view, SIGNAL(sceneUpdated()));
    m_updateSpy.wait(30);

    // Check starting state
    // MasterCylinder should be visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    // "Slide 1 Rect" should be visible
    QVERIFY(isNodeVisible(m_slide1Rect));
    QVERIFY(!isNodeVisible(m_slide4Cone));

    // Set second slide
    m_sceneManager->setCurrentSlide(m_presentationSlide2);
    // Verify second slide state
    // MasterCylinder should be visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    // "Slide 2 Sphere" should be visible
    QVERIFY(isNodeVisible(m_slide2Sphere));
    // "Slide 1 Rect" should not be visible
    QVERIFY(!isNodeVisible(m_slide1Rect));

    // Go back to first slide
    m_sceneManager->setCurrentSlide(m_presentationSlide1);
    // Check first slide state
    // MasterCylinder should be visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    // "Slide 1 Rect" should be visible
    QVERIFY(isNodeVisible(m_slide1Rect));
    QVERIFY(!isNodeVisible(m_slide4Cone));
    // "Slide 2 Sphere" should not be visible
    QVERIFY(!isNodeVisible(m_slide2Sphere));

    // Set 3rd Slide (with Component)
    m_sceneManager->setCurrentSlide(m_presentationSlide3);
    // MasterCylinder should be visible
    QVERIFY(isNodeVisible(m_masterCylinder));
    // Component Should be visible
    QVERIFY(isNodeVisible(m_slide3Component));
    // "Slide 2 Sphere" should not be visible
    QVERIFY(!isNodeVisible(m_slide2Sphere));
    // "Slide 1 Rect" should not be visible
    QVERIFY(!isNodeVisible(m_slide1Rect));

    // Set 4th Slide (from Component)
    m_sceneManager->setCurrentSlide(m_presentationSlide4);
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
    m_sceneManager->setCurrentSlide(m_presentationSlide4);
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
    m_sceneManager->setCurrentSlide(m_presentationSlide1);

    // DynamicSphere exists on the master slide (visible by default)
    // DynamicSphere has eyeball set to false on Slides 2 and 4
    QVERIFY(isNodeVisible(m_dynamicSphere));

    // Go to Second Slide
    m_sceneManager->setCurrentSlide(m_presentationSlide2);
    QVERIFY(!isNodeVisible(m_dynamicSphere));

    // Go to Third Slide (requires rollback)
    m_sceneManager->setCurrentSlide(m_presentationSlide3);
    QVERIFY(isNodeVisible(m_dynamicSphere));

    // Go to Forth Slide
    m_sceneManager->setCurrentSlide(m_presentationSlide4);
    QVERIFY(!isNodeVisible(m_dynamicSphere));

    // Go to another slide with eyeball false (no rollback)
    m_sceneManager->setCurrentSlide(m_presentationSlide2);
    QVERIFY(!isNodeVisible(m_dynamicSphere));

    // Make sure rollback still works
    m_sceneManager->setCurrentSlide(m_presentationSlide1);
    QVERIFY(isNodeVisible(m_dynamicSphere));

}

void tst_Q3DSSlides::setComponentSlides()
{
    // Set the presentation slide to Slide3 (contains component)
    m_sceneManager->setCurrentSlide(m_presentationSlide3);
    QVERIFY(isNodeVisible(m_slide3Component));

    // Initial slide should be m_componentSlide1
    QVERIFY(isNodeVisible(m_componentMasterCube));
    QVERIFY(isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));

    // Switch to Component Slide 2 (text)
    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide2);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));

    // Switch back to Component Slide 1
    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide1);
    QVERIFY(isNodeVisible(m_componentMasterCube));
    QVERIFY(isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));

    // Switch to Component Slide 3 (deep component)
    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide3);
    QVERIFY(isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(isNodeVisible(m_componentSlide3Component));

    // Switch to same slide
    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide3);
    QVERIFY(isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(isNodeVisible(m_componentSlide3Component));
}

void tst_Q3DSSlides::componentRollback()
{
    // Set the presentation slide to Slide3 (contains component)
    m_sceneManager->setCurrentSlide(m_presentationSlide3);
    QVERIFY(isNodeVisible(m_slide3Component));

    // Set first slide which contains "Master Cube"
    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide1);
    QVERIFY(isNodeVisible(m_componentMasterCube));

    // Second slide sets Master Cube eyeball to false
    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide2);
    QVERIFY(!isNodeVisible(m_componentMasterCube));

    // Switch back to first slide (requires rollback)
    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide1);
    QVERIFY(isNodeVisible(m_componentMasterCube));

    // Move to slide 3 (no change)
    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide3);
    QVERIFY(isNodeVisible(m_componentMasterCube));
}

void tst_Q3DSSlides::setDeepComponentSlides()
{
    // Deep Component is a nested component Slide
    // PresentationSlide3 -> ComponentSlide3 -> DeepComponentSlide[n]
    // Set the presentation slide to Slide3 (contains component)
    m_sceneManager->setCurrentSlide(m_presentationSlide3);
    QVERIFY(isNodeVisible(m_slide3Component));
    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide3);
    QVERIFY(isNodeVisible(m_componentSlide3Component));

    // At this point the first slide of deep component should be active
    QVERIFY(m_componentSlide3Component->currentSlide() == m_deepComponentSlide1);
    QVERIFY(isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));

    // Switch to second deep component slide
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3Component, m_deepComponentSlide2);
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(isNodeVisible(m_deepComponentSlide2Moon));

    // Switch back to deep component first slide
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3Component, m_deepComponentSlide1);
    QVERIFY(isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));

    // Set same slide again
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3Component, m_deepComponentSlide1);
    QVERIFY(isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
}

void tst_Q3DSSlides::deepComponentRollback()
{
    // Deep Component is a nested component Slide
    // PresentationSlide3 -> ComponentSlide3 -> DeepComponentSlide[n]
    // Set the presentation slide to Slide3 (contains component)
    m_sceneManager->setCurrentSlide(m_presentationSlide3);
    QVERIFY(isNodeVisible(m_slide3Component));
    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide3);
    QVERIFY(isNodeVisible(m_componentSlide3Component));

    // First slide has "master text" active
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3Component, m_deepComponentSlide1);
    QVERIFY(isNodeVisible(m_deepComponentMasterText));

    // Second slide has eyeball set to false for master text
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3Component, m_deepComponentSlide2);
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    // Switch back to first slide to perform rollback
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3Component, m_deepComponentSlide1);
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
    m_sceneManager->setCurrentSlide(m_presentationSlide1);
    QVERIFY(!isNodeVisible(m_slide3Component));

    // m_slide3Component is only visible on m_presentationSlide3
    // change m_slide3Component's slides and make sure the contents
    // do not become visible.
    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide1);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide2);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    m_sceneManager->setComponentCurrentSlide(m_slide3Component, m_componentSlide3);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    // also change deep compoents slides (now active in component slide3)
    m_sceneManager->setComponentCurrentSlide(m_componentSlide3Component, m_deepComponentSlide1);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    m_sceneManager->setComponentCurrentSlide(m_componentSlide3Component, m_deepComponentSlide2);
    QVERIFY(!isNodeVisible(m_componentMasterCube));
    QVERIFY(!isNodeVisible(m_componentSlide1Cone));
    QVERIFY(!isNodeVisible(m_componentSlide2Text));
    QVERIFY(!isNodeVisible(m_componentSlide3Component));
    QVERIFY(!isNodeVisible(m_deepComponentSlide1Cylinder));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Sphere));
    QVERIFY(!isNodeVisible(m_deepComponentSlide2Moon));
    QVERIFY(!isNodeVisible(m_deepComponentMasterText));

    // Now make component visible by switching to slide 3
    m_sceneManager->setCurrentSlide(m_presentationSlide3);
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
    m_sceneManager->setCurrentSlide(m_presentationSlide1);
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
    if (!node) {
        return false;
    }

    auto nodeAttached = static_cast<Q3DSNodeAttached *>(node->attached());
    auto entity = nodeAttached->entity;
    if (!entity)
        return false;
    auto layerAttached = static_cast<Q3DSLayerAttached *>(nodeAttached->layer3DS->attached());

    if (entity->components().contains(layerAttached->opaqueTag) || entity->components().contains(layerAttached->transparentTag))
        return true;

    return false;
}

QTEST_MAIN(tst_Q3DSSlides);

#include "tst_q3dsslides.moc"
