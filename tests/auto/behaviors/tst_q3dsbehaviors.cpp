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

#include <QtTest>

#include <private/q3dsengine_p.h>
#include <private/q3dswindow_p.h>
#include <private/q3dsutils_p.h>

#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>

class tst_Q3DSBehaviors : public QObject
{
    Q_OBJECT

public:
    tst_Q3DSBehaviors();
    ~tst_Q3DSBehaviors();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void behaviorLoad();
    void behaviorUnload();
    void behaviorReload();
    void events();

private:
    Q3DSEngine *m_engine = nullptr;
    Q3DSWindow *m_view = nullptr;
    Q3DSUipPresentation *m_presentation = nullptr;
};

tst_Q3DSBehaviors::tst_Q3DSBehaviors()
{
}

tst_Q3DSBehaviors::~tst_Q3DSBehaviors()
{
}

void tst_Q3DSBehaviors::initTestCase()
{
    if (!QGuiApplicationPrivate::instance()->platformIntegration()->hasCapability(QPlatformIntegration::OpenGL))
        QSKIP("This platform does not support OpenGL");

    QSurfaceFormat::setDefaultFormat(Q3DSEngine::surfaceFormat());
    m_engine = new Q3DSEngine;
    m_view = new Q3DSWindow;
    m_view->setEngine(m_engine);
    m_view->forceResize(640, 480);

    Q3DSUtils::setDialogsEnabled(false);
    QVERIFY(m_view->engine()->setSource(QLatin1String(":/data/behaviors.uip")));

    m_view->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_view));

    QVERIFY(m_view->engine()->presentation());
    QVERIFY(m_view->engine()->sceneManager());

    m_presentation = m_view->engine()->presentation();
}

void tst_Q3DSBehaviors::cleanupTestCase()
{
    delete m_engine;
    delete m_view;
}

void tst_Q3DSBehaviors::behaviorLoad()
{
    auto bi = m_presentation->objectByName<Q3DSBehaviorInstance>(QLatin1String("Behavior instance 1"));
    QVERIFY(bi);

    const QVariantMap props = bi->customProperties();
    QCOMPARE(props.value(QLatin1String("target")).toString(), QStringLiteral("Scene.Layer.Camera"));
    QCOMPARE(props.value(QLatin1String("startImmediately")).toBool(), true);

    const Q3DSEngine::BehaviorMap loadedBehaviors = m_engine->behaviorHandles();
    QCOMPARE(loadedBehaviors.count(), 2);
    QVERIFY(loadedBehaviors.contains(bi));

    Q3DSBehaviorHandle h = loadedBehaviors.value(bi);
    QCOMPARE(h.behaviorInstance, bi);
    QVERIFY(h.component);
    QVERIFY(h.object);

    // check if default values for the properties have been updated to the object
    QCOMPARE(h.object->property("target").toString(), QStringLiteral("Scene.Layer.Camera"));
    QCOMPARE(h.object->property("startImmediately").toBool(), true);

    // see if the built-in signals got emitted, make sure rendering is up and
    // running first since it's all hooked up to the frame callbacks
    QSignalSpy spy(m_engine, SIGNAL(nextFrameStarting()));
    QTRY_VERIFY(spy.count() >= 10);
    QCOMPARE(h.object->property("gotInitialize").toBool(), true);
    QCOMPARE(h.object->property("gotActivate").toBool(), true);
    QVERIFY(h.object->property("updateCount").toInt() > 0);
}

// the QML component and the created object can be destroyed at any time via
// unloadBehaviorInstance
void tst_Q3DSBehaviors::behaviorUnload()
{
    auto bi = m_presentation->objectByName<Q3DSBehaviorInstance>(QLatin1String("Behavior instance 1"));
    QVERIFY(bi);
    QVERIFY(m_engine->behaviorHandles().contains(bi));

    m_engine->unloadBehaviorInstance(bi);
    QVERIFY(!m_engine->behaviorHandles().contains(bi));
}

// when adding a new Q3DSBehaviorInstance to the scenegraph at runtime, the QML
// component and object can be created by calling loadBehavior manually
void tst_Q3DSBehaviors::behaviorReload()
{
    auto bi = m_presentation->objectByName<Q3DSBehaviorInstance>(QLatin1String("Behavior instance 1"));
    QVERIFY(bi);
    QVERIFY(!m_engine->behaviorHandles().contains(bi));

    bool loadOk = false;
    m_engine->loadBehaviorInstance(bi, m_presentation,
                                   [bi, &loadOk](Q3DSBehaviorInstance *behaviorInstance, const QString &error)
    {
        if (error.isEmpty() && bi == behaviorInstance)
            loadOk = true;
    });
    QTRY_VERIFY(loadOk);
    QVERIFY(m_engine->behaviorHandles().contains(bi));
    Q3DSBehaviorHandle h = m_engine->behaviorHandles().value(bi);
    QCOMPARE(h.behaviorInstance, bi);
    QVERIFY(h.component);
    QVERIFY(h.object);
    QCOMPARE(h.object->property("target").toString(), QStringLiteral("Scene.Layer.Camera"));
    QCOMPARE(h.object->property("startImmediately").toBool(), true);
}

void tst_Q3DSBehaviors::events()
{
    auto bi = m_presentation->objectByName<Q3DSBehaviorInstance>(QLatin1String("Event handling behavior instance"));
    QVERIFY(bi);

    Q3DSBehaviorHandle h = m_engine->behaviorHandles().value(bi);
    QTRY_COMPARE(h.object->property("gotInitialize").toBool(), true);
    QTRY_VERIFY(h.object->property("updateCount").toInt() > 0);
    int u = h.object->property("updateCount").toInt();

    QCOMPARE(bi->parent()->id(), QByteArrayLiteral("Rectangle"));
    auto behaviorOwner = bi->parent();

    QCOMPARE(h.object->property("eventCount").toInt(), 0);
    const QString eventKey = QLatin1String("meltdown");

    behaviorOwner->processEvent(eventKey);
    QTRY_VERIFY(h.object->property("updateCount").toInt() > u);
    u = h.object->property("updateCount").toInt();
    QCOMPARE(h.object->property("eventCount").toInt(), 1);

    behaviorOwner->processEvent(eventKey);
    QTRY_VERIFY(h.object->property("updateCount").toInt() > u);
    u = h.object->property("updateCount").toInt();
    QCOMPARE(h.object->property("eventCount").toInt(), 2);

    behaviorOwner->processEvent(QLatin1String("not interested"));
    QTRY_VERIFY(h.object->property("updateCount").toInt() > u);
    u = h.object->property("updateCount").toInt();
    QCOMPARE(h.object->property("eventCount").toInt(), 2); // must not have changed

    // wait until the 50th update when the script unregisters its event handler
    QTRY_VERIFY(h.object->property("updateCount").toInt() > 50);
    u = h.object->property("updateCount").toInt();

    behaviorOwner->processEvent(eventKey);
    QTRY_VERIFY(h.object->property("updateCount").toInt() > u);
    u = h.object->property("updateCount").toInt();
    QCOMPARE(h.object->property("eventCount").toInt(), 2); // must not have changed
}

QTEST_MAIN(tst_Q3DSBehaviors);

#include "tst_q3dsbehaviors.moc"
