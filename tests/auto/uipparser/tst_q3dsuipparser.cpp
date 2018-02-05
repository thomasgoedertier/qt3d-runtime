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

#include <QtTest/QtTest>
#include <private/q3dsuipparser_p.h>
#include <private/q3dsutils_p.h>
#include <private/q3dsdatamodelparser_p.h>

class tst_Q3DSUipParser : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void testEmpty();
    void testInvalid();
    void testRepeatedLoad();
    void assetRef();
    void uipAndProjectTags();
    void projectSettings();
    void sceneRoot();
    void dataModelParser();
    void nodeDefaultValues();
    void sceneProps();
    void masterSlide();
    void graph();
    void slideNodeRefs();
    void slidePropertySet();
    void animationTrack();
    void layerProps();
    void imageObj();
    void customMaterial();
    void effect();
    void primitiveMeshes();
    void customMesh();
    void group();
    void text();
    void component();
    void aoProps();
    void lightmapProps();
    void iblProps();
};

void tst_Q3DSUipParser::initTestCase()
{
    Q3DSUtils::setDialogsEnabled(false);
}

void tst_Q3DSUipParser::cleanup()
{
}

void tst_Q3DSUipParser::testEmpty()
{
    {
        Q3DSUipParser parser;
    }

    {
        Q3DSUipParser parser;
        QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String("invalid_file")));
        QVERIFY(pres.isNull());

    }
}

void tst_Q3DSUipParser::testInvalid()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/invalid1.uip")));
    QVERIFY(pres.isNull());
    QVERIFY(!parser.readerErrorString().isEmpty());
}

void tst_Q3DSUipParser::testRepeatedLoad()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/invalid1.uip")));
    QVERIFY(pres.isNull());

    pres.reset(parser.parse(QLatin1String(":/data/modded_cube.uip")));
    QVERIFY(!pres.isNull());

    pres.reset(parser.parse(QLatin1String(":/data/custom_mesh.uip")));
    QVERIFY(!pres.isNull());
}

void tst_Q3DSUipParser::assetRef()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/modded_cube.uip")));
    QVERIFY(!pres.isNull());


    int part = -123;
    QString fn = parser.assetFileName(".\\Headphones\\meshes\\Headphones.mesh#1", &part);
    QCOMPARE(part, 1);
    QCOMPARE(fn, QLatin1String(":/data/Headphones/meshes/Headphones.mesh"));

    fn = parser.assetFileName("something", nullptr);
    QCOMPARE(fn, QLatin1String(":/data/something"));

    part = -123;
    fn = parser.assetFileName("something", &part);
    QCOMPARE(fn, QLatin1String(":/data/something"));
    QCOMPARE(part, 1);

    fn = parser.assetFileName("/absolute/blah#32", &part);
    QCOMPARE(fn, QLatin1String("/absolute/blah"));
    QCOMPARE(part, 32);

    part = 26;
    fn = parser.assetFileName("bad_part#abcd", &part);
    QVERIFY(fn.isEmpty());
    QCOMPARE(part, 26);

    part = -123;
    fn = parser.assetFileName("#Cube", &part);
    QCOMPARE(fn, QLatin1String("#Cube"));
    QCOMPARE(part, 1);
}

void tst_Q3DSUipParser::uipAndProjectTags()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/wrong_version.uip")));
    QVERIFY(pres.isNull());

    pres.reset(parser.parse(QLatin1String(":/data/wrong_projects.uip")));
    QVERIFY(pres.isNull());
}

void tst_Q3DSUipParser::projectSettings()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/modded_cube.uip")));
    QVERIFY(!pres.isNull());
    QCOMPARE(pres->presentationWidth(), 800);
    QCOMPARE(pres->presentationHeight(), 480);
    QCOMPARE(pres->presentationRotation(), Q3DSPresentation::Clockwise270);
    QCOMPARE(pres->maintainAspectRatio(), true);
    QCOMPARE(pres->company(), QLatin1String("Qt"));
    QCOMPARE(pres->author(), QString());

    pres.reset(parser.parse(QLatin1String(":/data/wrong_projectsettings.uip")));
    QVERIFY(pres.isNull());
}

void tst_Q3DSUipParser::sceneRoot()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/modded_cube.uip")));
    QVERIFY(!pres.isNull());
    Q3DSScene *scene = pres->scene();
    QVERIFY(scene);
    QVERIFY(scene->childCount() > 0);
    for (int i = 0; i < scene->childCount(); ++i) {
        Q3DSGraphObject *n = scene->childAtIndex(i);
        QVERIFY(n);
        QCOMPARE(n->parent(), scene);
        if (i == 0)
            QCOMPARE(n->type(), Q3DSGraphObject::Layer);
    }
    QCOMPARE(scene->firstChild(), scene->childAtIndex(0));
    QCOMPARE(scene->lastChild(), scene->childAtIndex(scene->childCount() - 1));
    QCOMPARE(scene->parent(), nullptr);
}

