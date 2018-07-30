/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of Qt 3D Studio.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QGuiApplication>

#include <private/q3dsengine_p.h>
#include <private/q3dswindow_p.h>

int dyncounter = 1;

void buildDynamicSpawner(Q3DSUipPresentation *pres, Q3DSLayerNode *layer, Q3DSSlide *slide)
{
    Q3DSTextNode *text = pres->newObject<Q3DSTextNode>("dynspawntext");
    text->setText(QLatin1String("Click to spawn"));
    text->setPosition(QVector3D(0, -200, 0));
    text->setColor(Qt::lightGray);

    // Now add some dynamic behavior whenever the text is picked.
    text->addEventHandler(Q3DSGraphObjectEvents::pressureDownEvent(), [pres, layer, slide](const Q3DSGraphObject::Event &) {
        // Create a sphere with a default material. It is important to create
        // and associate them before parenting the model to the scene's
        // hierarchy.
        Q3DSModelNode *newmodel = pres->newObject<Q3DSModelNode>(QByteArrayLiteral("dynmodel") + QByteArray::number(dyncounter));
        Q3DSDefaultMaterial *newmat = pres->newObject<Q3DSDefaultMaterial>(QByteArrayLiteral("dynmat") + QByteArray::number(dyncounter));
        ++dyncounter;

        newmodel->appendChildNode(newmat);
        newmodel->setMesh(QLatin1String("#Sphere"));
        newmodel->setPosition(QVector3D((qrand() % 600) - 300, (qrand() % 600) - 300, 0));

        slide->addObject(newmodel);
        slide->addObject(newmat);

        // Ready to go.
        layer->appendChildNode(newmodel);
    });

    slide->addObject(text);
    layer->appendChildNode(text);
}

void buildCustomMesh(Q3DSUipPresentation *pres, Q3DSLayerNode *layer, Q3DSSlide *slide)
{
    Q3DSModelNode *model = pres->newObject<Q3DSModelNode>("custommeshmodel");
    Q3DSDefaultMaterial *mat = pres->newObject<Q3DSDefaultMaterial>("custommeshmat");
    model->appendChildNode(mat);
    model->setPosition(QVector3D(-300, 200, 0));

    // a triangle, non-indexed
    Q3DSGeometry *geom = new Q3DSGeometry;
    geom->setUsageType(Q3DSGeometry::DynamicMesh);
    geom->setPrimitiveType(Q3DSGeometry::Triangles);
    geom->setDrawCount(3);

    // need position and normal data at minimum for a non-textured default material
    Q3DSGeometry::Buffer b;
    const int stride = (3 + 3) * sizeof(float);
    b.data.resize(geom->drawCount() * stride);
    float *p = reinterpret_cast<float *>(b.data.data());
    // the built-in primitives like the cube go from -50..50, follow this scale for now
    // front face is CCW
    *p++ = -50.0f; *p++ = -50.0f; *p++ = 0.0f; /* normal */ *p++ = 0.0f; *p++ = 0.0f; *p++ = 1.0f;
    *p++ = 50.0f; *p++ = -50.0f; *p++ = 0.0f; /* normal */ *p++ = 0.0f; *p++ = 0.0f; *p++ = 1.0f;
    *p++ = 0.0f; *p++ = 50.0f; *p++ = 0.0f; /* normal */ *p++ = 0.0f; *p++ = 0.0f; *p++ = 1.0f;
    geom->addBuffer(b);

    Q3DSGeometry::Attribute a;
    a.bufferIndex = 0;
    a.semantic = Q3DSGeometry::Attribute::PositionSemantic;
    a.componentCount = 3;
    a.offset = 0;
    a.stride = stride;
    geom->addAttribute(a);

    a.bufferIndex = 0;
    a.semantic = Q3DSGeometry::Attribute::NormalSemantic;
    a.componentCount = 3;
    a.offset = 3 * sizeof(float);
    a.stride = stride;
    geom->addAttribute(a);

    model->setCustomMesh(geom);

    slide->addObject(model);
    slide->addObject(mat);

    Q3DSAnimationTrack anim(Q3DSAnimationTrack::Linear, model, QLatin1String("rotation.y"));
    anim.setKeyFrames({ { 0, 0 }, { 1, 45 }, { 2, 0 }, { 3, -45 }, { 4, 0 }, { 5, 45 }, { 6, 0 }, { 7, -45 }, { 8, 0 } });
    slide->addAnimation(anim);

    layer->appendChildNode(model);

    // now demonstrate dynamic buffer updates
    Q3DSTextNode *text = pres->newObject<Q3DSTextNode>("dynbuftext");
    text->setText(QLatin1String("Move first vertex"));
    text->setPosition(QVector3D(-300, -300, 0));
    text->setColor(Qt::lightGray);
    text->addEventHandler(Q3DSGraphObjectEvents::pressureDownEvent(), [geom, model](const Q3DSGraphObject::Event &) {
        // change the first vertex's y
        float *p = reinterpret_cast<float *>(geom->buffer(0)->data.data());
        *(p + 1) -= 10.0f;
        model->updateCustomMeshBuffer(0, 0, 3 * sizeof(float));
    });
    slide->addObject(text);
    layer->appendChildNode(text);

    text = pres->newObject<Q3DSTextNode>("dynbuftext2");
    text->setText(QLatin1String("Add more vertices"));
    text->setPosition(QVector3D(300, -300, 0));
    text->setColor(Qt::lightGray);
    text->addEventHandler(Q3DSGraphObjectEvents::pressureDownEvent(), [geom, model](const Q3DSGraphObject::Event &) {
        // add a new triangle
        QByteArray *buf = &geom->buffer(0)->data;
        buf->resize(buf->size() + 3 * stride);
        // figure out the last triangle's first vertex's x
        float *p = reinterpret_cast<float *>(buf->data() + (geom->drawCount() - 1) * stride);
        float x = *p + 50.0f;
        p += 6;
        // add 3 new vertices at the end
        *p++ = x; *p++ = -50.0f; *p++ = 0.0f; /* normal */ *p++ = 0.0f; *p++ = 0.0f; *p++ = 1.0f;
        *p++ = x + 100.0f; *p++ = -50.0f; *p++ = 0.0f; /* normal */ *p++ = 0.0f; *p++ = 0.0f; *p++ = 1.0f;
        *p++ = x + 50.0f; *p++ = 50.0f; *p++ = 0.0f; /* normal */ *p++ = 0.0f; *p++ = 0.0f; *p++ = 1.0f;
        geom->setDrawCount(geom->drawCount() + 3);
        model->updateCustomMeshBuffer(0);
    });
    slide->addObject(text);
    layer->appendChildNode(text);
}

