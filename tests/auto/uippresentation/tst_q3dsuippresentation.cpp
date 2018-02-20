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
#include <private/q3dsuippresentation_p.h>
#include <private/q3dsutils_p.h>

class tst_Q3DSUipPresentation : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void basic();
    void propertyChangeNotification();
    void sceneChangeNotification();

private:
    void makePresentation(Q3DSUipPresentation &presentation);
};

void tst_Q3DSUipPresentation::initTestCase()
{
    Q3DSUtils::setDialogsEnabled(false);
}

void tst_Q3DSUipPresentation::cleanup()
{
}

void tst_Q3DSUipPresentation::makePresentation(Q3DSUipPresentation &presentation)
{
    // Let's construct a presentation programatically. There is no sourceFile in this case.

    // *** presentation
    // not so important metadata
    presentation.setAuthor(QLatin1String("Qt"));
    presentation.setCompany(QLatin1String("The Qt Company"));

    // original design size
    presentation.setPresentationWidth(800);
    presentation.setPresentationHeight(480);

    QCOMPARE(presentation.presentationWidth(), 800);
    QCOMPARE(presentation.presentationHeight(), 480);

    // *** scene
    Q3DSScene *scene = presentation.newObject<Q3DSScene>("scene");
    QCOMPARE(scene->id(), QByteArrayLiteral("scene"));

    presentation.setScene(scene); // takes ownership

    // *** master slide
    Q3DSSlide *masterSlide = presentation.newObject<Q3DSSlide>("master");
    QCOMPARE(masterSlide->id(), QByteArrayLiteral("master"));

    // *** slides
    // slides are children of the master slide
    Q3DSSlide *slide1 = presentation.newObject<Q3DSSlide>("slide1");
    masterSlide->appendChildNode(slide1);
    QCOMPARE(masterSlide->firstChild(), slide1);
    QCOMPARE(slide1->parent(), masterSlide);
    QCOMPARE(slide1->nextSibling(), nullptr);

    Q3DSSlide *slide2 = presentation.newObject<Q3DSSlide>("slide2");
    masterSlide->appendChildNode(slide2);
    QCOMPARE(masterSlide->firstChild(), slide1);
    QCOMPARE(slide2->parent(), masterSlide);
    QCOMPARE(slide1->nextSibling(), slide2);
    QCOMPARE(slide2->nextSibling(), nullptr);
    QCOMPARE(slide2->previousSibling(), slide1);

    presentation.setMasterSlide(masterSlide); // takes ownership

    // *** layers
    // a scene is expected to have at least one layer as its child
    Q3DSLayerNode *layer1 = presentation.newObject<Q3DSLayerNode>("layer1");
    // properties conveniently default a to a normal, full-size layer
    scene->appendChildNode(layer1);
    QVERIFY(presentation.object("layer1") == layer1);
    QCOMPARE(layer1->parent(), scene);
    QCOMPARE(scene->firstChild(), layer1);

    // test object removal a bit. assume we want to get rid of layer1:
    presentation.unlinkObject(layer1);
    QCOMPARE(layer1->parent(), nullptr);
    QCOMPARE(scene->firstChild(), nullptr);
    QVERIFY(presentation.object("layer1") == nullptr);

    // re-add it. invoke registerObject manually since newObject does this for
    // us usually but not here.
    scene->appendChildNode(layer1);
    presentation.registerObject(layer1->id(), layer1);

    QVERIFY(presentation.object("layer1") == layer1);
    QCOMPARE(layer1->parent(), scene);
    QCOMPARE(scene->firstChild(), layer1);

    // *** camera
    // each layer uses the first active camera encountered while walking depth-first
    Q3DSCameraNode *camera1 = presentation.newObject<Q3DSCameraNode>("camera1");
    // Defaults to a perspective camera with fov 60, near/far 10/5000. This is
    // good as it is in many cases.
    layer1->appendChildNode(camera1);
    QVERIFY(presentation.object("camera1") == camera1);
    QCOMPARE(camera1->parent(), layer1);
    QCOMPARE(layer1->firstChild(), camera1);

    // *** light
    // does not hurt to have one light at least...
    Q3DSLightNode *light1 = presentation.newObject<Q3DSLightNode>("light1");
    // Defaults to a white directional light.
    layer1->appendChildNode(light1);
    QCOMPARE(layer1->firstChild(), camera1);
    QCOMPARE(camera1->nextSibling(), light1);
    QCOMPARE(light1->previousSibling(), camera1);
    QCOMPARE(light1->parent(), camera1->parent());

    // *** now the actual scene contents (models and texts)
    Q3DSModelNode *model1 = presentation.newObject<Q3DSModelNode>("model1");
    // A model needs a mesh. Meshes are retrieved via
    // Q3DSUipPresentation::mesh() which loads or returns a cached one.
    model1->setMesh(presentation.mesh(QLatin1String("#Cube"))); // let's use a built-in primitive
    layer1->appendChildNode(model1);
    QCOMPARE(model1->previousSibling(), light1);
    QCOMPARE(model1->parent(), layer1);
    MeshList mesh = model1->mesh();
    QVERIFY(!mesh.isNull());
    QCOMPARE(mesh->count(), 1);
    // rotate the cube around the X and Y axes
    model1->setRotation(QVector3D(45, 30, 0));
    QCOMPARE(model1->rotation(), QVector3D(45, 30, 0));

    // *** material for models
    Q3DSDefaultMaterial *mat1 = presentation.newObject<Q3DSDefaultMaterial>("mat1");
    // defaults to a white material with no texture maps
    model1->appendChildNode(mat1);
    QCOMPARE(mat1->parent(), model1);
    QCOMPARE(model1->firstChild(), mat1);
    // now we have a white cube

    // *** associate objects with slides
    masterSlide->addObject(layer1);
    masterSlide->addObject(camera1);
    masterSlide->addObject(light1);
    QCOMPARE(masterSlide->objects().count(), 3);
    // put model1 onto slide1, meaning it wont be visible on slide2
    slide1->addObject(model1);
    slide1->addObject(mat1);
    QCOMPARE(slide1->objects().count(), 2); // does not include objects inherited from master

    // done, this is a full presentation with a layer, camera, a light and a cube
}

