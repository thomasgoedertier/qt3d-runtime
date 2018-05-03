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
#include <Qt3DStudioRuntime2/q3dssurfaceviewer.h>
#include <Qt3DStudioRuntime2/q3dspresentation.h>

#include "../shared/shared.h"

class tst_SurfaceViewer : public QObject
{
    Q_OBJECT

public:
    tst_SurfaceViewer();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testWindow();
    void testWindowWithReload();
    void testCreateDestroy();
    void testGrab();
};

tst_SurfaceViewer::tst_SurfaceViewer()
{
}

void tst_SurfaceViewer::initTestCase()
{
    if (!isOpenGLGoodEnough())
        QSKIP("This platform does not support OpenGL proper");

    QSurfaceFormat::setDefaultFormat(Q3DS::surfaceFormat());
}

void tst_SurfaceViewer::cleanupTestCase()
{
}

class Window : public QWindow
{
public:
    Window();
    void setViewer(Q3DSSurfaceViewer *viewer) { m_viewer = viewer; }
    QOpenGLContext *context() const { return m_context; }

protected:
    bool event(QEvent *) override;

private:
    Q3DSSurfaceViewer *m_viewer = nullptr;
    QOpenGLContext *m_context;
};

Window::Window()
{
    setSurfaceType(QSurface::OpenGLSurface);
    m_context = new QOpenGLContext(this);
    m_context->create();
}

bool Window::event(QEvent *e)
{
    if (e->type() == QEvent::Expose || e->type() == QEvent::UpdateRequest) {
        m_viewer->update();
        requestUpdate();
    }
    return QWindow::event(e);
}

void tst_SurfaceViewer::testWindow()
{
    Window w;
    Q3DSSurfaceViewer viewer;
    QSignalSpy errorSpy(&viewer, SIGNAL(errorChanged()));
    QSignalSpy runningSpy(&viewer, SIGNAL(runningChanged()));
    QSignalSpy presLoadedSpy(&viewer, SIGNAL(presentationLoaded()));
    QSignalSpy frameSpy(&viewer, SIGNAL(frameUpdate()));

    QVERIFY(viewer.settings());
    QCOMPARE(viewer.isRunning(), false);
    const QUrl source = QUrl(QLatin1String("qrc:/data/primitives.uip"));
    viewer.presentation()->setSource(source);
    QCOMPARE(viewer.isRunning(), false);
    QVERIFY(viewer.create(&w, w.context()));
    QCOMPARE(viewer.isRunning(), true);

    w.setViewer(&viewer);
    w.resize(1024, 768);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    QCOMPARE(viewer.error(), QString());
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(runningSpy.count(), 1);
    QCOMPARE(presLoadedSpy.count(), 1);

    // wait for 60 frames at least
    QTRY_VERIFY(frameSpy.count() >= 60);

    QCOMPARE(viewer.presentation()->source(), source);
}

void tst_SurfaceViewer::testWindowWithReload()
{
    Window w;
    Q3DSSurfaceViewer viewer;
    QSignalSpy errorSpy(&viewer, SIGNAL(errorChanged()));
    QSignalSpy runningSpy(&viewer, SIGNAL(runningChanged()));
    QSignalSpy presLoadedSpy(&viewer, SIGNAL(presentationLoaded()));
    QSignalSpy frameSpy(&viewer, SIGNAL(frameUpdate()));

    const QUrl source = QUrl(QLatin1String("qrc:/data/primitives.uip"));
    viewer.presentation()->setSource(source);
    QVERIFY(viewer.create(&w, w.context()));

    w.setViewer(&viewer);
    w.resize(1024, 768);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    QCOMPARE(viewer.error(), QString());
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(runningSpy.count(), 1);
    QCOMPARE(presLoadedSpy.count(), 1);

    QTRY_VERIFY(frameSpy.count() >= 60);

    viewer.presentation()->reload();

    QCOMPARE(viewer.error(), QString());
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(viewer.presentation()->source(), source);
    QCOMPARE(presLoadedSpy.count(), 2);

    QTRY_VERIFY(frameSpy.count() >= 120);
}

void tst_SurfaceViewer::testCreateDestroy()
{
    Window w;
    Q3DSSurfaceViewer viewer;
    QSignalSpy frameSpy(&viewer, SIGNAL(frameUpdate()));

    const QUrl source = QUrl(QLatin1String("qrc:/data/primitives.uip"));
    viewer.presentation()->setSource(source);
    QVERIFY(viewer.create(&w, w.context()));
    w.setViewer(&viewer);
    w.resize(1024, 768);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));
    QTRY_VERIFY(frameSpy.count() >= 60);

    viewer.destroy();
    QVERIFY(viewer.create(&w, w.context()));
    QTRY_VERIFY(frameSpy.count() >= 120);

    // now without destroy(), create() in itself should recreate as well
    QVERIFY(viewer.create(&w, w.context()));
    QTRY_VERIFY(frameSpy.count() >= 180);
}

void tst_SurfaceViewer::testGrab()
{
    Window w;
    Q3DSSurfaceViewer viewer;
    QSignalSpy frameSpy(&viewer, SIGNAL(frameUpdate()));

    QCOMPARE(viewer.isRunning(), false);
    const QUrl source = QUrl(QLatin1String("qrc:/data/primitives.uip"));
    viewer.presentation()->setSource(source);
    QVERIFY(viewer.create(&w, w.context()));
    w.setViewer(&viewer);
    w.resize(1024, 768);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w));

    QTRY_VERIFY(frameSpy.count() >= 60);
    QImage fullImage = viewer.grab();
    QCOMPARE(fullImage.size(), w.size() * w.devicePixelRatio());
    // verify at least that the background is there
    QColor c = fullImage.pixel(5, 5);
    QVERIFY(qFuzzyCompare(float(c.redF()), 0.929412f));
    QVERIFY(qFuzzyCompare(float(c.greenF()), 0.956863f));
    QVERIFY(qFuzzyCompare(float(c.blueF()), 0.34902f));

    QImage partialImage = viewer.grab(QRect(10, 10, 100, 100));
    QCOMPARE(partialImage.size(), QSize(100, 100));
    c = fullImage.pixel(5, 5);
    QVERIFY(qFuzzyCompare(float(c.redF()), 0.929412f));
    QVERIFY(qFuzzyCompare(float(c.greenF()), 0.956863f));
    QVERIFY(qFuzzyCompare(float(c.blueF()), 0.34902f));
}

QTEST_MAIN(tst_SurfaceViewer)

#include "tst_surfaceviewer.moc"
