/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "q3dsscenebuilder.h"
#include "q3dsdefaultmaterialgenerator.h"
#include "q3dstextmaterialgenerator.h"
#include "q3dsanimationbuilder.h"
#include "q3dstextrenderer.h"
#include "q3dsutils.h"
#include "shadergenerator/q3dsshadermanager_p.h"

#include <QDir>
#include <QLoggingCategory>
#include <qmath.h>

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>

#include <Qt3DRender/QRenderSettings>
#include <Qt3DRender/QRenderSurfaceSelector>
#include <Qt3DRender/QRenderTargetSelector>
#include <Qt3DRender/QRenderTarget>
#include <Qt3DRender/QLayerFilter>
#include <Qt3DRender/QLayer>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QCameraSelector>
#include <Qt3DRender/QViewport>
#include <Qt3DRender/QClearBuffers>
#include <Qt3DRender/QSortPolicy>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QTextureImage>
#include <Qt3DRender/QPaintedTextureImage>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QTechniqueFilter>
#include <Qt3DRender/QGraphicsApiFilter>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QRenderPassFilter>
#include <Qt3DRender/QFilterKey>
#include <Qt3DRender/QNoDraw>
#include <Qt3DRender/QFrontFace>
#include <Qt3DRender/QBlendEquation>
#include <Qt3DRender/QBlendEquationArguments>
#include <Qt3DRender/QDepthTest>
#include <Qt3DRender/QNoDepthMask>
#include <Qt3DRender/QColorMask>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QBuffer>

#include <Qt3DAnimation/QClipAnimator>

#include <Qt3DExtras/QPlaneMesh>

#include <Qt3DInput/QInputSettings>

#include <Qt3DLogic/QFrameAction>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcScene, "q3ds.scene")

/*
    Approx. scene structure:

    // m_rootEntity
    Entity {
        components: [
            RenderSettings {
                activeFrameGraph: RenderSurfaceSelector { // frameGraphRoot

                    // Layer #1 framegraph
                    RenderTargetSelector { Viewport { CameraSelector {

                        // objects in the scene will be tagged with these. note
                        // that entities without geometryrenderers will have
                        // both so that we do not lose the transforms.
                        Layer { id: layer1Opaque }
                        Layer { id: layer1Transparent }

                        // 0.1 optional texture prefiltering via compute [not currently implemented, although material provides renderpass already]
                        TechniqueFilter {
                            matchAll: [ FilterKey { name: "type"; value: "bsdfPrefilter"} ]
                            NoDraw { }
                            // [creation of the rest below is deferred]
                            DispatchCompute {
                               ... // with appropriate technique filter
                            }
                        }

                        TechniqueFilter {
                            matchAll: [ FilterKey { name: "type"; value: "main"} ]

                            // 1.1 optional depth texture generation
                            RenderTargetSelector {
                                NoDraw { } // so that we do not issue drawcalls when depth texture is not needed
                                // [creation of the rest below is deferred]
                                ... // depth texture (depth attachment only)
                                ClearBuffers {
                                    // depth clear
                                    enabled: when depth texture is needed
                                    NoDraw { }
                                }
                                RenderPassFilter {
                                    matchAny: [ FilterKey { name: "pass"; value: "depth" } ]
                                    LayerFilter {
                                        layers: [ layer1Opaque ] or empty list if depth texture gets disabled afterwards
                                    }
                                }
                                RenderPassFilter {
                                    matchAny: [ FilterKey { name: "pass"; value: "depth" } ]
                                    SortPolicy {
                                        sortTypes: [ SortPolicy.BackToFront ]
                                        LayerFilter {
                                            layers: [ layer1Transparent ] or empty list if depth texture gets disabled afterwards
                                        }
                                    }
                                }
                            }

                            // 1.2 optional SSAO texture generation (depends on depth texture)
                            RenderTargetSelector {
                                NoDraw { } // so that we do not issue drawcalls when depth texture is not needed
                                // [creation of the rest below is deferred]
                                ... // color attachment only, rgba8 texture
                                ClearBuffers {
                                    // color clear
                                    enabled: when ssao is enabled
                                    NoDraw { }
                                }
                                RenderPassFilter {
                                    matchAny: [ FilterKey { name: "pass"; value: "ssao" } ]
                                    LayerFilter {
                                        layers: [ fsQuad ] or empty list if ssao gets disabled afterwards
                                    }
                                }
                            }

                            // 1.3 optional shadow map generation
                            FrameGraphNode {
                                NoDraw { } // so that we do not issue drawcalls when there are no shadow casting lights in the scene
                                // [the creation of the rest is deferred and is *repeated for each shadow casting light* hence the need for a dummy FrameGraphNode as our sub-root ]
                                for each cubemap face (if cubemap is used):
                                    RenderTargetSelector {
                                        ... // per-light shadow map texture (2d or cubemap) of R16 as color, throwaway depth-stencil; select the current face when cubemap
                                        Viewport {
                                            ClearBuffers {
                                                NoDraw { }
                                            }
                                            RenderPassFilter {
                                                matchAny: [ FilterKey { name: "pass"; value: "shadowCube" } ] // or "shadowOrtho" depending on the light type
                                                LayerFilter {
                                                    layers: [ layer1Opaque ]
                                                }
                                            }
                                            RenderPassFilter {
                                                matchAny: [ FilterKey { name: "pass"; value: "shadowCube" } ] // or "shadowOrtho" depending on the light type
                                                SortPolicy {
                                                    sortTypes: [ SortPolicy.BackToFront ]
                                                    LayerFilter {
                                                        layers: [ layer1Transparent ] or empty list when depth texture is not needed
                                                    }
                                                }
                                            }
                                        }
                                    }
                                // cubemap blur X
                                RenderTargetSelector {
                                    ... // input is 6 cubemap faces attached to color0-5, output is another cubemap
                                    RenderPassFilter {
                                        matchAny: [ FilterKey { name: "pass"; value: "shadowCubeBlurX" } ]
                                        LayerFilter {
                                            layers: [ fsQuad ]
                                        }
                                    }
                                }
                                // repeat for cubemap blur Y
                                RenderTargetSelector {
                                    ...
                                }
                                ... // repeat for orthographic blur, concept is the same, input/output is a 2d texture, passes are shadowOrthoBlurX and Y
                            }

                            // 2. main clear
                            ClearBuffers {
                                NoDraw { }
                            }

                            // 3. depth pre-pass, optional depending on layer flags
                            RenderPassFilter {
                                matchAny: [ FilterKey { name: "pass"; value: "depth" } ]
                                LayerFilter {
                                    layers: [ layer1Opaque ] or empty list when DisableDepthPrePass is set
                                }
                            }

                            // 4. opaque pass
                            RenderPassFilter {
                                matchAny: [ FilterKey { name: "pass"; value: "opaque" } ]
                                LayerFilter {
                                    layers: [ layer1Opaque ]
                                }
                            }

                            // 5. transparent pass
                            RenderPassFilter {
                                matchAny: [ FilterKey { name: "pass"; value: "transparent" } ]
                                SortPolicy {
                                    sortTypes: [ SortPolicy.BackToFront ]
                                    LayerFilter {
                                        layers: [ layer1Transparent ]
                                    }
                                }
                            }
                        }
                    } } }

                    // Layer #2 framegraph
                    ...

                    // compositor framegraph
                    LayerFilter {
                        Layer { id: compositorTag }
                        layers: [ compositorTag ]
                        ...
                    }
                }
            },
            InputSettings { }
        ]

        // compositorEntity
        Entity { ... } // tagged with compositorTag

        // fullscreen quad for orthographic and cube shadow blurs and SSAO texture generation
        Entity {
            ... // tagged with fsQuadTag
            material component has renderpasses with keys:
              shadowOrthoBlurX, shadowOrthoBlurY
              shadowCubeBlurX, shadowCubeBlurY
              ssao
        }
    }

    Generic materials are expected to provide a technique with a number of render passes.
    Specialized materials may drop some of the passes (e.g. text only provides transparent).

    filterKeys: [ FilterKey { name: "type"; value: "main" } ]
    renderPasses: [
        RenderPass {
            filterKeys: [ FilterKey { id: depthKey; name: "pass"; value: "depth" } ]
            renderStates: [
                DepthTest {
                    depthFunction: DepthTest.LessOrEqual
                },
                ColorMask {
                    redMasked: false; greenMasked: false; blueMasked: false; alphaMasked: false
                }
            ]
            ...
        },
        RenderPass {
            filterKeys: [ FilterKey { id: shadowOrthoKey; name: "pass"; value: "shadowOrtho" } ]
            renderStates: [
                DepthTest {
                    depthFunction: DepthTest.LessOrEqual
                }
            ]
            ...
        },
        RenderPass {
            filterKeys: [ FilterKey { id: shadowCubeKey; name: "pass"; value: "shadowCube" } ]
            renderStates: [
                DepthTest {
                    depthFunction: DepthTest.LessOrEqual
                }
            ]
            ...
        },
        RenderPass {
            filterKeys: [ FilterKey { id: opaqueKey; name: "pass"; value: "opaque" } ]
            renderStates: [
                DepthTest {
                    depthFunction: DepthTest.LessOrEqual
                },
                NoDepthMask {
                    enabled: when DisableDepthPrePass is not set
                }
            ]
            ...
        },
        RenderPass {
            filterKeys: [ FilterKey { id: transparentKey; name: "pass"; value: "transparent" } ]
            renderStates: [
                DepthTest {
                    depthFunction: DepthTest.LessOrEqual
                },
                NoDepthMask {
                },
                BlendEquation {
                    blendFunction: BlendEquation.Add
                },
                BlendEquationArguments {
                    sourceRgb: BlendEquationArguments.One
                    destinationRgb: BlendEquationArguments.OneMinusSourceAlpha
                    sourceAlpha: BlendEquationArguments.One
                    destinationAlpha: BlendEquationArguments.OneMinusSourceAlpha
                }
            ]
            ...
        }
    ]

    Compute pass for texture prefiltering is provided as a separate technique with "type" == "bsdfPrefilter".

*/

Q3DSSceneBuilder::Q3DSSceneBuilder(const Q3DSGraphicsLimits &limits)
    : m_gfxLimits(limits),
      m_matGen(new Q3DSDefaultMaterialGenerator),
      m_textMatGen(new Q3DSTextMaterialGenerator),
      m_animBuilder(new Q3DSAnimationBuilder),
      m_textRenderer(new Q3DSTextRenderer)
{
    const QString fontDir = Q3DSUtils::resourcePrefix() + QLatin1String("res/Font");
    m_textRenderer->registerFonts({ fontDir });
}

Q3DSSceneBuilder::~Q3DSSceneBuilder()
{
    delete m_textRenderer;
    delete m_animBuilder;
    delete m_textMatGen;
    delete m_matGen;
    delete m_frameUpdater;
}

void Q3DSSceneBuilder::updateSizes(QWindow *window)
{
    if (!m_scene)
        return;

    qCDebug(lcScene) << "Window size" << window->size() << "DPR" << window->devicePixelRatio();

    Q3DSPresentation::forAllLayers(m_scene, [=](Q3DSLayerNode *layer3DS) {
        updateSizesForLayer(layer3DS, window->size() * window->devicePixelRatio()); });
}

void Q3DSSceneBuilder::prepareSceneChange()
{
    qCDebug(lcScene, "prepareSceneChange");

    delete m_frameUpdater;
    m_frameUpdater = nullptr;

    m_animBuilder->clearPendingChanges();

    Q3DSShaderManager::instance().invalidate();
}