void tst_Q3DSUipParser::dataModelParser()
{
    Q3DSDataModelParser *p = Q3DSDataModelParser::instance();
    QVERIFY(p);

    QCOMPARE(p->propertiesForType(QStringLiteral("abcdef")), nullptr);

    QVERIFY(p->propertiesForType(QStringLiteral("Asset")) != nullptr);
    QVERIFY(p->propertiesForType(QStringLiteral("Layer")) != nullptr);
    QVERIFY(p->propertiesForType(QStringLiteral("Scene")) != nullptr);
    QVERIFY(p->propertiesForType(QStringLiteral("Material")) != nullptr);
    QVERIFY(p->propertiesForType(QStringLiteral("Model")) != nullptr);
    QVERIFY(p->propertiesForType(QStringLiteral("Node")) != nullptr);

    const QVector<Q3DSDataModelParser::Property> *v = p->propertiesForType(QStringLiteral("Node"));
    QVERIFY(!v->isEmpty());
    QCOMPARE(v->count(), 9);
    auto it = std::find_if(v->cbegin(), v->cend(), [](const Q3DSDataModelParser::Property &prop) { return prop.name == QStringLiteral("rotation"); });
    QVERIFY(it != v->cend());
    QCOMPARE(it->type, Q3DS::Rotation);
    it = std::find_if(v->cbegin(), v->cend(), [](const Q3DSDataModelParser::Property &prop) { return prop.name == QStringLiteral("scale"); });
    QVERIFY(it != v->cend());
    QCOMPARE(it->type, Q3DS::Vector);
    QCOMPARE(it->defaultValue, QStringLiteral("1 1 1"));

    v = p->propertiesForType(QStringLiteral("Layer"));
    QVERIFY(!v->isEmpty());
    QVERIFY(v->count() >= 48);
    it = std::find_if(v->cbegin(), v->cend(), [](const Q3DSDataModelParser::Property &prop) { return prop.name == QStringLiteral("width"); });
    QVERIFY(it != v->cend());
    QCOMPARE(it->type, Q3DS::Float);
    QCOMPARE(it->defaultValue, QStringLiteral("100"));
    it = std::find_if(v->cbegin(), v->cend(), [](const Q3DSDataModelParser::Property &prop) { return prop.name == QStringLiteral("background"); });
    QVERIFY(it != v->cend());
    QCOMPARE(it->type, Q3DS::Enum);
    QCOMPARE(it->defaultValue, QStringLiteral("Transparent"));
    QCOMPARE(it->enumValues, QStringList() << QStringLiteral("Transparent") << QStringLiteral("SolidColor") << QStringLiteral("Unspecified"));
}

void tst_Q3DSUipParser::nodeDefaultValues()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/modded_cube.uip")));
    QVERIFY(!pres.isNull());
    Q3DSScene *scene = pres->scene();
    QVERIFY(scene && scene->childCount() > 0);

    Q3DSNode *layer = static_cast<Q3DSNode *>(scene->firstChild());
    QVERIFY(layer);
    QCOMPARE(layer->type(), Q3DSGraphObject::Layer);

    QCOMPARE(layer->position(), QVector3D());
    QCOMPARE(layer->rotation(), QVector3D());
    QCOMPARE(layer->scale(), QVector3D(1, 1, 1));
    QCOMPARE(layer->rotationOrder(), Q3DSNode::YXZ);
    QCOMPARE(layer->orientation(), Q3DSNode::LeftHanded);
    QVERIFY(layer->flags().testFlag(Q3DSNode::Active));
}

void tst_Q3DSUipParser::sceneProps()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/modded_cube.uip")));
    QVERIFY(!pres.isNull());
    QCOMPARE(pres->scene()->type(), Q3DSGraphObject::Scene);

    Q3DSScene *scene = pres->scene();
    QCOMPARE(scene->useClearColor(), false);
    QCOMPARE(scene->clearColor(), QColor::fromRgbF(0.27451f, 0.776471f, 0.52549f));
    QCOMPARE(scene->name(), QStringLiteral("Scene")); // default from metadata
}

void tst_Q3DSUipParser::masterSlide()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/multislide.uip")));
    QVERIFY(!pres.isNull());
    QVERIFY(pres->masterSlide() != nullptr);

    Q3DSSlide *master = pres->masterSlide();
    QCOMPARE(master->type(), Q3DSGraphObject::Slide);
    QCOMPARE(master->playMode(), Q3DSSlide::PingPong);
    QCOMPARE(master->initialPlayState(), Q3DSSlide::Play);
    QCOMPARE(master->playThroughHasExplicitValue(), true);
    QCOMPARE(master->playThroughValue(), 42);

    QCOMPARE(master->childCount(), 4);
    QCOMPARE(master->firstChild()->type(), Q3DSGraphObject::Slide);
    for (int i = 0; i < master->childCount(); ++i) {
        Q3DSSlide *slide = static_cast<Q3DSSlide *>(master->childAtIndex(i));
        QCOMPARE(slide->type(), Q3DSGraphObject::Slide);
        switch (i) {
        case 0:
            QVERIFY(slide->nextSibling() == master->childAtIndex(1));
            // <State id="Scene-Slide1" name="Slide1" >
            QCOMPARE(slide->playMode(), Q3DSSlide::StopAtEnd);
            QCOMPARE(slide->initialPlayState(), Q3DSSlide::Play);
            QCOMPARE(slide->playThroughHasExplicitValue(), false);
            QCOMPARE(slide->playThrough(), Q3DSSlide::Next);
            break;
        case 1:
            QVERIFY(slide->previousSibling() == master->childAtIndex(0));
            QVERIFY(slide->nextSibling() == master->childAtIndex(2));
            // <State id="Scene-Slide-1" name="Slide-1" playthroughto="Previous" >
            QCOMPARE(slide->playMode(), Q3DSSlide::StopAtEnd);
            QCOMPARE(slide->initialPlayState(), Q3DSSlide::Play);
            QCOMPARE(slide->playThroughHasExplicitValue(), false);
            QCOMPARE(slide->playThrough(), Q3DSSlide::Previous);
            break;
        case 2:
            QVERIFY(slide->previousSibling() == master->childAtIndex(1));
            // <State id="Scene-Slide0" name="Slide0" initialplaystate="Play" playmode="Stop at end" playthroughto="Previous" >
            QCOMPARE(slide->playMode(), Q3DSSlide::StopAtEnd);
            QCOMPARE(slide->initialPlayState(), Q3DSSlide::Play);
            QCOMPARE(slide->playThroughHasExplicitValue(), false);
            QCOMPARE(slide->playThrough(), Q3DSSlide::Previous);
            break;
        default:
            break;
        }
    }
}

