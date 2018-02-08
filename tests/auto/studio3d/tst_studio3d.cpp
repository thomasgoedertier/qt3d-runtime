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

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>

#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickview.h>
#include <QtCore/qurl.h>

#include <q3dsruntimeglobal.h>

class tst_Studio3D : public QObject
{
    Q_OBJECT

public:
    tst_Studio3D();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testSimple();
};

tst_Studio3D::tst_Studio3D()
{
}

void tst_Studio3D::initTestCase()
{
    QSurfaceFormat::setDefaultFormat(Q3DS::surfaceFormat());

    // do not bother disabling dialogs, Studio3D is expected to avoid those implicitly
}

void tst_Studio3D::cleanupTestCase()
{
}

void tst_Studio3D::testSimple()
{
    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setSource(QUrl(QLatin1String("qrc:/data/simple.qml")));

    QVERIFY(view.rootObject());
    QVERIFY(view.rootObject()->childItems().count() > 0);

    QQuickItem *s3d = view.rootObject()->childItems()[0];
    QVERIFY(s3d);
    const QMetaObject *s3dMeta = s3d->metaObject();
    QVERIFY(!strcmp(s3dMeta->className(), "Q3DSStudio3DItem"));

    view.resize(600, 500);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    if (view.rendererInterface()->graphicsApi() != QSGRendererInterface::OpenGL)
        QSKIP("This test needs Qt Quick on OpenGL");

    QCOMPARE(s3d->property("error").toString(), QString());
    QTRY_COMPARE(s3d->property("running").toBool(), true);
    QCOMPARE(s3d->property("error").toString(), QString());

    // Ensure that the frameUpdate signal works and at the same time wait for a
    // number of frames to make sure Qt 3D gets to render something (this
    // conveniently prevents teardown deadlocks as well).
    QSignalSpy spy(s3d, SIGNAL(frameUpdate()));
    QTRY_VERIFY(spy.count() >= 10);
}

QTEST_MAIN(tst_Studio3D)

#include "tst_studio3d.moc"