Q3DSSceneBuilder::Scene Q3DSSceneBuilder::buildScene(Q3DSPresentation *presentation, QWindow *window, SceneBuilderFlags flags)
{
    if (!presentation->scene()) {
        qWarning("Q3DSSceneBuilder: No scene?");
        return Scene();
    }

    qCDebug(lcScene, "Building scene for %s (window %p, flags 0x%x)", qPrintable(presentation->sourceFile()), window, int(flags));

    const QString projectFontDir = QFileInfo(presentation->sourceFile()).canonicalPath() + QLatin1Char('/') + QLatin1String("fonts");
    if (QDir(projectFontDir).exists())
        m_textRenderer->registerFonts({ projectFontDir });

    m_flags = flags;
    // Layer MSAA is only available through multisample textures (GLES 3.1+ or GL 3.2+) at the moment. (QTBUG-63382)
    // Drop the flag is this is not supported.
    if (m_flags.testFlag(LayerMSAA4x) && !m_gfxLimits.multisampleTextureSupported)
        m_flags.setFlag(LayerMSAA4x, false);

    m_presentation = presentation;
    m_presentationSize = QSize(m_presentation->presentationWidth(), m_presentation->presentationHeight());
    m_scene = m_presentation->scene();
    m_masterSlide = m_presentation->masterSlide();
    m_currentSlide = nullptr;
    m_pendingNodeShow.clear();
    m_pendingNodeHide.clear();

    // Enter the first slide. (apply property changes from master+first)
    if (!m_masterSlide) {
        qWarning("Q3DSSceneBuilder: No master slide?");
        return Scene();
    }
    if (!m_masterSlide->firstChild()) {
        qWarning("Q3DSSceneBuilder: No slides?");
        return Scene();
    }
    m_currentSlide = static_cast<Q3DSSlide *>(m_masterSlide->firstChild());
    if (!m_masterSlide->attached())
        m_masterSlide->setAttached(new Q3DSSlideAttached);
    if (!m_currentSlide->attached())
        m_currentSlide->setAttached(new Q3DSSlideAttached);

    m_presentation->applySlidePropertyChanges(m_currentSlide);

    // Kick off the Qt3D scene.
    m_rootEntity = new Qt3DCore::QEntity;
    m_rootEntity->setObjectName(QLatin1String("root"));

    // Property change processing happens in a frame action
    Qt3DLogic::QFrameAction *nodeUpdater = new Qt3DLogic::QFrameAction;
    m_frameUpdater = new Q3DSFrameUpdater(this);
    QObject::connect(nodeUpdater, &Qt3DLogic::QFrameAction::triggered, m_frameUpdater, &Q3DSFrameUpdater::frameAction);
    m_rootEntity->addComponent(nodeUpdater);

    Qt3DRender::QRenderSettings *frameGraphComponent = new Qt3DRender::QRenderSettings(m_rootEntity);
    Qt3DRender::QRenderSurfaceSelector *frameGraphRoot = new Qt3DRender::QRenderSurfaceSelector;

    const QSize winPixelSize = window->size() * window->devicePixelRatio();

    m_fsQuadTag = new Qt3DRender::QLayer;

    // Prepare image objects (these are non-nodes and not covered in layer building below).
    m_presentation->forAllImages([this](Q3DSImage *image) {
        Q3DSImageAttached *data = new Q3DSImageAttached;
        data->entity = m_rootEntity; // must set an entity to to make Q3DSImage properties animatable, just use the root
        image->setAttached(data);
        image->addPropertyChangeObserver(std::bind(&Q3DSSceneBuilder::handlePropertyChange, this,
                                                   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    });

    // Build the (offscreen) Qt3D scene
    Q3DSPresentation::forAllLayers(m_scene, [=](Q3DSLayerNode *layer3DS) { buildLayer(layer3DS, frameGraphRoot, winPixelSize); });

    // Onscreen compositor
    buildCompositor(frameGraphRoot, m_rootEntity);

    // Fullscreen quad for bluring the shadow map/cubemap
    Q3DSShaderManager &sm(Q3DSShaderManager::instance());
    QStringList fsQuadPassNames { QLatin1String("shadowOrthoBlurX"), QLatin1String("shadowOrthoBlurY") };
    QVector<Qt3DRender::QShaderProgram *> fsQuadPassProgs { sm.getOrthoShadowBlurXShader(), sm.getOrthoShadowBlurYShader() };
    if (m_gfxLimits.maxDrawBuffers >= 6) { // ###
        fsQuadPassNames << QLatin1String("shadowCubeBlurX") << QLatin1String("shadowCubeBlurY");
        fsQuadPassProgs << sm.getCubeShadowBlurXShader(m_gfxLimits) << sm.getCubeShadowBlurYShader(m_gfxLimits);
    }
    fsQuadPassNames << QLatin1String("ssao");
    fsQuadPassProgs << sm.getSsaoTextureShader();
    buildFsQuad(m_rootEntity, fsQuadPassNames, fsQuadPassProgs, m_fsQuadTag);

    // Ready to go (except that the sizes calculated from window->size() are likely bogus, those will get updated in updateSizes())
    frameGraphRoot->setSurface(window);
    frameGraphComponent->setActiveFrameGraph(frameGraphRoot);
    m_rootEntity->addComponent(frameGraphComponent);

    // Input
    Qt3DInput::QInputSettings *inputSettings = new Qt3DInput::QInputSettings;
    inputSettings->setEventSource(window);
    m_rootEntity->addComponent(inputSettings);

    // Set visibility of objects in the scene and start animations.
    Q3DSPresentation::forAllObjectsOfType(m_masterSlide, Q3DSGraphObject::Slide,
                                          [this](Q3DSGraphObject *s) {
        for (Q3DSGraphObject *obj : *static_cast<Q3DSSlide *>(s)->objects()) {
            const bool visible = scheduleNodeVisibilityUpdate(obj);
            if (obj->type() == Q3DSGraphObject::Component) {
                // objects on the Component's current (or master) slide
                Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
                for (Q3DSGraphObject *cobj : *comp->masterSlide()->objects())
                    scheduleNodeVisibilityUpdate(cobj, comp);
                for (Q3DSGraphObject *cobj : *comp->currentSlide()->objects())
                    scheduleNodeVisibilityUpdate(cobj, comp);
                if (visible) { // if Component is not on the current slide, then don't care for now
                    updateAnimations(comp->masterSlide(), comp->currentSlide());
                    updateAnimations(comp->currentSlide(), comp->currentSlide());
                }
            }
        }
    });
    updateAnimations(m_masterSlide, m_currentSlide);
    updateAnimations(m_currentSlide, m_currentSlide);

    Scene sc;
    sc.rootEntity = m_rootEntity;
    sc.renderSettings = frameGraphComponent;
    sc.inputSettings = inputSettings;
    return sc;
}

// layers use the first Active (eyeball on) camera for rendering
static Q3DSCameraNode *findFirstCamera(Q3DSLayerNode *layer3DS)
{
    // Pick the first active camera encountered when walking depth-first.
    std::function<Q3DSCameraNode *(Q3DSGraphObject *)> f;
    f = [&f](Q3DSGraphObject *obj) -> Q3DSCameraNode* {
        while (obj) {
            if (obj->type() == Q3DSGraphObject::Camera) {
                Q3DSCameraNode *cam = static_cast<Q3DSCameraNode *>(obj);
                // ### should use globalVisibility (which is only set in buildLayerCamera first...)
                const bool active = cam->flags().testFlag(Q3DSNode::Active);
                if (active)
                    return cam;
            }
            if (Q3DSCameraNode *c = f(obj->firstChild()))
                return c;
            obj = obj->nextSibling();
        }
        return nullptr;
    };
    return f(layer3DS->firstChild());
}

static Qt3DRender::QSortPolicy *opaquePassSortPolicy(Qt3DCore::QNode *parent = nullptr)
{
    Qt3DRender::QSortPolicy *sortPolicy = new Qt3DRender::QSortPolicy(parent);
    sortPolicy->setSortTypes(QVector<Qt3DRender::QSortPolicy::SortType>() << Qt3DRender::QSortPolicy::FrontToBack);
    return sortPolicy;
}

static Qt3DRender::QSortPolicy *transparentPassSortPolicy(Qt3DCore::QNode *parent = nullptr)
{
    Qt3DRender::QSortPolicy *sortPolicy = new Qt3DRender::QSortPolicy(parent);
    sortPolicy->setSortTypes(QVector<Qt3DRender::QSortPolicy::SortType>() << Qt3DRender::QSortPolicy::BackToFront);
    return sortPolicy;
}

Qt3DRender::QFrameGraphNode *Q3DSSceneBuilder::buildLayer(Q3DSLayerNode *layer3DS,
                                                          Qt3DRender::QFrameGraphNode *parent,
                                                          const QSize &parentSize)
{
    Qt3DRender::QRenderTargetSelector *rtSelector = new Qt3DRender::QRenderTargetSelector(parent);
    Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;

    Qt3DRender::QRenderTargetOutput *color = new Qt3DRender::QRenderTargetOutput;
    color->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
    Qt3DRender::QAbstractTexture *colorTex;
    if (m_flags.testFlag(LayerMSAA4x)) {
        colorTex = new Qt3DRender::QTexture2DMultisample;
        colorTex->setSamples(4);
    } else {
        colorTex = new Qt3DRender::QTexture2D;
    }
    colorTex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
    colorTex->setWidth(parentSize.width());
    colorTex->setHeight(parentSize.height());
    colorTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    colorTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    color->setTexture(colorTex);

    // GLES <= 3.1 does not have glFramebufferTexture and support for combined
    // depth-stencil textures. Here we rely on the fact the Qt3D will
    // transparently switch to using a renderbuffer in place of a texture. Using
    // separate textures is problematic for stencil since the suitable texture
    // type is an ES 3.1 extension, so that is not an option either for <= 3.1.
    //
    // The internal difference (renderbuffer vs. texture) won't matter as long
    // as a custom material or other thing does not need the depth texture.
    // When that happens, we will be in trouble when running on GLES < 3.2 ...
    // An option then would be to get rid of stencil since plain depth textures
    // work in GLES >= 3.0.

    Qt3DRender::QRenderTargetOutput *ds = new Qt3DRender::QRenderTargetOutput;
    ds->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::DepthStencil);
    Qt3DRender::QAbstractTexture *dsTexOrRb;
    if (m_flags.testFlag(LayerMSAA4x)) {
        dsTexOrRb = new Qt3DRender::QTexture2DMultisample;
        dsTexOrRb->setSamples(4);
    } else {
        dsTexOrRb = new Qt3DRender::QTexture2D;
    }
    dsTexOrRb->setFormat(Qt3DRender::QAbstractTexture::D24S8);
    dsTexOrRb->setWidth(parentSize.width());
    dsTexOrRb->setHeight(parentSize.height());
    dsTexOrRb->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    dsTexOrRb->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    ds->setTexture(dsTexOrRb);

    rt->addOutput(color);
    rt->addOutput(ds);
    rtSelector->setTarget(rt);

    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(rtSelector);
    viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

    Qt3DRender::QCameraSelector *cameraSelector = new Qt3DRender::QCameraSelector(viewport);
    // pick and build the camera; will insert the new QCamera into the entity hierarchy later
    Qt3DRender::QCamera *camera = nullptr;
    Q3DSCameraNode *cam3DS = chooseLayerCamera(layer3DS, &camera);
    cameraSelector->setCamera(camera);

    Qt3DRender::QTechniqueFilter *mainTechniqueSelector = new Qt3DRender::QTechniqueFilter(cameraSelector);
    Qt3DRender::QFilterKey *techniqueFilterKey = new Qt3DRender::QFilterKey;
    techniqueFilterKey->setName(QLatin1String("type"));
    techniqueFilterKey->setValue(QLatin1String("main"));
    mainTechniqueSelector->addMatch(techniqueFilterKey);

    Qt3DRender::QLayer *opaqueTag = new Qt3DRender::QLayer;
    opaqueTag->setObjectName(QObject::tr("Opaque pass tag"));
    Qt3DRender::QLayer *transparentTag = new Qt3DRender::QLayer;
    transparentTag->setObjectName(QObject::tr("Transparent pass tag"));

    // Depth texture pass, optional, with its own rendertarget and clear. Just
    // a placeholder for now since it is disabled by default.
    Qt3DRender::QRenderTargetSelector *depthRtSelector = new Qt3DRender::QRenderTargetSelector(mainTechniqueSelector);
    // Having a NoDraw leaf is essential while the depth texture is not
    // generated (since we must not make draw calls from this leaf), and will
    // [hopefully] not cause any harm later on, when depthRtSelector gets
    // another child, either.
    new Qt3DRender::QNoDraw(depthRtSelector);

    // ditto for SSAO
    Qt3DRender::QRenderTargetSelector *ssaoRtSelector = new Qt3DRender::QRenderTargetSelector(mainTechniqueSelector);
    new Qt3DRender::QNoDraw(ssaoRtSelector);

    // ditto for shadow maps but here there will be multiple rendertargetselectors later, so use a dummy node as root
    Qt3DRender::QFrameGraphNode *shadowRoot = new Qt3DRender::QFrameGraphNode(mainTechniqueSelector);
    new Qt3DRender::QNoDraw(shadowRoot);

    // Clear op for depth pre, opaque, transparent passes.
    Qt3DRender::QClearBuffers *clearBuffers = new Qt3DRender::QClearBuffers(mainTechniqueSelector);
    clearBuffers->setBuffers(Qt3DRender::QClearBuffers::ColorDepthStencilBuffer);
    new Qt3DRender::QNoDraw(clearBuffers);

    // Depth pre-pass, optional
    Qt3DRender::QRenderPassFilter *depthPreFilter = new Qt3DRender::QRenderPassFilter(mainTechniqueSelector);
    Qt3DRender::QFilterKey *depthFilterKey = new Qt3DRender::QFilterKey;
    depthFilterKey->setName(QLatin1String("pass"));
    depthFilterKey->setValue(QLatin1String("depth"));
    depthPreFilter->addMatch(depthFilterKey);
    Qt3DRender::QSortPolicy *depthPreSortPolicy = opaquePassSortPolicy(depthPreFilter);
    Qt3DRender::QLayerFilter *depthPreLayerFilter = new Qt3DRender::QLayerFilter(depthPreSortPolicy);
    if (!layer3DS->layerFlags().testFlag(Q3DSLayerNode::DisableDepthPrePass))
        depthPreLayerFilter->addLayer(opaqueTag); // opaque only, transparent objects must not be present

    // Opaque pass
    Qt3DRender::QRenderPassFilter *opaqueFilter = new Qt3DRender::QRenderPassFilter(mainTechniqueSelector);
    Qt3DRender::QFilterKey *opaqueFilterKey = new Qt3DRender::QFilterKey;
    opaqueFilterKey->setName(QLatin1String("pass"));
    opaqueFilterKey->setValue(QLatin1String("opaque"));
    opaqueFilter->addMatch(opaqueFilterKey);
    Qt3DRender::QSortPolicy *opaqueSortPolicy = opaquePassSortPolicy(opaqueFilter);
    Qt3DRender::QLayerFilter *opaqueLayerFilter = new Qt3DRender::QLayerFilter(opaqueSortPolicy);
    opaqueLayerFilter->addLayer(opaqueTag);

    // Transparent pass, sort back to front
    Qt3DRender::QRenderPassFilter *transFilter = new Qt3DRender::QRenderPassFilter(mainTechniqueSelector);
    Qt3DRender::QFilterKey *transFilterKey = new Qt3DRender::QFilterKey;
    transFilterKey->setName(QLatin1String("pass"));
    transFilterKey->setValue(QLatin1String("transparent"));
    transFilter->addMatch(transFilterKey);
    Qt3DRender::QSortPolicy *transSortPolicy = transparentPassSortPolicy(transFilter);
    Qt3DRender::QLayerFilter *transLayerFilter = new Qt3DRender::QLayerFilter(transSortPolicy);
    transLayerFilter->addLayer(transparentTag);

    Q3DSLayerAttached *data = new Q3DSLayerAttached;
    data->entity = m_rootEntity; // must set an entity to to make Q3DSLayerNode properties animatable, just use the root
    data->layer3DS = layer3DS;
    data->cam3DS = cam3DS;
    data->cameraSelector = cameraSelector;
    data->clearBuffers = clearBuffers;
    data->layerSize = calculateLayerSize(layer3DS, parentSize);
    data->parentSize = parentSize;
    data->opaqueTag = opaqueTag;
    data->transparentTag = transparentTag;
    data->cameraPropertiesParam = new Qt3DRender::QParameter(QLatin1String("camera_properties"), QVector2D(10, 5000));
    if (cam3DS)
        data->cameraPropertiesParam->setValue(QVector2D(cam3DS->clipNear(), cam3DS->clipFar()));

    data->depthTextureData.rtSelector = depthRtSelector;
    data->ssaoTextureData.rtSelector = ssaoRtSelector;
    data->shadowMapData.shadowRoot = shadowRoot;

    // textures that are resized automatically to match the layer's dimensions
    data->sizeManagedTextures << colorTex << dsTexOrRb;

    layer3DS->setAttached(data);

    setLayerSizeProperties(layer3DS); // mostly no-op at this stage but just in case
    setLayerProperties(layer3DS);

    layer3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneBuilder::handlePropertyChange, this,
                                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // Now add the scene contents.
    Q3DSGraphObject *obj = layer3DS->firstChild();
    Qt3DCore::QEntity *layerSceneRootEntity = nullptr;
    while (obj) {
        if (!layerSceneRootEntity) {
            layerSceneRootEntity = new Qt3DCore::QEntity(rtSelector);
            layerSceneRootEntity->setObjectName(QObject::tr("root for %1").arg(QString::fromUtf8(layer3DS->id())));
        }
        buildLayerScene(obj, layer3DS, layerSceneRootEntity);
        obj = obj->nextSibling();
    }

    // Phase 2: deferred stuff

    // Gather lights for this layer.
    gatherLights(layer3DS, &data->allLights, &data->nonAreaLights, &data->areaLights, &data->lightNodes);
    qCDebug(lcScene, "Layer %s has %d lights in total (%d non-area, %d area)", layer3DS->id().constData(),
            data->allLights.count(), data->nonAreaLights.count(), data->areaLights.count());
    updateShadowMapStatus(layer3DS); // must be done before generating materials below

    // Enable SSAO (and depth texture generation) when needed. Must be done
    // before generating materials below.
    updateSsaoStatus(layer3DS);

    // Generate Qt3D material components.
    Q3DSPresentation::forAllModels(layer3DS->firstChild(),
                                   [this](Q3DSModelNode *model3DS) { buildModelMaterial(model3DS); },
                                   true); // include hidden ones too

    // Make sure the QCamera we will use is parented correctly.
    reparentCamera(layer3DS);

    return rtSelector;
}

void Q3DSSceneBuilder::reparentCamera(Q3DSLayerNode *layer3DS)
{
    // Insert the active QCamera into the hierarchy. This matters when the
    // Q3DSCameraNode is a child and so the QCamera' viewMatrix is expected to
    // get multiplied with the parent's worldTransform. Qt 3D handles this, but
    // for that we have to parent the QCamera correctly.
    //
    // (NB! applying a rotation on a parent node is not the same as rotating
    // the camera itself via its rotation property since the former multiplies
    // the lookAt matrix with a rotation matrix whereas the latter recalculates
    // the lookAt transform with new parameters)

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    if (layerData->cam3DS && layerData->cam3DS->parent()) {
        Q3DSGraphObject *obj = layerData->cam3DS->parent();
        if (obj->isNode()) {
            Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(obj->attached());
            if (data && data->entity)
                layerData->cameraSelector->camera()->setParent(data->entity);
        }
    }
}

static void addDepthTest(Qt3DRender::QRenderPass *pass, Qt3DRender::QDepthTest::DepthFunction depthFunction = Qt3DRender::QDepthTest::LessOrEqual)
{
    Qt3DRender::QDepthTest *depthTest = new Qt3DRender::QDepthTest;
    depthTest->setDepthFunction(depthFunction);
    pass->addRenderState(depthTest);
}

static void addNoColorWrite(Qt3DRender::QRenderPass *pass)
{
    Qt3DRender::QColorMask *noColorWrite = new Qt3DRender::QColorMask;
    noColorWrite->setRedMasked(false);
    noColorWrite->setGreenMasked(false);
    noColorWrite->setBlueMasked(false);
    noColorWrite->setAlphaMasked(false);
    pass->addRenderState(noColorWrite);
}

// This is for materials (as opposed to layers)
static void setBlending(Qt3DRender::QBlendEquation *blendFunc,
                        Qt3DRender::QBlendEquationArguments *blendArgs,
                        Q3DSDefaultMaterial::BlendMode blendMode)
{
    switch (blendMode) {
    case Q3DSLayerNode::Screen:
        blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
        blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::SourceAlpha);
        blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::One);
        break;

    case Q3DSLayerNode::Multiply:
        blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
        blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::DestinationColor);
        blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::Zero);
        blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::One);
        break;

    // ### add extension specific modes

    default: // Normal
        blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
        blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::SourceAlpha);
        blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
        blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
        break;
    }
}

QVector<Qt3DRender::QRenderPass *> Q3DSSceneBuilder::standardRenderPasses(Qt3DRender::QShaderProgram *program,
                                                                          Q3DSLayerNode *layer3DS,
                                                                          Q3DSDefaultMaterial::BlendMode blendMode,
                                                                          bool hasDisplacement)
{
    Qt3DRender::QFilterKey *shadowOrthoFilterKey = new Qt3DRender::QFilterKey;
    shadowOrthoFilterKey->setName(QLatin1String("pass"));
    shadowOrthoFilterKey->setValue(QLatin1String("shadowOrtho"));
    Qt3DRender::QFilterKey *shadowCubeFilterKey = new Qt3DRender::QFilterKey;
    shadowCubeFilterKey->setName(QLatin1String("pass"));
    shadowCubeFilterKey->setValue(QLatin1String("shadowCube"));
    Qt3DRender::QFilterKey *depthFilterKey = new Qt3DRender::QFilterKey;
    depthFilterKey->setName(QLatin1String("pass"));
    depthFilterKey->setValue(QLatin1String("depth"));
    Qt3DRender::QFilterKey *opaqueFilterKey = new Qt3DRender::QFilterKey;
    opaqueFilterKey->setName(QLatin1String("pass"));
    opaqueFilterKey->setValue(QLatin1String("opaque"));
    Qt3DRender::QFilterKey *transFilterKey = new Qt3DRender::QFilterKey;
    transFilterKey->setName(QLatin1String("pass"));
    transFilterKey->setValue(QLatin1String("transparent"));

    // Shadow map for directional lights.
    Qt3DRender::QRenderPass *shadowOrthoPass = new Qt3DRender::QRenderPass;
    shadowOrthoPass->addFilterKey(shadowOrthoFilterKey);
    addDepthTest(shadowOrthoPass);

    // Shadow map for point and area lights. (omnidirectional shadows -> uses a cubemap texture)
    Qt3DRender::QRenderPass *shadowCubePass = new Qt3DRender::QRenderPass;
    shadowCubePass->addFilterKey(shadowCubeFilterKey);
    addDepthTest(shadowCubePass);

    // Depth pre-pass to help early Z. Also used to generate the optional depth texture.
    Qt3DRender::QRenderPass *depthPass = new Qt3DRender::QRenderPass;
    depthPass->addFilterKey(depthFilterKey);
    addDepthTest(depthPass);
    addNoColorWrite(depthPass);

    // Opaque objects.
    Qt3DRender::QRenderPass *opaquePass = new Qt3DRender::QRenderPass;
    opaquePass->addFilterKey(opaqueFilterKey);
    addDepthTest(opaquePass);
    // Depth buffer is already filled when depth prepass is enabled.
    Qt3DRender::QNoDepthMask *opaqueNoDepthWrite = new Qt3DRender::QNoDepthMask;
    opaqueNoDepthWrite->setEnabled(!layer3DS->layerFlags().testFlag(Q3DSLayerNode::DisableDepthPrePass));
    opaquePass->addRenderState(opaqueNoDepthWrite);

    // Transparent objects.
    Qt3DRender::QRenderPass *transPass = new Qt3DRender::QRenderPass;
    transPass->addFilterKey(transFilterKey);
    addDepthTest(transPass); // ### todo handle layer.flags & DisableDepthTest
    Qt3DRender::QNoDepthMask *transNoDepthWrite = new Qt3DRender::QNoDepthMask;
    transPass->addRenderState(transNoDepthWrite);
    // blending. the shaders produce non-premultiplied results.
    auto blendFunc = new Qt3DRender::QBlendEquation;
    auto blendArgs = new Qt3DRender::QBlendEquationArguments;
    setBlending(blendFunc, blendArgs, blendMode);

    transPass->addRenderState(blendFunc);
    transPass->addRenderState(blendArgs);

    shadowOrthoPass->setShaderProgram(Q3DSShaderManager::instance().getOrthographicDepthNoTessShader());
    shadowCubePass->setShaderProgram(Q3DSShaderManager::instance().getCubeDepthNoTessShader());
    depthPass->setShaderProgram(Q3DSShaderManager::instance().getDepthPrepassShader(hasDisplacement));
    opaquePass->setShaderProgram(program);
    transPass->setShaderProgram(program);

    return { shadowOrthoPass, shadowCubePass, depthPass, opaquePass, transPass };
}

QVector<Qt3DRender::QTechnique *> Q3DSSceneBuilder::computeTechniques()
{
    Qt3DRender::QTechnique *bsdfPrefilter = new Qt3DRender::QTechnique;

    Qt3DRender::QFilterKey *bsdfPrefilterFilterKey = new Qt3DRender::QFilterKey;
    bsdfPrefilterFilterKey->setName(QLatin1String("type"));
    bsdfPrefilterFilterKey->setValue(QLatin1String("bsdfPrefilter"));
    bsdfPrefilter->addFilterKey(bsdfPrefilterFilterKey);

    Q3DSDefaultMaterialGenerator::addDefaultApiFilter(bsdfPrefilter);

    Qt3DRender::QRenderPass *bsdfComputePass = new Qt3DRender::QRenderPass;
    bsdfComputePass->setShaderProgram(Q3DSShaderManager::instance().getBsdfMipPreFilterShader());
    bsdfPrefilter->addRenderPass(bsdfComputePass);

    return { bsdfPrefilter };
}