void tst_Q3DSUipParser::graph()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/multislide.uip")));
    QVERIFY(!pres.isNull());
    QVERIFY(pres->scene() != nullptr);
    QVERIFY(pres->masterSlide() != nullptr);

    Q3DSScene *scene = pres->scene();
    Q3DSSlide *master = pres->masterSlide();

    QCOMPARE(scene->childCount(), 1);
    QCOMPARE(scene->firstChild()->childCount(), 5);

    Q3DSLayerNode *rootLayer = static_cast<Q3DSLayerNode *>(scene->firstChild());

    QVERIFY(pres->object(QByteArrayLiteral("Scene")) == scene);
    QVERIFY(pres->object(QByteArrayLiteral("Scene-Master")) == master);
    QVERIFY(pres->object(QByteArrayLiteral("Layer")) == rootLayer);

    QCOMPARE(scene->id(), QByteArrayLiteral("Scene"));
    QCOMPARE(master->id(), QByteArrayLiteral("Scene-Master"));
    QCOMPARE(rootLayer->id(), QByteArrayLiteral("Layer"));

    /*
                    <Camera id="Camera" />
                    <Light id="Light" />
                    <Model id="Cylinder" >
                        <Material id="Material" />
                    </Model>
                    <Model id="Cone" >
                        <Material id="Material_001" />
                    </Model>
                    <Model id="Sphere" >
                        <Material id="Material_002" />
                    </Model>
    */

    Q3DSGraphObject *n = rootLayer->firstChild();
    int idx = 0;
    while (n) {
        switch (idx) {
        case 0:
            QVERIFY(pres->object(QByteArrayLiteral("Camera")) == n);
            QCOMPARE(n->id(), QByteArrayLiteral("Camera"));
            QCOMPARE(n->childCount(), 0);
            QCOMPARE(static_cast<Q3DSCameraNode *>(n)->name(), QStringLiteral("Camera")); // default name from metadata
            break;
        case 1:
            QVERIFY(pres->object(QByteArrayLiteral("Light")) == n);
            QCOMPARE(n->id(), QByteArrayLiteral("Light"));
            QCOMPARE(n->childCount(), 0);
            QCOMPARE(static_cast<Q3DSLightNode *>(n)->name(), QStringLiteral("Light")); // default name from metadata
            break;
        case 2:
            QVERIFY(pres->object(QByteArrayLiteral("Cylinder")) == n);
            QCOMPARE(n->id(), QByteArrayLiteral("Cylinder"));
            QCOMPARE(n->childCount(), 1);
            QCOMPARE(static_cast<Q3DSModelNode *>(n)->name(), QStringLiteral("Cylinder"));
            QCOMPARE(n->firstChild()->type(), Q3DSGraphObject::DefaultMaterial);
            QCOMPARE(n->firstChild(), pres->object(QByteArrayLiteral("Material")));
            QCOMPARE(static_cast<Q3DSDefaultMaterial *>(n->firstChild())->name(), QStringLiteral("Material"));
            break;
        case 3:
            QVERIFY(pres->object(QByteArrayLiteral("Cone")) == n);
            QCOMPARE(n->id(), QByteArrayLiteral("Cone"));
            QCOMPARE(n->childCount(), 1);
            QCOMPARE(static_cast<Q3DSModelNode *>(n)->name(), QStringLiteral("Cone"));
            QCOMPARE(n->firstChild()->type(), Q3DSGraphObject::DefaultMaterial);
            QCOMPARE(n->firstChild(), pres->object(QByteArrayLiteral("Material_001")));
            QCOMPARE(static_cast<Q3DSDefaultMaterial *>(n->firstChild())->name(), QStringLiteral("Material")); // default name from metadata
            break;
        case 4:
            QVERIFY(pres->object(QByteArrayLiteral("Sphere")) == n);
            QCOMPARE(n->id(), QByteArrayLiteral("Sphere"));
            QCOMPARE(n->childCount(), 1);
            QCOMPARE(static_cast<Q3DSModelNode *>(n)->name(), QStringLiteral("Sphere"));
            QCOMPARE(n->firstChild()->type(), Q3DSGraphObject::DefaultMaterial);
            QCOMPARE(n->firstChild(), pres->object(QByteArrayLiteral("Material_002")));
            QCOMPARE(static_cast<Q3DSDefaultMaterial *>(n->firstChild())->name(), QStringLiteral("Material")); // default name from metadata
            break;
        default:
            break;
        }
        ++idx;
        n = n->nextSibling();
    }
    QCOMPARE(idx, 5);

    QCOMPARE(master->childCount(), 4);

    Q3DSCameraNode *cam = static_cast<Q3DSCameraNode *>(pres->object(QByteArrayLiteral("Camera")));
    QVERIFY(cam);
    QCOMPARE(cam->position(), QVector3D(0, 0, -600)); // overridden default from metadata for position inherited from node
}


