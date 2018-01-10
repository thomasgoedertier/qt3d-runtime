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

#include <private/q3dsutils_p.h>
#include <private/q3dswindow_p.h>
#include <private/q3dsengine_p.h>
#include <private/q3dsuippresentation_p.h>
#include <private/q3dsscenemanager_p.h>
#include <private/q3dsslideplayer_p.h>

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QLayer>

#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>

class tst_Q3DSSlidePlayer : public QObject
{
    Q_OBJECT

public:
    tst_Q3DSSlidePlayer();
    ~tst_Q3DSSlidePlayer();
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void tst_slideDeck();

private:
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
};

tst_Q3DSSlidePlayer::tst_Q3DSSlidePlayer()
{
}

tst_Q3DSSlidePlayer::~tst_Q3DSSlidePlayer()
{
    delete m_engine;
    delete m_view;
}

void tst_Q3DSSlidePlayer::initTestCase()
{
    if (!QGuiApplicationPrivate::instance()->platformIntegration()->hasCapability(QPlatformIntegration::OpenGL))
        QSKIP("This platform does not support OpenGL");

    QSurfaceFormat::setDefaultFormat(Q3DSEngine::surfaceFormat());
    m_engine = new Q3DSEngine;
    m_view = new Q3DSWindow;
    m_view->setEngine(m_engine);
    m_view->forceResize(640, 480);

    Q3DSUtils::setDialogsEnabled(false);
    m_view->engine()->setSource(QLatin1String(":/simpleslides.uip"));
    QVERIFY(m_view->engine()->presentation());
    QVERIFY(m_view->engine()->sceneManager());

    m_presentation = m_view->engine()->presentation();
    m_sceneManager = m_view->engine()->sceneManager();
    m_scene = m_presentation->scene();

    // Presentation Slides
    m_presentationMasterSlide = m_presentation->masterSlide();
    QVERIFY(m_presentationMasterSlide->childCount() == 3);
    m_presentationSlide1 = static_cast<Q3DSSlide *>(m_presentationMasterSlide->firstChild());
    QVERIFY(m_presentationSlide1);
    m_presentationSlide2 = static_cast<Q3DSSlide *>(m_presentationSlide1->nextSibling());
    QVERIFY(m_presentationSlide2);
    m_presentationSlide3 = static_cast<Q3DSSlide *>(m_presentationSlide2->nextSibling());
    QVERIFY(m_presentationSlide3);

    m_view->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_view));
}

void tst_Q3DSSlidePlayer::cleanupTestCase()
{
    if (m_view)
        m_view->close();
}

void tst_Q3DSSlidePlayer::tst_slideDeck()
{
    Q3DSSlideDeck slideDeck(m_presentationMasterSlide);
    QVERIFY(slideDeck.slideCount() == 3);
    QVERIFY(!slideDeck.isEmpty());
    QVERIFY(slideDeck.currentSlide() == m_presentationSlide1);
    QVERIFY(slideDeck.nextSlide() == m_presentationSlide2);
    QVERIFY(slideDeck.currentSlide() == m_presentationSlide2);
    QVERIFY(slideDeck.nextSlide() == m_presentationSlide3);
    QVERIFY(slideDeck.currentSlide() == m_presentationSlide3);
    QVERIFY(slideDeck.nextSlide() == nullptr); // No further slides, current slide is unchanged
    QVERIFY(slideDeck.currentSlide() == m_presentationSlide3);

    slideDeck.setCurrentSlide(0);
    QVERIFY(slideDeck.currentSlide() == m_presentationSlide1);
    slideDeck.setCurrentSlide(2);
    QVERIFY(slideDeck.currentSlide() == m_presentationSlide3);
    slideDeck.setCurrentSlide(-1); // Invalid, should not change the current slide
    QVERIFY(slideDeck.currentSlide() == m_presentationSlide3);

    QVERIFY(slideDeck.previousSlide() == m_presentationSlide2);
    QVERIFY(slideDeck.currentSlide() == m_presentationSlide2);
    QVERIFY(slideDeck.previousSlide() == m_presentationSlide1);
    QVERIFY(slideDeck.currentSlide() == m_presentationSlide1);
    QVERIFY(slideDeck.previousSlide() == nullptr); // No further slides, current slide is unchanged
    QVERIFY(slideDeck.currentSlide() == m_presentationSlide1);
}

QTEST_MAIN(tst_Q3DSSlidePlayer);

#include "tst_q3dsslideplayer.moc"