void Q3DSSceneBuilder::markAsMainTechnique(Qt3DRender::QTechnique *technique)
{
    Qt3DRender::QFilterKey *techniqueFilterKey = new Qt3DRender::QFilterKey;
    techniqueFilterKey->setName(QLatin1String("type"));
    techniqueFilterKey->setValue(QLatin1String("main"));
    technique->addFilterKey(techniqueFilterKey);
}

QSize Q3DSSceneBuilder::calculateLayerSize(Q3DSLayerNode *layer3DS, const QSize &parentSize)
{
    return QSize(qRound(layer3DS->widthUnits() == Q3DSLayerNode::Percent ? layer3DS->width() * 0.01f * parentSize.width() : layer3DS->width()),
                 qRound(layer3DS->heightUnits() == Q3DSLayerNode::Percent ? layer3DS->height() * 0.01f * parentSize.height() : layer3DS->height()));
}

QPointF Q3DSSceneBuilder::calculateLayerPos(Q3DSLayerNode *layer3DS, const QSize &parentSize)
{
    return QPointF(layer3DS->leftUnits() == Q3DSLayerNode::Percent ? layer3DS->left() * 0.01f * parentSize.width() : layer3DS->left(),
                   layer3DS->topUnits() == Q3DSLayerNode::Percent ? layer3DS->top() * 0.01f * parentSize.height() : layer3DS->top());
}

void Q3DSSceneBuilder::updateSizesForLayer(Q3DSLayerNode *layer3DS, const QSize &newParentSize)
{
    if (newParentSize.isEmpty())
        return;

    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(data);

    // ### only supports Left/Width+Top/Height only for now
    data->layerPos = calculateLayerPos(layer3DS, newParentSize);
    data->layerSize = calculateLayerSize(layer3DS, newParentSize);
    data->parentSize = newParentSize;

    setLayerCameraSizeProperties(layer3DS);
    setLayerSizeProperties(layer3DS);
}

static bool isFitTypeScaleMode(Q3DSCameraNode *cam3DS)
{
    return cam3DS->scaleMode() == Q3DSCameraNode::Fit
            || cam3DS->scaleMode() == Q3DSCameraNode::FitHorizontal
            || cam3DS->scaleMode() == Q3DSCameraNode::FitVertical;
}

static bool isVerticalAdjust(Q3DSCameraNode *cam3DS, float presentationAspect, float aspect)
{
    return (cam3DS->scaleMode() == Q3DSCameraNode::Fit && aspect >= presentationAspect)
            || cam3DS->scaleMode() == Q3DSCameraNode::FitVertical;
}

void Q3DSSceneBuilder::setLayerCameraSizeProperties(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    if (!data->cam3DS)
        return;

    Qt3DRender::QCamera *camera = static_cast<Qt3DRender::QCamera *>(data->cameraSelector->camera());

    // The aspect ratio is not simply based on the viewport (layer)
    const float layerAspect = data->layerSize.height() ? data->layerSize.width() / float(data->layerSize.height()) : 0.0f;
    // ...but may take the presentation design size into account as well.
    const float presentationAspect = m_presentationSize.height() ? m_presentationSize.width() / float(m_presentationSize.height()) : 0.0f;

    const float aspect = isFitTypeScaleMode(data->cam3DS) ? layerAspect : presentationAspect;

    // First thing to do is always to reset the type back from Custom to Perspective/Orthographic.
    if (data->cam3DS->orthographic()) {
        camera->setProjectionType(Qt3DRender::QCameraLens::OrthographicProjection);
        float halfWidth = m_presentationSize.width() / 2.0f;
        float halfHeight = m_presentationSize.height() / 2.0f;
        QMatrix4x4 proj = camera->projectionMatrix();
        float *projData = proj.data();
        if (isVerticalAdjust(data->cam3DS, presentationAspect, aspect)) {
            projData[0] = 1.0f / (halfHeight * aspect);
            projData[5] = 1.0f / halfHeight;
        } else {
            projData[0] = 1.0f / halfWidth;
            projData[5] = 1.0f / (halfWidth / aspect);
        }
        camera->setProjectionMatrix(proj);
    } else {
        camera->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
        if (isVerticalAdjust(data->cam3DS, presentationAspect, aspect)) {
            camera->setAspectRatio(aspect);
        } else {
            camera->setAspectRatio(presentationAspect);
            QMatrix4x4 proj = camera->projectionMatrix();
            float *projData = proj.data();
            if (!qFuzzyIsNull(presentationAspect))
                projData[5] *= aspect / presentationAspect;
            // Mode changes to Custom.
            camera->setProjectionMatrix(proj);
        }
    }
}

void Q3DSSceneBuilder::setLayerSizeProperties(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    for (const Q3DSLayerAttached::SizeManagedTexture &t : data->sizeManagedTextures) {
        t.texture->setWidth(data->layerSize.width());
        t.texture->setHeight(data->layerSize.height());
        if (t.sizeChangeCallback)
            t.sizeChangeCallback(layer3DS);
    }
    if (data->updateCompositorCalculations)
        data->updateCompositorCalculations();
}

static void setClearColorForClearBuffers(Qt3DRender::QClearBuffers *clearBuffers, Q3DSLayerNode *layer3DS)
{
    switch (layer3DS->layerBackground()) {
    case Q3DSLayerNode::Transparent:
        clearBuffers->setClearColor(Qt::transparent);
        break;
    case Q3DSLayerNode::SolidColor:
        clearBuffers->setClearColor(layer3DS->backgroundColor());
        break;
    default:
        clearBuffers->setClearColor(Qt::black);
        break;
    }
}

// non-size-dependent properties
void Q3DSSceneBuilder::setLayerProperties(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(data);

    setClearColorForClearBuffers(data->clearBuffers, layer3DS);

    if (data->compositorEntity) // may not exist if this is still buildLayer()
        data->compositorEntity->setEnabled(layer3DS->flags().testFlag(Q3DSNode::Active));
}

Q3DSCameraNode *Q3DSSceneBuilder::chooseLayerCamera(Q3DSLayerNode *layer3DS, Qt3DRender::QCamera **camera)
{
    Q3DSCameraNode *cam3DS = findFirstCamera(layer3DS);
    qCDebug(lcScene, "Layer %s uses camera %s", layer3DS->id().constData(), cam3DS ? cam3DS->id().constData() : "null");
    *camera = buildLayerCamera(layer3DS, cam3DS);
    return cam3DS;
}

Qt3DRender::QCamera *Q3DSSceneBuilder::buildLayerCamera(Q3DSLayerNode *layer3DS, Q3DSCameraNode *camNode)
{
    Qt3DRender::QCamera *camera = new Qt3DRender::QCamera;
    // do not set aspect ratio yet
    if (camNode) {
        camera->setObjectName(QObject::tr("camera %1 for %2").arg(QString::fromUtf8(camNode->id())).arg(QString::fromUtf8(layer3DS->id())));
        Q3DSCameraAttached *data = new Q3DSCameraAttached;
        data->transform = new Qt3DCore::QTransform;
        data->camera = camera;
        data->layer3DS = layer3DS;
        camNode->setAttached(data);
        // make sure data->entity, globalTransform, etc. are usable
        setNodeProperties(camNode, camera, data->transform, NodePropUpdateAttached);
        setCameraProperties(camNode, Q3DSPropertyChangeList::ALL_CHANGE_FLAGS);
        camNode->addPropertyChangeObserver(std::bind(&Q3DSSceneBuilder::handlePropertyChange, this,
                                                     std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    } else {
        camera->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
        camera->setFieldOfView(60);
        camera->setPosition(QVector3D(0, 0, -600));
        camera->setViewCenter(QVector3D(0, 0, 0));
        camera->setNearPlane(10);
        camera->setFarPlane(5000);
    }

    return camera;
}

static QMatrix4x4 composeTransformMatrix(const QVector3D &position, const QVector3D &rotation, const QVector3D &scale)
{
    const QMatrix3x3 rot3x3 = QQuaternion::fromEulerAngles(rotation).toRotationMatrix();
    QMatrix4x4 m;
    m(0, 0) = scale.x() * rot3x3(0, 0); m(0, 1) = scale.y() * rot3x3(0, 1); m(0, 2) = scale.z() * rot3x3(0, 2); m(0, 3) = position.x();
    m(1, 0) = scale.x() * rot3x3(1, 0); m(1, 1) = scale.y() * rot3x3(1, 1); m(1, 2) = scale.z() * rot3x3(1, 2); m(1, 3) = position.y();
    m(2, 0) = scale.x() * rot3x3(2, 0); m(2, 1) = scale.y() * rot3x3(2, 1); m(2, 2) = scale.z() * rot3x3(2, 2); m(2, 3) = position.z();
    m(3, 0) = 0.0f; m(3, 1) = 0.0f; m(3, 2) = 0.0f; m(3, 3) = 1.0f;
    return m;
}

static QVector3D directionFromTransform(const QMatrix4x4 &t, bool lh)
{
    const QMatrix3x3 m = t.normalMatrix();
    const QVector3D c0(m(0, 0), m(1, 0), m(2, 0));
    const QVector3D c1(m(0, 1), m(1, 1), m(2, 1));
    const QVector3D c2(m(0, 2), m(1, 2), m(2, 2));
    const QVector3D d(0, 0, lh ? -1 : 1);
    QVector3D v(c0 * d.x() + c1 * d.y() + c2 * d.z());
    return v.normalized();
}

static void adjustRotationLeftToRight(QMatrix4x4 *m)
{
    float *p = m->data();
    p[2] *= -1;
    p[4 + 2] *= -1;
    p[8] *= -1;
    p[8 + 1] *= -1;
}

void Q3DSSceneBuilder::setCameraProperties(Q3DSCameraNode *camNode, int changeFlags)
{
    Q3DSCameraAttached *data = static_cast<Q3DSCameraAttached *>(camNode->attached());
    Q_ASSERT(data);
    Qt3DRender::QCamera *camera = data->camera;

    camera->setProjectionType(camNode->orthographic() ? Qt3DRender::QCameraLens::OrthographicProjection
                                                      : Qt3DRender::QCameraLens::PerspectiveProjection);
    if (camNode->orthographic()) {
        camera->setLeft(-1);
        camera->setRight(1);
        camera->setTop(1);
        camera->setBottom(-1);
    } else {
        camera->setFieldOfView(camNode->fov());
    }
    camera->setNearPlane(camNode->clipNear());
    camera->setFarPlane(camNode->clipFar());

    const Q3DSPropertyChangeList::Flags cf = Q3DSPropertyChangeList::Flags(changeFlags);
    if (!cf.testFlag(Q3DSPropertyChangeList::NodeTransformChanges))
        return;

    // Q3DSCameraNode is like an ordinary node with pos/rot/scale, whereas Qt3D needs a position + view center
    const bool leftHanded = camNode->orientation() == Q3DSNode::LeftHanded;
    const float lhFactor = leftHanded ? -1.0f : 1.0f;
    // ### pivot?
    QMatrix4x4 t = composeTransformMatrix(camNode->position(), camNode->rotation(), camNode->scale());
    if (leftHanded)
        adjustRotationLeftToRight(&t);
    const QVector3D pos(t(0, 3), t(1, 3), lhFactor * t(2, 3));
    camera->setPosition(pos);
    // For the viewCenter make up some point in the correct direction.
    const QVector3D d = directionFromTransform(t, leftHanded);
    const QVector3D center(pos + d);
    camera->setViewCenter(center);
    // roll is handled in the up vector
    QVector3D upVec(0, 1, 0);
    QMatrix4x4 rotZ;
    rotZ.rotate(camNode->rotation().z(), 0, 0, -lhFactor);
    upVec = rotZ * upVec;
    camera->setUpVector(upVec);
}

static void prepareSizeDependentTexture(Qt3DRender::QAbstractTexture *texture,
                                        Q3DSLayerNode *layer3DS,
                                        Q3DSLayerAttached::SizeManagedTexture::SizeChangeCallback callback = nullptr)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());

    const QSize layerSize = data->parentSize;
    // may not be available yet, use a temporary size then
    texture->setWidth(layerSize.width() > 0 ? layerSize.width() : 32);
    texture->setHeight(layerSize.height() > 0 ? layerSize.height() : 32);

    // the layer will resize the texture automatically
    data->sizeManagedTextures << Q3DSLayerAttached::SizeManagedTexture(texture, callback);
}

void Q3DSSceneBuilder::setDepthTextureEnabled(Q3DSLayerNode *layer3DS, bool enabled)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(data);
    if (enabled == data->depthTextureData.enabled)
        return;

    data->depthTextureData.enabled = enabled;
    qCDebug(lcScene, "Depth texture enabled for layer %s is now %d", layer3DS->id().constData(), enabled);
    if (enabled) {
        if (!data->depthTextureData.depthTexture) {
            Qt3DRender::QTexture2D *depthTex = new Qt3DRender::QTexture2D;
            depthTex->setFormat(Qt3DRender::QAbstractTexture::DepthFormat);
            depthTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
            depthTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
            prepareSizeDependentTexture(depthTex, layer3DS);
            data->depthTextureData.depthTexture = depthTex;

            Qt3DRender::QRenderTarget *depthRt = new Qt3DRender::QRenderTarget;
            Qt3DRender::QRenderTargetOutput *depthRtOutput = new Qt3DRender::QRenderTargetOutput;
            depthRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Depth);
            depthRtOutput->setTexture(data->depthTextureData.depthTexture);
            depthRt->addOutput(depthRtOutput);
            qRegisterMetaType<Qt3DRender::QRenderTarget *>("Qt3DRender::QRenderTarget*"); // wtf??
            data->depthTextureData.rtSelector->setTarget(depthRt);

            data->depthTextureData.clearBuffers = new Qt3DRender::QClearBuffers(data->depthTextureData.rtSelector);
            data->depthTextureData.clearBuffers->setBuffers(Qt3DRender::QClearBuffers::DepthBuffer);
            new Qt3DRender::QNoDraw(data->depthTextureData.clearBuffers);

            Qt3DRender::QRenderPassFilter *depthTexFilter = new Qt3DRender::QRenderPassFilter(data->depthTextureData.rtSelector);
            Qt3DRender::QFilterKey *depthFilterKey = new Qt3DRender::QFilterKey;
            depthFilterKey->setName(QLatin1String("pass"));
            depthFilterKey->setValue(QLatin1String("depth"));
            depthTexFilter->addMatch(depthFilterKey);
            Qt3DRender::QSortPolicy *sortPolicy = opaquePassSortPolicy(depthTexFilter);
            data->depthTextureData.layerFilterOpaque = new Qt3DRender::QLayerFilter(sortPolicy);

            depthTexFilter = new Qt3DRender::QRenderPassFilter(data->depthTextureData.rtSelector);
            depthTexFilter->addMatch(depthFilterKey);
            sortPolicy = transparentPassSortPolicy(depthTexFilter);
            data->depthTextureData.layerFilterTransparent = new Qt3DRender::QLayerFilter(sortPolicy);
        }

        data->depthTextureData.clearBuffers->setEnabled(true);
        data->depthTextureData.layerFilterOpaque->addLayer(data->opaqueTag);
        data->depthTextureData.layerFilterTransparent->addLayer(data->transparentTag);
    } else if (data->depthTextureData.depthTexture) {
        data->depthTextureData.clearBuffers->setEnabled(false);
        data->depthTextureData.layerFilterOpaque->removeLayer(data->opaqueTag);
        data->depthTextureData.layerFilterTransparent->removeLayer(data->transparentTag);
    }
}

void Q3DSSceneBuilder::setSsaoTextureEnabled(Q3DSLayerNode *layer3DS, bool enabled)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(data);
    if (enabled == data->ssaoTextureData.enabled)
        return;

    data->ssaoTextureData.enabled = enabled;
    qCDebug(lcScene, "SSAO enabled for layer %s is now %d", layer3DS->id().constData(), enabled);

    if (enabled) {
        setDepthTextureEnabled(layer3DS, true);

        if (!data->ssaoTextureData.ssaoTexture) {
            Qt3DRender::QTexture2D *ssaoTex = new Qt3DRender::QTexture2D;
            ssaoTex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
            ssaoTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
            ssaoTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
            // it's not just the texture that needs dynamic resizing, but values
            // derived from the size have to be recalculated as well, hence the callback.
            prepareSizeDependentTexture(ssaoTex, layer3DS,
                                        std::bind(&Q3DSSceneBuilder::updateAoParameters, this, std::placeholders::_1));
            data->ssaoTextureData.ssaoTexture = ssaoTex;

            Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
            Qt3DRender::QRenderTargetOutput *rtOutput = new Qt3DRender::QRenderTargetOutput;
            rtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
            rtOutput->setTexture(data->ssaoTextureData.ssaoTexture);
            rt->addOutput(rtOutput);
            data->ssaoTextureData.rtSelector->setTarget(rt);

            data->ssaoTextureData.clearBuffers = new Qt3DRender::QClearBuffers(data->ssaoTextureData.rtSelector);
            data->ssaoTextureData.clearBuffers->setBuffers(Qt3DRender::QClearBuffers::ColorBuffer);
            setClearColorForClearBuffers(data->ssaoTextureData.clearBuffers, layer3DS);
            new Qt3DRender::QNoDraw(data->ssaoTextureData.clearBuffers);

            Qt3DRender::QRenderPassFilter *ssaoFilter = new Qt3DRender::QRenderPassFilter(data->ssaoTextureData.rtSelector);
            Qt3DRender::QFilterKey *ssaoFilterKey = new Qt3DRender::QFilterKey;
            ssaoFilterKey->setName(QLatin1String("pass"));
            ssaoFilterKey->setValue(QLatin1String("ssao"));
            ssaoFilter->addMatch(ssaoFilterKey);
            data->ssaoTextureData.layerFilter = new Qt3DRender::QLayerFilter(ssaoFilter);

            data->ssaoTextureData.depthSampler = new Qt3DRender::QParameter;
            data->ssaoTextureData.depthSampler->setName(QLatin1String("depth_sampler"));
            data->ssaoTextureData.depthSampler->setValue(QVariant::fromValue(data->depthTextureData.depthTexture));

            data->ssaoTextureData.aoDataBuf = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::UniformBuffer);
            data->ssaoTextureData.aoDataBuf->setObjectName(QLatin1String("ambient occlusion pass constant buffer"));
            updateAoParameters(layer3DS);

            data->ssaoTextureData.aoDataBufParam = new Qt3DRender::QParameter;
            data->ssaoTextureData.aoDataBufParam->setName(QLatin1String("cbAoShadow"));
            data->ssaoTextureData.aoDataBufParam->setValue(QVariant::fromValue(data->ssaoTextureData.aoDataBuf));

            ssaoFilter->addParameter(data->ssaoTextureData.depthSampler);
            ssaoFilter->addParameter(data->cameraPropertiesParam);
            ssaoFilter->addParameter(data->ssaoTextureData.aoDataBufParam);

            // for the main passes
            data->ssaoTextureData.ssaoTextureSampler = new Qt3DRender::QParameter;
            data->ssaoTextureData.ssaoTextureSampler->setName(QLatin1String("ao_sampler"));
            data->ssaoTextureData.ssaoTextureSampler->setValue(QVariant::fromValue(data->ssaoTextureData.ssaoTexture));
        }

        data->ssaoTextureData.clearBuffers->setEnabled(true);
        data->ssaoTextureData.layerFilter->addLayer(m_fsQuadTag);
    } else {
        data->ssaoTextureData.clearBuffers->setEnabled(false);
        data->ssaoTextureData.layerFilter->removeLayer(m_fsQuadTag);
    }
}

