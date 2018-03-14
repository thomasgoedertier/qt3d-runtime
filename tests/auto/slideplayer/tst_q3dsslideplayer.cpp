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
    enum class Slide {
        PlayToNext,
        StopAtEnd,
        PlayToPrevious,
        Ping,
        PingPong,
        Looping,
        PlayToSlide,
        Dummy,
        FinalSlide
    };

public:
    tst_Q3DSSlidePlayer();
    ~tst_Q3DSSlidePlayer();
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void tst_slideDeck();
    void tst_playModes();

private:
    Q3DSEngine *m_engine = nullptr;
    Q3DSWindow *m_view = nullptr;
    Q3DSUipPresentation *m_presentation = nullptr;
    Q3DSSceneManager *m_sceneManager = nullptr;
    Q3DSScene *m_scene = nullptr;

    // Slides
    // Presentation Slides
    Q3DSSlide *m_masterSlide = nullptr;
    Q3DSSlide *m_playToNext = nullptr;
    Q3DSSlide *m_stopAtEnd = nullptr;
    Q3DSSlide *m_playToPrevious = nullptr;
    Q3DSSlide *m_ping = nullptr;
    Q3DSSlide *m_pingPong = nullptr;
    Q3DSSlide *m_looping = nullptr;
    Q3DSSlide *m_playToSlide = nullptr;
    Q3DSSlide *m_dummy = nullptr;
    Q3DSSlide *m_finalSlide = nullptr;
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
    m_masterSlide = m_presentation->masterSlide();
    QCOMPARE(m_masterSlide->childCount(), 9);
    m_playToNext = static_cast<Q3DSSlide *>(m_masterSlide->firstChild());
    QVERIFY(m_playToNext);
    m_stopAtEnd = static_cast<Q3DSSlide *>(m_playToNext->nextSibling());
    QVERIFY(m_stopAtEnd);
    m_playToPrevious = static_cast<Q3DSSlide *>(m_stopAtEnd->nextSibling());
    QVERIFY(m_playToPrevious);
    m_ping = static_cast<Q3DSSlide *>(m_playToPrevious->nextSibling());
    QVERIFY(m_ping);
    m_pingPong = static_cast<Q3DSSlide *>(m_ping->nextSibling());
    QVERIFY(m_pingPong);
    m_looping = static_cast<Q3DSSlide *>(m_pingPong->nextSibling());
    QVERIFY(m_looping);
    m_playToSlide = static_cast<Q3DSSlide *>(m_looping->nextSibling());
    QVERIFY(m_playToSlide);
    m_dummy = static_cast<Q3DSSlide *>(m_playToSlide->nextSibling());
    QVERIFY(m_dummy);
    m_finalSlide = static_cast<Q3DSSlide *>(m_dummy->nextSibling());
    QVERIFY(m_finalSlide);

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

}