Q3DSUipPresentation *build()
{
    QScopedPointer<Q3DSUipPresentation> mainPres(new Q3DSUipPresentation);
    mainPres->setPresentationWidth(800);
    mainPres->setPresentationHeight(480);

    Q3DSScene *scene = mainPres->newObject<Q3DSScene>("scene");
    mainPres->setScene(scene);

    Q3DSSlide *masterSlide = mainPres->newObject<Q3DSSlide>("master");
    Q3DSSlide *slide1 = mainPres->newObject<Q3DSSlide>("slide1");
    slide1->setName(QLatin1String("Slide 1"));
    masterSlide->appendChildNode(slide1);
    Q3DSSlide *slide2 = mainPres->newObject<Q3DSSlide>("slide2");
    slide2->setName(QLatin1String("Slide 2"));
    masterSlide->appendChildNode(slide2);
    mainPres->setMasterSlide(masterSlide);

    // a scene is expected to have at least one layer as its child
    Q3DSLayerNode *layer1 = mainPres->newObject<Q3DSLayerNode>("layer1");
    // properties conveniently default a to a normal, full-size layer
    scene->appendChildNode(layer1);

    // each layer uses the first active camera encountered while walking depth-first
    Q3DSCameraNode *camera1 = mainPres->newObject<Q3DSCameraNode>("camera1");
    // Defaults to a perspective camera with position (0, 0, -600), fov 60,
    // near/far 10/5000. This is good as it is in many cases.
    layer1->appendChildNode(camera1);

    // let's have a light
    Q3DSLightNode *light1 = mainPres->newObject<Q3DSLightNode>("light1");
    // Defaults to a white directional light.
    layer1->appendChildNode(light1);

    Q3DSModelNode *model1 = mainPres->newObject<Q3DSModelNode>("model1");
    // A model needs a mesh
    model1->setMesh(QLatin1String("#Cube")); // let's use a built-in primitive
    layer1->appendChildNode(model1);

    Q3DSDefaultMaterial *mat1 = mainPres->newObject<Q3DSDefaultMaterial>("mat1");
    // defaults to a white material with no texture maps
    model1->appendChildNode(mat1);
    // now we have a white cube

    // associate objects with slides
    masterSlide->addObject(layer1);
    masterSlide->addObject(camera1);
    masterSlide->addObject(light1);
    // put model1 onto slide1, meaning it wont be visible on slide2
    slide1->addObject(model1);
    slide1->addObject(mat1);

    // bonus: let's animate the cube
    // z rotation is 0 at 0 s, 360 at 10 s
    Q3DSAnimationTrack anim(Q3DSAnimationTrack::Linear, model1, QLatin1String("rotation.z"));
    anim.setKeyFrames({ { 0, 0 }, { 10, 360 } });
    slide1->addAnimation(anim);

    // There's a catch: if a channel (rotation in thise case) is not fully
    // specified, the unspecified channels (x and y here) will get dummy keyframes
    // with value 0. Rather, let's rotate a bit around X and Y  as well.
    Q3DSAnimationTrack animX(Q3DSAnimationTrack::Linear, model1, QLatin1String("rotation.x"));
    animX.setKeyFrames({ { 0, 30 } });
    slide1->addAnimation(animX);
    Q3DSAnimationTrack animY(Q3DSAnimationTrack::Linear, model1, QLatin1String("rotation.y"));
    animY.setKeyFrames({ { 0, 40 } });
    slide1->addAnimation(animY);

    buildDynamicSpawner(mainPres.data(), layer1, slide1);

    buildCustomMesh(mainPres.data(), layer1, slide1);

    return mainPres.take();
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    QSurfaceFormat::setDefaultFormat(Q3DS::surfaceFormat());

    Q3DSEngine::Flags flags = Q3DSEngine::EnableProfiling;

    QScopedPointer<Q3DSEngine> engine(new Q3DSEngine);
    QScopedPointer<Q3DSWindow> view(new Q3DSWindow);
    view->setEngine(engine.data());
    engine->setFlags(flags);
    engine->setPresentation(build());

    view->show();

    engine->setOnDemandRendering(true);
    engine->setAutoToggleProfileUi(false);

    return app.exec();
}