void tst_Q3DSUipParser::slideNodeRefs()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/multislide.uip")));
    QVERIFY(!pres.isNull());
    Q3DSSlide *master = pres->masterSlide();
    QVERIFY(master);

    QCOMPARE(master->name(), QStringLiteral("Master Slide"));
    QCOMPARE(master->objects()->count(), 5);
    QVERIFY(master->objects()->contains(pres->object(QByteArrayLiteral("Layer"))));
    QVERIFY(master->objects()->contains(pres->object(QByteArrayLiteral("Camera"))));
    QVERIFY(master->objects()->contains(pres->object(QByteArrayLiteral("Light"))));
    QVERIFY(master->objects()->contains(pres->object(QByteArrayLiteral("Sphere"))));
    QVERIFY(master->objects()->contains(pres->object(QByteArrayLiteral("Material_002"))));

    Q3DSNode *sphere = static_cast<Q3DSNode *>(pres->object(QByteArrayLiteral("Sphere")));
    QVERIFY(sphere);
    QCOMPARE(sphere->position(), QVector3D(-412.805f, 152.998f, 0.0f));

    Q3DSNode *cone = static_cast<Q3DSNode *>(pres->object(QByteArrayLiteral("Cone")));
    QVERIFY(cone);
    QCOMPARE(cone->position(), QVector3D(139.388f, -26.567f, 7.36767f));
    QCOMPARE(cone->rotation(), QVector3D(-15.5f, 0.0f, 0.0f));

    for (int i = 0; i < master->childCount(); ++i) {
        Q3DSSlide *slide = static_cast<Q3DSSlide *>(master->childAtIndex(i));
        switch (i) {
        case 0:
            QCOMPARE(slide->name(), QStringLiteral("Slide1"));
            QCOMPARE(slide->objects()->count(), 2);
            QVERIFY(slide->objects()->contains(pres->object(QByteArrayLiteral("Cylinder"))));
            QVERIFY(slide->objects()->contains(pres->object(QByteArrayLiteral("Material"))));
            break;
        case 1:
            QCOMPARE(slide->name(), QStringLiteral("Slide-1"));
            QCOMPARE(slide->objects()->count(), 2);
            QVERIFY(slide->objects()->contains(pres->object(QByteArrayLiteral("Cone"))));
            QVERIFY(slide->objects()->contains(pres->object(QByteArrayLiteral("Material_001"))));
            break;
        case 2:
            QCOMPARE(slide->name(), QStringLiteral("Slide0"));
            QCOMPARE(slide->objects()->count(), 0);
            break;
        default:
            break;
        }
    }
}

void tst_Q3DSUipParser::slidePropertySet()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/multislide.uip")));
    QVERIFY(!pres.isNull());
    Q3DSSlide *master = pres->masterSlide();
    QVERIFY(master && master->childCount() == 4);

    Q3DSNode *sphere = static_cast<Q3DSNode *>(pres->object(QByteArrayLiteral("Sphere")));
    QVERIFY(sphere);
    QCOMPARE(sphere->position(), QVector3D(-412.805f, 152.998f, 0.0f));

    bool gotPropertyChange = false;
    int id = sphere->addPropertyChangeObserver([&](Q3DSGraphObject *obj, const QSet<QString> &keys, int changeFlags) {
        if (obj == sphere && keys.count() == 1
            && keys.contains(QStringLiteral("position")) && (changeFlags & Q3DSPropertyChangeList::NodeTransformChanges))
            gotPropertyChange = true;
    });
    QVERIFY(id >= 0);
#if 0
    // Simulate entering the 3rd sub-slide that changes an unlinked property of the sphere on the master slide.
    Q3DSSlide *slide = static_cast<Q3DSSlide *>(master->childAtIndex(2));
    pres->setCurrentSlide(slide, nullptr);
    QCOMPARE(pres->currentSlide(), slide);
    QCOMPARE(sphere->position(), QVector3D(-12.9904f, -47.6315f, 0.0f));
    QCOMPARE(sphere->flags().testFlag(Q3DSNode::Active), true);
    QVERIFY(gotPropertyChange);

    // Now the 4th slide that deactivates (hides) the sphere.
    bool got2 = false;
    sphere->addPropertyChangeObserver([&](Q3DSGraphObject *obj, const QSet<QString> &keys, int changeFlags) {
        qDebug() << obj << keys;
        if (obj == sphere && keys.count() == 2
            && keys.contains(QStringLiteral("position")) && (changeFlags & Q3DSPropertyChangeList::NodeTransformChanges)
            && keys.contains(QStringLiteral("eyeball")) && (changeFlags & Q3DSPropertyChangeList::EyeballChanges))
            got2 = true;
    });
    pres->setCurrentSlide(static_cast<Q3DSSlide *>(slide->nextSibling()), nullptr);
    QCOMPARE(sphere->position(), QVector3D(-12.9904f, -47.6315f, 0.0f));
    QCOMPARE(sphere->flags().testFlag(Q3DSNode::Active), false);
    QVERIFY(got2);
#endif
}