void Q3DSSceneBuilder::updateAoParameters(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(data);
    if (!data->ssaoTextureData.aoDataBuf)
        return;

    QByteArray buf;
    buf.resize(sizeof(Q3DSAmbientOcclusionData));
    Q3DSAmbientOcclusionData *p = reinterpret_cast<Q3DSAmbientOcclusionData *>(buf.data());

    p->aoProperties = QVector4D(layer3DS->aoStrength() * 0.01f,
                                layer3DS->aoDistance() * 0.4f,
                                layer3DS->aoSoftness() * 0.02f,
                                layer3DS->aoBias());

    p->aoProperties2 = QVector4D(layer3DS->aoSampleRate(),
                                 layer3DS->aoDither() ? 1.0f : 0.0f,
                                 0.0f,
                                 0.0f);

    p->shadowProperties = QVector4D(layer3DS->shadowStrength() * 0.01f,
                                    layer3DS->shadowDist(),
                                    layer3DS->shadowSoftness() * 0.01f,
                                    layer3DS->shadowBias());

    float R2 = layer3DS->aoDistance() * layer3DS->aoDistance() * 0.16f;
    float rw = data->depthTextureData.depthTexture->width();
    float rh = data->depthTextureData.depthTexture->height();
    float fov = data->cam3DS ? qDegreesToRadians(data->cam3DS->fov()) : 1.0f;
    float tanHalfFovY = qTan(0.5f * fov * (rh / rw));
    float invFocalLenX = tanHalfFovY * (rw / rh);

    p->aoScreenConst = QVector4D(1.0f / R2,
                                 rh / (2.0f * tanHalfFovY),
                                 1.0f / rw,
                                 1.0f / rh);

    p->uvToEyeConst = QVector4D(2.0f * invFocalLenX,
                                -2.0f * tanHalfFovY,
                                -invFocalLenX,
                                tanHalfFovY);

    data->ssaoTextureData.aoDataBuf->setData(buf);
}

void Q3DSSceneBuilder::updateSsaoStatus(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(data);

    const bool needsSsao = layer3DS->aoStrength() > 0.0f;
    const bool hasSsao = data->ssaoTextureData.enabled;

    if (needsSsao == hasSsao) {
        if (hasSsao)
            updateAoParameters(layer3DS);
        return;
    }

    setSsaoTextureEnabled(layer3DS, needsSsao);
}

void Q3DSSceneBuilder::updateShadowMapStatus(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(layerData);
    int lightIdx = 0;

    for (Q3DSLightNode *light3DS : qAsConst(layerData->lightNodes)) {
        const QString lightIndexStr = QString::number(lightIdx++);

        // assume castShadow cannot dynamically change
        if (light3DS->castShadow()) {
            auto it = std::find_if(layerData->shadowMapData.shadowCasters.begin(), layerData->shadowMapData.shadowCasters.end(),
                                   [light3DS](const Q3DSLayerAttached::PerLightShadowMapData &d) { return d.lightNode == light3DS; });
            Q3DSLayerAttached::PerLightShadowMapData *d;
            if (it != layerData->shadowMapData.shadowCasters.end()) {
                d = it;
            } else {
                layerData->shadowMapData.shadowCasters.append(Q3DSLayerAttached::PerLightShadowMapData());
                d = &layerData->shadowMapData.shadowCasters.last();
                d->lightNode = light3DS;
            }

            const qint32 size = 1 << light3DS->shadowMapRes();

            if (!layerData->shadowMapData.shadowDS) {
                Qt3DRender::QTexture2D *dsTexOrRb = new Qt3DRender::QTexture2D;
                dsTexOrRb->setFormat(Qt3DRender::QAbstractTexture::D24S8);
                dsTexOrRb->setWidth(size);
                dsTexOrRb->setHeight(size);
                dsTexOrRb->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
                dsTexOrRb->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
                layerData->shadowMapData.shadowDS = dsTexOrRb;
            }

            if (!d->shadowMapTexture) {
                Q_ASSERT(d->lightNode == light3DS);
                const bool isCube = light3DS->lightType() != Q3DSLightNode::Directional;
                Qt3DCore::QNode *texParent = layerData->shadowMapData.shadowRoot;
                if (isCube) {
                    d->shadowMapTexture = new Qt3DRender::QTextureCubeMap(texParent);
                    d->shadowMapTextureTemp = new Qt3DRender::QTextureCubeMap(texParent);
                } else {
                    d->shadowMapTexture = new Qt3DRender::QTexture2D(texParent);
                    d->shadowMapTextureTemp = new Qt3DRender::QTexture2D(texParent);
                }

                d->shadowMapTexture->setFormat(Qt3DRender::QAbstractTexture::R16_UNorm);
                d->shadowMapTextureTemp->setFormat(Qt3DRender::QAbstractTexture::R16_UNorm);

                d->shadowMapTexture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
                d->shadowMapTextureTemp->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
                d->shadowMapTexture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
                d->shadowMapTextureTemp->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);

                qCDebug(lcScene, "Shadow cube map size is %d", size);
                // do not add to layerData->sizeManagedTextures since the shadow map size is fixed
                d->shadowMapTexture->setSize(size, size, 1);
                d->shadowMapTextureTemp->setSize(size, size, 1);

                Q3DSLightAttached *lightData = static_cast<Q3DSLightAttached *>(light3DS->attached());

                // now the framegraph subtree

                if (isCube) {
                    // we do not use geometry shaders for some reason, so run a separate pass for each cubemap face instead
                    static const Qt3DRender::QAbstractTexture::CubeMapFace faceIds[6] = {
                        Qt3DRender::QAbstractTexture::CubeMapPositiveX,
                        Qt3DRender::QAbstractTexture::CubeMapNegativeX,
                        Qt3DRender::QAbstractTexture::CubeMapPositiveY,
                        Qt3DRender::QAbstractTexture::CubeMapNegativeY,
                        Qt3DRender::QAbstractTexture::CubeMapPositiveZ,
                        Qt3DRender::QAbstractTexture::CubeMapNegativeZ
                    };
                    static const QVector3D up[6] = {
                        QVector3D(0, -1, 0),
                        QVector3D(0, -1, 0),
                        QVector3D(0, 0, 1),
                        QVector3D(0, 0, -1),
                        QVector3D(0, -1, 0),
                        QVector3D(0, -1, 0)
                    };
                    static const QVector3D dir[6] = {
                        QVector3D(1, 0, 0),
                        QVector3D(-1, 0, 0),
                        QVector3D(0, 1, 0),
                        QVector3D(0, -1, 0),
                        QVector3D(0, 0, 1),
                        QVector3D(0, 0, -1)
                    };

                    const QVector3D lightGlobalPos = lightData->globalTransform.column(3).toVector3D();

                    // camera_properties comes from the actual camera, so will reuse the parameter used by normal passes
                    // camera_position is for the light the viewpoint of which we are rendering from
                    d->cameraPositionParam = new Qt3DRender::QParameter(QLatin1String("camera_position"),
                                                                        QVector3D(lightGlobalPos.x(), lightGlobalPos.y(), -lightGlobalPos.z())); // because the shader wants Z this way

                    // Passes to fill up the 6 faces of the cubemap texture
                    for (int faceIdx = 0; faceIdx < 6; ++faceIdx) {
                        Qt3DRender::QRenderTargetSelector *shadowRtSelector = new Qt3DRender::QRenderTargetSelector(layerData->shadowMapData.shadowRoot);
                        Qt3DRender::QRenderTarget *shadowRt = new Qt3DRender::QRenderTarget;
                        Qt3DRender::QRenderTargetOutput *shadowRtOutput = new Qt3DRender::QRenderTargetOutput;
                        shadowRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
                        shadowRtOutput->setTexture(d->shadowMapTexture);
                        shadowRtOutput->setFace(faceIds[faceIdx]);
                        shadowRt->addOutput(shadowRtOutput);

                        shadowRtOutput = new Qt3DRender::QRenderTargetOutput;
                        shadowRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::DepthStencil);
                        shadowRtOutput->setTexture(layerData->shadowMapData.shadowDS);
                        shadowRt->addOutput(shadowRtOutput);

                        shadowRtSelector->setTarget(shadowRt);

                        // must have a viewport in order to let the renderer pick up the correct output dimensions and reset the viewport state
                        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(shadowRtSelector);
                        viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

                        Qt3DRender::QCameraSelector *camSel = new Qt3DRender::QCameraSelector(viewport);
                        Qt3DRender::QCamera *shadowCam = new Qt3DRender::QCamera;

                        shadowCam->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
                        shadowCam->setFieldOfView(90);
                        shadowCam->setNearPlane(1.0f);
                        shadowCam->setFarPlane(qMax(2.0f, light3DS->shadowMapFar()));

                        shadowCam->setPosition(lightGlobalPos);
                        const QVector3D center(lightGlobalPos + dir[faceIdx]);
                        shadowCam->setViewCenter(center);
                        shadowCam->setUpVector(up[faceIdx]);
                        shadowCam->setAspectRatio(1);

                        camSel->setCamera(shadowCam);

                        Qt3DRender::QClearBuffers *clearBuffers = new Qt3DRender::QClearBuffers(camSel);
                        clearBuffers->setBuffers(Qt3DRender::QClearBuffers::ColorDepthStencilBuffer);
                        clearBuffers->setClearColor(Qt::white);
                        new Qt3DRender::QNoDraw(clearBuffers);

                        Qt3DRender::QFilterKey *shadowFilterKey = new Qt3DRender::QFilterKey;
                        shadowFilterKey->setName(QLatin1String("pass"));
                        shadowFilterKey->setValue(QLatin1String("shadowCube"));

                        Qt3DRender::QRenderPassFilter *shadowFilter = new Qt3DRender::QRenderPassFilter(camSel);
                        shadowFilter->addMatch(shadowFilterKey);
                        Qt3DRender::QSortPolicy *sortPolicyOpaque = opaquePassSortPolicy(shadowFilter);
                        Qt3DRender::QLayerFilter *layerFilterOpaque = new Qt3DRender::QLayerFilter(sortPolicyOpaque);
                        layerFilterOpaque->addLayer(layerData->opaqueTag);
                        shadowFilter->addParameter(d->cameraPositionParam);
                        shadowFilter->addParameter(layerData->cameraPropertiesParam);

                        shadowFilter = new Qt3DRender::QRenderPassFilter(camSel);
                        shadowFilter->addMatch(shadowFilterKey);
                        Qt3DRender::QSortPolicy *sortPolicyTransparent = transparentPassSortPolicy(shadowFilter);
                        Qt3DRender::QLayerFilter *layerFilterTransparent = new Qt3DRender::QLayerFilter(sortPolicyTransparent);
                        layerFilterTransparent->addLayer(layerData->transparentTag);
                        shadowFilter->addParameter(d->cameraPositionParam);
                        shadowFilter->addParameter(layerData->cameraPropertiesParam);
                    }

                    // Now two blur passes that output to the final texture, play ping pong.

                    // Draws a fullscreen quad into the 6 faces of the cubemap texture (COLOR0..5), with the other texture as input.
                    auto genCubeBlurPassFg = [=](Qt3DRender::QAbstractTexture *inTex, Qt3DRender::QAbstractTexture *outTex, const QString &passName)
                    {
                        Qt3DRender::QRenderTargetSelector *rtSelector = new Qt3DRender::QRenderTargetSelector(layerData->shadowMapData.shadowRoot);
                        Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
                        for (int faceIdx = 0; faceIdx < 6; ++faceIdx) {
                            Qt3DRender::QRenderTargetOutput *rtOutput = new Qt3DRender::QRenderTargetOutput;
                            rtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::AttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0 + faceIdx));
                            rtOutput->setTexture(outTex);
                            rtOutput->setFace(faceIds[faceIdx]);
                            rt->addOutput(rtOutput);
                        }
                        rtSelector->setTarget(rt);

                        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(rtSelector);
                        viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

                        // No Camera since the shaders do not care about view or projection.

                        Qt3DRender::QFilterKey *filterKey = new Qt3DRender::QFilterKey;
                        filterKey->setName(QLatin1String("pass"));
                        filterKey->setValue(passName);

                        Qt3DRender::QParameter *depthCubeParam = new Qt3DRender::QParameter(QLatin1String("depthCube"),
                                                                                            QVariant::fromValue(inTex));

                        Qt3DRender::QRenderPassFilter *filter = new Qt3DRender::QRenderPassFilter(viewport);
                        filter->addMatch(filterKey);
                        Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(filter);
                        layerFilter->addLayer(m_fsQuadTag);

                        Qt3DRender::QParameter *camPropsParam = new Qt3DRender::QParameter(QLatin1String("camera_properties"),
                                                                                           QVector2D(light3DS->shadowFilter(), light3DS->shadowMapFar()));
                        filter->addParameter(depthCubeParam);
                        filter->addParameter(camPropsParam);
                    };

                    if (m_gfxLimits.maxDrawBuffers >= 6) { // ###
                        genCubeBlurPassFg(d->shadowMapTexture, d->shadowMapTextureTemp, QLatin1String("shadowCubeBlurX"));
                        genCubeBlurPassFg(d->shadowMapTextureTemp, d->shadowMapTexture, QLatin1String("shadowCubeBlurY"));
                    }

                    d->shadowSampler = new Qt3DRender::QParameter;
                    d->shadowSampler->setName(QLatin1String("shadowcube") + lightIndexStr);
                    d->shadowSampler->setValue(QVariant::fromValue(d->shadowMapTexture));

                    d->shadowMatrixParam = new Qt3DRender::QParameter;
                    d->shadowMatrixParam->setName(QLatin1String("shadowmap") + lightIndexStr + QLatin1String("_matrix"));
                    d->shadowMatrixParam->setValue(QVariant::fromValue(lightData->globalTransform.inverted()));

                } else {

                    QVector3D lightDir = lightData->globalTransform.column(2).toVector3D().normalized();

                    Qt3DRender::QRenderTargetSelector *shadowRtSelector = new Qt3DRender::QRenderTargetSelector(layerData->shadowMapData.shadowRoot);
                    Qt3DRender::QRenderTarget *shadowRt = new Qt3DRender::QRenderTarget;
                    Qt3DRender::QRenderTargetOutput *shadowRtOutput = new Qt3DRender::QRenderTargetOutput;
                    shadowRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
                    shadowRtOutput->setTexture(d->shadowMapTexture);
                    shadowRt->addOutput(shadowRtOutput);

                    shadowRtOutput = new Qt3DRender::QRenderTargetOutput;
                    shadowRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::DepthStencil);
                    shadowRtOutput->setTexture(layerData->shadowMapData.shadowDS);
                    shadowRt->addOutput(shadowRtOutput);

                    shadowRtSelector->setTarget(shadowRt);

                    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(shadowRtSelector);
                    viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

                    Qt3DRender::QCameraSelector *camSel = new Qt3DRender::QCameraSelector(viewport);
                    Qt3DRender::QCamera *shadowCam = new Qt3DRender::QCamera;

                    shadowCam->setProjectionType(Qt3DRender::QCameraLens::OrthographicProjection);
                    shadowCam->setFieldOfView(90);
                    shadowCam->setAspectRatio(1);

                    // Pick a shadow camera position based on the real camera.

                    // For example, if the default camera at 0,0,-600 is used,
                    // then the shadow camera's position will be 0,0,N where N
                    // is something based on the real camera's frustum.

                    Q3DSCameraAttached *sceneCamData = static_cast<Q3DSCameraAttached *>(layerData->cam3DS->attached());
                    Qt3DRender::QCamera *sceneCamera = sceneCamData->camera;
                    QVector3D camX = sceneCamData->globalTransform.column(0).toVector3D();
                    QVector3D camY = sceneCamData->globalTransform.column(1).toVector3D();
                    QVector3D camZ = sceneCamData->globalTransform.column(2).toVector3D();
                    float tanFOV = qTan(qDegreesToRadians(sceneCamera->fieldOfView()) * 0.5f);
                    float asTanFOV = tanFOV; /* * viewport.height / viewport.width but this is always 1 */
                    QVector3D camEdges[4];
                    camEdges[0] = -tanFOV * camX + asTanFOV * camY + camZ;
                    camEdges[1] = tanFOV * camX + asTanFOV * camY + camZ;
                    camEdges[2] = tanFOV * camX - asTanFOV * camY + camZ;
                    camEdges[3] = -tanFOV * camX - asTanFOV * camY + camZ;
                    QVector3D camVerts[8];
                    const QVector3D camLocalPos = layerData->cam3DS->position();
                    camVerts[0] = camLocalPos + camEdges[0] * sceneCamera->nearPlane();
                    camVerts[1] = camLocalPos + camEdges[0] * sceneCamera->farPlane();
                    camVerts[2] = camLocalPos + camEdges[1] * sceneCamera->nearPlane();
                    camVerts[3] = camLocalPos + camEdges[1] * sceneCamera->farPlane();
                    camVerts[4] = camLocalPos + camEdges[2] * sceneCamera->nearPlane();
                    camVerts[5] = camLocalPos + camEdges[2] * sceneCamera->farPlane();
                    camVerts[6] = camLocalPos + camEdges[3] * sceneCamera->nearPlane();
                    camVerts[7] = camLocalPos + camEdges[3] * sceneCamera->farPlane();
                    QVector3D lightPos = camVerts[0];
                    for (int i = 1; i < 8; ++i)
                        lightPos += camVerts[i];
                    lightPos *= 0.125f;

                    float dd = 0.5f * (light3DS->shadowMapFar() + 1.0f);
                    lightPos += lightDir * dd;

                    const QVector3D camDir = sceneCamData->globalTransform.column(3).toVector3D().normalized();
                    float o1 = dd * 2.0f * qTan(0.5f * qDegreesToRadians(60.0f)); // ?! ported as-is, SCamera's default is 60...
                    float o2 = light3DS->shadowMapFar() - 1.0f;
                    float o = qFabs(QVector3D::dotProduct(lightDir, camDir));
                    o = (1.0f - o) * o2 + o * o1;

                    float clipNear = 1.0f;
                    float clipFar = light3DS->shadowMapFar();

                    lightPos -= lightDir * dd;
                    clipFar += sceneCamera->nearPlane();

                    shadowCam->setNearPlane(clipNear);
                    shadowCam->setFarPlane(clipFar);

                    // The shadow camera's projection is (more or less?) an ordinary orthographic projection.
                    float deltaZ = clipFar - clipNear;
                    float halfWidth = (M_PI / 2 * o) / 2;
                    if (deltaZ != 0) {
                        QMatrix4x4 proj;
                        float *writePtr = proj.data();
                        writePtr[0] = 1.0f / halfWidth;
                        writePtr[5] = 1.0f / (halfWidth / shadowCam->aspectRatio());
                        writePtr[10] = -2.0f / deltaZ;
                        writePtr[11] = 0.0f;
                        writePtr[14] = -(clipNear + clipFar) / deltaZ;
                        writePtr[15] = 1.0f;

                        shadowCam->setProjectionMatrix(proj);
                    }

                    // Shadow camera's view matrix.
                    lightPos.setZ(-lightPos.z()); // invert, who knows why (Left vs. Right handed mess in 3DS)
                    shadowCam->setPosition(lightPos);
                    shadowCam->setViewCenter(lightPos - lightDir); // ditto
                    shadowCam->setUpVector(QVector3D(0, 1, 0)); // no roll needed

                    camSel->setCamera(shadowCam);

                    Qt3DRender::QClearBuffers *clearBuffers = new Qt3DRender::QClearBuffers(camSel);
                    clearBuffers->setBuffers(Qt3DRender::QClearBuffers::ColorDepthStencilBuffer);
                    clearBuffers->setClearColor(Qt::white);
                    new Qt3DRender::QNoDraw(clearBuffers);

                    Qt3DRender::QFilterKey *shadowFilterKey = new Qt3DRender::QFilterKey;
                    shadowFilterKey->setName(QLatin1String("pass"));
                    shadowFilterKey->setValue(QLatin1String("shadowOrtho"));

                    Qt3DRender::QRenderPassFilter *shadowFilter = new Qt3DRender::QRenderPassFilter(camSel);
                    shadowFilter->addMatch(shadowFilterKey);
                    Qt3DRender::QSortPolicy *sortPolicyOpaque = opaquePassSortPolicy(shadowFilter);
                    Qt3DRender::QLayerFilter *layerFilterOpaque = new Qt3DRender::QLayerFilter(sortPolicyOpaque);
                    layerFilterOpaque->addLayer(layerData->opaqueTag);

                    shadowFilter = new Qt3DRender::QRenderPassFilter(camSel);
                    shadowFilter->addMatch(shadowFilterKey);
                    Qt3DRender::QSortPolicy *sortPolicyTransparent = transparentPassSortPolicy(shadowFilter);
                    Qt3DRender::QLayerFilter *layerFilterTransparent = new Qt3DRender::QLayerFilter(sortPolicyTransparent);
                    layerFilterTransparent->addLayer(layerData->transparentTag);

                    // 2 blur passes
                    auto genOrthoBlurPassFg = [=](Qt3DRender::QAbstractTexture *inTex, Qt3DRender::QAbstractTexture *outTex, const QString &passName)
                    {
                        Qt3DRender::QRenderTargetSelector *rtSelector = new Qt3DRender::QRenderTargetSelector(layerData->shadowMapData.shadowRoot);
                        Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
                        Qt3DRender::QRenderTargetOutput *rtOutput = new Qt3DRender::QRenderTargetOutput;
                        rtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
                        rtOutput->setTexture(outTex);
                        rt->addOutput(rtOutput);
                        rtSelector->setTarget(rt);

                        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(rtSelector);
                        viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

                        // No Camera since the shaders do not care about view or projection.

                        Qt3DRender::QFilterKey *filterKey = new Qt3DRender::QFilterKey;
                        filterKey->setName(QLatin1String("pass"));
                        filterKey->setValue(passName);

                        Qt3DRender::QParameter *texParam = new Qt3DRender::QParameter(QLatin1String("depthSrc"),
                                                                                      QVariant::fromValue(inTex));

                        Qt3DRender::QRenderPassFilter *filter = new Qt3DRender::QRenderPassFilter(viewport);
                        filter->addMatch(filterKey);
                        Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(filter);
                        layerFilter->addLayer(m_fsQuadTag);

                        Qt3DRender::QParameter *camPropsParam = new Qt3DRender::QParameter(QLatin1String("camera_properties"),
                                                                                           QVector2D(light3DS->shadowFilter(), light3DS->shadowMapFar()));
                        filter->addParameter(texParam);
                        filter->addParameter(camPropsParam);
                    };

                    genOrthoBlurPassFg(d->shadowMapTexture, d->shadowMapTextureTemp, QLatin1String("shadowOrthoBlurX"));
                    genOrthoBlurPassFg(d->shadowMapTextureTemp, d->shadowMapTexture, QLatin1String("shadowOrthoBlurY"));

                    // Uniforms
                    d->shadowSampler = new Qt3DRender::QParameter;
                    d->shadowSampler->setName(QLatin1String("shadowmap") + lightIndexStr);
                    d->shadowSampler->setValue(QVariant::fromValue(d->shadowMapTexture));

                    d->shadowMatrixParam = new Qt3DRender::QParameter;
                    d->shadowMatrixParam->setName(QLatin1String("shadowmap") + lightIndexStr + QLatin1String("_matrix"));
                    // [-1, 1] -> [0, 1]
                    const QMatrix4x4 bias(0.5f, 0.0f, 0.0f, 0.5f, // ctor takes row major
                                          0.0f, 0.5f, 0.0f, 0.5f,
                                          0.0f, 0.0f, 0.5f, 0.5f,
                                          0.0f, 0.0f, 0.0f, 1.0f);
                    const QMatrix4x4 lightVP = shadowCam->projectionMatrix() * shadowCam->viewMatrix();
                    d->shadowMatrixParam->setValue(QVariant::fromValue(bias * lightVP));
                }

                d->shadowControlParam = new Qt3DRender::QParameter;
                d->shadowControlParam->setName(QLatin1String("shadowmap") + lightIndexStr + QLatin1String("_control"));
                d->shadowControlParam->setValue(QVariant::fromValue(QVector4D(light3DS->shadowBias(), light3DS->shadowFactor(), light3DS->shadowMapFar(), 0)));
            }
        }
    }

    if (!layerData->shadowMapData.shadowCasters.isEmpty())
        qCDebug(lcScene, "Layer %s has %d shadow casting lights", layer3DS->id().constData(), layerData->shadowMapData.shadowCasters.count());
}

