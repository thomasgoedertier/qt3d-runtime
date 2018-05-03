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

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>

#include <Qt3DStudioRuntime2/q3dsruntimeglobal.h>
#include <Qt3DStudioRuntime2/q3dswidget.h>
#include <Qt3DStudioRuntime2/q3dspresentation.h>

#include "../shared/shared.h"

class tst_Widget : public QObject
{
    Q_OBJECT

public:
    tst_Widget();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testSimple();
    void testReload();
};

tst_Widget::tst_Widget()
{
}

void tst_Widget::initTestCase()
{
    if (!isOpenGLGoodEnough())
        QSKIP("This platform does not support OpenGL proper");

    QSurfaceFormat::setDefaultFormat(Q3DS::surfaceFormat());

    // do not bother disabling dialogs, Q3DSWidget is expected to avoid those implicitly
}

void tst_Widget::cleanupTestCase()
{
}

void tst_Widget::testSimple()
{
    Q3DSWidget w;
    QSignalSpy errorSpy(&w, SIGNAL(errorChanged()));
    QSignalSpy runningSpy(&w, SIGNAL(runningChanged()));
    QSignalSpy frameSpy(&w, SIGNAL(frameUpdate()));
    QSignalSpy presLoadedSpy(&w, SIGNAL(presentationLoaded()));

    QVERIFY(w.settings());
    w.presentation()->setSource(QUrl(QLatin1String("qrc:/data/primitives.uip")));
    QCOMPARE(w.isRunning(), false);

    // qWaitForWindowExposed is not ideal when we have continuous updates that
    // involve blocking on vsync. Therefore start with rendering only a single
    // frame and turn on continuous rendering only once exposed.
    w.setUpdateInterval(-1);

    w.resize(640, 480);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    QCOMPARE(w.error(), QString());
    QTRY_COMPARE(w.isRunning(), true);
    QCOMPARE(w.error(), QString());
    QCOMPARE(runningSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(presLoadedSpy.count(), 1);

    w.setUpdateInterval(0);
    QTRY_VERIFY(frameSpy.count() >= 60);
}

void tst_Widget::testReload()
{
    Q3DSWidget w;
    QSignalSpy frameSpy(&w, SIGNAL(frameUpdate()));
    QSignalSpy presLoadedSpy(&w, SIGNAL(presentationLoaded()));

    w.presentation()->setSource(QUrl(QLatin1String("qrc:/data/primitives.uip")));
    QCOMPARE(w.isRunning(), false);

    w.setUpdateInterval(-1);

    w.resize(640, 480);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    QCOMPARE(w.error(), QString());
    QTRY_COMPARE(w.isRunning(), true);
    QCOMPARE(w.error(), QString());
    QCOMPARE(presLoadedSpy.count(), 1);

    w.setUpdateInterval(0);
    QTRY_VERIFY(frameSpy.count() >= 60);

    w.presentation()->reload();
    QTRY_COMPARE(presLoadedSpy.count(), 2);
    QTRY_VERIFY(frameSpy.count() >= 120);
}

QTEST_MAIN(tst_Widget)

#include "tst_widget.moc"