void tst_Q3DSUipParser::animationTrack()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/multislide.uip")));
    QVERIFY(!pres.isNull());
    Q3DSSlide *master = pres->masterSlide();
    QVERIFY(master && master->childCount() == 4);

    Q3DSSlide *slide = static_cast<Q3DSSlide *>(master->childAtIndex(1));
    QVERIFY(slide->animations() != nullptr);
    QVERIFY(!slide->animations()->isEmpty());
    QCOMPARE(slide->animations()->count(), 3);

    // <Add ref="#Cone" ... > <AnimationTrack property="position.x" type="EaseInOut" >0 139.388 100 100 4.72 -100 100 100</AnimationTrack>
    Q3DSNode *cone = static_cast<Q3DSNode *>(pres->object(QByteArrayLiteral("Cone")));
    QVERIFY(cone);
    const Q3DSAnimationTrack &a0(slide->animations()->at(0));
    QCOMPARE(a0.target(), cone);
    QCOMPARE(a0.property(), QStringLiteral("position.x"));
    QVERIFY(!a0.isDynamic());
    QCOMPARE(a0.type(), Q3DSAnimationTrack::EaseInOut);
    QCOMPARE(a0.keyFrames()->count(), 2);
    const QVector<Q3DSAnimationTrack::KeyFrame> *kf = a0.keyFrames();
    QCOMPARE(kf->at(0).time, 0.0f);
    QCOMPARE(kf->at(0).value, 139.388f);
    QCOMPARE(kf->at(0).easeIn, 100.0f);
    QCOMPARE(kf->at(0).easeOut, 100.0f);
    QCOMPARE(kf->at(1).time, 4.72f);
    QCOMPARE(kf->at(1).value, -100.0f);
    QCOMPARE(kf->at(1).easeIn, 100.0f);
    QCOMPARE(kf->at(1).easeOut, 100.0f);

    const Q3DSAnimationTrack &a1(slide->animations()->at(1));
    QCOMPARE(a1.target(), cone);
    QCOMPARE(a1.property(), QStringLiteral("position.y"));
    QVERIFY(!a1.isDynamic());
    QCOMPARE(a1.type(), Q3DSAnimationTrack::EaseInOut);
    QCOMPARE(a1.keyFrames()->count(), 2);
    kf = a1.keyFrames();
    // "0 -26.567 100 100 4.72 -26.567 100 100"
    QCOMPARE(kf->at(0).time, 0.0f);
    QCOMPARE(kf->at(0).value, -26.567f);
    QCOMPARE(kf->at(0).easeIn, 100.0f);
    QCOMPARE(kf->at(0).easeOut, 100.0f);
    QCOMPARE(kf->at(1).time, 4.72f);
    QCOMPARE(kf->at(1).value, -26.567f);
    QCOMPARE(kf->at(1).easeIn, 100.0f);
    QCOMPARE(kf->at(1).easeOut, 100.0f);

    const Q3DSAnimationTrack &a2(slide->animations()->at(2));
    QCOMPARE(a2.target(), cone);
    QCOMPARE(a2.property(), QStringLiteral("position.z"));
    QVERIFY(!a2.isDynamic());
    QCOMPARE(a2.type(), Q3DSAnimationTrack::EaseInOut);
    QCOMPARE(a2.keyFrames()->count(), 2);
    kf = a2.keyFrames();
    // "0 7.36767 100 100 4.72 7.36767 100 100"
    QCOMPARE(kf->at(0).time, 0.0f);
    QCOMPARE(kf->at(0).value, 7.36767f);
    QCOMPARE(kf->at(0).easeIn, 100.0f);
    QCOMPARE(kf->at(0).easeOut, 100.0f);
    QCOMPARE(kf->at(1).time, 4.72f);
    QCOMPARE(kf->at(1).value, 7.36767f);
    QCOMPARE(kf->at(1).easeIn, 100.0f);
    QCOMPARE(kf->at(1).easeOut, 100.0f);
}

void tst_Q3DSUipParser::layerProps()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/multislide.uip")));
    QVERIFY(!pres.isNull());
    Q3DSLayerNode *layer = static_cast<Q3DSLayerNode *>(pres->scene()->firstChild());
    QVERIFY(layer->flags().testFlag(Q3DSNode::Active));
    QCOMPARE(layer->blendType(), Q3DSLayerNode::Normal);
    QVERIFY(layer->layerFlags() == Q3DSLayerNode::Flags(Q3DSLayerNode::FastIBL));
    QVERIFY(layer->sourcePath().isEmpty());
    QCOMPARE(layer->widthUnits(), Q3DSLayerNode::Percent);
    QCOMPARE(layer->width(), 100.0f);
}

void tst_Q3DSUipParser::imageObj()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/image.uip")));
    QVERIFY(!pres.isNull());
    QVERIFY(pres->object(QByteArrayLiteral("Layer_lightprobe")));
    QVERIFY(pres->object(QByteArrayLiteral("Material_diffusemap")));
    QVERIFY(pres->object(QByteArrayLiteral("Material_001_diffusemap")));

    Q3DSImage *img = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Material_001_diffusemap")));

    QCOMPARE(img->type(), Q3DSGraphObject::Image);
    QCOMPARE(img->parent()->type(), Q3DSGraphObject::DefaultMaterial);
    QCOMPARE(img->parent()->parent()->type(), Q3DSGraphObject::Model);
    QCOMPARE(img->parent()->parent()->parent()->type(), Q3DSGraphObject::Layer);

    QCOMPARE(img->name(), QStringLiteral("Image"));
    QCOMPARE(img->sourcePath(), QStringLiteral(":/data/QT-badge.png"));
    QCOMPARE(img->mappingMode(), Q3DSImage::UVMapping);

    // Image reference in Layer
    Q3DSLayerNode *layer = static_cast<Q3DSLayerNode *>(img->parent()->parent()->parent());
    Q3DSImage *probeImage = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Layer_lightprobe")));
    QCOMPARE(layer->lightProbe(), probeImage);

    // Image reference in (default) Material
    Q3DSDefaultMaterial *mat = static_cast<Q3DSDefaultMaterial *>(img->parent());
    QCOMPARE(mat->diffuseMap(), img);
}

void tst_Q3DSUipParser::customMaterial()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/cube_with_custom_material.uip")));
    QVERIFY(!pres.isNull());

    Q3DSGraphObject *cube = pres->object(QByteArrayLiteral("Cube"));
    QVERIFY(cube);
    QVERIFY(cube->childCount() > 0);
    QCOMPARE(cube->firstChild()->type(), Q3DSGraphObject::CustomMaterial);
    Q3DSCustomMaterialInstance *mat = static_cast<Q3DSCustomMaterialInstance *>(cube->firstChild());
    QVERIFY(mat->material());

    const Q3DSCustomMaterial *m = mat->material();
    QCOMPARE(m->properties().count(), 14);
    QCOMPARE(m->shaders().count(), 1);

    QVERIFY(mat->materialPropertyValues());
    QCOMPARE(mat->materialPropertyValues()->count(), 14);
    const QVariantMap *p = mat->materialPropertyValues();
    QVERIFY(p->contains(QStringLiteral("uEnvironmentTexture")));
    QCOMPARE(p->value("uEnvironmentTexture").toString(), QStringLiteral(":/data/maps/materials/spherical_checker.png"));
}