void tst_Q3DSSlidePlayer::tst_playModes()
{
    Q3DSSlidePlayer *player = m_sceneManager->slidePlayer();
    QVERIFY(player);
    struct LoopCounter
    {
        int counter = 0;
        int pingPong = 0;
        bool started = false;
        void reset() { counter = 0; pingPong = 0; started = false; }
    } loopCounter;

    connect(player, &Q3DSSlidePlayer::positionChanged, [player, &loopCounter](float pos) {
        if (loopCounter.started && (pos == player->duration()))
            ++loopCounter.counter;
        if (loopCounter.started && (pos == 0.0f))
            ++loopCounter.pingPong;

        if (!loopCounter.started)
            loopCounter.started = true;
    });
    QSignalSpy slideChangedSpy(player, &Q3DSSlidePlayer::slideChanged);
    QSignalSpy stateChangeSpy(player, &Q3DSSlidePlayer::stateChanged);

    // PLAY TO NEXT
    player->stop();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    stateChangeSpy.clear();
    slideChangedSpy.clear();

    player->slideDeck()->setCurrentSlide(int(Slide::PlayToNext));
    QCOMPARE(player->duration(), 1000);
    player->play();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Playing);
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    // Stopped -> playing -> stopped
    QCOMPARE(stateChangeSpy.count(), 2);
    // StopAtEnd -> PlayToNext -> StopAtEnd
    QCOMPARE(slideChangedSpy.count(), 2);
    QCOMPARE(player->slideDeck()->currentSlide(), m_stopAtEnd);

    // STOP AT END
    player->stop();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    stateChangeSpy.clear();
    slideChangedSpy.clear();

    player->slideDeck()->setCurrentSlide(int(Slide::StopAtEnd));
    QCOMPARE(player->duration(), 1000);
    player->play();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Playing);
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    // Stopped -> playing -> stopped
    QCOMPARE(stateChangeSpy.count(), 2);
    QCOMPARE(slideChangedSpy.count(), 0);
    QCOMPARE(player->slideDeck()->currentSlide(), m_stopAtEnd);

    // PLAY TO PREVIOUS
    player->stop();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    slideChangedSpy.clear();

    player->slideDeck()->setCurrentSlide(int(Slide::PlayToPrevious));
    QCOMPARE(player->duration(), 1000);
    player->stop();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    stateChangeSpy.clear();

    player->play();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Playing);
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    // Stopped -> playing -> stopped
    QCOMPARE(stateChangeSpy.count(), 2);
    // StopAtEnd -> PlayToPrevious -> StopAtEnd
    QCOMPARE(slideChangedSpy.count(), 2);
    QCOMPARE(player->slideDeck()->currentSlide(), m_stopAtEnd);

    // PING
    slideChangedSpy.clear();

    player->slideDeck()->setCurrentSlide(int(Slide::Ping));
    QCOMPARE(player->duration(), 1000);
    player->stop();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    stateChangeSpy.clear();
    loopCounter.reset();

    player->play();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Playing);
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    // Stopped -> playing -> stopped
    QCOMPARE(stateChangeSpy.count(), 2);
    // StopAtEnd -> Ping
    QCOMPARE(slideChangedSpy.count(), 1);
    QCOMPARE(player->slideDeck()->currentSlide(), m_ping);
    QCOMPARE(loopCounter.pingPong, 1);

    // PING PONG
    slideChangedSpy.clear();

    player->slideDeck()->setCurrentSlide(int(Slide::PingPong));
    QCOMPARE(player->duration(), 1000);
    player->stop();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    stateChangeSpy.clear();
    loopCounter.reset();

    player->play();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Playing);
    QTRY_VERIFY(loopCounter.pingPong >= 2);
    player->stop();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    // Stopped -> playing -> stopped
    QCOMPARE(stateChangeSpy.count(), 2);
    // Ping -> PingPong
    QCOMPARE(slideChangedSpy.count(), 1);
    QCOMPARE(player->slideDeck()->currentSlide(), m_pingPong);

    // LOOPING
    slideChangedSpy.clear();

    player->slideDeck()->setCurrentSlide(int(Slide::Looping));
    QCOMPARE(player->duration(), 1000);
    player->stop();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    stateChangeSpy.clear();
    loopCounter.reset();

    player->play();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Playing);
    QTRY_VERIFY(loopCounter.counter >= 3);
    player->stop();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    // Stopped -> playing -> stopped
    QCOMPARE(stateChangeSpy.count(), 2);
    // PingPong -> Looping
    QCOMPARE(slideChangedSpy.count(), 1);
    QCOMPARE(player->slideDeck()->currentSlide(), m_looping);

    // PLAY TO SLIDE
    player->stop();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    slideChangedSpy.clear();

    player->slideDeck()->setCurrentSlide(int(Slide::PlayToSlide));
    QCOMPARE(player->duration(), 1000);
    player->stop();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    stateChangeSpy.clear();

    player->play();
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Playing);
    QTRY_COMPARE(player->state(), Q3DSSlidePlayer::PlayerState::Stopped);

    // Stopped -> playing -> stopped
    QCOMPARE(stateChangeSpy.count(), 2);
    // Looping -> PlayToSlide -> FinalSlide
    QCOMPARE(slideChangedSpy.count(), 2);
    QCOMPARE(player->slideDeck()->currentSlide(), m_finalSlide);
}

QTEST_MAIN(tst_Q3DSSlidePlayer);

#include "tst_q3dsslideplayer.moc"
