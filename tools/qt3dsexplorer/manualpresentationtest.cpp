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

#include "manualpresentationtest.h"

QT_BEGIN_NAMESPACE

QVector<Q3DSUipPresentation *> ManualPresentationTest::build()
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
    // Defaults to a perspective camera with fov 60, near/far 10/5000. This is
    // good as it is in many cases.
    camera1->setPosition(QVector3D(0, 0, -600));
    layer1->appendChildNode(camera1);

    // let's have a light
    Q3DSLightNode *light1 = mainPres->newObject<Q3DSLightNode>("light1");
    // Defaults to a white directional light.
    layer1->appendChildNode(light1);

    Q3DSModelNode *model1 = mainPres->newObject<Q3DSModelNode>("model1");
    // A model needs a mesh. Meshes are retrieved via
    // Q3DSUipPresentation::mesh() which loads or returns a cached one.
    model1->setMesh(mainPres->mesh(QLatin1String("#Cube"))); // let's use a built-in primitive
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
    // there's a catch: channels must be fully specified, so add dummies for x and y
    Q3DSAnimationTrack dummyX(Q3DSAnimationTrack::Linear, model1, QLatin1String("rotation.x"));
    dummyX.setKeyFrames({ { 0, 30 }, { 10, 30 } });
    slide1->addAnimation(dummyX);
    Q3DSAnimationTrack dummyY(Q3DSAnimationTrack::Linear, model1, QLatin1String("rotation.y"));
    dummyY.setKeyFrames({ { 0, 40 }, { 10, 40 } });
    slide1->addAnimation(dummyY);

    // done, this is a full presentation with a layer, camera, a light and a cube

    return { mainPres.take() };
}

QT_END_NAMESPACE