void tst_Q3DSUipParser::effect()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/effect.uip")));
    QVERIFY(!pres.isNull());

    Q3DSGraphObject *eff = pres->object(QByteArrayLiteral("Depth Of Field HQ Blur_001"));
    QVERIFY(eff);
    QCOMPARE(eff->type(), Q3DSGraphObject::Effect);
    QCOMPARE(eff->parent(), pres->scene()->firstChild());

    Q3DSEffectInstance *e = static_cast<Q3DSEffectInstance *>(eff);
    const QVariantMap *p = e->effectPropertyValues();
    QVERIFY(p);
    QCOMPARE(p->count(), 5);
    QCOMPARE(p->value(QStringLiteral("FocusDistance")).toFloat(), 100.0f);
}

void tst_Q3DSUipParser::primitiveMeshes()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/multislide.uip")));
    QVERIFY(!pres.isNull());

    Q3DSModelNode *sphere = static_cast<Q3DSModelNode *>(pres->object(QByteArrayLiteral("Sphere")));
    QCOMPARE(sphere->type(), Q3DSGraphObject::Model);
    QVERIFY(sphere);

    Q3DSModelNode *cone = static_cast<Q3DSModelNode *>(pres->object(QByteArrayLiteral("Cone")));
    QCOMPARE(cone->type(), Q3DSGraphObject::Model);
    QVERIFY(cone);

    Q3DSModelNode *cylinder = static_cast<Q3DSModelNode *>(pres->object(QByteArrayLiteral("Cylinder")));
    QCOMPARE(cylinder->type(), Q3DSGraphObject::Model);
    QVERIFY(cylinder);

    MeshList m = sphere->mesh();
    QVERIFY(!m.isNull());
    QCOMPARE(m->count(), 1);
    QVERIFY(m->first()->vertexCount() > 0);

    m = cone->mesh();
    QVERIFY(!m.isNull());
    QCOMPARE(m->count(), 1);
    QVERIFY(m->first()->vertexCount() > 0);

    m = cylinder->mesh();
    QVERIFY(!m.isNull());
    QCOMPARE(m->count(), 1);
    QVERIFY(m->first()->vertexCount() > 0);
}

void tst_Q3DSUipParser::customMesh()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/custom_mesh.uip")));
    QVERIFY(!pres.isNull());
    Q3DSModelNode *h = static_cast<Q3DSModelNode *>(pres->object(QByteArrayLiteral("Headphones")));
    QVERIFY(h);
    QCOMPARE(h->childCount(), 6);
    MeshList m = h->mesh();
    QVERIFY(!m.isNull());
    QCOMPARE(m->count(), 6);
    for (int i = 0; i < m->count(); ++i)
        QVERIFY(m->at(i)->vertexCount() > 0);
}

void tst_Q3DSUipParser::group()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/group.uip")));
    QVERIFY(!pres.isNull());
    Q3DSGraphObject *obj = pres->object(QByteArrayLiteral("powerup"));
    QVERIFY(obj);
    QCOMPARE(obj->type(), Q3DSGraphObject::Group);
    QVERIFY(obj->childCount() > 0);
}

void tst_Q3DSUipParser::text()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/text.uip")));
    QVERIFY(!pres.isNull());
    Q3DSGraphObject *obj = pres->object(QByteArrayLiteral("Text"));
    QVERIFY(obj);
    QCOMPARE(obj->type(), Q3DSGraphObject::Text);
    QVERIFY(obj->childCount() == 0);
    Q3DSTextNode *txt = static_cast<Q3DSTextNode *>(obj);
    QCOMPARE(txt->text(), QStringLiteral("A barrel"));
}

void tst_Q3DSUipParser::component()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/component.uip")));
    QVERIFY(!pres.isNull());

    Q3DSComponentNode *comp1 = static_cast<Q3DSComponentNode *>(pres->object(QByteArrayLiteral("CubeComp")));
    QVERIFY(comp1);
    QVERIFY(comp1->masterSlide());
    QCOMPARE(comp1->childCount(), 2);

    Q3DSComponentNode *comp2 = static_cast<Q3DSComponentNode *>(pres->object(QByteArrayLiteral("CubeComp2")));
    QVERIFY(comp2);
    QVERIFY(comp2->masterSlide());
    QCOMPARE(comp2->childCount(), 1);

    QVERIFY(comp1->masterSlide() != comp2->masterSlide());
    QVERIFY(comp1->masterSlide() != pres->masterSlide());

    QCOMPARE(pres->masterSlide()->childCount(), 2);
    QCOMPARE(comp1->masterSlide()->childCount(), 2);
    QCOMPARE(comp2->masterSlide()->childCount(), 2);

    QCOMPARE(comp1->masterSlide()->id(), QByteArrayLiteral("CubeComp-Master"));
    Q3DSSlide *slide = static_cast<Q3DSSlide *>(comp1->masterSlide()->firstChild());
    QCOMPARE(slide->id(), QByteArrayLiteral("CubeComp1-Comp1Slide1"));
    slide = static_cast<Q3DSSlide *>(slide->nextSibling());
    QCOMPARE(slide->id(), QByteArrayLiteral("CubeComp1-Comp1Slide2"));

    QCOMPARE(comp1->firstChild()->id(), QByteArrayLiteral("Cube"));
    QCOMPARE(comp1->firstChild()->nextSibling()->id(), QByteArrayLiteral("Text"));

    QCOMPARE(comp2->firstChild()->id(), QByteArrayLiteral("Cube_001"));
}

void tst_Q3DSUipParser::aoProps()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/barrel_with_ao.uip")));
    QVERIFY(!pres.isNull());

    Q3DSLayerNode *layer = static_cast<Q3DSLayerNode *>(pres->scene()->firstChild());
    QVERIFY(layer);

    QCOMPARE(layer->aoStrength(), 100);
    QCOMPARE(layer->aoSoftness(), 30);
    QCOMPARE(layer->aoBias(), 0.2f);

    QCOMPARE(layer->aoDistance(), 5); // default
}