static void setBlending(Qt3DRender::QBlendEquation *blendFunc,
                        Qt3DRender::QBlendEquationArguments *blendArgs,
                        Q3DSLayerNode *layer3DS)
{
    switch (layer3DS->blendType()) {
    case Q3DSLayerNode::Screen:
        blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
        blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::SourceAlpha);
        blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::One);
        break;

    case Q3DSLayerNode::Multiply:
        blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
        blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::DestinationColor);
        blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::Zero);
        blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::One);
        break;

    case Q3DSLayerNode::Add:
        blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
        blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::One);
        break;

    case Q3DSLayerNode::Subtract:
        blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::ReverseSubtract);
        blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::One);
        break;

    // ### add extension specific modes

    default: // Normal
        blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
        blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
        blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
        break;
    }
}

Qt3DRender::QFrameGraphNode *Q3DSSceneBuilder::buildCompositor(Qt3DRender::QFrameGraphNode *parent,
                                                               Qt3DCore::QEntity *parentEntity)
{
    Qt3DRender::QCamera *camera = new Qt3DRender::QCamera;
    camera->setObjectName(QObject::tr("compositor camera"));
    camera->setProjectionType(Qt3DRender::QCameraLens::OrthographicProjection);
    camera->setLeft(-1);
    camera->setRight(1);
    camera->setTop(1);
    camera->setBottom(-1);
    camera->setPosition(QVector3D(0, 0, 1));
    camera->setViewCenter(QVector3D(0, 0, 0));

    Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(parent);

    Qt3DRender::QLayer *tag = new Qt3DRender::QLayer;
    layerFilter->addLayer(tag);

    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(layerFilter);
    viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

    Qt3DRender::QCameraSelector *cameraSelector = new Qt3DRender::QCameraSelector(viewport);
    cameraSelector->setCamera(camera);

    Qt3DRender::QClearBuffers *clearBuffers = new Qt3DRender::QClearBuffers(cameraSelector);
    if (m_scene->useClearColor()) {
        clearBuffers->setBuffers(Qt3DRender::QClearBuffers::ColorDepthStencilBuffer);
        clearBuffers->setClearColor(m_scene->clearColor());
    } else {
        clearBuffers->setBuffers(Qt3DRender::QClearBuffers::DepthStencilBuffer);
    }

    Q3DSPresentation::forAllLayers(m_scene, [=](Q3DSLayerNode *layer3DS) {
        Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
        Q_ASSERT(data);
        Qt3DCore::QEntity *compositorEntity = new Qt3DCore::QEntity(parentEntity);
        compositorEntity->setObjectName(QObject::tr("compositor for %1").arg(QString::fromUtf8(layer3DS->id())));
        data->compositorEntity = compositorEntity;
        if (!layer3DS->flags().testFlag(Q3DSNode::Active))
            compositorEntity->setEnabled(false);

        // QPlaneMesh works here because the compositor shader is provided by
        // us, not imported from 3DS, and so the VS uses the Qt3D attribute names.
        Qt3DExtras::QPlaneMesh *mesh = new Qt3DExtras::QPlaneMesh;
        mesh->setWidth(2);
        mesh->setHeight(2);
        mesh->setMirrored(true);
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
        transform->setRotationX(90);

        // defer the sizing and positioning
        data->updateCompositorCalculations = [=]() {
            mesh->setWidth(2 * data->layerSize.width() / float(data->parentSize.width()));
            mesh->setHeight(2 * data->layerSize.height() / float(data->parentSize.height()));
            const float x = data->layerPos.x() / float(data->parentSize.width()) * 2;
            const float y = -data->layerPos.y() / float(data->parentSize.height()) * 2;
            transform->setTranslation(QVector3D(-(2.0f - mesh->width()) / 2 + x,
                                                (2.0f - mesh->height()) / 2 + y,
                                                0));
        };

        Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial;
        material->addParameter(new Qt3DRender::QParameter(QLatin1String("tex"), data->sizeManagedTextures.first().texture)); // color

        Qt3DRender::QEffect *effect = new Qt3DRender::QEffect;
        Qt3DRender::QTechnique *technique = new Qt3DRender::QTechnique;
        bool isGLES = false;
        Q3DSDefaultMaterialGenerator::addDefaultApiFilter(technique, &isGLES);

        Qt3DRender::QRenderPass *renderPass = new Qt3DRender::QRenderPass;

        Qt3DRender::QBlendEquation *blendFunc = new Qt3DRender::QBlendEquation;
        Qt3DRender::QBlendEquationArguments *blendArgs = new Qt3DRender::QBlendEquationArguments;
        setBlending(blendFunc, blendArgs, layer3DS);
        renderPass->addRenderState(blendFunc);
        renderPass->addRenderState(blendArgs);
        Qt3DRender::QDepthTest *depthTest = new Qt3DRender::QDepthTest;
        depthTest->setDepthFunction(Qt3DRender::QDepthTest::Always);
        renderPass->addRenderState(depthTest);

        Qt3DRender::QShaderProgram *shaderProgram = new Qt3DRender::QShaderProgram;
        QString vertSuffix;
        QString fragSuffix;
        if (isGLES) {
            vertSuffix = QLatin1String(".vert");
            fragSuffix = QLatin1String(".frag");
        } else {
            vertSuffix = QLatin1String("_core.vert");
            fragSuffix = QLatin1String("_core.frag");
        }
        shaderProgram->setVertexShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl(QLatin1String("qrc:/q3ds/shaders/compositor") + vertSuffix)));
        QString fragSrc = QLatin1String("qrc:/q3ds/shaders/compositor") + fragSuffix;
        if (m_flags.testFlag(LayerMSAA4x))
            fragSrc = QLatin1String("qrc:/q3ds/shaders/compositor_ms") + fragSuffix;
        shaderProgram->setFragmentShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl(fragSrc)));
        renderPass->setShaderProgram(shaderProgram);

        technique->addRenderPass(renderPass);
        effect->addTechnique(technique);
        material->setEffect(effect);

        compositorEntity->addComponent(tag);
        compositorEntity->addComponent(mesh);
        compositorEntity->addComponent(transform);
        compositorEntity->addComponent(material);
    }, true); // process layers in reverse order

    return layerFilter;
}

void Q3DSSceneBuilder::buildFsQuad(Qt3DCore::QEntity *parentEntity,
                                   const QStringList &passNames,
                                   const QVector<Qt3DRender::QShaderProgram *> &passProgs,
                                   Qt3DRender::QLayer *tag)
{
    Qt3DCore::QEntity *fsQuadEntity = new Qt3DCore::QEntity(parentEntity);
    fsQuadEntity->setObjectName(QObject::tr("fullscreen quad"));

    // The shaders should be prepared for Qt3D attribute names...
    Qt3DExtras::QPlaneMesh *mesh = new Qt3DExtras::QPlaneMesh;
    mesh->setWidth(2);
    mesh->setHeight(2);
    // ...and for a transform which is necessary due to QPlaneGeometry using the X-Z plane instead of X-Y.
    Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
    transform->setRotationX(90);
    mesh->setMirrored(true);

    Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial;

    Qt3DRender::QEffect *effect = new Qt3DRender::QEffect;
    Qt3DRender::QTechnique *technique = new Qt3DRender::QTechnique;
    Q3DSDefaultMaterialGenerator::addDefaultApiFilter(technique);
    markAsMainTechnique(technique); // just to make the TechniqueFilter happy

    for (int i = 0; i < passNames.count(); ++i) {
        Qt3DRender::QFilterKey *filterKey = new Qt3DRender::QFilterKey;
        filterKey->setName(QLatin1String("pass"));
        filterKey->setValue(passNames[i]);

        Qt3DRender::QRenderPass *pass = new Qt3DRender::QRenderPass;
        pass->addFilterKey(filterKey);
        pass->addRenderState(new Qt3DRender::QNoDepthMask);

        pass->setShaderProgram(passProgs[i]);

        technique->addRenderPass(pass);
    }

    effect->addTechnique(technique);
    material->setEffect(effect);

    fsQuadEntity->addComponent(tag);
    fsQuadEntity->addComponent(mesh);
    fsQuadEntity->addComponent(transform);
    fsQuadEntity->addComponent(material);
}

