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

    return mainPres.take();
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

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