void tst_Q3DSUipParser::lightmapProps()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/imageBasedLighting.uip")));
    QVERIFY(!pres.isNull());
    QVERIFY(pres->object(QByteArrayLiteral("Default")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_lightmapindirect")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_lightmapradiosity")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_lightmapshadow")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_001")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_001_lightmapindirect")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_001_lightmapradiosity")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_001_lightmapshadow")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_002")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_002_lightmapindirect")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_002_lightmapradiosity")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_002_lightmapshadow")));


    // Default Material
    Q3DSDefaultMaterial *defautMaterial = static_cast<Q3DSDefaultMaterial *>(pres->object(QByteArrayLiteral("Default_001")));
    Q3DSImage *defaultMaterialLightmapIndirect = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_001_lightmapindirect")));
    QCOMPARE(defaultMaterialLightmapIndirect->type(), Q3DSGraphObject::Image);
    QCOMPARE(defaultMaterialLightmapIndirect->parent(), defautMaterial);
    QCOMPARE(defaultMaterialLightmapIndirect->sourcePath(), QByteArrayLiteral(":/data/maps/Gold_01.jpg"));
    QCOMPARE(defaultMaterialLightmapIndirect->mappingMode(), Q3DSImage::UVMapping);

    Q3DSImage *defaultMaterialLightmapRadiosity = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_001_lightmapradiosity")));
    QCOMPARE(defaultMaterialLightmapRadiosity->type(), Q3DSGraphObject::Image);
    QCOMPARE(defaultMaterialLightmapRadiosity->parent(), defautMaterial);
    QCOMPARE(defaultMaterialLightmapRadiosity->sourcePath(), QByteArrayLiteral(":/data/maps/Gold_01.jpg"));
    QCOMPARE(defaultMaterialLightmapRadiosity->mappingMode(), Q3DSImage::UVMapping);

    Q3DSImage *defaultMaterialLightmapShadow = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_001_lightmapshadow")));
    QCOMPARE(defaultMaterialLightmapShadow->type(), Q3DSGraphObject::Image);
    QCOMPARE(defaultMaterialLightmapShadow->parent(), defautMaterial);
    QCOMPARE(defaultMaterialLightmapShadow->sourcePath(), QByteArrayLiteral(":/data/maps/Gold_01.jpg"));
    QCOMPARE(defaultMaterialLightmapShadow->mappingMode(), Q3DSImage::UVMapping);

    QCOMPARE(defautMaterial->lightmapIndirectMap(), defaultMaterialLightmapIndirect);
    QCOMPARE(defautMaterial->lightmapRadiosityMap(), defaultMaterialLightmapRadiosity);
    QCOMPARE(defautMaterial->lightmapShadowMap(), defaultMaterialLightmapShadow);

    // Custom Material
    Q3DSCustomMaterialInstance *customMaterial = static_cast<Q3DSCustomMaterialInstance *>(pres->object(QByteArrayLiteral("Default")));
    Q3DSImage *customMaterialLightmapIndirect = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_lightmapindirect")));
    QCOMPARE(customMaterialLightmapIndirect->type(), Q3DSGraphObject::Image);
    QCOMPARE(customMaterialLightmapIndirect->parent(), customMaterial);
    QCOMPARE(customMaterialLightmapIndirect->sourcePath(), QByteArrayLiteral(":/data/maps/Gold_01.jpg"));
    QCOMPARE(customMaterialLightmapIndirect->mappingMode(), Q3DSImage::UVMapping);

    Q3DSImage *customMaterialLightmapRadiosity = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_lightmapradiosity")));
    QCOMPARE(customMaterialLightmapRadiosity->type(), Q3DSGraphObject::Image);
    QCOMPARE(customMaterialLightmapRadiosity->parent(), customMaterial);
    QCOMPARE(customMaterialLightmapRadiosity->sourcePath(), QByteArrayLiteral(":/data/maps/Gold_01.jpg"));
    QCOMPARE(customMaterialLightmapRadiosity->mappingMode(), Q3DSImage::UVMapping);

    Q3DSImage *customMaterialLightmapShadow = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_lightmapshadow")));
    QCOMPARE(customMaterialLightmapShadow->type(), Q3DSGraphObject::Image);
    QCOMPARE(customMaterialLightmapShadow->parent(), customMaterial);
    QCOMPARE(customMaterialLightmapShadow->sourcePath(), QByteArrayLiteral(":/data/maps/Gold_01.jpg"));
    QCOMPARE(customMaterialLightmapShadow->mappingMode(), Q3DSImage::UVMapping);

    QCOMPARE(customMaterial->lightmapIndirectMap(), customMaterialLightmapIndirect);
    QCOMPARE(customMaterial->lightmapRadiosityMap(), customMaterialLightmapRadiosity);
    QCOMPARE(customMaterial->lightmapShadowMap(), customMaterialLightmapShadow);

    // ReferencedMaterial
    Q3DSReferencedMaterial *referencedMaterial = static_cast<Q3DSReferencedMaterial *>(pres->object(QByteArrayLiteral("Default_002")));
    Q3DSImage *referencedMaterialLightmapIndirect = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_002_lightmapindirect")));
    QCOMPARE(referencedMaterialLightmapIndirect->type(), Q3DSGraphObject::Image);
    QCOMPARE(referencedMaterialLightmapIndirect->parent(), referencedMaterial);
    QCOMPARE(referencedMaterialLightmapIndirect->sourcePath(), QByteArrayLiteral(":/data/maps/Metal_Bronze.png"));
    QCOMPARE(referencedMaterialLightmapIndirect->mappingMode(), Q3DSImage::UVMapping);

    Q3DSImage *referencedMaterialLightmapRadiosity = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_002_lightmapradiosity")));
    QCOMPARE(referencedMaterialLightmapRadiosity->type(), Q3DSGraphObject::Image);
    QCOMPARE(referencedMaterialLightmapRadiosity->parent(), referencedMaterial);
    QCOMPARE(referencedMaterialLightmapRadiosity->sourcePath(), QByteArrayLiteral(":/data/maps/Metal_Bronze.png"));
    QCOMPARE(referencedMaterialLightmapRadiosity->mappingMode(), Q3DSImage::UVMapping);

    Q3DSImage *referencedMaterialLightmapShadow = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_002_lightmapshadow")));
    QCOMPARE(referencedMaterialLightmapShadow->type(), Q3DSGraphObject::Image);
    QCOMPARE(referencedMaterialLightmapShadow->parent(), referencedMaterial);
    QCOMPARE(referencedMaterialLightmapShadow->sourcePath(), QByteArrayLiteral(":/data/maps/Metal_Bronze.png"));
    QCOMPARE(referencedMaterialLightmapShadow->mappingMode(), Q3DSImage::UVMapping);

    QCOMPARE(referencedMaterial->lightmapIndirectMap(), referencedMaterialLightmapIndirect);
    QCOMPARE(referencedMaterial->lightmapRadiosityMap(), referencedMaterialLightmapRadiosity);
    QCOMPARE(referencedMaterial->lightmapShadowMap(), referencedMaterialLightmapShadow);
}