void Q3DSSceneBuilder::buildLayerScene(Q3DSGraphObject *obj, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    if (!obj)
        return;

    auto addChildren = [this, layer3DS](Q3DSGraphObject *obj, Qt3DCore::QEntity *parent) {
        obj = obj->firstChild();
        while (obj) {
            buildLayerScene(obj, layer3DS, parent);
            obj = obj->nextSibling();
        }
    };

    if (!obj->isNode()) {
        obj->addPropertyChangeObserver(std::bind(&Q3DSSceneBuilder::handlePropertyChange, this,
                                                 std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        return;
    }

    Qt3DCore::QEntity *newEntity = nullptr;

    switch (obj->type()) {
    case Q3DSGraphObject::Light:
        newEntity = buildLight(static_cast<Q3DSLightNode *>(obj), layer3DS, parent);
        break;

    case Q3DSGraphObject::Group:
        newEntity = buildGroup(static_cast<Q3DSGroupNode *>(obj), layer3DS, parent);
        addChildren(obj, newEntity);
        break;

    case Q3DSGraphObject::Model:
        newEntity = buildModel(static_cast<Q3DSModelNode *>(obj), layer3DS, parent);
        addChildren(obj, newEntity);
        break;

    case Q3DSGraphObject::Text:
        newEntity = buildText(static_cast<Q3DSTextNode *>(obj), layer3DS, parent);
        addChildren(obj, newEntity);
        break;

    case Q3DSGraphObject::Component:
    {
        Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
        newEntity = buildComponent(comp, layer3DS, parent);
        // enter the first slide of the Component
        if (!comp->currentSlide() && comp->masterSlide()) {
            if (!comp->masterSlide()->attached())
                comp->masterSlide()->setAttached(new Q3DSSlideAttached);
            Q3DSSlide *s = static_cast<Q3DSSlide *>(comp->masterSlide()->firstChild());
            if (s) {
                if (!s->attached())
                    s->setAttached(new Q3DSSlideAttached);
                comp->m_currentSlide = s;
                m_presentation->applySlidePropertyChanges(s);
                // cannot start animations before the Qt3D scene building completes -> defer
            }
        }
        // build the Component subtree
        addChildren(obj, newEntity);
    }
        break;

    default: // ignore Camera here
        break;
    }
}

Qt3DCore::QTransform *Q3DSSceneBuilder::initEntityForNode(Qt3DCore::QEntity *entity, Q3DSNode *node, Q3DSLayerNode *layer3DS)
{
    Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
    setNodeProperties(node, entity, transform, NodePropUpdateAttached);
    entity->addComponent(transform);
    Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(node->attached());
    data->layer3DS = layer3DS;
    return transform;
}

namespace {
QMatrix4x4 generateRotationMatrix(const QVector3D &nodeRotation, Q3DSNode::RotationOrder order)
{
    QMatrix4x4 rotationMatrix;
    switch (order) {
    case Q3DSNode::XYZ:
    case Q3DSNode::ZYXr:
        rotationMatrix.rotate(nodeRotation.x(), QVector3D(1, 0, 0));
        rotationMatrix.rotate(nodeRotation.y(), QVector3D(0, 1, 0));
        rotationMatrix.rotate(nodeRotation.z(), QVector3D(0, 0, 1));
        break;
    case Q3DSNode::XYZr: // XYZr is what the editor outputs in practice
    case Q3DSNode::ZYX:
        rotationMatrix.rotate(nodeRotation.z(), QVector3D(0, 0, 1));
        rotationMatrix.rotate(nodeRotation.y(), QVector3D(0, 1, 0));
        rotationMatrix.rotate(nodeRotation.x(), QVector3D(1, 0, 0));
        break;
    case Q3DSNode::YZX:
    case Q3DSNode::XZYr:
        rotationMatrix.rotate(nodeRotation.y(), QVector3D(0, 1, 0));
        rotationMatrix.rotate(nodeRotation.z(), QVector3D(0, 0, 1));
        rotationMatrix.rotate(nodeRotation.x(), QVector3D(1, 0, 0));
        break;
    case Q3DSNode::ZXY:
    case Q3DSNode::YXZr:
        rotationMatrix.rotate(nodeRotation.z(), QVector3D(0, 0, 1));
        rotationMatrix.rotate(nodeRotation.x(), QVector3D(1, 0, 0));
        rotationMatrix.rotate(nodeRotation.y(), QVector3D(0, 1, 0));
        break;
    case Q3DSNode::XZY:
    case Q3DSNode::YZXr:
        rotationMatrix.rotate(nodeRotation.x(), QVector3D(1, 0, 0));
        rotationMatrix.rotate(nodeRotation.z(), QVector3D(0, 0, 1));
        rotationMatrix.rotate(nodeRotation.y(), QVector3D(0, 1, 0));
        break;
    case Q3DSNode::YXZ:
    case Q3DSNode::ZXYr:
        rotationMatrix.rotate(nodeRotation.y(), QVector3D(0, 1, 0));
        rotationMatrix.rotate(nodeRotation.x(), QVector3D(1, 0, 0));
        rotationMatrix.rotate(nodeRotation.z(), QVector3D(0, 0, 1));
        break;
    default:
        break;
    }
    return rotationMatrix;
}
}

void Q3DSSceneBuilder::setNodeProperties(Q3DSNode *node, Qt3DCore::QEntity *entity,
                                         Qt3DCore::QTransform *transform, SetNodePropFlags flags)
{
    Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(node->attached());
    Q_ASSERT(data);

    // Our scene uses a right-handed coordinate system. Left handed nodes
    // (which is the default for 3DS) must have their translation and rotation
    // adjusted accordingly.
    const bool leftHanded = node->orientation() == Q3DSNode::LeftHanded;

    QMatrix4x4 rot = generateRotationMatrix(node->rotation(), node->rotationOrder());

    QMatrix4x4 m;
    float *mp = m.data();
    const QVector3D pos = node->position();
    const QVector3D scale = node->scale();
    const QVector3D scaledPivot = -node->pivot() * scale;
    mp[0] = scale.x();
    mp[5] = scale.y();
    mp[10] = scale.z();
    mp[12] = scaledPivot.x();
    mp[13] = scaledPivot.y();
    mp[14] = leftHanded ? scaledPivot.z() : -scaledPivot.z();
    m = rot * m;
    mp[12] += pos.x();
    mp[13] += pos.y();
    mp[14] += leftHanded ? pos.z() : -pos.z();
    if (leftHanded) {
        adjustRotationLeftToRight(&m);
        mp[14] *= -1;
    }

    if (node->type() == Q3DSGraphObject::Text)
        m.rotate(90, 1, 0, 0); // adjust for QPlaneMesh's X-Z default

    transform->setMatrix(m); // this will also decompose into the individual properties -> good!

    if (flags.testFlag(NodePropUpdateAttached)) {
        Q_ASSERT(entity);
        data->entity = entity;
        data->transform = transform;
    }

    UpdateGlobalFlags ugflags = 0;
    if (flags.testFlag(NodePropUpdateGlobalsRecursively))
        ugflags |= UpdateGlobalsRecursively;
    updateGlobals(node, ugflags);
}

void Q3DSSceneBuilder::updateGlobals(Q3DSNode *node, UpdateGlobalFlags flags)
{
    Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(node->attached());
    if (!data)
        return;

    Q3DSNodeAttached *parentData = node->parent()->isNode()
            ? static_cast<Q3DSNodeAttached *>(node->parent()->attached()) : nullptr;

    QMatrix4x4 globalTransform;
    float globalOpacity;
    bool globalVisibility;

    if (parentData) {
        if (!flags.testFlag(UpdateGlobalsSkipTransform)) {
            // cache the global transform in the Q3DSNode for easy access
            globalTransform = parentData->globalTransform * data->transform->matrix();
        }
        // update the global, inherited opacity
        globalOpacity = parentData->globalOpacity * node->localOpacity();
        // update inherited visibility
        globalVisibility = node->flags().testFlag(Q3DSNode::Active) && parentData->globalVisibility;
    } else {
        if (!flags.testFlag(UpdateGlobalsSkipTransform))
            globalTransform = data->transform->matrix();
        globalOpacity = node->localOpacity();
        globalVisibility = node->flags().testFlag(Q3DSNode::Active);
    }

    if (!flags.testFlag(UpdateGlobalsSkipTransform) && globalTransform != data->globalTransform) {
        data->globalTransform = globalTransform;
        data->dirty.setFlag(Q3DSGraphObjectAttached::GlobalTransformDirty, true);
    }
    if (globalOpacity != data->globalOpacity) {
        data->globalOpacity = globalOpacity;
        data->dirty.setFlag(Q3DSGraphObjectAttached::GlobalOpacityDirty, true);
    }
    if (globalVisibility != data->globalVisibility) {
        data->globalVisibility = globalVisibility;
        data->dirty.setFlag(Q3DSGraphObjectAttached::GlobalVisibilityDirty, true);
    }

    if (flags.testFlag(UpdateGlobalsRecursively)) {
        Q3DSGraphObject *obj = node->firstChild();
        while (obj) {
            if (obj->isNode())
                updateGlobals(static_cast<Q3DSNode *>(obj), flags);
            obj = obj->nextSibling();
        }
    }
}

Qt3DCore::QEntity *Q3DSSceneBuilder::buildGroup(Q3DSGroupNode *group3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSGroupAttached *data = new Q3DSGroupAttached;
    group3DS->setAttached(data);

    Qt3DCore::QEntity *group = new Qt3DCore::QEntity(parent);
    group->setObjectName(QObject::tr("group %1").arg(QString::fromUtf8(group3DS->id())));
    initEntityForNode(group, group3DS, layer3DS);

    group3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneBuilder::handlePropertyChange, this,
                                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    return group;
}

Qt3DCore::QEntity *Q3DSSceneBuilder::buildComponent(Q3DSComponentNode *comp3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSComponentAttached *data = new Q3DSComponentAttached;
    comp3DS->setAttached(data);

    Qt3DCore::QEntity *comp = new Qt3DCore::QEntity(parent);
    comp->setObjectName(QObject::tr("component %1").arg(QString::fromUtf8(comp3DS->id())));
    initEntityForNode(comp, comp3DS, layer3DS);

    comp3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneBuilder::handlePropertyChange, this,
                                                 std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    return comp;
}

class Q3DSTextImage : public Qt3DRender::QPaintedTextureImage
{
public:
    Q3DSTextImage(Q3DSTextNode *text3DS, Q3DSTextRenderer *textRenderer)
        : m_text3DS(text3DS), m_textRenderer(textRenderer)
    { }

    void paint(QPainter *painter) override;

private:
    Q3DSTextNode *m_text3DS;
    Q3DSTextRenderer *m_textRenderer;
};

void Q3DSTextImage::paint(QPainter *painter)
{
    m_textRenderer->renderText(painter, m_text3DS);
}

Qt3DCore::QEntity *Q3DSSceneBuilder::buildText(Q3DSTextNode *text3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSTextAttached *data = new Q3DSTextAttached;
    text3DS->setAttached(data);

    Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(parent);
    entity->setObjectName(QObject::tr("text %1").arg(QString::fromUtf8(text3DS->id())));
    initEntityForNode(entity, text3DS, layer3DS);

    text3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneBuilder::handlePropertyChange, this,
                                                 std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    QSize sz = m_textRenderer->textImageSize(text3DS);
    if (sz.isEmpty())
        return entity;

    Qt3DExtras::QPlaneMesh *mesh = new Qt3DExtras::QPlaneMesh;
    mesh->setWidth(sz.width());
    mesh->setHeight(sz.height());
    mesh->setMirrored(true);
    entity->addComponent(mesh);

    data->opacityParam = new Qt3DRender::QParameter;
    data->opacityParam->setName(QLatin1String("opacity"));
    data->opacityParam->setValue(data->globalOpacity);

    data->colorParam = new Qt3DRender::QParameter;
    data->colorParam->setName(QLatin1String("color"));
    data->colorParam->setValue(text3DS->color());

    data->texture = new Qt3DRender::QTexture2D;
    data->textureImage = new Q3DSTextImage(text3DS, m_textRenderer);
    data->textureImage->setSize(sz);
    data->texture->addTextureImage(data->textureImage);

    data->textureParam = new Qt3DRender::QParameter;
    data->textureParam->setName(QLatin1String("tex"));
    data->textureParam->setValue(QVariant::fromValue(data->texture));

    Qt3DRender::QMaterial *material = m_textMatGen->generateMaterial({ data->opacityParam, data->colorParam, data->textureParam });
    entity->addComponent(material);

    return entity;
}

void Q3DSSceneBuilder::updateText(Q3DSTextNode *text3DS, bool needsNewImage)
{
    Q3DSTextAttached *data = static_cast<Q3DSTextAttached *>(text3DS->attached());
    Q_ASSERT(data);

    if (data->dirty.testFlag(Q3DSGraphObjectAttached::GlobalOpacityDirty))
        data->opacityParam->setValue(data->globalOpacity);

    data->colorParam->setValue(text3DS->color());

    if (needsNewImage) // textstring, leading, tracking
        data->textureImage->update();
}

Qt3DCore::QEntity *Q3DSSceneBuilder::buildLight(Q3DSLightNode *light3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSLightAttached *data = new Q3DSLightAttached;
    light3DS->setAttached(data);

    Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(parent);
    entity->setObjectName(QObject::tr("light %1").arg(QString::fromUtf8(light3DS->id())));
    initEntityForNode(entity, light3DS, layer3DS);

    setLightProperties(light3DS, true);

    light3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneBuilder::handlePropertyChange, this,
                                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    return entity;
}

static inline QColor mulColor(const QColor &c, float f)
{
    return QColor::fromRgbF(c.redF() * f, c.greenF() * f, c.blueF() * f);
}

void Q3DSSceneBuilder::setLightProperties(Q3DSLightNode *light3DS, bool forceUpdate)
{
    // only parameter setup, the uniform buffer is handled separately elsewhere

    Q3DSLightAttached *data = static_cast<Q3DSLightAttached *>(light3DS->attached());
    Q_ASSERT(data);

    // Instead of generating Qt3D light objects, do lighting on our own. The
    // two worlds (Qt3D / 3DS) are just not compatible.
    Q3DSLightSource *ls = &data->lightSource;
    const bool leftHanded = light3DS->orientation() == Q3DSNode::LeftHanded;

    if (forceUpdate || data->dirty.testFlag(Q3DSGraphObjectAttached::GlobalTransformDirty)) {
        if (!ls->positionParam)
            ls->positionParam = new Qt3DRender::QParameter;
        ls->positionParam->setName(QLatin1String("position"));
        ls->positionParam->setValue(data->globalTransform.column(3).toVector3D());

        if (!ls->directionParam)
            ls->directionParam = new Qt3DRender::QParameter;
        ls->directionParam->setName(QLatin1String("direction"));
        ls->directionParam->setValue(directionFromTransform(data->globalTransform, leftHanded));
    }

    if (!ls->upParam)
        ls->upParam = new Qt3DRender::QParameter;
    ls->upParam->setName(QLatin1String("up"));
    if (!ls->rightParam)
        ls->rightParam = new Qt3DRender::QParameter;
    ls->rightParam->setName(QLatin1String("right"));
    if (light3DS->lightType() == Q3DSLightNode::Area) {
        QVector4D v = data->globalTransform * QVector4D(0, 1, 0, 0);
        v.setW(light3DS->areaHeight());
        ls->upParam->setValue(v);
        v = data->globalTransform * QVector4D(1, 0, 0, 0);
        v.setW(light3DS->areaWidth());
        ls->rightParam->setValue(v);
    } else {
        ls->upParam->setValue(QVector4D());
        ls->rightParam->setValue(QVector4D());
    }

    const float normalizedBrightness = light3DS->brightness() / 100.0f;

    if (!ls->diffuseParam)
        ls->diffuseParam = new Qt3DRender::QParameter;
    ls->diffuseParam->setName(QLatin1String("diffuse"));
    ls->diffuseParam->setValue(mulColor(light3DS->diffuse(), normalizedBrightness));

    if (!ls->ambientParam)
        ls->ambientParam = new Qt3DRender::QParameter;
    ls->ambientParam->setName(QLatin1String("ambient"));
    ls->ambientParam->setValue(light3DS->ambient());

    if (!ls->specularParam)
        ls->specularParam = new Qt3DRender::QParameter;
    ls->specularParam->setName(QLatin1String("specular"));
    ls->specularParam->setValue(mulColor(light3DS->specular(), normalizedBrightness));

/*
    if (!ls->spotExponentParam)
        ls->spotExponentParam = new Qt3DRender::QParameter;
    ls->spotExponentParam->setName(QLatin1String("spotExponent"));
    ls->spotExponentParam->setValue(1.0f);

    if (!ls->spotCutoffParam)
        ls->spotCutoffParam = new Qt3DRender::QParameter;
    ls->spotCutoffParam->setName(QLatin1String("spotCutoff"));
    ls->spotCutoffParam->setValue(180.0f);
*/
    if (!ls->constantAttenuationParam)
        ls->constantAttenuationParam = new Qt3DRender::QParameter;
    ls->constantAttenuationParam->setName(QLatin1String("constantAttenuation"));
    ls->constantAttenuationParam->setValue(1.0f);

    if (!ls->linearAttenuationParam)
        ls->linearAttenuationParam = new Qt3DRender::QParameter;
    ls->linearAttenuationParam->setName(QLatin1String("linearAttenuation"));
    ls->linearAttenuationParam->setValue(light3DS->linearFade());

    if (!ls->quadraticAttenuationParam)
        ls->quadraticAttenuationParam = new Qt3DRender::QParameter;
    ls->quadraticAttenuationParam->setName(QLatin1String("quadraticAttenuation"));
    ls->quadraticAttenuationParam->setValue(light3DS->expFade());

    if (!ls->widthParam)
        ls->widthParam = new Qt3DRender::QParameter;
    ls->widthParam->setName(QLatin1String("width"));
    ls->widthParam->setValue(light3DS->areaWidth());

    if (!ls->heightParam)
        ls->heightParam = new Qt3DRender::QParameter;
    ls->heightParam->setName(QLatin1String("height"));
    ls->heightParam->setValue(light3DS->areaHeight());

    // is this shadow stuff for lightmaps?
    const float shadowDist = 5000.0f; // camera->clipFar() ### FIXME later
    if (!ls->shadowControlsParam)
        ls->shadowControlsParam = new Qt3DRender::QParameter;
    ls->shadowControlsParam->setName(QLatin1String("shadowControls"));
    ls->shadowControlsParam->setValue(QVector4D(light3DS->shadowBias(), light3DS->shadowFactor(), shadowDist, 0));

    if (!ls->shadowViewParam)
        ls->shadowViewParam = new Qt3DRender::QParameter;
    ls->shadowViewParam->setName(QLatin1String("shadowView"));
    if (light3DS->lightType() == Q3DSLightNode::Point)
        ls->shadowViewParam->setValue(QMatrix4x4());
    else
        ls->shadowViewParam->setValue(data->globalTransform);

    if (!ls->shadowIdxParam)
        ls->shadowIdxParam = new Qt3DRender::QParameter;
    ls->shadowIdxParam->setName(QLatin1String("shadowIdx"));
    ls->shadowIdxParam->setValue(0); // ### FIXME later

    // ### scoped lights not supported yet
}

Qt3DCore::QEntity *Q3DSSceneBuilder::buildModel(Q3DSModelNode *model3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSModelAttached *data = new Q3DSModelAttached;
    model3DS->setAttached(data);

    Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(parent);
    entity->setObjectName(QObject::tr("model %1").arg(QString::fromUtf8(model3DS->id())));
    initEntityForNode(entity, model3DS, layer3DS);

    // Get List of Materials
    QVector<Q3DSGraphObject *> materials;
    for (int i = 0; i < model3DS->childCount(); ++i) {
        Q3DSGraphObject *child = model3DS->childAtIndex(i);
        if (child->type() == Q3DSGraphObject::DefaultMaterial
                || child->type() == Q3DSGraphObject::CustomMaterial
                || child->type() == Q3DSGraphObject::ReferencedMaterial)
            materials.append(child);
    }

    MeshList meshList = model3DS->mesh();
    if (!meshList)
        return entity;

    const int meshCount = meshList->count();
    Q_ASSERT(materials.count() == meshCount);
    Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(model3DS->attached());
    modelData->subMeshes.reserve(meshCount);

    for (int i = 0; i < meshCount; ++i) {
        auto material = materials.at(i);
        auto mesh = meshList->at(i);

        Q3DSModelAttached::SubMesh sm;
        sm.entity = new Qt3DCore::QEntity(entity);
        sm.entity->setObjectName(QObject::tr("model %1 submesh #%2").arg(QString::fromUtf8(model3DS->id())).arg(i));
        sm.entity->addComponent(mesh);
        sm.material = material;
        sm.resolvedMaterial = material;
        // follow the reference for ReferencedMaterial
        if (sm.material->type() == Q3DSGraphObject::ReferencedMaterial) {
            Q3DSReferencedMaterial *matRef = static_cast<Q3DSReferencedMaterial *>(sm.material);
            if (matRef->referencedMaterial())
                sm.resolvedMaterial = matRef->referencedMaterial();
        }
        // leave sm.materialComponent unset for now -> defer until the scene is processed once and so all lights are known
        modelData->subMeshes.append(sm);
    }

    // update submesh entities wrt opaque vs transparent
    retagSubMeshes(model3DS);

    model3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneBuilder::handlePropertyChange, this,
                                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    return entity;
}

void Q3DSSceneBuilder::buildModelMaterial(Q3DSModelNode *model3DS)
{
    // Scene building phase 2: all lights are known -> generate actual Qt3D materials

    Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(model3DS->attached());
    if (!modelData)
        return;

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(modelData->layer3DS->attached());
    Q_ASSERT(layerData);

    for (Q3DSModelAttached::SubMesh &sm : modelData->subMeshes) {
        if (sm.resolvedMaterial && !sm.materialComponent) {
            if (sm.resolvedMaterial->type() == Q3DSGraphObject::DefaultMaterial) {
                Q3DSDefaultMaterial *defaultMaterial = static_cast<Q3DSDefaultMaterial *>(sm.resolvedMaterial);

                // Create parameters to the shader.
                QVector<Qt3DRender::QParameter *> params = prepareDefaultMaterial(defaultMaterial, model3DS);
                // Update parameter values.
                Q3DSDefaultMaterialAttached *defMatData = static_cast<Q3DSDefaultMaterialAttached *>(defaultMaterial->attached());
                defMatData->opacity = modelData->globalOpacity * defaultMaterial->opacity();
                updateDefaultMaterial(defaultMaterial);

                // Setup camera properties
                if (layerData->cameraPropertiesParam != nullptr)
                    params.append(layerData->cameraPropertiesParam);

                // Setup ambient light total
                QVector3D lightAmbientTotal;
                for (auto light : layerData->lightNodes) {
                    lightAmbientTotal += QVector3D(light->ambient().redF(),
                                                   light->ambient().greenF(),
                                                   light->ambient().blueF());
                }
                if (!layerData->lightAmbientTotalParamenter)
                    layerData->lightAmbientTotalParamenter = new Qt3DRender::QParameter;
                layerData->lightAmbientTotalParamenter->setName(QLatin1String("light_ambient_total"));
                layerData->lightAmbientTotalParamenter->setValue(lightAmbientTotal);
                params.append(layerData->lightAmbientTotalParamenter);

                // Setup lights, use combined buffer for the default material.
                if (!layerData->allLightsConstantBuffer) {
                    layerData->allLightsConstantBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::UniformBuffer);
                    layerData->allLightsConstantBuffer->setObjectName(QLatin1String("all lights constant buffer"));
                    updateLightsBuffer(layerData->allLights, layerData->allLightsConstantBuffer);
                }
                if (!layerData->allLightsParam)
                    layerData->allLightsParam = new Qt3DRender::QParameter;
                layerData->allLightsParam->setName(QLatin1String("cbBufferLights"));
                layerData->allLightsParam->setValue(QVariant::fromValue(layerData->allLightsConstantBuffer));
                params.append(layerData->allLightsParam);

                for (const Q3DSLayerAttached::PerLightShadowMapData &sd : qAsConst(layerData->shadowMapData.shadowCasters)) {
                    if (sd.shadowSampler)
                        params.append(sd.shadowSampler);
                    if (sd.shadowMatrixParam)
                        params.append(sd.shadowMatrixParam);
                    if (sd.shadowControlParam)
                        params.append(sd.shadowControlParam);
                }

                if (layerData->ssaoTextureData.enabled && layerData->ssaoTextureData.ssaoTextureSampler)
                    params.append(layerData->ssaoTextureData.ssaoTextureSampler);

                sm.materialComponent = m_matGen->generateMaterial(defaultMaterial, params, layerData->lightNodes, modelData->layer3DS);
                sm.entity->addComponent(sm.materialComponent);
            } else if (sm.resolvedMaterial->type() == Q3DSGraphObject::CustomMaterial) {

                // ### custom materials - lots of code to be added, light handling should be something like the following:

                // Here lights are provided in two separate buffers.
                if (!layerData->nonAreaLightsConstantBuffer) {
                    layerData->nonAreaLightsConstantBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::UniformBuffer);
                    layerData->nonAreaLightsConstantBuffer->setObjectName(QLatin1String("non-area lights constant buffer"));
                    updateLightsBuffer(layerData->nonAreaLights, layerData->nonAreaLightsConstantBuffer);
                }
                if (!layerData->nonAreaLightsParam)
                    layerData->nonAreaLightsParam = new Qt3DRender::QParameter;
                layerData->nonAreaLightsParam->setName(QLatin1String("cbBufferLights")); // i.e. this cannot be combined with allLightsParam
                layerData->nonAreaLightsParam->setValue(QVariant::fromValue(layerData->nonAreaLightsConstantBuffer));
                //params.append(layerData->nonAreaLightsParam);

                if (!layerData->areaLightsConstantBuffer) {
                    layerData->areaLightsConstantBuffer = new Qt3DRender::QBuffer(Qt3DRender::QBuffer::UniformBuffer);
                    layerData->areaLightsConstantBuffer->setObjectName(QLatin1String("area lights constant buffer"));
                    updateLightsBuffer(layerData->areaLights, layerData->areaLightsConstantBuffer);
                }
                if (!layerData->areaLightsParam)
                    layerData->areaLightsParam = new Qt3DRender::QParameter;
                layerData->areaLightsParam->setName(QLatin1String("cbBufferAreaLights"));
                layerData->areaLightsParam->setValue(QVariant::fromValue(layerData->areaLightsConstantBuffer));
                //params.append(layerData->areaLightsParam);
            }
        }
    }
}