void tst_Q3DSUipPresentation::basic()
{
    Q3DSUipPresentation presentation;
    makePresentation(presentation);
}

void tst_Q3DSUipPresentation::propertyChangeNotification()
{
    Q3DSUipPresentation presentation;
    makePresentation(presentation);

    // scene -> layer -> camera, light, model -> material
    Q3DSLayerNode *layer1 = static_cast<Q3DSLayerNode *>(presentation.scene()->firstChild());
    Q3DSModelNode *model1 = static_cast<Q3DSModelNode *>(layer1->firstChild()->nextSibling()->nextSibling());
    QCOMPARE(model1->id(), QByteArrayLiteral("model1"));

    bool ok = true;
    int ncount = 0;
    model1->addPropertyChangeObserver([&ok, &ncount, model1](Q3DSGraphObject *obj, const QSet<QString> &keys, int changeFlags) {
        qDebug() << obj << keys << changeFlags;
        if (ncount == 0) {
            if (obj != model1 || !keys.isEmpty() || changeFlags != 0)
                ok = false;
        } else if (ncount == 1) {
            if (obj != model1 || keys.count() != 1 || changeFlags != Q3DSNode::TransformChanges)
                ok = false;
        }
        ++ncount;
    });
    // this does not notify on its own
    model1->setPosition(QVector3D(0, 1, 0));
    // this does but there is no actual change
    model1->notifyPropertyChanges({ model1->setPosition(QVector3D(0, 1, 0)) });
    // this does
    model1->notifyPropertyChanges({ model1->setPosition(QVector3D(0, 2, 0)) });
    QCOMPARE(ncount, 2);
    QVERIFY(ok);
}

void tst_Q3DSUipPresentation::sceneChangeNotification()
{
    Q3DSUipPresentation presentation;
    makePresentation(presentation);

    Q3DSScene *scene = presentation.object<Q3DSScene>("scene");
    QVERIFY(scene);
    QCOMPARE(presentation.scene(), scene);

    Q3DSModelNode *model1 = presentation.object<Q3DSModelNode>("model1");
    QVERIFY(model1);

    QByteArrayList added;
    QByteArrayList removed;
    auto reset = [&added, &removed, &scene] {
        scene->resetDirtyLists();
        added.clear();
        removed.clear();
    };
    auto obs = [&added, &removed](Q3DSScene *scene) {
        for (Q3DSGraphObject *obj : scene->dirtyNodesAdded()) {
            qDebug("  added: %s", obj->id().constData());
            added.append(obj->id());
        }
        for (Q3DSGraphObject *obj : scene->dirtyNodesRemoved()) {
            // note that accessing the object should be exercised with care
            // since the object may be in the process of being destroyed. id()
            // should still work.
            qDebug("  removed: %s", obj->id().constData());
            removed.append(obj->id());
        }
        scene->resetDirtyLists();
    };
    reset();
    int obsId = scene->addSceneChangeObserver(obs);

    Q3DSModelNode *model2 = presentation.newObject<Q3DSModelNode>("model2");
    model2->setMesh(presentation.mesh(QLatin1String("#Cylinder")));
    Q3DSDefaultMaterial *mat2 = presentation.newObject<Q3DSDefaultMaterial>("mat2");
    // this will not cause a notification since model2 does not have a parent
    // and so is not associated with the scene yet
    model2->appendChildNode(mat2);

    // this should trigger a DirtyNodeAdded
    model1->appendChildNode(model2);

    QVERIFY(added.count() == 1);
    QVERIFY(removed.isEmpty());
    QCOMPARE(added[0], QByteArrayLiteral("model2"));

    reset();
    // save mat1 since we want to re-add it later
    Q3DSDefaultMaterial *mat1 = presentation.object<Q3DSDefaultMaterial>("mat1");
    model1->removeAllChildNodes();
    QVERIFY(added.isEmpty());
    QVERIFY(removed.count() == 2);
    QCOMPARE(removed[0], QByteArrayLiteral("mat1"));
    QCOMPARE(removed[1], QByteArrayLiteral("model2"));

    reset();
    model1->appendChildNode(mat1);
    model1->appendChildNode(model2);
    QVERIFY(added.count() == 2);
    QVERIFY(removed.isEmpty());
    QCOMPARE(added[0], QByteArrayLiteral("mat1"));
    QCOMPARE(added[1], QByteArrayLiteral("model2"));

    reset();
    // the proper way to take ownership back for an object that we do not plan
    // to re-add to the graph later on
    presentation.unlinkObject(model2);
    QVERIFY(added.isEmpty());
    QVERIFY(removed.count() == 1);
    QCOMPARE(removed[0], QByteArrayLiteral("model2"));
    delete model2;

    reset();
    scene->removeSceneChangeObserver(obsId);
    model1->removeChildNode(mat1);
    model1->appendChildNode(mat1);
    QVERIFY(added.isEmpty());
    QVERIFY(removed.isEmpty());

    // everything other than model2 will be destroyed when the presentation goes out of scope
}

#include <tst_q3dsuippresentation.moc>
QTEST_MAIN(tst_Q3DSUipPresentation)