void tst_Q3DSUipParser::iblProps()
{
    Q3DSUipParser parser;
    QScopedPointer<Q3DSPresentation> pres(parser.parse(QLatin1String(":/data/imageBasedLighting.uip")));
    QVERIFY(!pres.isNull());
    QVERIFY(pres->object(QByteArrayLiteral("Layer")));
    QVERIFY(pres->object(QByteArrayLiteral("Layer_lightprobe")));
    QVERIFY(pres->object(QByteArrayLiteral("Layer_lightprobe2")));
    QVERIFY(pres->object(QByteArrayLiteral("Default")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_iblprobe")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_001")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_001_iblprobe")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_002")));
    QVERIFY(pres->object(QByteArrayLiteral("Default_002_iblprobe")));


    // Layer
    Q3DSLayerNode *layerNode = static_cast<Q3DSLayerNode *>(pres->object(QByteArrayLiteral("Layer")));
    Q3DSImage *layerLightprobe1 = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Layer_lightprobe")));
    QCOMPARE(layerLightprobe1->type(), Q3DSGraphObject::Image);
    QCOMPARE(layerLightprobe1->parent(), layerNode);
    QCOMPARE(layerLightprobe1->sourcePath(), QByteArrayLiteral(":/data/maps/TestEnvironment-512.hdr"));
    QCOMPARE(layerLightprobe1->mappingMode(), Q3DSImage::LightProbe);
    Q3DSImage *layerLightprobe2 = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Layer_lightprobe2")));
    QCOMPARE(layerLightprobe2->type(), Q3DSGraphObject::Image);
    QCOMPARE(layerLightprobe2->parent(), layerNode);
    QCOMPARE(layerLightprobe2->sourcePath(), QByteArrayLiteral(":/data/maps/TestEnvironment-512.hdr"));
    QCOMPARE(layerLightprobe2->mappingMode(), Q3DSImage::LightProbe);

    QCOMPARE(layerNode->lightProbe(), layerLightprobe1);
    QCOMPARE(layerNode->lightProbe2(), layerLightprobe2);

    // Default Material
    Q3DSDefaultMaterial *defautMaterial = static_cast<Q3DSDefaultMaterial *>(pres->object(QByteArrayLiteral("Default_001")));
    Q3DSImage *defaultLightprobe = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_001_iblprobe")));
    QCOMPARE(defaultLightprobe->type(), Q3DSGraphObject::Image);
    QCOMPARE(defaultLightprobe->parent(), defautMaterial);
    QCOMPARE(defaultLightprobe->sourcePath(), QByteArrayLiteral(":/data/maps/TestEnvironment-512.hdr"));
    QCOMPARE(defaultLightprobe->mappingMode(), Q3DSImage::IBLOverride);
    QCOMPARE(defautMaterial->lightProbe(), defaultLightprobe);

    // Custom Material
    Q3DSCustomMaterialInstance *customMaterial = static_cast<Q3DSCustomMaterialInstance *>(pres->object(QByteArrayLiteral("Default")));
    Q3DSImage *customLightprobe = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_iblprobe")));
    QCOMPARE(customLightprobe->type(), Q3DSGraphObject::Image);
    QCOMPARE(customLightprobe->parent(), customMaterial);
    QCOMPARE(customLightprobe->sourcePath(), QByteArrayLiteral(":/data/maps/TestEnvironment-512.hdr"));
    QCOMPARE(customLightprobe->mappingMode(), Q3DSImage::IBLOverride);
    QCOMPARE(customMaterial->lightProbe(), customLightprobe);

    // ReferencedMaterial
    Q3DSReferencedMaterial *referencedMaterial = static_cast<Q3DSReferencedMaterial *>(pres->object(QByteArrayLiteral("Default_002")));
    Q3DSImage *referencedLightprobe = static_cast<Q3DSImage *>(pres->object(QByteArrayLiteral("Default_002_iblprobe")));
    QCOMPARE(referencedLightprobe->type(), Q3DSGraphObject::Image);
    QCOMPARE(referencedLightprobe->parent(), referencedMaterial);
    QCOMPARE(referencedLightprobe->sourcePath(), QByteArrayLiteral(":/data/maps/TestEnvironment-512.hdr"));
    QCOMPARE(referencedLightprobe->mappingMode(), Q3DSImage::IBLOverride);
    QCOMPARE(referencedMaterial->lightProbe(), referencedLightprobe);
}

#include <tst_q3dsuipparser.moc>
QTEST_MAIN(tst_Q3DSUipParser)