void Q3DSSceneBuilder::retagSubMeshes(Q3DSModelNode *model3DS)
{
    Q3DSModelAttached *data = static_cast<Q3DSModelAttached *>(model3DS->attached());
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(data->layer3DS->attached());

    for (Q3DSModelAttached::SubMesh &sm : data->subMeshes) {
        float opacity = data->globalOpacity;
        bool hasTransparency = false;
        if (sm.resolvedMaterial->type() == Q3DSGraphObject::DefaultMaterial) {
            auto defaultMaterial = static_cast<Q3DSDefaultMaterial *>(sm.resolvedMaterial);
            opacity *= defaultMaterial->opacity();
            // Check maps for transparency as well
            hasTransparency = ((defaultMaterial->diffuseMap() && defaultMaterial->diffuseMap()->hasTransparency()) ||
                               (defaultMaterial->diffuseMap2() && defaultMaterial->diffuseMap2()->hasTransparency()) ||
                               (defaultMaterial->diffuseMap3() && defaultMaterial->diffuseMap3()->hasTransparency()) ||
                               defaultMaterial->opacityMap() ||
                               defaultMaterial->translucencyMap() ||
                               defaultMaterial->displacementmap() ||
                               defaultMaterial->blendMode() != Q3DSDefaultMaterial::Normal);
        } else if (sm.resolvedMaterial->type() == Q3DSGraphObject::CustomMaterial) {
            // ### custom material
        }

        sm.hasTransparency = opacity < 1.0f || hasTransparency;

        Qt3DRender::QLayer *newTag = sm.hasTransparency ? layerData->transparentTag : layerData->opaqueTag;
        if (!sm.entity->components().contains(newTag)) {
            Qt3DRender::QLayer *prevTag = newTag == layerData->transparentTag ? layerData->opaqueTag : layerData->transparentTag;
            sm.entity->removeComponent(prevTag);
            sm.entity->addComponent(newTag);
        }
    }
}

static void prepareTextureParameters(Q3DSTextureParameters &textureParameters, const QString &name)
{
    textureParameters.sampler = new Qt3DRender::QParameter;
    textureParameters.sampler->setName(name + QLatin1String("_sampler"));

    textureParameters.offsets = new Qt3DRender::QParameter;
    textureParameters.offsets->setName(name + QLatin1String("_offsets"));

    textureParameters.rotations = new Qt3DRender::QParameter;
    textureParameters.rotations->setName(name + QLatin1String("_rotations"));

    textureParameters.texture = new Qt3DRender::QTexture2D;
    textureParameters.textureImage = new Qt3DRender::QTextureImage;
    textureParameters.texture->addTextureImage(textureParameters.textureImage);
}

static void updateTextureParameters(Q3DSTextureParameters &textureParameters, Q3DSImage *image)
{
    textureParameters.textureImage->setSource(QUrl::fromLocalFile(image->sourcePath()));

    Qt3DRender::QTextureWrapMode wrapMode;
    switch (image->horizontalTiling()) {
    case Q3DSImage::Tiled:
        wrapMode.setX(Qt3DRender::QTextureWrapMode::Repeat);
        break;
    case Q3DSImage::Mirrored:
        wrapMode.setX(Qt3DRender::QTextureWrapMode::MirroredRepeat);
        break;
    default:
        wrapMode.setX(Qt3DRender::QTextureWrapMode::ClampToEdge);
        break;
    }
    switch (image->verticalTiling()) {
    case Q3DSImage::Tiled:
        wrapMode.setY(Qt3DRender::QTextureWrapMode::Repeat);
        break;
    case Q3DSImage::Mirrored:
        wrapMode.setY(Qt3DRender::QTextureWrapMode::MirroredRepeat);
        break;
    default:
        wrapMode.setY(Qt3DRender::QTextureWrapMode::ClampToEdge);
        break;
    }

    textureParameters.texture->setGenerateMipMaps(true);
    textureParameters.texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    textureParameters.texture->setMinificationFilter(Qt3DRender::QAbstractTexture::LinearMipMapLinear);
    textureParameters.texture->setWrapMode(wrapMode);
    textureParameters.sampler->setValue(QVariant::fromValue(textureParameters.texture));

    const QMatrix4x4 &textureTransform = image->textureTransform();
    const float *m = textureTransform.constData();

    QVector3D offsets(m[12], m[13], 0.0f); // ### z = isPremultiplied?
    textureParameters.offsets->setValue(offsets);

    QVector4D rotations(m[0], m[4], m[1], m[5]);
    textureParameters.rotations->setValue(rotations);
}

QVector<Qt3DRender::QParameter *> Q3DSSceneBuilder::prepareDefaultMaterial(Q3DSDefaultMaterial *m, Q3DSModelNode *model3DS)
{
    QVector<Qt3DRender::QParameter *> params;

    if (!m->attached())
        m->setAttached(new Q3DSDefaultMaterialAttached);

    Q3DSDefaultMaterialAttached *data = static_cast<Q3DSDefaultMaterialAttached *>(m->attached());
    Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(model3DS->attached());
    data->entity = modelData->entity;
    data->model3DS = model3DS;

    if (!data->diffuseParam) {
        data->diffuseParam = new Qt3DRender::QParameter;
        data->diffuseParam->setName(QLatin1String("diffuse_color"));
    }
    params.append(data->diffuseParam);

    if (!data->materialDiffuseParam) {
        data->materialDiffuseParam = new Qt3DRender::QParameter;
        data->materialDiffuseParam->setName(QLatin1String("material_diffuse"));
    }
    params.append(data->materialDiffuseParam);

    if (!data->specularParam) {
        data->specularParam = new Qt3DRender::QParameter;
        data->specularParam->setName(QLatin1String("material_specular"));
    }
    params.append(data->specularParam);

    if (!data->fresnelPowerParam) {
        data->fresnelPowerParam = new Qt3DRender::QParameter;
        data->fresnelPowerParam->setName(QLatin1String("fresnelPower"));
    }
    params.append(data->fresnelPowerParam);

    if (!data->bumpAmountParam) {
        data->bumpAmountParam = new Qt3DRender::QParameter;
        data->bumpAmountParam->setName(QLatin1String("bumpAmount"));
    }
    params.append(data->bumpAmountParam);

    if (!data->materialPropertiesParam) {
        data->materialPropertiesParam = new Qt3DRender::QParameter;
        data->materialPropertiesParam->setName(QLatin1String("material_properties"));
    }
    params.append(data->materialPropertiesParam);

    if (!data->translucentFalloffParam) {
        data->translucentFalloffParam = new Qt3DRender::QParameter;
        data->translucentFalloffParam->setName(QLatin1String("translucentFalloff"));
    }
    params.append(data->translucentFalloffParam);

    if (!data->diffuseLightWrapParam) {
        data->diffuseLightWrapParam = new Qt3DRender::QParameter;
        data->diffuseLightWrapParam->setName(QLatin1String("diffuseLightWrap"));
    }
    params.append(data->diffuseLightWrapParam);

    if (!data->displaceAmountParam) {
        data->displaceAmountParam = new Qt3DRender::QParameter;
        data->displaceAmountParam->setName(QLatin1String("displaceAmount"));
    }
    params.append(data->displaceAmountParam);

    // ### the texture maps are not dynamic right now, meaning not having one
    // and then assigning one in a slide change will not work as expected.

    if (m->diffuseMap()) {
        prepareTextureParameters(data->diffuseMapParams, QLatin1String("diffuseMap"));
        params.append(data->diffuseMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->diffuseMap()->attached())->referencingMaterials.insert(m);
    }

    if (m->diffuseMap2()) {
        prepareTextureParameters(data->diffuseMap2Params, QLatin1String("diffuseMap2"));
        params.append(data->diffuseMap2Params.parameters());
        static_cast<Q3DSImageAttached *>(m->diffuseMap2()->attached())->referencingMaterials.insert(m);
    }

    if (m->diffuseMap3()) {
        prepareTextureParameters(data->diffuseMap3Params, QLatin1String("diffuseMap3"));
        params.append(data->diffuseMap3Params.parameters());
        static_cast<Q3DSImageAttached *>(m->diffuseMap3()->attached())->referencingMaterials.insert(m);
    }

    if (m->specularReflection()) {
        prepareTextureParameters(data->specularReflectionParams, QLatin1String("specularreflection"));
        params.append(data->specularReflectionParams.parameters());
        static_cast<Q3DSImageAttached *>(m->specularReflection()->attached())->referencingMaterials.insert(m);
    }

    if (m->specularMap()) {
        prepareTextureParameters(data->specularMapParams, QLatin1String("specularMap"));
        params.append(data->specularMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->specularMap()->attached())->referencingMaterials.insert(m);
    }

    if (m->bumpMap()) {
        prepareTextureParameters(data->bumpMapParams, QLatin1String("bumpMap"));
        params.append(data->bumpMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->bumpMap()->attached())->referencingMaterials.insert(m);
    }

    if (m->normalMap()) {
        prepareTextureParameters(data->normalMapParams, QLatin1String("normalMap"));
        params.append(data->normalMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->normalMap()->attached())->referencingMaterials.insert(m);
    }

    if (m->displacementmap()) {
        prepareTextureParameters(data->displacementMapParams, QLatin1String("displacementMap"));
        params.append(data->displacementMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->displacementmap()->attached())->referencingMaterials.insert(m);
    }

    if (m->opacityMap()) {
        prepareTextureParameters(data->opacityMapParams, QLatin1String("opacityMap"));
        params.append(data->opacityMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->opacityMap()->attached())->referencingMaterials.insert(m);
    }

    if (m->emissiveMap()) {
        prepareTextureParameters(data->emissiveMapParams, QLatin1String("emissiveMap"));
        params.append(data->emissiveMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->emissiveMap()->attached())->referencingMaterials.insert(m);
    }

    if (m->emissiveMap2()) {
        prepareTextureParameters(data->emissiveMap2Params, QLatin1String("emissiveMap2"));
        params.append(data->emissiveMap2Params.parameters());
        static_cast<Q3DSImageAttached *>(m->emissiveMap2()->attached())->referencingMaterials.insert(m);
    }

    if (m->translucencyMap()) {
        prepareTextureParameters(data->translucencyMapParams, QLatin1String("translucencyMap"));
        params.append(data->translucencyMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->translucencyMap()->attached())->referencingMaterials.insert(m);
    }

    return params;
}

void Q3DSSceneBuilder::updateDefaultMaterial(Q3DSDefaultMaterial *m)
{
    Q3DSDefaultMaterialAttached *data = static_cast<Q3DSDefaultMaterialAttached *>(m->attached());
    Q_ASSERT(data && data->diffuseParam);

    data->diffuseParam->setValue(m->diffuse());

    float emissivePower = 1.0f;
    bool hasLighting = m->shaderLighting() != Q3DSDefaultMaterial::ShaderLighting::NoShaderLighting;
    if (hasLighting)
        emissivePower = m->emissivePower() / 100.0f;

    QVector4D material_diffuse(m->emissiveColor().redF() * emissivePower,
                               m->emissiveColor().greenF() * emissivePower,
                               m->emissiveColor().blueF() * emissivePower,
                               data->opacity);
    data->materialDiffuseParam->setValue(material_diffuse);

    QVector4D material_specular(m->specularTint().redF(),
                                m->specularTint().greenF(),
                                m->specularTint().blueF(),
                                m->ior());
    data->specularParam->setValue(material_specular);

    data->fresnelPowerParam->setValue(m->fresnelPower());

    data->bumpAmountParam->setValue(m->bumpAmount());

    data->translucentFalloffParam->setValue(m->translucentFalloff());

    data->diffuseLightWrapParam->setValue(m->diffuseLightWrap());

    data->displaceAmountParam->setValue(m->displaceAmount());

    auto materialProperties = QVector4D(m->specularAmount(),
                                        m->specularRoughness(),
                                        m->emissivePower(),
                                        0.0f);
    data->materialPropertiesParam->setValue(materialProperties);

    if (m->diffuseMap())
        updateTextureParameters(data->diffuseMapParams, m->diffuseMap());

    if (m->diffuseMap2())
        updateTextureParameters(data->diffuseMap2Params, m->diffuseMap2());

    if (m->diffuseMap3())
        updateTextureParameters(data->diffuseMap3Params, m->diffuseMap3());

    if (m->specularReflection())
        updateTextureParameters(data->specularReflectionParams, m->specularReflection());

    if (m->specularMap())
        updateTextureParameters(data->specularMapParams, m->specularMap());

    if (m->bumpMap())
        updateTextureParameters(data->bumpMapParams, m->bumpMap());

    if (m->normalMap())
        updateTextureParameters(data->normalMapParams, m->normalMap());

    if (m->displacementmap())
        updateTextureParameters(data->displacementMapParams, m->displacementmap());

    if (m->opacityMap())
        updateTextureParameters(data->opacityMapParams, m->opacityMap());

    if (m->emissiveMap())
        updateTextureParameters(data->emissiveMapParams, m->emissiveMap());

    if (m->emissiveMap2())
        updateTextureParameters(data->emissiveMap2Params, m->emissiveMap2());

    if (m->translucencyMap())
        updateTextureParameters(data->translucencyMapParams, m->translucencyMap());
}

void Q3DSSceneBuilder::gatherLights(Q3DSGraphObject *root,
                                    QVector<Q3DSLightSource> *allLights,
                                    QVector<Q3DSLightSource> *nonAreaLights,
                                    QVector<Q3DSLightSource> *areaLights,
                                    QVector<Q3DSLightNode *> *lightNodes)
{
    if (!root)
        return;

    Q3DSGraphObject *obj = root->firstChild();
    while (obj) {
        if (obj->type() == Q3DSGraphObject::Light) {
            Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(obj);
            if (light3DS->flags().testFlag(Q3DSNode::Active)) {
                Q3DSLightAttached *data = static_cast<Q3DSLightAttached *>(light3DS->attached());
                Q_ASSERT(data);
                lightNodes->append(light3DS);
                allLights->append(data->lightSource);
                if (light3DS->lightType() == Q3DSLightNode::Area)
                    areaLights->append(data->lightSource);
                else
                    nonAreaLights->append(data->lightSource);
            }
        }
        gatherLights(obj, allLights, nonAreaLights, areaLights, lightNodes);
        obj = obj->nextSibling();
    }
}

void Q3DSSceneBuilder::updateLightsBuffer(const QVector<Q3DSLightSource> &lights, Qt3DRender::QBuffer *uniformBuffer)
{
    if (!uniformBuffer) // no models in the layer -> no buffers -> handle gracefully
        return; // can also get here when no custom material-specific buffers exist for a given layer because it only uses default material, this is normal

    QByteArray lightBufferData((sizeof(Q3DSLightSourceData) * Q3DS_MAX_NUM_LIGHTS) + (4 * sizeof(qint32)), '\0');
    // Set the number of lights
    qint32 *numLights = reinterpret_cast<qint32 *>(lightBufferData.data());
    *numLights = lights.count();
    // Set the lightData
    Q3DSLightSourceData *lightData = reinterpret_cast<Q3DSLightSourceData *>(lightBufferData.data() + (4 * sizeof(qint32)));
    for (int i = 0; i < lights.count(); ++i) {
        lightData[i].m_position = lights[i].positionParam->value().value<QVector3D>().toVector4D();
        lightData[i].m_direction = lights[i].directionParam->value().value<QVector3D>().toVector4D();
        lightData[i].m_up = lights[i].upParam->value().value<QVector4D>();
        lightData[i].m_right = lights[i].rightParam->value().value<QVector4D>();
        // diffuse
        // Normally this is where the materials diffuse color would be mixed with the light
        // diffuse color, but since we can't update the uniformbuffer between draw calls yet
        // This is moved back into the shader for now.
        //auto lightDiffuseColor = lights[i].diffuseParam->value().value<QColor>();
        //            lightData[i].m_diffuse = QVector4D(materialDiffuseColor.redF() * lightDiffuseColor.redF(),
        //                                               materialDiffuseColor.greenF() * lightDiffuseColor.greenF(),
        //                                               materialDiffuseColor.blueF() * lightDiffuseColor.blueF(),
        //                                               1.0f);
        auto diffuseColor = lights[i].diffuseParam->value().value<QColor>();
        lightData[i].m_diffuse = QVector4D(diffuseColor.redF(), diffuseColor.greenF(), diffuseColor.blueF(), diffuseColor.alphaF());
        // ambient
        auto ambientColor = lights[i].ambientParam->value().value<QColor>();
        lightData[i].m_ambient = QVector4D(ambientColor.redF(), ambientColor.greenF(), ambientColor.blueF(), ambientColor.alphaF());
        // specular
        auto specularColor = lights[i].specularParam->value().value<QColor>();
        lightData[i].m_specular = QVector4D(specularColor.redF(), specularColor.greenF(), specularColor.blueF(), specularColor.alphaF());
        // spotExponent
        // TODO spotExponent
        // spotCutoff
        // TODO spotCutfoff
        // constantAttenuation
        lightData[i].m_constantAttenuation = lights[i].constantAttenuationParam->value().toFloat();
        // linearAttenuation
        lightData[i].m_linearAttenuation = lights[i].linearAttenuationParam->value().toFloat();
        // quadraticAttenuation
        lightData[i].m_quadraticAttenuation = lights[i].quadraticAttenuationParam->value().toFloat();
        // range
        // TODO range
        // width
        lightData[i].m_width = lights[i].widthParam->value().toFloat();
        // height
        lightData[i].m_height = lights[i].heightParam->value().toFloat();
        // shadowControls
        lightData[i].m_shadowControls = lights[i].shadowControlsParam->value().value<QVector4D>();
        // shadowView
        lightData[i].m_shadowView = lights[i].shadowViewParam->value().value<QMatrix4x4>();
        // shadowIdx
        lightData[i].m_shadowIdx = lights[i].shadowIdxParam->value().toInt();
    }
    uniformBuffer->setData(lightBufferData);
}

void Q3DSSceneBuilder::updateModel(Q3DSModelNode *model3DS)
{
    Q3DSModelAttached *data = static_cast<Q3DSModelAttached *>(model3DS->attached());
    Q_ASSERT(data);

    if (data->dirty.testFlag(Q3DSGraphObjectAttached::GlobalOpacityDirty)) {
        // Apply opaque or transparent pass tag to the submeshes.
        retagSubMeshes(model3DS);
        // The model's and material's opacity are both used to determine the final
        // opacity that goes to the shader. Update this.
        Q3DSModelAttached *data = static_cast<Q3DSModelAttached *>(model3DS->attached());
        for (Q3DSModelAttached::SubMesh &sm : data->subMeshes) {
            if (sm.resolvedMaterial->type() == Q3DSGraphObject::DefaultMaterial) {
                auto m = static_cast<Q3DSDefaultMaterial *>(sm.resolvedMaterial);
                auto d = static_cast<Q3DSDefaultMaterialAttached *>(m->attached());
                if (d && d->materialDiffuseParam) {
                    const float opacity = data->globalOpacity * m->opacity();
                    QVector4D c = d->materialDiffuseParam->value().value<QVector4D>();
                    c.setW(opacity);
                    d->materialDiffuseParam->setValue(c);
                }
            } else if (sm.resolvedMaterial->type() == Q3DSGraphObject::CustomMaterial) {
                // ### custom material
            }
        }
    }
}

// when entering a slide, or when animating a property
void Q3DSSceneBuilder::handlePropertyChange(Q3DSGraphObject *obj, const QSet<QString> &keys, int changeFlags)
{
    Q_UNUSED(keys);
    if (!obj->attached()) // Qt3D stuff not yet built for this object -> nothing to do
        return;

    const Q3DSPropertyChangeList::Flags cf = Q3DSPropertyChangeList::Flags(changeFlags);

    switch (obj->type()) {
    case Q3DSGraphObject::Layer:
    {
        Q3DSLayerNode *layer3DS = static_cast<Q3DSLayerNode *>(obj);
        Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
        updateSizesForLayer(layer3DS, data->parentSize);
        setLayerCameraSizeProperties(layer3DS);
        setLayerSizeProperties(layer3DS);
        setLayerProperties(layer3DS);
        if (cf.testFlag(Q3DSPropertyChangeList::AoOrShadowChanges))
            updateSsaoStatus(layer3DS); // ### futile when aoStrength was 0 before since materials won't get upgraded to SSAO-enabled ones...
    }
        break;
    case Q3DSGraphObject::Camera:
    {
        Q3DSCameraNode *cam3DS = static_cast<Q3DSCameraNode *>(obj);
        Q3DSCameraAttached *data = static_cast<Q3DSCameraAttached *>(cam3DS->attached());
        if (cf.testFlag(Q3DSPropertyChangeList::EyeballChanges)) {
            cam3DS = chooseLayerCamera(data->layer3DS, &data->camera);
            Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(data->layer3DS->attached());
            layerData->cam3DS = cam3DS;
            layerData->cameraSelector->setCamera(data->camera);
            reparentCamera(data->layer3DS);
        }
        setCameraProperties(cam3DS, changeFlags); // handles both Node- and Camera-level properties
        setLayerCameraSizeProperties(data->layer3DS);
        // still have to keep data->globalTransform and globalVisibility up-to-date
        data->dirty |= Q3DSGraphObjectAttached::CameraDirty;
        data->changeFlags |= cf;
    }
        break;

    // Objects with fully deferred processing

    case Q3DSGraphObject::DefaultMaterial:
    {
        Q3DSDefaultMaterial *mat3DS = static_cast<Q3DSDefaultMaterial *>(obj);
        Q3DSDefaultMaterialAttached *data = static_cast<Q3DSDefaultMaterialAttached *>(mat3DS->attached());
        data->dirty |= Q3DSGraphObjectAttached::DefaultMaterialDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Image:
    {
        Q3DSImage *image3DS = static_cast<Q3DSImage *>(obj);
        Q3DSImageAttached *data = static_cast<Q3DSImageAttached *>(image3DS->attached());
        data->dirty |= Q3DSGraphObjectAttached::ImageDirty;
        data->changeFlags |= cf;
    }
        break;

    // Nodes with fully deferred processing

    case Q3DSGraphObject::Group:
    {
        Q3DSGroupNode *group3DS = static_cast<Q3DSGroupNode *>(obj);
        Q3DSGroupAttached *data = static_cast<Q3DSGroupAttached *>(group3DS->attached());
        data->dirty |= Q3DSGraphObjectAttached::GroupDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Component:
    {
        Q3DSComponentNode *comp3DS = static_cast<Q3DSComponentNode *>(obj);
        Q3DSComponentAttached *data = static_cast<Q3DSComponentAttached *>(comp3DS->attached());
        data->dirty |= Q3DSGraphObjectAttached::ComponentDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Light:
    {
        Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(obj);
        Q3DSLightAttached *data = static_cast<Q3DSLightAttached *>(light3DS->attached());
        data->dirty |= Q3DSGraphObjectAttached::LightDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Model:
    {
        Q3DSModelNode *model3DS = static_cast<Q3DSModelNode *>(obj);
        Q3DSModelAttached *data = static_cast<Q3DSModelAttached *>(model3DS->attached());
        data->dirty |= Q3DSGraphObjectAttached::ModelDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Text:
    {
        Q3DSTextNode *text3DS = static_cast<Q3DSTextNode *>(obj);
        Q3DSTextAttached *data = static_cast<Q3DSTextAttached *>(text3DS->attached());
        data->dirty |= Q3DSGraphObjectAttached::TextDirty;
        data->changeFlags |= cf;
    }
        break;

    default:
        break;
    }

    // Note the lack of call to updateNode(). That happens in a QFrameAction once per frame.
}

void Q3DSSceneBuilder::updateSubTree(Q3DSGraphObject *obj)
{
    m_layersWithDirtyLights.clear();

    updateSubTreeRecursive(obj);

    for (Q3DSLayerNode *layer3DS : m_layersWithDirtyLights) {
        Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
        // Attempt to update all buffers, if some do not exist (null) that's fine too.
        updateLightsBuffer(layerData->allLights, layerData->allLightsConstantBuffer);
        updateLightsBuffer(layerData->nonAreaLights, layerData->nonAreaLightsConstantBuffer);
        updateLightsBuffer(layerData->areaLights, layerData->areaLightsConstantBuffer);
        updateShadowMapStatus(layer3DS); // ### futile when there were no casters before since materials won't get upgraded to SSM-enabled ones...
    }

    if (!m_pendingNodeHide.isEmpty()) {
        for (Q3DSNode *node : m_pendingNodeHide)
            setNodeVisibility(node, false);
        m_pendingNodeHide.clear();
    }
    if (!m_pendingNodeShow.isEmpty()) {
        for (Q3DSNode *node : m_pendingNodeShow)
            setNodeVisibility(node, true);
        m_pendingNodeShow.clear();
    }
}

void Q3DSSceneBuilder::prepareNextFrame()
{
    updateSubTree(m_scene);
}

void Q3DSSceneBuilder::updateSubTreeRecursive(Q3DSGraphObject *obj)
{
    if (obj->isNode()) {
        Q3DSNode *node = static_cast<Q3DSNode *>(obj);
        switch (node->type()) {
        case Q3DSGraphObject::Group:
            Q_FALLTHROUGH();
        case Q3DSGraphObject::Component:
        {
            // Group and Component inherit all interesting properties from Node
            Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(node->attached());
            if (data)
                updateNodeFromChangeFlags(node, data->transform, data->changeFlags);
        }
            break;
        case Q3DSGraphObject::Text:
        {
            Q3DSTextNode *text3DS = static_cast<Q3DSTextNode *>(node);
            Q3DSTextAttached *data = static_cast<Q3DSTextAttached *>(text3DS->attached());
            if (data) {
                updateNodeFromChangeFlags(text3DS, data->transform, data->changeFlags);
                if (data->dirty & (Q3DSGraphObjectAttached::TextDirty | Q3DSGraphObjectAttached::GlobalOpacityDirty)) {
                    const bool needsNewImage = data->changeFlags.testFlag(Q3DSPropertyChangeList::TextTextureImageDepChanges);
                    updateText(static_cast<Q3DSTextNode *>(node), needsNewImage);
                }
            }
        }
            break;
        case Q3DSGraphObject::Light:
        {
            Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(node);
            Q3DSLightAttached *data = static_cast<Q3DSLightAttached *>(light3DS->attached());
            if (data) {
                updateNodeFromChangeFlags(light3DS, data->transform, data->changeFlags);
                if (data->dirty & (Q3DSGraphObjectAttached::LightDirty | Q3DSGraphObjectAttached::GlobalTransformDirty)) {
                    setLightProperties(static_cast<Q3DSLightNode *>(node));
                    if (!data->changeFlags.testFlag(Q3DSPropertyChangeList::EyeballChanges)) // already done if eyeball changed
                        m_layersWithDirtyLights.insert(data->layer3DS);
                }
            }
        }
            break;
        case Q3DSGraphObject::Model:
        {
            Q3DSModelNode *model3DS = static_cast<Q3DSModelNode *>(node);
            Q3DSModelAttached *data = static_cast<Q3DSModelAttached *>(model3DS->attached());
            if (data) {
                updateNodeFromChangeFlags(model3DS, data->transform, data->changeFlags);
                if (data->dirty & (Q3DSGraphObjectAttached::ModelDirty | Q3DSGraphObjectAttached::GlobalOpacityDirty))
                    updateModel(static_cast<Q3DSModelNode *>(node));
            }
        }
            break;

        case Q3DSGraphObject::Camera:
        {
            Q3DSCameraNode *cam3DS = static_cast<Q3DSCameraNode *>(node);
            Q3DSCameraAttached *data = static_cast<Q3DSCameraAttached *>(cam3DS->attached());
            if (data)
                updateNodeFromChangeFlags(cam3DS, data->transform, data->changeFlags);
        }
            break;

        default:
            break;
        }
    } else {
        switch (obj->type()) {
        case Q3DSGraphObject::DefaultMaterial:
        {
            Q3DSDefaultMaterial *mat3DS = static_cast<Q3DSDefaultMaterial *>(obj);
            Q3DSDefaultMaterialAttached *data = static_cast<Q3DSDefaultMaterialAttached *>(mat3DS->attached());
            if (data && (data->dirty & Q3DSGraphObjectAttached::DefaultMaterialDirty)) {
                Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(data->model3DS->attached());
                data->opacity = modelData->globalOpacity * mat3DS->opacity();
                updateDefaultMaterial(mat3DS);
            }
        }
            break;
        case Q3DSGraphObject::Image:
        {
            Q3DSImage *image3DS = static_cast<Q3DSImage *>(obj);
            Q3DSImageAttached *data = static_cast<Q3DSImageAttached *>(image3DS->attached());
            if (data && (data->dirty & Q3DSGraphObjectAttached::ImageDirty)) {
                image3DS->calculateTextureTransform();
                for (Q3DSDefaultMaterial *m : data->referencingMaterials)
                    updateDefaultMaterial(m);
            }
        }
            break;

        default:
            break;
        }
    }

    Q3DSGraphObjectAttached *data = obj->attached();
    if (data) {
        data->dirty = 0;
        data->changeFlags = 0;
    }

    obj = obj->firstChild();
    while (obj) {
        updateSubTreeRecursive(static_cast<Q3DSNode *>(obj));
        obj = obj->nextSibling();
    }
}

void Q3DSSceneBuilder::updateNodeFromChangeFlags(Q3DSNode *node, Qt3DCore::QTransform *transform, int changeFlags)
{
    const Q3DSPropertyChangeList::Flags cf = Q3DSPropertyChangeList::Flags(changeFlags);
    if (cf.testFlag(Q3DSPropertyChangeList::NodeTransformChanges)
            || cf.testFlag(Q3DSPropertyChangeList::NodeOpacityChanges))
    {
        setNodeProperties(node, nullptr, transform, NodePropUpdateGlobalsRecursively);
    }

    if (cf.testFlag(Q3DSPropertyChangeList::EyeballChanges)) {
        if (node->type() == Q3DSGraphObject::Light) {
            Q3DSLightAttached *lightData = static_cast<Q3DSLightAttached *>(node->attached());
            Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(lightData->layer3DS->attached());
            layerData->allLights.clear();
            layerData->nonAreaLights.clear();
            layerData->areaLights.clear();
            gatherLights(lightData->layer3DS, &layerData->allLights, &layerData->nonAreaLights, &layerData->areaLights, &layerData->lightNodes);
            m_layersWithDirtyLights.insert(lightData->layer3DS);
        } else if (node->type() != Q3DSGraphObject::Camera) {
            // Drop whatever is queued since that was based on now-invalid
            // input. (important when entering slides, where eyball property
            // changes get processed after an initial visit of all objects)
            m_pendingNodeShow.remove(node);
            m_pendingNodeHide.remove(node);

            const bool active = node->flags().testFlag(Q3DSNode::Active);
            setNodeVisibility(node, active);
        }
    }
}

void Q3DSSceneBuilder::setNodeVisibility(Q3DSNode *node, bool visible)
{
    Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(node->attached());
    Q_ASSERT(data);
    if (!data->entity)
        return;

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(data->layer3DS->attached());
    if (!visible) {
        data->entity->removeComponent(layerData->opaqueTag);
        data->entity->removeComponent(layerData->transparentTag);
    } else {
        if (node->type() != Q3DSGraphObject::Text)
            data->entity->addComponent(layerData->opaqueTag);
        data->entity->addComponent(layerData->transparentTag);
    }

    // For models the model root entity is tagged with both opaque and
    // transparent. The sub-mesh entities however should have the "real" tag
    // based on the material properties.
    if (node->type() == Q3DSGraphObject::Model) {
        Q3DSModelAttached *mdata = static_cast<Q3DSModelAttached *>(node->attached());
        for (Q3DSModelAttached::SubMesh &sm : mdata->subMeshes) {
            Qt3DRender::QLayer *tag = sm.hasTransparency ? layerData->transparentTag : layerData->opaqueTag;
            if (!visible)
                sm.entity->removeComponent(tag);
            else
                sm.entity->addComponent(tag);
        }
    }

    updateGlobals(node, UpdateGlobalsRecursively | UpdateGlobalsSkipTransform);
}

// There are different kind of visibilities: the global (inhertied) visibility
// is based on the Active flag (eyeball property) whereas the visibility
// defined by the object belonging to the current (or master) slide comes on
// top. The two get combined here.
bool Q3DSSceneBuilder::scheduleNodeVisibilityUpdate(Q3DSGraphObject *obj, Q3DSComponentNode *component)
{
    if (obj->isNode() && obj->type() != Q3DSGraphObject::Camera) {
        Q3DSNode *node = static_cast<Q3DSNode *>(obj);
        Q3DSNodeAttached *ndata = static_cast<Q3DSNodeAttached *>(node->attached());
        if (ndata) {
            bool visible = ndata->globalVisibility;
            Q3DSSlide *master = component ? component->masterSlide () : m_masterSlide;
            Q3DSSlide *currentSlide = component ? component->currentSlide() : m_currentSlide;
            if (!master->objects()->contains(node) && !currentSlide->objects()->contains(node))
                visible = false;
            if (component && !m_masterSlide->objects()->contains(component) && !m_currentSlide->objects()->contains(component))
                visible = false;
            QSet<Q3DSNode *> *targetSet = visible ? &m_pendingNodeShow : &m_pendingNodeHide;
            targetSet->insert(node);
            return visible;
        }
    }
    return false;
}

void Q3DSSceneBuilder::updateAnimations(Q3DSSlide *animSourceSlide, Q3DSSlide *playModeSourceSlide)
{
    // Called when entering a slide. Go through the slide's animations and add
    // Animator components for the affected entities (after removing existing ones).
    m_animBuilder->updateAnimations(animSourceSlide, playModeSourceSlide);
}

void Q3DSSceneBuilder::setAnimationsRunning(Q3DSSlide *slide, bool running)
{
    Q3DSSlideAttached *data = static_cast<Q3DSSlideAttached *>(slide->attached());
    for (Qt3DAnimation::QClipAnimator *animator : data->animators)
        animator->setRunning(running);
}

void Q3DSFrameUpdater::frameAction(float dt)
{
    static qint64 frameCounter = 0;
    static const bool animDebug = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 3;
    if (Q_UNLIKELY(animDebug)) {
        qDebug().nospace() << "frame action " << frameCounter << ", delta=" << dt << ", applying animations and updating nodes";
        ++frameCounter;
    }
    // Set and notify the value changes queued by animations.
    m_sceneBuilder->animationBuilder()->applyChanges();
    // Recursively check dirty flags and update inherited values, execute
    // pending visibility changes, update light cbuffers, etc.
    m_sceneBuilder->prepareNextFrame();
}

QT_END_NAMESPACE
