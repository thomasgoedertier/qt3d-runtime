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

#include "q3dsscenemanager.h"
#include "q3dsdefaultmaterialgenerator.h"
#include "q3dscustommaterialgenerator.h"
#include "q3dstextmaterialgenerator.h"
#include "q3dsanimationmanager.h"
#include "q3dstextrenderer.h"
#include "q3dsutils.h"
#include "q3dsprofiler_p.h"
#include "shadergenerator/q3dsshadermanager_p.h"
#include "profileui/q3dsprofileui_p.h"

#include <QDir>
#include <QLoggingCategory>
#include <QStack>
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
#include <Qt3DRender/QBlitFramebuffer>
#include <Qt3DRender/QStencilTest>
#include <Qt3DRender/QStencilTestArguments>
#include <Qt3DRender/QStencilMask>
#include <Qt3DRender/QStencilOperation>
#include <Qt3DRender/QStencilOperationArguments>

#include <Qt3DAnimation/QClipAnimator>

#include <Qt3DExtras/QPlaneMesh>

#include <Qt3DLogic/QFrameAction>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcScene, "q3ds.scene")

/*
    Approx. scene structure:

    // m_rootEntity when params.window
    Entity {
        components: [
            RenderSettings {
                activeFrameGraph: RenderSurfaceSelector { // frameGraphRoot when params.window

                    * Normally there would be a LayerFilter here with DiscardAnyMatchingLayers for m_guiData.guiTag
                    * covering everything up to the profiling gui subtree. However, we only have LayerFilter/NoDraw/DispatchCompute
                    * leaves that exclude gui entities by nature.

                    FrameGraphNode { // subPresFrameGraphRoot
                        NoDraw { }
                        // [rest may be missing when there are no subpresentations]
                        // Subpresentation #1 framegraph, same structure as below (1..N layer + compositor).
                        RenderTargetSelector { // frameGraphRoot when !params.window
                            // Due to only having LayerFilter (or NoDraw) leaves with (3DS-)layer or SceneManager specific
                            // QLayer tags, selection of the right entities for a subpresentation works automatically.
                        }
                        // Subpresentation #2 framegraph
                        RenderTargetSelector { ... }
                        ...
                    }

                    // Layer #1 framegraph
                    FrameGraphNode { // layer subtree root, children may get added/deleted dynamically due to progressive AA for example
                        // main layer framegraph, always present
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
                                    FrameGraphNode { // per-light sub-tree root
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

                                // 6. Post-processing effect passes
                                FrameGraphNode {
                                    NoDraw { } [so that the rest below can be added/deleted dynamically]
                                    // effect 1 pass 1
                                    RenderTargetSelector {
                                        // input is either layerTexture or the output of another pass
                                        // output is either effLayerTexture or one of the intermediate textures
                                        [BlitFramebuffer - NoDraw] ... // from BufferBlit commands or to resolve samples when layer is MSAA
                                        LayerFilter {
                                            layers: [ layer1_eff1_pass1_quad_tag ]
                                            // said quad has a single renderpass with the desired material and render states
                                        }
                                    }
                                    // effect 1 pass 2, ..., effect 2 pass 1, etc.
                                    ...
                                }
                            }
                        } } }

                        // Optional progressive AA pass, depends on the texture from the main layer pass
                        RenderTargetSelector {
                            ... // draw a quad using layer texture + accumulator texture with the prog AA blend shader
                        }
                    }

                    // Layer #2 framegraph
                    FrameGraphNode {
                        // main
                        RenderTargetSelector {
                            ... // like above in layer #1
                        }
                        // prog. AA
                        [RenderTargetSelector { ... } ]
                    }

                    ...

                    // compositor framegraph
                    LayerFilter {
                        Layer { id: compositorTag }
                        layers: [ compositorTag ]
                        ...
                    }

                    // Profiling gui framegraph (once only, not included again for subpresentations)
                    TechniqueFilter { CameraSelector { SortPolicy { LayerFilter { layers: [activeGuiTag] } } } }
                }
            },
            ... // InputSettings etc. these are not handled by SceneManager
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

        Entity { // m_rootEntity for a SceneManager with !params.window
           ... // subpresentation compositor and fs quad entities like above
        }
        ...

        // profiling gui entities
        Entity { ... } // tagged with guiTag
    }

    Entities for one (3DS) layer live under a per-layer root entity parented to
    the RenderTargetSelector corresponding to that layer.

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

Q3DSSceneManager::Q3DSSceneManager(const Q3DSGraphicsLimits &limits)
    : m_gfxLimits(limits),
      m_matGen(new Q3DSDefaultMaterialGenerator),
      m_customMaterialGen(new Q3DSCustomMaterialGenerator),
      m_textMatGen(new Q3DSTextMaterialGenerator),
      m_animationManager(new Q3DSAnimationManager),
      m_textRenderer(new Q3DSTextRenderer),
      m_profiler(new Q3DSProfiler(limits))
{
    const QString fontDir = Q3DSUtils::resourcePrefix() + QLatin1String("res/Font");
    m_textRenderer->registerFonts({ fontDir });

    qRegisterMetaType<Qt3DRender::QRenderTarget *>("Qt3DRender::QRenderTarget*"); // wtf??
}

Q3DSSceneManager::~Q3DSSceneManager()
{
    delete m_profileUi;
    delete m_textRenderer;
    delete m_animationManager;
    delete m_textMatGen;
    delete m_matGen;
    delete m_frameUpdater;
    delete m_profiler;
}

bool operator==(const Q3DSLayerAttached::SizeManagedTexture &a, const Q3DSLayerAttached::SizeManagedTexture &b)
{
    return a.texture == b.texture;
}

bool operator!=(const Q3DSLayerAttached::SizeManagedTexture &a, const Q3DSLayerAttached::SizeManagedTexture &b)
{
    return a.texture != b.texture;
}

void Q3DSSceneManager::updateSizes(const QSize &size, qreal dpr)
{
    if (!m_scene)
        return;

    qCDebug(lcScene) << "Size" << size << "DPR" << dpr;

    m_outputPixelSize = size * dpr;
    m_guiData.outputSize = size;
    m_guiData.outputDpr = dpr;

    if (m_guiData.camera) {
        m_guiData.camera->setRight(m_outputPixelSize.width());
        m_guiData.camera->setBottom(m_outputPixelSize.height());
    }

    for (auto callback : m_compositorOutputSizeChangeCallbacks)
        callback();

    Q3DSPresentation::forAllLayers(m_scene, [=](Q3DSLayerNode *layer3DS) {
        Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
        if (data) {
            // do it right away if there was no size set yet
            if (data->parentSize.isEmpty()) {
                updateSizesForLayer(layer3DS, m_outputPixelSize);
            } else {
                // Defer otherwise, like it is done for other property changes.
                // This is not merely an optimization, it is required to defer
                // everything that affects the logic for - for example -
                // progressive AA to prepareNextFrame().
                data->parentSize = m_outputPixelSize;
                data->dirty |= Q3DSGraphObjectAttached::LayerDirty;
            }
        }
    });
}

void Q3DSSceneManager::setCurrentSlide(Q3DSSlide *newSlide)
{
    if (m_currentSlide == newSlide)
        return;

    qCDebug(lcScene, "Setting new current slide %s", newSlide->id().constData());
    auto prevSlide = m_currentSlide;
    m_currentSlide = newSlide;

    handleSlideChange(prevSlide, m_currentSlide, m_masterSlide);
    updateSubTree(m_scene);
}

void Q3DSSceneManager::setComponentCurrentSlide(Q3DSComponentNode *component, Q3DSSlide *newSlide)
{
    if (!component || component->currentSlide() == newSlide)
        return;

    qCDebug(lcScene, "Setting new current slide %s for component %s", newSlide->id().constData(), component->id().constData());
    auto prevSlide = component->currentSlide();
    component->setCurrentSlide(newSlide);

    handleSlideChange(prevSlide, component->currentSlide(), component->masterSlide(), component);
    updateSubTree(m_scene);
}

QDebug operator<<(QDebug dbg, const Q3DSSceneManager::SceneBuilderParams &p)
{
    QDebugStateSaver saver(dbg);
    dbg << "SceneBuilderParams(" << p.flags << p.outputSize << p.outputDpr << p.window << ")";
    return dbg;
}

void Q3DSSceneManager::prepareEngineReset()
{
    qCDebug(lcScene, "prepareEngineReset on scenemanager %p", this);

    delete m_frameUpdater;
    m_frameUpdater = nullptr;

    m_animationManager->clearPendingChanges();

    if (m_profileUi)
        m_profileUi->releaseResources();
}

void Q3DSSceneManager::prepareEngineResetGlobal()
{
    qCDebug(lcScene, "prepareEngineResetGlobal");

    Q3DSShaderManager::instance().invalidate();
}

/*!
    Builds and "runs" a Qt 3D scene. To be called once per SceneManager instance.

    Ownership of the generated Qt 3D objects is managed primarily via parenting
    to somewhere under the returned rootEntity. SceneManager and the Attached
    objects in the 3DS scenegraph have thus no real ownership of the Qt 3D
    scene but they keep plenty of references to the Qt 3D objects. Scenes (as
    in Qt 3D (sub)scenes built from 3DS presentations) cannot be destroyed
    individually: it is only the entire Qt 3D "scene" (i.e. aspect engine) that
    can go away, including the (sub)scenes for all 3DS presentations, e.g. upon
    (re)opening the same or another uip/uia in a viewer. This gives a certain
    degree of freedom when parenting Qt 3D objects since reuse in another
    presentation's corresponding subtree (e.g. typical for cached shader
    programs) is allowed without any special considerations.

    When params.window is null, params.frameGraphRoot must be valid (e.g. a
    RenderTargetSelector). Here out.frameGraphRoot is the same as
    params.frameGraphRoot, and it is up to the caller to ensure out.rootEntity
    gets parented somewhere (typically to out.rootEntity from a previous buildScene call).
  */
Q3DSSceneManager::Scene Q3DSSceneManager::buildScene(Q3DSPresentation *presentation, const SceneBuilderParams &params)
{
    if (!presentation->scene()) {
        qWarning("Q3DSSceneBuilder: No scene?");
        return Scene();
    }

    qCDebug(lcScene) << "Building scene for" << presentation->sourceFile() << params; // NB params.outputSize==(0,0) is acceptable

    const QString projectFontDir = QFileInfo(presentation->sourceFile()).canonicalPath() + QLatin1Char('/') + QLatin1String("fonts");
    if (QDir(projectFontDir).exists())
        m_textRenderer->registerFonts({ projectFontDir });

    m_flags = params.flags;

    m_presentation = presentation;
    m_presentationSize = QSize(m_presentation->presentationWidth(), m_presentation->presentationHeight());
    m_scene = m_presentation->scene();
    m_masterSlide = m_presentation->masterSlide();
    m_currentSlide = nullptr;
    m_pendingNodeShow.clear();
    m_pendingNodeHide.clear();
    m_subPresLayers.clear();
    m_subPresImages.clear();
    m_subPresentations.clear();
    m_profiler->resetForNewScene(this);

    m_profiler->setEnabled(m_flags.testFlag(EnableProfiling));

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

    Qt3DRender::QRenderSettings *frameGraphComponent;
    Qt3DRender::QFrameGraphNode *frameGraphRoot;
    Qt3DRender::QFrameGraphNode *subPresFrameGraphRoot;
    if (params.window) {
        Q_ASSERT(!m_flags.testFlag(SubPresentation));
        frameGraphComponent = new Qt3DRender::QRenderSettings(m_rootEntity);
        frameGraphRoot = new Qt3DRender::QRenderSurfaceSelector;
        // a node under which subpresentation framegraphs can be added
        subPresFrameGraphRoot = new Qt3DRender::QFrameGraphNode(frameGraphRoot);
        // but do nothing there when there are no subpresentations
        new Qt3DRender::QNoDraw(subPresFrameGraphRoot);
    } else {
        Q_ASSERT(m_flags.testFlag(SubPresentation));
        frameGraphComponent = nullptr;
        frameGraphRoot = params.frameGraphRoot;
        subPresFrameGraphRoot = nullptr;
        Q_ASSERT(frameGraphRoot);
    }

    m_outputPixelSize = params.outputSize * params.outputDpr;
    m_guiData.outputSize = params.outputSize;
    m_guiData.outputDpr = params.outputDpr;

    // Parent it to anything (but not a QEntity of course since this is a
    // component). Cannot leave globally used components unparented since that
    // would mean they get parented to the first node they get added to - and
    // that might be something that gets destroyed over time, e.g. in a
    // framegraph subtree that gets removed or replaced at some point.
    m_fsQuadTag = new Qt3DRender::QLayer(frameGraphRoot);

    // Prepare image objects (these are non-nodes and not covered in layer building below).
    m_presentation->forAllImages([this](Q3DSImage *image) {
        Q3DSImageAttached *data = new Q3DSImageAttached;
        data->entity = m_rootEntity; // must set an entity to to make Q3DSImage properties animatable, just use the root
        image->setAttached(data);
        image->addPropertyChangeObserver(std::bind(&Q3DSSceneManager::handlePropertyChange, this,
                                                   std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    });

    // Build the (offscreen) Qt3D scene
    Q3DSPresentation::forAllLayers(m_scene, [=](Q3DSLayerNode *layer3DS) {
        if (layer3DS->sourcePath().isEmpty())
            buildLayer(layer3DS, frameGraphRoot, m_outputPixelSize);
        else
            buildSubPresentationLayer(layer3DS, m_outputPixelSize);
    });

    // Onscreen (or not) compositor (still offscreen when this is a subpresentation)
    buildCompositor(frameGraphRoot, m_rootEntity);

    // Profiling UI
    if (!m_flags.testFlag(SubPresentation)) {
        buildGuiPass(frameGraphRoot, m_rootEntity);
        m_profileUi = new Q3DSProfileUi(&m_guiData, m_profiler);
    }

    // Fullscreen quad for bluring the shadow map/cubemap
    Q3DSShaderManager &sm(Q3DSShaderManager::instance());
    QStringList fsQuadPassNames { QLatin1String("shadowOrthoBlurX"), QLatin1String("shadowOrthoBlurY") };
    QVector<Qt3DRender::QShaderProgram *> fsQuadPassProgs { sm.getOrthoShadowBlurXShader(m_rootEntity), sm.getOrthoShadowBlurYShader(m_rootEntity) };
    if (m_gfxLimits.maxDrawBuffers >= 6) { // ###
        fsQuadPassNames << QLatin1String("shadowCubeBlurX") << QLatin1String("shadowCubeBlurY");
        fsQuadPassProgs << sm.getCubeShadowBlurXShader(m_rootEntity, m_gfxLimits) << sm.getCubeShadowBlurYShader(m_rootEntity, m_gfxLimits);
    }
    fsQuadPassNames << QLatin1String("ssao") << QLatin1String("progaa");
    fsQuadPassProgs << sm.getSsaoTextureShader(m_rootEntity) << sm.getProgAABlendShader(m_rootEntity);
    FsQuadParams quadInfo;
    quadInfo.parentEntity = m_rootEntity;
    quadInfo.passNames = fsQuadPassNames;
    quadInfo.passProgs = fsQuadPassProgs;
    quadInfo.tag = m_fsQuadTag;
    buildFsQuad(quadInfo);

    Scene sc;
    sc.rootEntity = m_rootEntity;
    sc.frameGraphRoot = frameGraphRoot;
    sc.subPresFrameGraphRoot = subPresFrameGraphRoot;
    sc.frameAction = nodeUpdater;

    if (params.window) {
        // Ready to go (except that the sizes calculated from params.outputSize are
        // likely bogus when it is derived from QWindow::size() during app startup;
        // will get updated in updateSizes()).
        static_cast<Qt3DRender::QRenderSurfaceSelector *>(frameGraphRoot)->setSurface(params.window);
        frameGraphComponent->setActiveFrameGraph(frameGraphRoot);
        m_rootEntity->addComponent(frameGraphComponent);
        sc.renderSettings = frameGraphComponent;
    }

    // Set visibility of objects in the scene and start animations.
    Q3DSPresentation::forAllObjectsOfType(m_masterSlide, Q3DSGraphObject::Slide,
                                          [this](Q3DSGraphObject *s) {
        updateSlideObjectVisibilities(static_cast<Q3DSSlide *>(s));
    });
    updateAnimations(m_masterSlide, nullptr, m_currentSlide);
    updateAnimations(m_currentSlide, nullptr, m_currentSlide);

    return sc;
}

/*!
    To be called on the scenemanager corresponding to the main presentation
    once after all subpresentation buildScene() calls have succeeded. This is
    where the association of textures and subpresentation layers happens. That
    cannot be done in the first buildScene since the textures for
    subpresentations (and the corresponding framegraph subtrees) are not yet
    generated at that stage.
 */
void Q3DSSceneManager::finalizeMainScene(const QVector<Q3DSSubPresentation> &subPresentations)
{
    m_subPresentations = subPresentations;

    for (Q3DSLayerNode *layer3DS : m_subPresLayers) {
        const QString subPresId = layer3DS->sourcePath();
        Q_ASSERT(!subPresId.isEmpty());
        auto it = std::find_if(subPresentations.cbegin(), subPresentations.cend(),
                               [subPresId](const Q3DSSubPresentation &sp) { return sp.id == subPresId; });
        if (it != subPresentations.cend()) {
            qCDebug(lcScene, "Directing subpresentation %s to layer %s", qPrintable(it->id), layer3DS->id().constData());
            Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
            layerData->layerTexture = it->colorTex;
            layerData->compositorSourceParam->setValue(QVariant::fromValue(layerData->layerTexture));
            layerData->updateSubPresentationSize();
        } else {
            qCDebug(lcScene, "Subpresentation %s for layer %s not found",
                    qPrintable(subPresId), layer3DS->id().constData());
        }
    }

    for (auto p : m_subPresImages)
        setImageTextureFromSubPresentation(p.first, p.second);
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

static Qt3DRender::QAbstractTexture *newColorBuffer(const QSize &layerPixelSize, int msaaSampleCount)
{
    Qt3DRender::QAbstractTexture *colorTex;
    if (msaaSampleCount > 1) {
        colorTex = new Qt3DRender::QTexture2DMultisample;
        colorTex->setSamples(msaaSampleCount);
    } else {
        colorTex = new Qt3DRender::QTexture2D;
    }
    colorTex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
    colorTex->setWidth(layerPixelSize.width());
    colorTex->setHeight(layerPixelSize.height());
    colorTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    colorTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    return colorTex;
}

static Qt3DRender::QAbstractTexture *newDepthStencilBuffer(const QSize &layerPixelSize, int msaaSampleCount)
{
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

    Qt3DRender::QAbstractTexture *dsTexOrRb;
    if (msaaSampleCount > 1) {
        dsTexOrRb = new Qt3DRender::QTexture2DMultisample;
        dsTexOrRb->setSamples(msaaSampleCount);
    } else {
        dsTexOrRb = new Qt3DRender::QTexture2D;
    }
    dsTexOrRb->setFormat(Qt3DRender::QAbstractTexture::D24S8);
    dsTexOrRb->setWidth(layerPixelSize.width());
    dsTexOrRb->setHeight(layerPixelSize.height());
    dsTexOrRb->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    dsTexOrRb->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    return dsTexOrRb;
}

Qt3DRender::QRenderTarget *Q3DSSceneManager::newLayerRenderTarget(const QSize &layerPixelSize, int msaaSampleCount,
                                                                  Qt3DRender::QAbstractTexture **colorTex, Qt3DRender::QAbstractTexture **dsTexOrRb,
                                                                  Qt3DCore::QNode *textureParentNode, Q3DSLayerNode *layer3DS)
{
    Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;

    Qt3DRender::QRenderTargetOutput *color = new Qt3DRender::QRenderTargetOutput;
    color->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
    *colorTex = newColorBuffer(layerPixelSize, msaaSampleCount);
    m_profiler->trackNewObject(*colorTex, Q3DSProfiler::Texture2DObject,
                               "Color buffer for layer %s", layer3DS->id().constData());
    (*colorTex)->setParent(textureParentNode);
    color->setTexture(*colorTex);

    Qt3DRender::QRenderTargetOutput *ds = new Qt3DRender::QRenderTargetOutput;
    ds->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::DepthStencil);
    *dsTexOrRb = newDepthStencilBuffer(layerPixelSize, msaaSampleCount);
    m_profiler->trackNewObject(*dsTexOrRb, Q3DSProfiler::Texture2DObject,
                               "Depth-stencil buffer for layer %s", layer3DS->id().constData());
    (*dsTexOrRb)->setParent(textureParentNode);
    ds->setTexture(*dsTexOrRb);

    rt->addOutput(color);
    rt->addOutput(ds);

    return rt;
}

static QSize safeLayerPixelSize(const QSize &layerSize, int scaleFactor)
{
    const QSize layerPixelSize = layerSize * scaleFactor;
    return QSize(layerPixelSize.width() > 0 ? layerPixelSize.width() : 32,
                 layerPixelSize.height() > 0 ? layerPixelSize.height() : 32);
}

static QSize safeLayerPixelSize(Q3DSLayerAttached *data)
{
    return safeLayerPixelSize(data->layerSize, data->ssaaScaleFactor);
}

void Q3DSSceneManager::buildLayer(Q3DSLayerNode *layer3DS,
                                  Qt3DRender::QFrameGraphNode *parent,
                                  const QSize &parentSize)
{
    Qt3DRender::QFrameGraphNode *layerFgRoot = new Qt3DRender::QFrameGraphNode(parent);

    // main passes, generating the layer texture
    Qt3DRender::QRenderTargetSelector *rtSelector = new Qt3DRender::QRenderTargetSelector(layerFgRoot);

    int ssaaScaleFactor = 1;
    if (layer3DS->multisampleAA() == Q3DSLayerNode::SSAA) {
        ssaaScaleFactor = 2;
        qCDebug(lcScene, "Layer %s uses %dx SSAA", layer3DS->id().constData(), ssaaScaleFactor);
    }

    int msaaSampleCount = 0;
    switch (layer3DS->multisampleAA()) {
    case Q3DSLayerNode::MSAA2x:
        msaaSampleCount = 2;
        break;
    case Q3DSLayerNode::MSAA4x:
        msaaSampleCount = 4;
        break;
    default:
        break;
    }
    if (m_flags.testFlag(Force4xMSAA))
        msaaSampleCount = 4;

    // Layer MSAA is only available through multisample textures (GLES 3.1+ or GL 3.2+) at the moment. (QTBUG-63382)
    // Revert to no-MSAA when this is not supported.
    if (msaaSampleCount > 1 && !m_gfxLimits.multisampleTextureSupported) {
        qCDebug(lcScene, "Layer MSAA requested but not supported; ignoring request");
        msaaSampleCount = 0;
    }

    if (msaaSampleCount > 1)
        qCDebug(lcScene, "Layer %s uses multisample texture", layer3DS->id().constData());

    // parentSize could well be (0, 0) at this stage still, nevermind that
    const QSize layerSize = calculateLayerSize(layer3DS, parentSize);
    const QSize layerPixelSize = safeLayerPixelSize(layerSize, ssaaScaleFactor);

    // Create color and depth-stencil buffers for this layer
    Qt3DRender::QAbstractTexture *colorTex;
    Qt3DRender::QAbstractTexture *dsTexOrRb;
    Qt3DRender::QRenderTarget *rt = newLayerRenderTarget(layerPixelSize, msaaSampleCount, &colorTex, &dsTexOrRb, layerFgRoot, layer3DS);
    m_profiler->trackNewObject(rt, Q3DSProfiler::RenderTargetObject,
                               "RT for layer %s", layer3DS->id().constData());
    rtSelector->setTarget(rt);

    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(rtSelector);
    viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

    Qt3DRender::QCameraSelector *cameraSelector = new Qt3DRender::QCameraSelector(viewport);
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
    const bool depthPrePassEnabled = !layer3DS->layerFlags().testFlag(Q3DSLayerNode::DisableDepthPrePass)
            && !layer3DS->layerFlags().testFlag(Q3DSLayerNode::DisableDepthTest);
    if (depthPrePassEnabled)
        depthPreLayerFilter->addLayer(opaqueTag); // opaque only, transparent objects must not be present

    const bool transparentPassOnly = layer3DS->layerFlags().testFlag(Q3DSLayerNode::DisableDepthTest);

    // Opaque pass
    if (!transparentPassOnly) {
        Qt3DRender::QRenderPassFilter *opaqueFilter = new Qt3DRender::QRenderPassFilter(mainTechniqueSelector);
        Qt3DRender::QFilterKey *opaqueFilterKey = new Qt3DRender::QFilterKey;
        opaqueFilterKey->setName(QLatin1String("pass"));
        opaqueFilterKey->setValue(QLatin1String("opaque"));
        opaqueFilter->addMatch(opaqueFilterKey);
        Qt3DRender::QSortPolicy *opaqueSortPolicy = opaquePassSortPolicy(opaqueFilter);
        Qt3DRender::QLayerFilter *opaqueLayerFilter = new Qt3DRender::QLayerFilter(opaqueSortPolicy);
        opaqueLayerFilter->addLayer(opaqueTag);
    }

    // Transparent pass, sort back to front
    Qt3DRender::QRenderPassFilter *transFilter = new Qt3DRender::QRenderPassFilter(mainTechniqueSelector);
    Qt3DRender::QFilterKey *transFilterKey = new Qt3DRender::QFilterKey;
    transFilterKey->setName(QLatin1String("pass"));
    transFilterKey->setValue(QLatin1String("transparent"));
    transFilter->addMatch(transFilterKey);
    Qt3DRender::QSortPolicy *transSortPolicy = transparentPassSortPolicy(transFilter);
    Qt3DRender::QLayerFilter *transLayerFilter = new Qt3DRender::QLayerFilter(transSortPolicy);
    transLayerFilter->addLayer(transparentTag);
    if (transparentPassOnly)
        transLayerFilter->addLayer(opaqueTag);

    // Post-processing effect passes
    Qt3DRender::QFrameGraphNode *effectRoot = new Qt3DRender::QFrameGraphNode(mainTechniqueSelector);
    new Qt3DRender::QNoDraw(effectRoot);

    Q3DSLayerAttached *layerData = new Q3DSLayerAttached;
    layerData->entity = m_rootEntity; // must set an entity to to make Q3DSLayerNode properties animatable, just use the root
    layerData->layer3DS = layer3DS;
    layerData->layerFgRoot = layerFgRoot;
    layerData->cameraSelector = cameraSelector;
    layerData->clearBuffers = clearBuffers;
    layerData->rtSelector = rtSelector;
    layerData->layerTexture = colorTex;
    layerData->layerDS = dsTexOrRb;
    layerData->compositorSourceParam = new Qt3DRender::QParameter(QLatin1String("tex"), layerData->layerTexture);
    layerData->layerSize = layerSize;
    layerData->parentSize = parentSize;
    layerData->msaaSampleCount = msaaSampleCount;
    layerData->ssaaScaleFactor = ssaaScaleFactor;
    layerData->opaqueTag = opaqueTag;
    layerData->transparentTag = transparentTag;
    layerData->cameraPropertiesParam = new Qt3DRender::QParameter(QLatin1String("camera_properties"), QVector2D(10, 5000), m_rootEntity);
    layerData->depthTextureData.rtSelector = depthRtSelector;
    layerData->ssaoTextureData.rtSelector = ssaoRtSelector;
    layerData->shadowMapData.shadowRoot = shadowRoot;
    layerData->effectData.effectRoot = effectRoot;

    // textures that are resized automatically to match the layer's dimensions
    layerData->sizeManagedTextures << colorTex << dsTexOrRb;

    layer3DS->setAttached(layerData);

    // Now add the scene contents.
    Q3DSGraphObject *obj = layer3DS->firstChild();
    Qt3DCore::QEntity *layerSceneRootEntity = nullptr;
    m_componentNodeStack.clear();
    m_componentNodeStack.push(nullptr);
    while (obj) {
        if (!layerSceneRootEntity) {
            layerSceneRootEntity = new Qt3DCore::QEntity(rtSelector);
            layerSceneRootEntity->setObjectName(QObject::tr("root for %1").arg(QString::fromUtf8(layer3DS->id())));
        }
        buildLayerScene(obj, layer3DS, layerSceneRootEntity);
        obj = obj->nextSibling();
    }

    // Find the active camera for this layer and set it up
    setActiveLayerCamera(findFirstCamera(layer3DS), layer3DS);

    setLayerProperties(layer3DS);

    layer3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneManager::handlePropertyChange, this,
                                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // Phase 2: deferred stuff

    // Gather lights for this layer.
    gatherLights(layer3DS, &layerData->allLights, &layerData->nonAreaLights, &layerData->areaLights, &layerData->lightNodes);
    qCDebug(lcScene, "Layer %s has %d lights in total (%d non-area, %d area)", layer3DS->id().constData(),
            layerData->allLights.count(), layerData->nonAreaLights.count(), layerData->areaLights.count());
    updateShadowMapStatus(layer3DS); // must be done before generating materials below

    // Enable SSAO (and depth texture generation) when needed. Must be done
    // before generating materials below.
    updateSsaoStatus(layer3DS);

    // Generate Qt3D material components.
    Q3DSPresentation::forAllModels(layer3DS->firstChild(),
                                   [this](Q3DSModelNode *model3DS) { buildModelMaterial(model3DS); },
                                   true); // include hidden ones too

    // Set up effects.
    finalizeEffects(layer3DS);
}

Qt3DRender::QTexture2D *Q3DSSceneManager::dummyTexture()
{
    if (!m_dummyTex) {
        m_dummyTex = new Qt3DRender::QTexture2D(m_rootEntity);
        m_dummyTex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
        m_dummyTex->setWidth(64);
        m_dummyTex->setHeight(64);
        m_profiler->trackNewObject(m_dummyTex, Q3DSProfiler::Texture2DObject, "dummy");
    }
    return m_dummyTex;
}

void Q3DSSceneManager::buildSubPresentationLayer(Q3DSLayerNode *layer3DS, const QSize &parentSize)
{
    m_subPresLayers.insert(layer3DS);

    Q3DSLayerAttached *data = new Q3DSLayerAttached;
    data->entity = m_rootEntity; // must set an entity to to make Q3DSLayerNode properties animatable, just use the root
    data->layer3DS = layer3DS;
    data->layerSize = calculateLayerSize(layer3DS, parentSize);
    data->parentSize = parentSize;

    // camera and stuff stays null, no such thing for subpresentation layers

    // leave compositorSourceParam dummy for now, we don't know the actual texture yet
    data->compositorSourceParam = new Qt3DRender::QParameter(QLatin1String("tex"), dummyTexture());

    // subpresentations associated with layers follow the size of the layer
    data->updateSubPresentationSize = [this, layer3DS, data]() {
        const QSize sz = data->layerSize;
        if (sz.isEmpty())
            return;
        const QSize layerPixelSize = safeLayerPixelSize(data);
        auto it = std::find_if(m_subPresentations.cbegin(), m_subPresentations.cend(),
                               [this, layer3DS](const Q3DSSubPresentation &sp) { return sp.id == layer3DS->sourcePath(); });
        if (it != m_subPresentations.cend()) {
            qCDebug(lcScene, "Resizing subpresentation %s for layer %s to %dx%d",
                    qPrintable(layer3DS->sourcePath()), layer3DS->id().constData(), sz.width(), sz.height());
            // Resize the offscreen subpresentation buffers
            it->colorTex->setWidth(layerPixelSize.width());
            it->colorTex->setHeight(layerPixelSize.height());
            if (it->dsTex) {
                it->dsTex->setWidth(layerPixelSize.width());
                it->dsTex->setHeight(layerPixelSize.height());
            }
            // and communicate the new size to the subpresentation's renderer
            // (viewport, camera, etc. need to adapt), just like a window would
            // do to an onscreen presentation's scenemanager upon receiving a
            // resizeEvent().
            if (it->sceneManager)
                it->sceneManager->updateSizes(sz, 1);
        }
    };

    layer3DS->setAttached(data);

    setLayerProperties(layer3DS);
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

static void setMaterialBlending(Qt3DRender::QBlendEquation *blendFunc,
                                Qt3DRender::QBlendEquationArguments *blendArgs,
                                Q3DSDefaultMaterial::BlendMode blendMode)
{
    switch (blendMode) {
    case Q3DSDefaultMaterial::Screen:
        blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
        blendArgs->setSourceRgb(Qt3DRender::QBlendEquationArguments::SourceAlpha);
        blendArgs->setDestinationRgb(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setSourceAlpha(Qt3DRender::QBlendEquationArguments::One);
        blendArgs->setDestinationAlpha(Qt3DRender::QBlendEquationArguments::One);
        break;

    case Q3DSDefaultMaterial::Multiply:
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

QVector<Qt3DRender::QRenderPass *> Q3DSSceneManager::standardRenderPasses(Qt3DRender::QShaderProgram *program,
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
    Qt3DRender::QDepthTest::DepthFunction mainPassDepthFunc = !layer3DS->layerFlags().testFlag(Q3DSLayerNode::DisableDepthTest)
            ? Qt3DRender::QDepthTest::LessOrEqual
            : Qt3DRender::QDepthTest::Always;
    Qt3DRender::QRenderPass *opaquePass = new Qt3DRender::QRenderPass;
    opaquePass->addFilterKey(opaqueFilterKey);
    addDepthTest(opaquePass, mainPassDepthFunc);
    // Depth buffer is already filled when depth prepass is enabled.
    Qt3DRender::QNoDepthMask *opaqueNoDepthWrite = new Qt3DRender::QNoDepthMask;
    const bool depthPrePassEnabled = !layer3DS->layerFlags().testFlag(Q3DSLayerNode::DisableDepthPrePass)
            && !layer3DS->layerFlags().testFlag(Q3DSLayerNode::DisableDepthTest);
    opaqueNoDepthWrite->setEnabled(depthPrePassEnabled);
    opaquePass->addRenderState(opaqueNoDepthWrite);

    // Transparent objects.
    Qt3DRender::QRenderPass *transPass = new Qt3DRender::QRenderPass;
    transPass->addFilterKey(transFilterKey);
    addDepthTest(transPass, mainPassDepthFunc);
    Qt3DRender::QNoDepthMask *transNoDepthWrite = new Qt3DRender::QNoDepthMask;
    transPass->addRenderState(transNoDepthWrite);
    // blending. the shaders produce non-premultiplied results.
    auto blendFunc = new Qt3DRender::QBlendEquation;
    auto blendArgs = new Qt3DRender::QBlendEquationArguments;
    setMaterialBlending(blendFunc, blendArgs, blendMode);

    transPass->addRenderState(blendFunc);
    transPass->addRenderState(blendArgs);

    // Make sure the shared shader programs are not parented to the renderpass
    // and thus the material. (the material can be rebuilt later on, cannot let
    // its destroy tear down shared resources)
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(layerData && layerData->entity);

    shadowOrthoPass->setShaderProgram(Q3DSShaderManager::instance().getOrthographicDepthNoTessShader(layerData->entity));
    shadowCubePass->setShaderProgram(Q3DSShaderManager::instance().getCubeDepthNoTessShader(layerData->entity));
    depthPass->setShaderProgram(Q3DSShaderManager::instance().getDepthPrepassShader(layerData->entity, hasDisplacement));
    opaquePass->setShaderProgram(program);
    transPass->setShaderProgram(program);

    return { shadowOrthoPass, shadowCubePass, depthPass, opaquePass, transPass };
}

QVector<Qt3DRender::QTechnique *> Q3DSSceneManager::computeTechniques(Q3DSLayerNode *layer3DS)
{
    Qt3DRender::QTechnique *bsdfPrefilter = new Qt3DRender::QTechnique;

    Qt3DRender::QFilterKey *bsdfPrefilterFilterKey = new Qt3DRender::QFilterKey;
    bsdfPrefilterFilterKey->setName(QLatin1String("type"));
    bsdfPrefilterFilterKey->setValue(QLatin1String("bsdfPrefilter"));
    bsdfPrefilter->addFilterKey(bsdfPrefilterFilterKey);

    Q3DSDefaultMaterialGenerator::addDefaultApiFilter(bsdfPrefilter);

    Qt3DRender::QRenderPass *bsdfComputePass = new Qt3DRender::QRenderPass;
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(layerData && layerData->entity);
    bsdfComputePass->setShaderProgram(Q3DSShaderManager::instance().getBsdfMipPreFilterShader(layerData->entity));
    bsdfPrefilter->addRenderPass(bsdfComputePass);

    return { bsdfPrefilter };
}

void Q3DSSceneManager::markAsMainTechnique(Qt3DRender::QTechnique *technique)
{
    Qt3DRender::QFilterKey *techniqueFilterKey = new Qt3DRender::QFilterKey;
    techniqueFilterKey->setName(QLatin1String("type"));
    techniqueFilterKey->setValue(QLatin1String("main"));
    technique->addFilterKey(techniqueFilterKey);
}

QSize Q3DSSceneManager::calculateLayerSize(Q3DSLayerNode *layer3DS, const QSize &parentSize)
{
    int w = 0;
    switch (layer3DS->horizontalFields()) {
    case Q3DSLayerNode::LeftWidth:
        Q_FALLTHROUGH();
    case Q3DSLayerNode::WidthRight:
    {
        int width = qRound(layer3DS->widthUnits() == Q3DSLayerNode::Percent ? layer3DS->width() * 0.01f * parentSize.width() : layer3DS->width());
        w = width;
    }
        break;
    case Q3DSLayerNode::LeftRight:
    {
        float left = layer3DS->leftUnits() == Q3DSLayerNode::Percent ? layer3DS->left() * 0.01f * parentSize.width() : layer3DS->left();
        float right = layer3DS->rightUnits() == Q3DSLayerNode::Percent ? parentSize.width() - (layer3DS->right() * 0.01f * parentSize.width()) : layer3DS->right();
        w = qRound(right - left) + 1;
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    int h = 0;
    switch (layer3DS->verticalFields()) {
    case Q3DSLayerNode::TopHeight:
        Q_FALLTHROUGH();
    case Q3DSLayerNode::HeightBottom:
    {
        int height = qRound(layer3DS->heightUnits() == Q3DSLayerNode::Percent ? layer3DS->height() * 0.01f * parentSize.height() : layer3DS->height());
        h = height;
    }
        break;
    case Q3DSLayerNode::TopBottom:
    {
        float top = layer3DS->topUnits() == Q3DSLayerNode::Percent ? layer3DS->top() * 0.01f * parentSize.height() : layer3DS->top();
        float bottom = layer3DS->bottomUnits() == Q3DSLayerNode::Percent ? parentSize.height() - (layer3DS->bottom() * 0.01f * parentSize.height()) : layer3DS->bottom();
        h = qRound(bottom - top) + 1;
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    return QSize(qMax(0, w), qMax(0, h));
}

QPointF Q3DSSceneManager::calculateLayerPos(Q3DSLayerNode *layer3DS, const QSize &parentSize)
{
    float x = 0;
    switch (layer3DS->horizontalFields()) {
    case Q3DSLayerNode::LeftWidth:
        Q_FALLTHROUGH();
    case Q3DSLayerNode::LeftRight:
    {
        float left = layer3DS->leftUnits() == Q3DSLayerNode::Percent ? layer3DS->left() * 0.01f * parentSize.width() : layer3DS->left();
        x = left;
    }
        break;
    case Q3DSLayerNode::WidthRight:
    {
        float right = layer3DS->rightUnits() == Q3DSLayerNode::Percent ? parentSize.width() - (layer3DS->right() * 0.01f * parentSize.width()) : layer3DS->right();
        float width = layer3DS->widthUnits() == Q3DSLayerNode::Percent ? layer3DS->width() * 0.01f * parentSize.width() : layer3DS->width();
        x = right - width + 1;
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    float y = 0;
    switch (layer3DS->verticalFields()) {
    case Q3DSLayerNode::TopHeight:
        Q_FALLTHROUGH();
    case Q3DSLayerNode::TopBottom:
    {
        float top = layer3DS->topUnits() == Q3DSLayerNode::Percent ? layer3DS->top() * 0.01f * parentSize.height() : layer3DS->top();
        y = top;
    }
        break;
    case Q3DSLayerNode::HeightBottom:
    {
        float bottom = layer3DS->bottomUnits() == Q3DSLayerNode::Percent ? parentSize.height() - (layer3DS->bottom() * 0.01f * parentSize.height()) : layer3DS->bottom();
        float height = layer3DS->heightUnits() == Q3DSLayerNode::Percent ? layer3DS->height() * 0.01f * parentSize.height() : layer3DS->height();
        y = bottom - height + 1;
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    return QPointF(x, y);
}

void Q3DSSceneManager::updateSizesForLayer(Q3DSLayerNode *layer3DS, const QSize &newParentSize)
{
    if (newParentSize.isEmpty())
        return;

    // note: no bail out when newParentSize == data->parentSize, the other values could still be out of date

    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(data);

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

void Q3DSSceneManager::setLayerCameraSizeProperties(Q3DSLayerNode *layer3DS)
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

void Q3DSSceneManager::setLayerSizeProperties(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    const QSize layerPixelSize = safeLayerPixelSize(data);
    const QSize layerSize = safeLayerPixelSize(data->layerSize, 1); // for when SSAA is to be ignored
    for (const Q3DSLayerAttached::SizeManagedTexture &t : data->sizeManagedTextures) {
        if (!t.flags.testFlag(Q3DSLayerAttached::SizeManagedTexture::CustomSizeCalculation)) {
            if (!t.flags.testFlag(Q3DSLayerAttached::SizeManagedTexture::IgnoreSSAA)) {
                t.texture->setWidth(layerPixelSize.width());
                t.texture->setHeight(layerPixelSize.height());
            } else {
                t.texture->setWidth(layerSize.width());
                t.texture->setHeight(layerSize.height());
            }
        }
        if (t.sizeChangeCallback)
            t.sizeChangeCallback(layer3DS);
    }
    if (data->updateCompositorCalculations)
        data->updateCompositorCalculations();
    if (data->updateSubPresentationSize)
        data->updateSubPresentationSize();
    for (Q3DSLayerAttached::SizeChangeCallback callback : qAsConst(data->layerSizeChangeCallbacks))
        callback(layer3DS);
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
void Q3DSSceneManager::setLayerProperties(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(data);

    if (data->clearBuffers) // not available for subpresentation layers
        setClearColorForClearBuffers(data->clearBuffers, layer3DS);

    if (data->compositorEntity) // may not exist if this is still buildLayer()
        data->compositorEntity->setEnabled(layer3DS->flags().testFlag(Q3DSNode::Active));
}

Qt3DRender::QCamera *Q3DSSceneManager::buildCamera(Q3DSCameraNode *cam3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Qt3DRender::QCamera *camera = new Qt3DRender::QCamera(parent);
    camera->setObjectName(QObject::tr("camera %1 for %2").arg(QString::fromUtf8(cam3DS->id())).arg(QString::fromUtf8(layer3DS->id())));
    Q3DSCameraAttached *data = new Q3DSCameraAttached;
    data->transform = new Qt3DCore::QTransform;
    data->camera = camera;
    data->layer3DS = layer3DS;
    cam3DS->setAttached(data);
    // make sure data->entity, globalTransform, etc. are usable
    setNodeProperties(cam3DS, camera, data->transform, NodePropUpdateAttached);
    setCameraProperties(cam3DS, Q3DSPropertyChangeList::ALL_CHANGE_FLAGS);
    cam3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneManager::handlePropertyChange, this,
                                                 std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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

void Q3DSSceneManager::setCameraProperties(Q3DSCameraNode *camNode, int changeFlags)
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

// Returns true if the camera actually changed
bool Q3DSSceneManager::setActiveLayerCamera(Q3DSCameraNode *camara3DS, Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    if (layerData->cam3DS != camara3DS) {
        layerData->cam3DS = camara3DS;
        if (camara3DS) {
            Q3DSCameraAttached *activeCameraData = static_cast<Q3DSCameraAttached *>(camara3DS->attached());
            layerData->cameraSelector->setCamera(activeCameraData->camera);
        } else {
            layerData->cameraSelector->setCamera(nullptr);
        }

        if (camara3DS) {
            setCameraProperties(camara3DS, Q3DSPropertyChangeList::ALL_CHANGE_FLAGS);
            layerData->cameraPropertiesParam->setValue(QVector2D(camara3DS->clipNear(), camara3DS->clipFar()));
        }


        // may not have a valid size yet
        if (!layerData->layerSize.isEmpty()) {
            setLayerCameraSizeProperties(layer3DS);
            setLayerSizeProperties(layer3DS);
        }

        layerData->wasDirty = true;

        qCDebug(lcScene, "Layer %s uses camera %s", layer3DS->id().constData(), camara3DS ? camara3DS->id().constData() : "null");
        return true;
    }
    return false;
}

void Q3DSSceneManager::updateLayerCamera(Q3DSLayerNode *layer3DS)
{
    if (setActiveLayerCamera(findFirstCamera(layer3DS), layer3DS)) {
        // Camera has changed -> trigger a property value update
        // for effects since they may rely on the camera clip range.
        auto layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
        for (Q3DSEffectInstance *eff3DS : layerData->effectData.effects)
            updateEffect(eff3DS);
    }
}

// layers use the first Active (eyeball on) camera for rendering
Q3DSCameraNode *Q3DSSceneManager::findFirstCamera(Q3DSLayerNode *layer3DS)
{
    // Pick the first active camera encountered when walking depth-first.
    std::function<Q3DSCameraNode *(Q3DSGraphObject *)> f;
    f = [&f,this](Q3DSGraphObject *obj) -> Q3DSCameraNode* {
        while (obj) {
            if (obj->type() == Q3DSGraphObject::Camera) {
                Q3DSCameraNode *cam = static_cast<Q3DSCameraNode *>(obj);
                // ### should use globalVisibility (which is only set in buildLayerCamera first...)
                const bool active = cam->flags().testFlag(Q3DSNode::Active);;
                if (active) {
                    // Check if camera is on the current slide
                    Q3DSNodeAttached *nodeData = static_cast<Q3DSNodeAttached *>(obj->attached());
                    Q3DSComponentNode *component = nullptr;
                    if (nodeData && nodeData->component)
                        component = nodeData->component;
                    // Check that object exists current slide scope (master + current)
                    Q3DSSlide *master = component ? component->masterSlide () : m_masterSlide;
                    Q3DSSlide *currentSlide = component ? component->currentSlide() : m_currentSlide;
                    if ((master->objects()->contains(cam) || currentSlide->objects()->contains(cam)) && isComponentVisible(component))
                        return cam;
                }
            }
            if (Q3DSCameraNode *c = f(obj->firstChild()))
                return c;
            obj = obj->nextSibling();
        }
        return nullptr;
    };
    return f(layer3DS->firstChild());
}

static void prepareSizeDependentTexture(Qt3DRender::QAbstractTexture *texture,
                                        Q3DSLayerNode *layer3DS,
                                        Q3DSLayerAttached::SizeChangeCallback callback = nullptr,
                                        Q3DSLayerAttached::SizeManagedTexture::Flags flags = 0)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());

    const QSize layerPixelSize = safeLayerPixelSize(data);
    texture->setWidth(layerPixelSize.width());
    texture->setHeight(layerPixelSize.height());

    // the layer will resize the texture automatically
    data->sizeManagedTextures << Q3DSLayerAttached::SizeManagedTexture(texture, callback, flags);
}

void Q3DSSceneManager::setDepthTextureEnabled(Q3DSLayerNode *layer3DS, bool enabled)
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
            m_profiler->trackNewObject(depthTex, Q3DSProfiler::Texture2DObject,
                                       "Depth texture for layer %s", layer3DS->id().constData());
            depthTex->setFormat(Qt3DRender::QAbstractTexture::DepthFormat);
            depthTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
            depthTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
            prepareSizeDependentTexture(depthTex, layer3DS);
            data->depthTextureData.depthTexture = depthTex;

            Qt3DRender::QRenderTarget *depthRt = new Qt3DRender::QRenderTarget;
            m_profiler->trackNewObject(depthRt, Q3DSProfiler::RenderTargetObject,
                                       "Depth texture RT for layer %s", layer3DS->id().constData());
            Qt3DRender::QRenderTargetOutput *depthRtOutput = new Qt3DRender::QRenderTargetOutput;
            depthRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Depth);
            depthRtOutput->setTexture(data->depthTextureData.depthTexture);
            depthRt->addOutput(depthRtOutput);
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

void Q3DSSceneManager::setSsaoTextureEnabled(Q3DSLayerNode *layer3DS, bool enabled)
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
            m_profiler->trackNewObject(ssaoTex, Q3DSProfiler::Texture2DObject,
                                       "SSAO texture for layer %s", layer3DS->id().constData());
            ssaoTex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
            ssaoTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
            ssaoTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
            // it's not just the texture that needs dynamic resizing, but values
            // derived from the size have to be recalculated as well, hence the callback.
            prepareSizeDependentTexture(ssaoTex, layer3DS,
                                        std::bind(&Q3DSSceneManager::updateAoParameters, this, std::placeholders::_1));
            data->ssaoTextureData.ssaoTexture = ssaoTex;

            Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
            m_profiler->trackNewObject(rt, Q3DSProfiler::RenderTargetObject,
                                       "SSAO texture RT for layer %s", layer3DS->id().constData());
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

            data->ssaoTextureData.aoDataBuf = new Qt3DRender::QBuffer;
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

void Q3DSSceneManager::updateAoParameters(Q3DSLayerNode *layer3DS)
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

void Q3DSSceneManager::updateSsaoStatus(Q3DSLayerNode *layer3DS, bool *aoDidChange)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(data);

    const bool needsSsao = layer3DS->aoStrength() > 0.0f;
    const bool hasSsao = data->ssaoTextureData.enabled;

    if (aoDidChange)
        *aoDidChange = needsSsao != hasSsao;

    if (needsSsao == hasSsao) {
        if (hasSsao)
            updateAoParameters(layer3DS);
        return;
    }

    setSsaoTextureEnabled(layer3DS, needsSsao);
}

static const Qt3DRender::QAbstractTexture::CubeMapFace qt3ds_shadowCube_faceIds[6] = {
    Qt3DRender::QAbstractTexture::CubeMapPositiveX,
    Qt3DRender::QAbstractTexture::CubeMapNegativeX,
    Qt3DRender::QAbstractTexture::CubeMapPositiveY,
    Qt3DRender::QAbstractTexture::CubeMapNegativeY,
    Qt3DRender::QAbstractTexture::CubeMapPositiveZ,
    Qt3DRender::QAbstractTexture::CubeMapNegativeZ
};

static const QVector3D qt3ds_shadowCube_up[6] = {
    QVector3D(0, -1, 0),
    QVector3D(0, -1, 0),
    QVector3D(0, 0, 1),
    QVector3D(0, 0, -1),
    QVector3D(0, -1, 0),
    QVector3D(0, -1, 0)
};

static const QVector3D qt3ds_shadowCube_dir[6] = {
    QVector3D(1, 0, 0),
    QVector3D(-1, 0, 0),
    QVector3D(0, 1, 0),
    QVector3D(0, -1, 0),
    QVector3D(0, 0, 1),
    QVector3D(0, 0, -1)
};

void Q3DSSceneManager::updateCubeShadowMapParams(Q3DSLayerAttached::PerLightShadowMapData *d, Q3DSLightNode *light3DS, const QString &lightIndexStr)
{
    Q_ASSERT(light3DS->lightType() != Q3DSLightNode::Directional);
    Q3DSLightAttached *lightData = static_cast<Q3DSLightAttached *>(light3DS->attached());

    const QVector3D lightGlobalPos = lightData->globalTransform.column(3).toVector3D();
    // camera_properties comes from the actual camera, so will reuse the parameter used by normal passes
    // camera_position is for the light the viewpoint of which we are rendering from
    d->cameraPositionParam->setName(QLatin1String("camera_position"));
    d->cameraPositionParam->setValue(QVector3D(lightGlobalPos.x(), lightGlobalPos.y(), -lightGlobalPos.z())); // because the shader wants Z this way

    d->materialParams.shadowSampler->setName(QLatin1String("shadowcube") + lightIndexStr);
    d->materialParams.shadowSampler->setValue(QVariant::fromValue(d->shadowMapTexture));

    d->materialParams.shadowMatrixParam->setName(QLatin1String("shadowmap") + lightIndexStr + QLatin1String("_matrix"));
    d->materialParams.shadowMatrixParam->setValue(QVariant::fromValue(lightData->globalTransform.inverted()));

    d->materialParams.shadowControlParam->setName(QLatin1String("shadowmap") + lightIndexStr + QLatin1String("_control"));
    d->materialParams.shadowControlParam->setValue(QVariant::fromValue(QVector4D(light3DS->shadowBias(), light3DS->shadowFactor(), light3DS->shadowMapFar(), 0)));

    d->shadowCamPropsParam->setName(QLatin1String("camera_properties"));
    d->shadowCamPropsParam->setValue(QVector2D(light3DS->shadowFilter(), light3DS->shadowMapFar()));
}

void Q3DSSceneManager::updateCubeShadowCam(Q3DSLayerAttached::PerLightShadowMapData *d, int faceIdx, Q3DSLightNode *light3DS)
{
    Q_ASSERT(light3DS->lightType() != Q3DSLightNode::Directional);
    Q3DSLightAttached *lightData = static_cast<Q3DSLightAttached *>(light3DS->attached());
    Qt3DRender::QCamera *shadowCam = d->shadowCamProj[faceIdx];

    shadowCam->setProjectionType(Qt3DRender::QCameraLens::PerspectiveProjection);
    shadowCam->setFieldOfView(light3DS->shadowMapFov());
    shadowCam->setNearPlane(1.0f);
    shadowCam->setFarPlane(qMax(2.0f, light3DS->shadowMapFar()));

    const QVector3D lightGlobalPos = lightData->globalTransform.column(3).toVector3D();
    shadowCam->setPosition(lightGlobalPos);
    const QVector3D center(lightGlobalPos + qt3ds_shadowCube_dir[faceIdx]);
    shadowCam->setViewCenter(center);
    shadowCam->setUpVector(qt3ds_shadowCube_up[faceIdx]);
    shadowCam->setAspectRatio(1);
}

void Q3DSSceneManager::genCubeBlurPassFg(Q3DSLayerAttached::PerLightShadowMapData *d, Qt3DRender::QAbstractTexture *inTex,
                                         Qt3DRender::QAbstractTexture *outTex, const QString &passName, Q3DSLightNode *light3DS)
{
    Qt3DRender::QRenderTargetSelector *rtSelector = new Qt3DRender::QRenderTargetSelector(d->subTreeRoot);
    Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
    m_profiler->trackNewObject(rt, Q3DSProfiler::RenderTargetObject,
                               "Shadow cube blur RT for light %s", light3DS->id().constData());
    for (int faceIdx = 0; faceIdx < 6; ++faceIdx) {
        Qt3DRender::QRenderTargetOutput *rtOutput = new Qt3DRender::QRenderTargetOutput;
        rtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::AttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0 + faceIdx));
        rtOutput->setTexture(outTex);
        rtOutput->setFace(qt3ds_shadowCube_faceIds[faceIdx]);
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

    filter->addParameter(depthCubeParam);
    filter->addParameter(d->shadowCamPropsParam);
}

void Q3DSSceneManager::updateOrthoShadowMapParams(Q3DSLayerAttached::PerLightShadowMapData *d, Q3DSLightNode *light3DS, const QString &lightIndexStr)
{
    Q_ASSERT(light3DS->lightType() == Q3DSLightNode::Directional);

    d->materialParams.shadowSampler->setName(QLatin1String("shadowmap") + lightIndexStr);
    d->materialParams.shadowSampler->setValue(QVariant::fromValue(d->shadowMapTexture));

    d->materialParams.shadowMatrixParam->setName(QLatin1String("shadowmap") + lightIndexStr + QLatin1String("_matrix"));
    // [-1, 1] -> [0, 1]
    const QMatrix4x4 bias(0.5f, 0.0f, 0.0f, 0.5f, // ctor takes row major
                          0.0f, 0.5f, 0.0f, 0.5f,
                          0.0f, 0.0f, 0.5f, 0.5f,
                          0.0f, 0.0f, 0.0f, 1.0f);
    const QMatrix4x4 lightVP = d->shadowCamOrtho->projectionMatrix() * d->shadowCamOrtho->viewMatrix();
    d->materialParams.shadowMatrixParam->setValue(QVariant::fromValue(bias * lightVP));

    d->materialParams.shadowControlParam->setName(QLatin1String("shadowmap") + lightIndexStr + QLatin1String("_control"));
    d->materialParams.shadowControlParam->setValue(QVariant::fromValue(QVector4D(light3DS->shadowBias(), light3DS->shadowFactor(), light3DS->shadowMapFar(), 0)));

    d->shadowCamPropsParam->setName(QLatin1String("camera_properties"));
    d->shadowCamPropsParam->setValue(QVector2D(light3DS->shadowFilter(), light3DS->shadowMapFar()));
}

void Q3DSSceneManager::updateOrthoShadowCam(Q3DSLayerAttached::PerLightShadowMapData *d, Q3DSLightNode *light3DS, Q3DSLayerAttached *layerData)
{
    Q_ASSERT(light3DS->lightType() == Q3DSLightNode::Directional);
    Q3DSLightAttached *lightData = static_cast<Q3DSLightAttached *>(light3DS->attached());

    d->shadowCamOrtho->setProjectionType(Qt3DRender::QCameraLens::OrthographicProjection);
    d->shadowCamOrtho->setFieldOfView(light3DS->shadowMapFov());
    d->shadowCamOrtho->setAspectRatio(1);

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

    const QVector3D lightDir = lightData->globalTransform.column(2).toVector3D().normalized();
    float dd = 0.5f * (light3DS->shadowMapFar() + 1.0f);
    lightPos += lightDir * dd;

    const QVector3D camDir = sceneCamData->globalTransform.column(3).toVector3D().normalized();
    float o1 = dd * 2.0f * qTan(0.5f * qDegreesToRadians(light3DS->shadowMapFov()));
    float o2 = light3DS->shadowMapFar() - 1.0f;
    float o = qFabs(QVector3D::dotProduct(lightDir, camDir));
    o = (1.0f - o) * o2 + o * o1;

    float clipNear = 1.0f;
    float clipFar = light3DS->shadowMapFar();

    lightPos -= lightDir * dd;
    clipFar += sceneCamera->nearPlane();

    d->shadowCamOrtho->setNearPlane(clipNear);
    d->shadowCamOrtho->setFarPlane(clipFar);

    // The shadow camera's projection is (more or less?) an ordinary orthographic projection.
    float deltaZ = clipFar - clipNear;
    float halfWidth = (M_PI / 2 * o) / 2;
    if (deltaZ != 0) {
        QMatrix4x4 proj;
        float *writePtr = proj.data();
        writePtr[0] = 1.0f / halfWidth;
        writePtr[5] = 1.0f / (halfWidth / d->shadowCamOrtho->aspectRatio());
        writePtr[10] = -2.0f / deltaZ;
        writePtr[11] = 0.0f;
        writePtr[14] = -(clipNear + clipFar) / deltaZ;
        writePtr[15] = 1.0f;

        d->shadowCamOrtho->setProjectionMatrix(proj);
    }

    // Shadow camera's view matrix.
    lightPos.setZ(-lightPos.z()); // invert, who knows why (Left vs. Right handed mess in 3DS)
    d->shadowCamOrtho->setPosition(lightPos);
    d->shadowCamOrtho->setViewCenter(lightPos - lightDir); // ditto
    d->shadowCamOrtho->setUpVector(QVector3D(0, 1, 0)); // no roll needed
}

void Q3DSSceneManager::genOrthoBlurPassFg(Q3DSLayerAttached::PerLightShadowMapData *d, Qt3DRender::QAbstractTexture *inTex,
                                          Qt3DRender::QAbstractTexture *outTex, const QString &passName, Q3DSLightNode *light3DS)
{
    Qt3DRender::QRenderTargetSelector *rtSelector = new Qt3DRender::QRenderTargetSelector(d->subTreeRoot);
    Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
    m_profiler->trackNewObject(rt, Q3DSProfiler::RenderTargetObject,
                               "Shadow ortho blur RT for light %s", light3DS->id().constData());
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

    filter->addParameter(texParam);
    filter->addParameter(d->shadowCamPropsParam);
}

void Q3DSSceneManager::updateShadowMapStatus(Q3DSLayerNode *layer3DS, bool *smDidChange)
{
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(layerData);
    const int oldShadowCasterCount = layerData->shadowMapData.shadowCasters.count();
    int lightIdx = 0;

    for (Q3DSLayerAttached::PerLightShadowMapData &d : layerData->shadowMapData.shadowCasters)
        d.active = false;

    // Go through the list of visible (eyeball==true) lights and pick the ones with castshadow==true.
    for (Q3DSLightNode *light3DS : qAsConst(layerData->lightNodes)) {
        Q_ASSERT(light3DS->flags().testFlag(Q3DSNode::Active));
        const QString lightIndexStr = QString::number(lightIdx++);

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
            d->active = true;

            const qint32 size = 1 << light3DS->shadowMapRes();
            bool needsNewTextures = false;
            if (d->shadowDS) {
                const QSize currentSize(d->shadowDS->width(), d->shadowDS->height());
                if (currentSize != QSize(size, size))
                    needsNewTextures = true;
            }
            if (d->shadowMapTexture) {
                const QSize currentSize(d->shadowMapTexture->width(), d->shadowMapTexture->height());
                if (currentSize != QSize(size, size))
                    needsNewTextures = true;
            }
            if (needsNewTextures) {
                qCDebug(lcScene, "Slow path! Recreating shadow map textures for light %s", light3DS->id().constData());
                d->shadowDS = nullptr;
                d->shadowMapTexture = nullptr;
                // Regenerate the whole framegraph. A change in shadow map resolution
                // is not something that should happen frequently (or at all).
                delete d->subTreeRoot;
                d->subTreeRoot = nullptr;
            }

            // Framegraph sub-tree root
            bool needsFramegraph = false;
            if (!d->subTreeRoot) {
                d->subTreeRoot = new Qt3DRender::QFrameGraphNode(layerData->shadowMapData.shadowRoot);
                needsFramegraph = true;
            }

            Qt3DCore::QNode *texParent = d->subTreeRoot;
            if (!d->shadowDS) {
                auto createDepthStencil = [size, this]() {
                    Qt3DRender::QTexture2D *dsTexOrRb = new Qt3DRender::QTexture2D;
                    m_profiler->trackNewObject(dsTexOrRb, Q3DSProfiler::Texture2DObject, "Shadow map depth");
                    dsTexOrRb->setFormat(Qt3DRender::QAbstractTexture::D24S8);
                    dsTexOrRb->setWidth(size);
                    dsTexOrRb->setHeight(size);
                    dsTexOrRb->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
                    dsTexOrRb->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
                    return dsTexOrRb;
                };

                // Try to optimize by reusing the same depth-stencil buffer for
                // all lights in the default case where they all have the same
                // shadow map resolution value.
                if (layerData->shadowMapData.defaultShadowDS) {
                    const QSize availSize = QSize(layerData->shadowMapData.defaultShadowDS->width(),
                                                  layerData->shadowMapData.defaultShadowDS->height());
                    if (availSize == QSize(size, size)) {
                        d->shadowDS = layerData->shadowMapData.defaultShadowDS;
                    } else {
                        d->shadowDS = createDepthStencil();
                        // owned by this framegraph subtree
                        d->shadowDS->setParent(texParent);
                    }
                } else {
                    layerData->shadowMapData.defaultShadowDS = createDepthStencil();
                    // parent it so that it outlives this framegraph subtree
                    layerData->shadowMapData.defaultShadowDS->setParent(layerData->entity);
                    d->shadowDS = layerData->shadowMapData.defaultShadowDS;
                }
            }

            Q_ASSERT(d->lightNode == light3DS);
            const bool isCube = light3DS->lightType() != Q3DSLightNode::Directional;

            if (!d->shadowMapTexture) {
                if (isCube) {
                    d->shadowMapTexture = new Qt3DRender::QTextureCubeMap(texParent);
                    m_profiler->trackNewObject(d->shadowMapTexture, Q3DSProfiler::TextureCubeObject,
                                               "Shadow map for light %s", light3DS->id().constData());
                    d->shadowMapTextureTemp = new Qt3DRender::QTextureCubeMap(texParent);
                    m_profiler->trackNewObject(d->shadowMapTexture, Q3DSProfiler::TextureCubeObject,
                                               "Shadow map temp buffer for light %s", light3DS->id().constData());
                } else {
                    d->shadowMapTexture = new Qt3DRender::QTexture2D(texParent);
                    m_profiler->trackNewObject(d->shadowMapTexture, Q3DSProfiler::Texture2DObject,
                                               "Shadow map for light %s", light3DS->id().constData());
                    d->shadowMapTextureTemp = new Qt3DRender::QTexture2D(texParent);
                    m_profiler->trackNewObject(d->shadowMapTexture, Q3DSProfiler::Texture2DObject,
                                               "Shadow map temp buffer for light %s", light3DS->id().constData());
                }

                d->shadowMapTexture->setFormat(Qt3DRender::QAbstractTexture::R16_UNorm);
                d->shadowMapTextureTemp->setFormat(Qt3DRender::QAbstractTexture::R16_UNorm);

                d->shadowMapTexture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
                d->shadowMapTextureTemp->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
                d->shadowMapTexture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
                d->shadowMapTextureTemp->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);

                qCDebug(lcScene, "Shadow cube map size for light %s is %d", light3DS->id().constData(), size);
                // do not add to layerData->sizeManagedTextures since the shadow map size is fixed
                d->shadowMapTexture->setSize(size, size, 1);
                d->shadowMapTextureTemp->setSize(size, size, 1);
            }

            // now the framegraph subtree
            if (needsFramegraph) {
                qCDebug(lcScene, "Generating framegraph for shadow casting light %s", light3DS->id().constData());
                d->shadowCamPropsParam = new Qt3DRender::QParameter;

                // These parameters will be referenced by the material which
                // means they must persist even if this framegraph subtree gets
                // recreated due to a texture change later on. Thus cannot be
                // parented to this part of the fg.
                if (!d->materialParams.shadowSampler)
                    d->materialParams.shadowSampler = new Qt3DRender::QParameter(layerData->entity);
                if (!d->materialParams.shadowMatrixParam)
                    d->materialParams.shadowMatrixParam = new Qt3DRender::QParameter(layerData->entity);
                if (!d->materialParams.shadowControlParam)
                    d->materialParams.shadowControlParam = new Qt3DRender::QParameter(layerData->entity);

                // verify no globally used parameters get parented to this volatile framegraph subtree
                Q_ASSERT(layerData->opaqueTag->parent());
                Q_ASSERT(layerData->transparentTag->parent());
                Q_ASSERT(layerData->cameraPropertiesParam->parent());
                Q_ASSERT(m_fsQuadTag->parent());

                if (isCube) {
                    d->cameraPositionParam = new Qt3DRender::QParameter;

                    // we do not use geometry shaders for some reason, so run a separate pass for each cubemap face instead
                    for (int faceIdx = 0; faceIdx < 6; ++faceIdx) {
                        Qt3DRender::QRenderTargetSelector *shadowRtSelector = new Qt3DRender::QRenderTargetSelector(d->subTreeRoot);
                        Qt3DRender::QRenderTarget *shadowRt = new Qt3DRender::QRenderTarget;
                        m_profiler->trackNewObject(shadowRt, Q3DSProfiler::RenderTargetObject,
                                                   "Shadow cube RT for light %s face %d", light3DS->id().constData(), faceIdx);
                        Qt3DRender::QRenderTargetOutput *shadowRtOutput = new Qt3DRender::QRenderTargetOutput;
                        shadowRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
                        shadowRtOutput->setTexture(d->shadowMapTexture);
                        shadowRtOutput->setFace(qt3ds_shadowCube_faceIds[faceIdx]);
                        shadowRt->addOutput(shadowRtOutput);

                        shadowRtOutput = new Qt3DRender::QRenderTargetOutput;
                        shadowRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::DepthStencil);
                        shadowRtOutput->setTexture(d->shadowDS);
                        shadowRt->addOutput(shadowRtOutput);

                        shadowRtSelector->setTarget(shadowRt);

                        // must have a viewport in order to let the renderer pick up the correct output dimensions and reset the viewport state
                        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(shadowRtSelector);
                        viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

                        Qt3DRender::QCameraSelector *camSel = new Qt3DRender::QCameraSelector(viewport);
                        d->shadowCamProj[faceIdx] = new Qt3DRender::QCamera;
                        updateCubeShadowCam(d, faceIdx, light3DS);
                        camSel->setCamera(d->shadowCamProj[faceIdx]);

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
                    if (m_gfxLimits.maxDrawBuffers >= 6) { // ###
                        // Draws a fullscreen quad into the 6 faces of the cubemap texture (COLOR0..5), with the other texture as input.
                        genCubeBlurPassFg(d, d->shadowMapTexture, d->shadowMapTextureTemp, QLatin1String("shadowCubeBlurX"), light3DS);
                        genCubeBlurPassFg(d, d->shadowMapTextureTemp, d->shadowMapTexture, QLatin1String("shadowCubeBlurY"), light3DS);
                    }

                    // set QParameter names and values
                    updateCubeShadowMapParams(d, light3DS, lightIndexStr);
                } else {
                    Qt3DRender::QRenderTargetSelector *shadowRtSelector = new Qt3DRender::QRenderTargetSelector(d->subTreeRoot);
                    Qt3DRender::QRenderTarget *shadowRt = new Qt3DRender::QRenderTarget;
                    m_profiler->trackNewObject(shadowRt, Q3DSProfiler::RenderTargetObject,
                                               "Shadow ortho RT for light %s", light3DS->id().constData());
                    Qt3DRender::QRenderTargetOutput *shadowRtOutput = new Qt3DRender::QRenderTargetOutput;
                    shadowRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
                    shadowRtOutput->setTexture(d->shadowMapTexture);
                    shadowRt->addOutput(shadowRtOutput);

                    shadowRtOutput = new Qt3DRender::QRenderTargetOutput;
                    shadowRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::DepthStencil);
                    shadowRtOutput->setTexture(d->shadowDS);
                    shadowRt->addOutput(shadowRtOutput);

                    shadowRtSelector->setTarget(shadowRt);

                    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(shadowRtSelector);
                    viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

                    Qt3DRender::QCameraSelector *camSel = new Qt3DRender::QCameraSelector(viewport);
                    d->shadowCamOrtho = new Qt3DRender::QCamera;
                    updateOrthoShadowCam(d, light3DS, layerData);
                    camSel->setCamera(d->shadowCamOrtho);

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
                    genOrthoBlurPassFg(d, d->shadowMapTexture, d->shadowMapTextureTemp, QLatin1String("shadowOrthoBlurX"), light3DS);
                    genOrthoBlurPassFg(d, d->shadowMapTextureTemp, d->shadowMapTexture, QLatin1String("shadowOrthoBlurY"), light3DS);

                    // set QParameter names and values
                    updateOrthoShadowMapParams(d, light3DS, lightIndexStr);
                } // if isCube
            } else {
                // !needsFramegraph -> update the values that could have changed
                if (isCube) {
                    for (int faceIdx = 0; faceIdx < 6; ++faceIdx)
                        updateCubeShadowCam(d, faceIdx, light3DS);

                    updateCubeShadowMapParams(d, light3DS, lightIndexStr);
                } else {
                    updateOrthoShadowCam(d, light3DS, layerData);
                    updateOrthoShadowMapParams(d, light3DS, lightIndexStr);
                }
            }
        } // if light3DS->castShadow
    }

    // Drop shadow map data for lights that are not casting anymore either due
    // to a castshadow or eyeball property change.
    for (int i = 0; i < layerData->shadowMapData.shadowCasters.count(); ++i) {
        auto &sc = layerData->shadowMapData.shadowCasters[i];
        if (!sc.active) {
            qCDebug(lcScene, "Shadow casting light %s is gone", sc.lightNode->id().constData());
            delete sc.subTreeRoot; // bye bye framegraph and shadow map textures
            layerData->shadowMapData.shadowCasters.remove(i--);
        }
    }

    const int newShadowCasterCount = layerData->shadowMapData.shadowCasters.count();
    if (smDidChange)
        *smDidChange = newShadowCasterCount != oldShadowCasterCount;

    if (newShadowCasterCount != oldShadowCasterCount)
        qCDebug(lcScene, "Layer %s has %d shadow casting lights", layer3DS->id().constData(), layerData->shadowMapData.shadowCasters.count());
}

static void offsetProjectionMatrix(QMatrix4x4 *m, const QVector2D &vertexOffset)
{
    QVector4D col3 = m->column(3);
    col3.setX(col3.x() + col3.w() * vertexOffset.x());
    col3.setY(col3.y() + col3.w() * vertexOffset.y());
    m->setColumn(3, col3);
}

//#define PAA_ALWAYS_ON

static const int MAX_AA_LEVELS = 8;

// Called once per frame (in the preparation step from the frame action) for
// each progressive AA enabled layer.
void Q3DSSceneManager::updateProgressiveAA(Q3DSLayerNode *layer3DS)
{
    if (m_flags.testFlag(SubPresentation)) // no PAA for subpresentation layers
        return;

    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    if (!data)
        return;

    // When a frame applies an offset to the projection matrix, the next frame
    // must reset it. This must happen regardless of having PAA active in the next frame.
    // ### broken, to be replaced, see below
    Qt3DRender::QCamera *camera = static_cast<Qt3DRender::QCamera *>(data->cameraSelector->camera());
    if (data->progAA.projMatAltered) {
        data->progAA.projMatAltered = false;
        if (camera->projectionMatrix() == data->progAA.alteredProjMat) // size change could have caused recalculating the camera
            camera->setProjectionMatrix(data->progAA.origProjMat);
    }

    if (data->layerSize.isEmpty())
        return;

    const QSize layerPixelSize = safeLayerPixelSize(data);

    // Progressive AA kicks in only when "movement has stopped", or rather,
    // when no properties have changed, meaning the frame was not dirty. Reset
    // the pass index otherwise.
#ifndef PAA_ALWAYS_ON
    if (data->wasDirty)
        data->progAA.pass = 0;
#endif

    // Do not start accumulating before at least 2 non-dirty frames. This is
    // not what 3DS1 does (there there's no delay) but our dirty flags do not
    // work the same way, apparently (or could be anim fw that causes the
    // difference in some test scenes, not sure).
    const int PROGAA_FRAME_DELAY = 1;

    int maxPass = 0;
    switch (layer3DS->progressiveAA()) {
    case Q3DSLayerNode::PAA2x:
        maxPass = 2 + PROGAA_FRAME_DELAY;
        break;
    case Q3DSLayerNode::PAA4x:
        maxPass = 4 + PROGAA_FRAME_DELAY;
        break;
    case Q3DSLayerNode::PAA8x:
        maxPass = 8 + PROGAA_FRAME_DELAY;
        break;
    default:
        Q_UNREACHABLE();
        break;
    }

    if (data->progAA.pass > maxPass) {
        if (data->progAA.layerFilter->layers().contains(m_fsQuadTag))
            data->progAA.layerFilter->removeLayer(m_fsQuadTag);
#ifdef PAA_ALWAYS_ON
        data->progAA.pass = 0;
#endif
        return;
    }

    if (data->progAA.pass < 1 + PROGAA_FRAME_DELAY) {
        ++data->progAA.pass;
        if (data->progAA.enabled) {
            qCDebug(lcScene, "Stopping progressive AA for layer %s", layer3DS->id().constData());
            data->progAA.enabled = false;
            data->compositorSourceParam->setValue(QVariant::fromValue(data->effLayerTexture ? data->effLayerTexture : data->layerTexture));

            // Do not delete and then recreate the framegraph subtree since
            // that is likely way too expensive. Keep it around instead and
            // disable it by making sure it has no renderable entities. This
            // could have already been done in the > maxPass branch above but
            // won't hurt to repeat since we may get a dirty frame before
            // reaching the maximum accumulation level.
            data->progAA.layerFilter->removeLayer(m_fsQuadTag);
        }
        return;
    }

    static QVector2D vertexOffsets[MAX_AA_LEVELS] = {
        QVector2D(-0.170840f, -0.553840f), // 1x
        QVector2D(0.162960f, -0.319340f),  // 2x
        QVector2D(0.360260f, -0.245840f),  // 3x
        QVector2D(-0.561340f, -0.149540f), // 4x
        QVector2D(0.249460f, 0.453460f),   // 5x
        QVector2D(-0.336340f, 0.378260f),  // 6x
        QVector2D(0.340000f, 0.166260f),   // 7x
        QVector2D(0.235760f, 0.527760f),   // 8x
    };
    // (frame blend factor, accumulator blend factor)
    static QVector2D blendFactors[MAX_AA_LEVELS] = {
        QVector2D(0.500000f, 0.500000f), // 1x
        QVector2D(0.333333f, 0.666667f), // 2x
        QVector2D(0.250000f, 0.750000f), // 3x
        QVector2D(0.200000f, 0.800000f), // 4x
        QVector2D(0.166667f, 0.833333f), // 5x
        QVector2D(0.142857f, 0.857143f), // 6x
        QVector2D(0.125000f, 0.875000f), // 7x
        QVector2D(0.111111f, 0.888889f), // 8x
    };

    int factorsIdx = data->progAA.pass - (1 + PROGAA_FRAME_DELAY);
    QVector2D vertexOffset = vertexOffsets[factorsIdx];
    vertexOffset.setX(vertexOffset.x() / (layerPixelSize.width() / 2.0f));
    vertexOffset.setY(vertexOffset.y() / (layerPixelSize.height() / 2.0f));
    QVector2D blendFactor = blendFactors[factorsIdx];

    if (!data->progAA.enabled && data->progAA.fg && data->progAA.layerFilter)
        data->progAA.layerFilter->addLayer(m_fsQuadTag);

    data->progAA.enabled = true;
    if (factorsIdx == 0)
        qCDebug(lcScene, "Kicking off progressive AA for layer %s", layer3DS->id().constData());

    // Alter the camera's projection matrix by a little movement based on the current vertexOffset.
    // This applies to the camera used by the main layer passes.

    // ### this is broken. The original approach was to apply the offset to the
    // final modelview-projection matrix. The projection here is not sufficient.
    QMatrix4x4 projMat = camera->projectionMatrix();
    data->progAA.origProjMat = projMat;
    offsetProjectionMatrix(&projMat, vertexOffset);
    data->progAA.alteredProjMat = projMat;
    camera->setProjectionMatrix(projMat);
    data->progAA.projMatAltered = true;

    // data->layerTexture is the original contents (albeit with jiggled
    // camera), generated by the main layer passes. Have two additional
    // textures, and swap them every frame (the one that was used as input will
    // be used as output in the next frame). The layer compositor will be
    // switched over to use always the one that is the (blended) output.

    if (!data->progAA.outputTex) {
        data->progAA.outputTex = new Qt3DRender::QTexture2D(data->layerFgRoot);
        m_profiler->trackNewObject(data->progAA.outputTex, Q3DSProfiler::Texture2DObject,
                                   "Progressive AA texture for layer %s", layer3DS->id().constData());
        data->progAA.outputTex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
        data->progAA.outputTex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
        data->progAA.outputTex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
        prepareSizeDependentTexture(data->progAA.outputTex, layer3DS);
    }

    // ### depth, ssao, shadow passes in the main layer framegraph subtree should be disabled when pass > 0

    // ### what if data->layerTexture is multisample?

    // ### all the below stuff does not yet handle data->effLayerTexture

    // For data->progAA.accumTex there is no new texture needed - instead,
    // steal data->layerTexture.
    if (factorsIdx == 0) {
        delete data->progAA.accumTex;
        data->progAA.accumTex = data->layerTexture;
        // create a whole new render target for the layer
        data->sizeManagedTextures.removeOne(data->layerTexture);
        data->sizeManagedTextures.removeOne(data->layerDS);
        int msaaSampleCount = 0; // ###
        Qt3DRender::QAbstractTexture *colorTex;
        Qt3DRender::QAbstractTexture *dsTexOrRb;
        Qt3DRender::QRenderTarget *rt = newLayerRenderTarget(layerPixelSize, msaaSampleCount,
                                                             &colorTex, &dsTexOrRb, data->layerFgRoot, layer3DS);
        Qt3DRender::QRenderTarget *oldRt = data->rtSelector->target();
        data->rtSelector->setTarget(rt);
        delete oldRt;
        data->sizeManagedTextures.insert(0, colorTex);
        data->sizeManagedTextures.insert(1, dsTexOrRb);
        data->layerTexture = colorTex;
        data->layerDS = dsTexOrRb;
    }

    //if (!data->progAA.fg) {
    // ### have to do this on every new PAA run since accumTex changes above.
    // It is an overkill though since the framegraph should be generated just once.
    if (factorsIdx == 0) {
        delete data->progAA.fg;

        // set up a framegraph subtree to render a quad

        // Due to QTBUG-64757 we will need to switch QRenderTargets, not just
        // the texture in QRenderTargetOutput. Hence the need for multiple
        // ones. This is probably more efficient anyways (since it results in
        // binding a different FBO, without altering attachments).

        // will render to outputTex, accumTex, outputTex, accumTex, outputTex, ...
        data->progAA.curTarget = 0;
        for (Qt3DRender::QAbstractTexture *t : { data->progAA.outputTex, data->progAA.accumTex }) {
            Qt3DRender::QRenderTargetOutput *rtOutput = new Qt3DRender::QRenderTargetOutput;
            rtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
            rtOutput->setTexture(t);
            Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
            m_profiler->trackNewObject(rt, Q3DSProfiler::RenderTargetObject,
                                       "Progressive AA RT for layer %s", layer3DS->id().constData());
            rt->addOutput(rtOutput);
            data->progAA.rts[data->progAA.curTarget++] = rt;
        }

        Qt3DRender::QRenderTargetSelector *rtSel = new Qt3DRender::QRenderTargetSelector(data->layerFgRoot);
        data->progAA.fg = rtSel;
        data->progAA.rtSel = rtSel;
        data->progAA.curTarget = 0;
        rtSel->setTarget(data->progAA.rts[0]);

        Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(rtSel);
        viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

        Qt3DRender::QFilterKey *filterKey = new Qt3DRender::QFilterKey;
        filterKey->setName(QLatin1String("pass"));
        filterKey->setValue(QLatin1String("progaa"));
        Qt3DRender::QRenderPassFilter *filter = new Qt3DRender::QRenderPassFilter(viewport);
        filter->addMatch(filterKey);

        Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(filter);
        data->progAA.layerFilter = layerFilter;
        layerFilter->addLayer(m_fsQuadTag);

        data->progAA.accumTexParam = new Qt3DRender::QParameter;
        data->progAA.accumTexParam->setName(QLatin1String("accumulator"));
        data->progAA.lastTexParam = new Qt3DRender::QParameter;
        data->progAA.lastTexParam->setName(QLatin1String("last_frame"));
        data->progAA.blendFactorsParam = new Qt3DRender::QParameter;
        data->progAA.blendFactorsParam->setName(QLatin1String("blend_factors"));

        filter->addParameter(data->progAA.accumTexParam);
        filter->addParameter(data->progAA.lastTexParam);
        filter->addParameter(data->progAA.blendFactorsParam);
    }

    // Input
    data->progAA.accumTexParam->setValue(QVariant::fromValue(data->progAA.accumTex));
    data->progAA.lastTexParam->setValue(QVariant::fromValue(data->layerTexture));
    data->progAA.blendFactorsParam->setValue(blendFactor);

    // Output
    data->progAA.rtSel->setTarget(data->progAA.rts[data->progAA.curTarget]);

    // have the compositor use the blended results instead of the layer texture
    data->compositorSourceParam->setValue(QVariant::fromValue(data->progAA.outputTex));

    // In the next PAA round (i.e. the frame after the next one) reuse accumTex
    // as the output and the current output as accumTex...
    std::swap(data->progAA.accumTex, data->progAA.outputTex);
    // ...whereas the output of the next frame will be used as input, so flip the
    // index to use the correct render target.
    data->progAA.curTarget = 1 - data->progAA.curTarget;

    ++data->progAA.pass;
}

static void setLayerBlending(Qt3DRender::QBlendEquation *blendFunc,
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

void Q3DSSceneManager::buildLayerQuadEntity(Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parentEntity,
                                            Qt3DRender::QLayer *tag, BuildLayerQuadFlags flags,
                                            Qt3DRender::QRenderPass **outRenderPass)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(data);
    Qt3DCore::QEntity *layerQuadEntity = new Qt3DCore::QEntity(parentEntity);
    layerQuadEntity->setObjectName(QObject::tr("compositor for %1").arg(QString::fromUtf8(layer3DS->id())));
    data->compositorEntity = layerQuadEntity;
    if (!layer3DS->flags().testFlag(Q3DSNode::Active))
        layerQuadEntity->setEnabled(false);

    // QPlaneMesh works here because the compositor shader is provided by
    // us, not imported from 3DS, and so the VS uses the Qt3D attribute names.
    Qt3DExtras::QPlaneMesh *mesh = new Qt3DExtras::QPlaneMesh;
    mesh->setWidth(2);
    mesh->setHeight(2);
    mesh->setMirrored(true);
    Qt3DCore::QTransform *transform = new Qt3DCore::QTransform;
    transform->setRotationX(90);

    // defer the sizing and positioning
    data->updateCompositorCalculations = [data, layerQuadEntity, tag, mesh, transform]() {
        if (data->layerSize.isEmpty()) {
            layerQuadEntity->removeComponent(tag);
            return;
        }
        if (!layerQuadEntity->components().contains(tag))
            layerQuadEntity->addComponent(tag);

        mesh->setWidth(2 * data->layerSize.width() / float(data->parentSize.width()));
        mesh->setHeight(2 * data->layerSize.height() / float(data->parentSize.height()));
        const float x = data->layerPos.x() / float(data->parentSize.width()) * 2;
        const float y = -data->layerPos.y() / float(data->parentSize.height()) * 2;
        transform->setTranslation(QVector3D(-(2.0f - mesh->width()) / 2 + x,
                                            (2.0f - mesh->height()) / 2 + y,
                                            0));
    };

    Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial;
    Qt3DRender::QEffect *effect = new Qt3DRender::QEffect;
    Qt3DRender::QTechnique *technique = new Qt3DRender::QTechnique;
    bool isGLES = false;
    Q3DSDefaultMaterialGenerator::addDefaultApiFilter(technique, &isGLES);

    Qt3DRender::QRenderPass *renderPass = new Qt3DRender::QRenderPass;

    if (flags.testFlag(LayerQuadBlend)) {
        Qt3DRender::QBlendEquation *blendFunc = new Qt3DRender::QBlendEquation;
        Qt3DRender::QBlendEquationArguments *blendArgs = new Qt3DRender::QBlendEquationArguments;
        setLayerBlending(blendFunc, blendArgs, layer3DS);
        renderPass->addRenderState(blendFunc);
        renderPass->addRenderState(blendArgs);
    }

    Qt3DRender::QDepthTest *depthTest = new Qt3DRender::QDepthTest;
    depthTest->setDepthFunction(Qt3DRender::QDepthTest::Always);
    renderPass->addRenderState(depthTest);
    Qt3DRender::QNoDepthMask *noDepthWrite = new Qt3DRender::QNoDepthMask;
    renderPass->addRenderState(noDepthWrite);

    if (outRenderPass)
        *outRenderPass = renderPass;

    if (!flags.testFlag(LayerQuadCustomShader)) {
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
        if (data->msaaSampleCount > 1)
            fragSrc = QLatin1String("qrc:/q3ds/shaders/compositor_ms") + QString::number(data->msaaSampleCount) + fragSuffix;
        shaderProgram->setFragmentShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl(fragSrc)));
        renderPass->setShaderProgram(shaderProgram);
        renderPass->addParameter(data->compositorSourceParam);
    }

    technique->addRenderPass(renderPass);
    effect->addTechnique(technique);
    material->setEffect(effect);

    layerQuadEntity->addComponent(tag);
    layerQuadEntity->addComponent(mesh);
    layerQuadEntity->addComponent(transform);
    layerQuadEntity->addComponent(material);
}

void Q3DSSceneManager::buildCompositor(Qt3DRender::QFrameGraphNode *parent, Qt3DCore::QEntity *parentEntity)
{
    // Simplified view (excluding advanced blending-specific nodes):
    // Viewport - CameraSelector - ClearBuffers - NoDraw
    //                           - LayerFilter for layer quad entity 0
    //                           - LayerFilter for layer quad entity 1
    //                             ...
    // With standard blend modes only this is simplified to a single LayerFilter and QLayer tag.

    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(parent);
    viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

    Qt3DRender::QCamera *camera = new Qt3DRender::QCamera;
    camera->setObjectName(QObject::tr("compositor camera"));
    camera->setProjectionType(Qt3DRender::QCameraLens::OrthographicProjection);
    camera->setLeft(-1);
    camera->setRight(1);
    camera->setTop(1);
    camera->setBottom(-1);
    camera->setPosition(QVector3D(0, 0, 1));
    camera->setViewCenter(QVector3D(0, 0, 0));

    Qt3DRender::QCameraSelector *cameraSelector = new Qt3DRender::QCameraSelector(viewport);
    cameraSelector->setCamera(camera);

    Qt3DRender::QClearBuffers *clearBuffers = new Qt3DRender::QClearBuffers(cameraSelector);
    clearBuffers->setBuffers(Qt3DRender::QClearBuffers::ColorDepthStencilBuffer);
    QColor clearColor = Qt::transparent;
    if (m_scene->useClearColor()) {
        // Alpha is 1 here even for subpresentations. Otherwise there would
        // be no way to get the background visible when used as a texture later on.
        clearColor = m_scene->clearColor();
    }
    clearBuffers->setClearColor(clearColor);

    new Qt3DRender::QNoDraw(clearBuffers);

    QVarLengthArray<Q3DSLayerNode *, 16> layers;
    Q3DSPresentation::forAllLayers(m_scene, [&layers](Q3DSLayerNode *layer3DS) {
        layers.append(layer3DS);
    }, true); // process layers in reverse order

    auto layerNeedsAdvancedBlending = [](Q3DSLayerNode *layer3DS) {
        return layer3DS->layerBackground() == Q3DSLayerNode::Transparent
                && (layer3DS->blendType() == Q3DSLayerNode::Overlay
                    || layer3DS->blendType() == Q3DSLayerNode::ColorBurn
                    || layer3DS->blendType() == Q3DSLayerNode::ColorDodge);
    };

    bool needsAdvanced = false;
    for (Q3DSLayerNode *layer3DS : layers) {
        if (layerNeedsAdvancedBlending(layer3DS)) {
            needsAdvanced = true;
            break;
        }
    }

    if (!needsAdvanced) {
        Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(cameraSelector);
        Qt3DRender::QLayer *tag = new Qt3DRender::QLayer;
        layerFilter->addLayer(tag);

        for (Q3DSLayerNode *layer3DS : layers) {
            BuildLayerQuadFlags flags = 0;
            if (layer3DS->layerBackground() == Q3DSLayerNode::Transparent)
                flags |= LayerQuadBlend;

            buildLayerQuadEntity(layer3DS, parentEntity, tag, flags, nullptr);
        }
    } else {
        /*
            1. get a fullscreen texture (screen_texture), clear it either to (0, 0, 0, 0) or (scene.clearColor, 1)
            2. for each layer:
                  if uses advanced blend mode:
                    - get a temp_texture matching the layer size
                    - blit with screen_texture into temp_texture (only the area covered by the layer in question)
                    - draw a quad into screen_texture with the appropriate blend shader with base_layer=temp_texture and blend_layer=layer_texture
                  else:
                    - draw a quad into screen_texture with layer_texture (with normal blending)
            3. fullscreen blit with screen_texture

            Now, the fullscreen texture is not normally needed since we can
            just the backbuffer (or the subpresentation's target texture), as
            long as the blits are performed via blitFramebuffer. (would not
            work with a draw quad)
         */

        for (Q3DSLayerNode *layer3DS : layers) {
            if (layerNeedsAdvancedBlending(layer3DS)) {
                Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
                Q_ASSERT(data);
                if (!data->advBlend.tempTexture) {
                    data->advBlend.tempTexture = new Qt3DRender::QTexture2D(data->entity);
                    m_profiler->trackNewObject(data->advBlend.tempTexture, Q3DSProfiler::Texture2DObject,
                                               "Advanced blend texture for layer %s", layer3DS->id().constData());
                    data->advBlend.tempTexture->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
                    data->advBlend.tempTexture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
                    data->advBlend.tempTexture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
                    const QSize sz = safeLayerPixelSize(data);
                    data->advBlend.tempTexture->setWidth(sz.width());
                    data->advBlend.tempTexture->setHeight(sz.height());
                    // must track layer size, but without the SSAA scale
                    Q3DSLayerAttached::SizeManagedTexture st(data->advBlend.tempTexture,
                                                             nullptr,
                                                             Q3DSLayerAttached::SizeManagedTexture::IgnoreSSAA);
                    data->sizeManagedTextures.append(st);
                }

                if (!data->advBlend.tempRt) {
                    // this assumes QTBUG-65080 is fixed
                    data->advBlend.tempRt = new Qt3DRender::QRenderTarget(data->entity);
                    m_profiler->trackNewObject(data->advBlend.tempRt, Q3DSProfiler::RenderTargetObject,
                                               "Advanced blend RT for layer %s", layer3DS->id().constData());
                    Qt3DRender::QRenderTargetOutput *tempTexOutput = new Qt3DRender::QRenderTargetOutput;
                    tempTexOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
                    tempTexOutput->setTexture(data->advBlend.tempTexture);
                    data->advBlend.tempRt->addOutput(tempTexOutput);
                }

                Qt3DRender::QBlitFramebuffer *bgBlit = new Qt3DRender::QBlitFramebuffer(cameraSelector);
                bgBlit->setDestination(data->advBlend.tempRt);
                new Qt3DRender::QNoDraw(bgBlit);

                // Layer size dependent properties have to be updated dynamically.
                auto setSizeDependentValues = [this, bgBlit, layer3DS, data](Q3DSLayerNode *changedLayer) {
                    if (changedLayer != layer3DS)
                        return;

                    if (data->layerSize.isEmpty())
                        return;

                    // this assumes QTBUG-65123 is fixed
                    QRectF srcRect(data->layerPos, data->layerSize);
                    bgBlit->setSourceRect(srcRect);
                    QRectF dstRect(QPointF(0, 0), data->layerSize);
                    bgBlit->setDestinationRect(dstRect);
                };
                data->layerSizeChangeCallbacks.append(setSizeDependentValues);
                // Set some initial sizes.
                setSizeDependentValues(layer3DS);

                Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(cameraSelector);
                Qt3DRender::QLayer *tag = new Qt3DRender::QLayer;
                layerFilter->addLayer(tag);

                // Now we do not need normal blending and will provide a custom shader program.
                Qt3DRender::QRenderPass *renderPass;
                BuildLayerQuadFlags flags = LayerQuadCustomShader;
                buildLayerQuadEntity(layer3DS, parentEntity, tag, flags, &renderPass);

                switch (layer3DS->blendType()) {
                case Q3DSLayerNode::Overlay:
                    renderPass->setShaderProgram(Q3DSShaderManager::instance().getBlendOverlayShader(data->entity, data->msaaSampleCount));
                    break;
                case Q3DSLayerNode::ColorBurn:
                    renderPass->setShaderProgram(Q3DSShaderManager::instance().getBlendColorBurnShader(data->entity, data->msaaSampleCount));
                    break;
                case Q3DSLayerNode::ColorDodge:
                    renderPass->setShaderProgram(Q3DSShaderManager::instance().getBlendColorDodgeShader(data->entity, data->msaaSampleCount));
                    break;
                default:
                    Q_UNREACHABLE();
                    break;
                }

                Qt3DRender::QParameter *baseLayerParam = new Qt3DRender::QParameter;
                baseLayerParam->setName(QLatin1String("base_layer"));
                baseLayerParam->setValue(QVariant::fromValue(data->advBlend.tempTexture));
                Qt3DRender::QParameter *blendLayerParam = new Qt3DRender::QParameter;
                blendLayerParam->setName(QLatin1String("blend_layer"));
                blendLayerParam->setValue(QVariant::fromValue(data->effLayerTexture ? data->effLayerTexture : data->layerTexture));
                renderPass->addParameter(baseLayerParam);
                renderPass->addParameter(blendLayerParam);
            } else {
                Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(cameraSelector);
                Qt3DRender::QLayer *tag = new Qt3DRender::QLayer;
                layerFilter->addLayer(tag);

                BuildLayerQuadFlags flags = 0;
                if (layer3DS->layerBackground() == Q3DSLayerNode::Transparent)
                    flags |= LayerQuadBlend;

                buildLayerQuadEntity(layer3DS, parentEntity, tag, flags, nullptr);
            }
        }
    }
}

void Q3DSSceneManager::buildGuiPass(Qt3DRender::QFrameGraphNode *parent, Qt3DCore::QEntity *parentEntity)
{
    // No dependencies to the actual gui renderer here. Isolate that to
    // profileui. The interface consists of tags and filter keys. profileui can
    // then assume the framegraph has the necessary LayerFilters both for
    // including and excluding.

    m_guiData.tag = new Qt3DRender::QLayer; // all gui entities are tagged with this
    m_guiData.activeTag = new Qt3DRender::QLayer; // active gui entities - added/removed to entities dynamically by imguimanager
    m_guiData.techniqueFilterKey = new Qt3DRender::QFilterKey;
    m_guiData.techniqueFilterKey->setName(QLatin1String("type"));
    m_guiData.techniqueFilterKey->setValue(QLatin1String("gui"));
    m_guiData.rootEntity = parentEntity;

    Qt3DRender::QTechniqueFilter *tfilter = new Qt3DRender::QTechniqueFilter(parent);
    tfilter->addMatch(m_guiData.techniqueFilterKey);

    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(tfilter);
    viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

    Qt3DRender::QCameraSelector *cameraSel = new Qt3DRender::QCameraSelector(viewport);
    m_guiData.camera = new Qt3DRender::QCamera;
    m_guiData.camera->setProjectionType(Qt3DRender::QCameraLens::OrthographicProjection);
    m_guiData.camera->setLeft(0);
    m_guiData.camera->setRight(m_guiData.outputSize.width() * m_guiData.outputDpr);
    m_guiData.camera->setTop(0);
    m_guiData.camera->setBottom(m_guiData.outputSize.height() * m_guiData.outputDpr);
    m_guiData.camera->setNearPlane(-1);
    m_guiData.camera->setFarPlane(1);
    cameraSel->setCamera(m_guiData.camera);

    Qt3DRender::QSortPolicy *sortPolicy = new Qt3DRender::QSortPolicy(cameraSel);
    sortPolicy->setSortTypes(QVector<Qt3DRender::QSortPolicy::SortType>() << Qt3DRender::QSortPolicy::BackToFront);

    Qt3DRender::QLayerFilter *lfilter = new Qt3DRender::QLayerFilter(sortPolicy);
    lfilter->addLayer(m_guiData.activeTag);
}

void Q3DSSceneManager::buildFsQuad(const FsQuadParams &info)
{
    Q_ASSERT(info.tag);
    Q_ASSERT(info.passNames.count() == info.passProgs.count());

    Qt3DCore::QEntity *fsQuadEntity = new Qt3DCore::QEntity(info.parentEntity);
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

    for (int i = 0; i < info.passNames.count(); ++i) {
        Qt3DRender::QFilterKey *filterKey = new Qt3DRender::QFilterKey;
        filterKey->setName(QLatin1String("pass"));
        filterKey->setValue(info.passNames[i]);

        Qt3DRender::QRenderPass *pass = new Qt3DRender::QRenderPass;
        pass->addFilterKey(filterKey);

        pass->setShaderProgram(info.passProgs[i]);

        for (Qt3DRender::QParameter *param : info.params)
            pass->addParameter(param);

        for (Qt3DRender::QRenderState *state : info.renderStates)
            pass->addRenderState(state);

        if (!info.flags.testFlag(FsQuadCustomDepthSettings))
            pass->addRenderState(new Qt3DRender::QNoDepthMask);

        technique->addRenderPass(pass);
    }

    effect->addTechnique(technique);
    material->setEffect(effect);

    fsQuadEntity->addComponent(info.tag);
    fsQuadEntity->addComponent(mesh);
    fsQuadEntity->addComponent(transform);
    fsQuadEntity->addComponent(material);
}

void Q3DSSceneManager::buildLayerScene(Q3DSGraphObject *obj, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
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
        obj->addPropertyChangeObserver(std::bind(&Q3DSSceneManager::handlePropertyChange, this,
                                                 std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        if (obj->type() == Q3DSGraphObject::Effect)
            buildEffect(static_cast<Q3DSEffectInstance *>(obj), layer3DS);

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
        m_componentNodeStack.push(comp);
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
        m_componentNodeStack.pop();
    }
        break;
    case Q3DSGraphObject::Camera:
        newEntity = buildCamera(static_cast<Q3DSCameraNode *>(obj), layer3DS, parent);
        addChildren(obj, newEntity);
        break;
    default:
        break;
    }
    auto nodeData = static_cast<Q3DSNodeAttached*>(obj->attached());
    if (nodeData)
        nodeData->component = m_componentNodeStack.top();
}

Qt3DCore::QTransform *Q3DSSceneManager::initEntityForNode(Qt3DCore::QEntity *entity, Q3DSNode *node, Q3DSLayerNode *layer3DS)
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

void Q3DSSceneManager::setNodeProperties(Q3DSNode *node, Qt3DCore::QEntity *entity,
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

void Q3DSSceneManager::updateGlobals(Q3DSNode *node, UpdateGlobalFlags flags)
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

Qt3DCore::QEntity *Q3DSSceneManager::buildGroup(Q3DSGroupNode *group3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSGroupAttached *data = new Q3DSGroupAttached;
    group3DS->setAttached(data);

    Qt3DCore::QEntity *group = new Qt3DCore::QEntity(parent);
    group->setObjectName(QObject::tr("group %1").arg(QString::fromUtf8(group3DS->id())));
    initEntityForNode(group, group3DS, layer3DS);

    group3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneManager::handlePropertyChange, this,
                                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    return group;
}

Qt3DCore::QEntity *Q3DSSceneManager::buildComponent(Q3DSComponentNode *comp3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSComponentAttached *data = new Q3DSComponentAttached;
    comp3DS->setAttached(data);

    Qt3DCore::QEntity *comp = new Qt3DCore::QEntity(parent);
    comp->setObjectName(QObject::tr("component %1").arg(QString::fromUtf8(comp3DS->id())));
    initEntityForNode(comp, comp3DS, layer3DS);

    comp3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneManager::handlePropertyChange, this,
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

Qt3DCore::QEntity *Q3DSSceneManager::buildText(Q3DSTextNode *text3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSTextAttached *data = new Q3DSTextAttached;
    text3DS->setAttached(data);

    Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(parent);
    entity->setObjectName(QObject::tr("text %1").arg(QString::fromUtf8(text3DS->id())));
    initEntityForNode(entity, text3DS, layer3DS);

    text3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneManager::handlePropertyChange, this,
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
    m_profiler->trackNewObject(data->texture, Q3DSProfiler::Texture2DObject,
                               "Texture for text item %s", text3DS->id().constData());
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

void Q3DSSceneManager::updateText(Q3DSTextNode *text3DS, bool needsNewImage)
{
    Q3DSTextAttached *data = static_cast<Q3DSTextAttached *>(text3DS->attached());
    Q_ASSERT(data);

    if (data->dirty.testFlag(Q3DSGraphObjectAttached::GlobalOpacityDirty))
        data->opacityParam->setValue(data->globalOpacity);

    data->colorParam->setValue(text3DS->color());

    if (needsNewImage) // textstring, leading, tracking
        data->textureImage->update();
}

Qt3DCore::QEntity *Q3DSSceneManager::buildLight(Q3DSLightNode *light3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSLightAttached *data = new Q3DSLightAttached;
    light3DS->setAttached(data);

    Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(parent);
    entity->setObjectName(QObject::tr("light %1").arg(QString::fromUtf8(light3DS->id())));
    initEntityForNode(entity, light3DS, layer3DS);

    setLightProperties(light3DS, true);

    light3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneManager::handlePropertyChange, this,
                                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    return entity;
}

void Q3DSSceneManager::setLightProperties(Q3DSLightNode *light3DS, bool forceUpdate)
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

        if (light3DS->lightType() == Q3DSLightNode::Directional) {
            // directional lights have a w value of 0 in position
            // position is used for direction in the custom material shader
            // custom material shader also wants the inverse handedness
            ls->positionParam->setValue(QVector4D(directionFromTransform(data->globalTransform, !leftHanded), 0.0f));
        } else {
            // point and area lights have w value of not-zero in position
            ls->positionParam->setValue(QVector4D(data->globalTransform.column(3).toVector3D(), 1.0f));
        }

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
    auto diffuseColor = QVector4D(float(light3DS->diffuse().redF()) * normalizedBrightness,
                                  float(light3DS->diffuse().greenF()) * normalizedBrightness,
                                  float(light3DS->diffuse().blueF()) * normalizedBrightness,
                                  float(light3DS->diffuse().alphaF()));
    ls->diffuseParam->setValue(diffuseColor);

    if (!ls->ambientParam)
        ls->ambientParam = new Qt3DRender::QParameter;
    ls->ambientParam->setName(QLatin1String("ambient"));
    auto ambientColor = QVector4D(float(light3DS->ambient().redF()),
                                  float(light3DS->ambient().greenF()),
                                  float(light3DS->ambient().blueF()),
                                  float(light3DS->ambient().alphaF()));
    ls->ambientParam->setValue(ambientColor);

    if (!ls->specularParam)
        ls->specularParam = new Qt3DRender::QParameter;
    ls->specularParam->setName(QLatin1String("specular"));
    auto specularColor = QVector4D(float(light3DS->specular().redF()) * normalizedBrightness,
                                   float(light3DS->specular().greenF()) * normalizedBrightness,
                                   float(light3DS->specular().blueF()) * normalizedBrightness,
                                   float(light3DS->specular().alphaF()));
    ls->specularParam->setValue(specularColor);

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
    ls->linearAttenuationParam->setValue(qBound(0.0f, light3DS->linearFade(), 1000.0f) * 0.0001f);

    if (!ls->quadraticAttenuationParam)
        ls->quadraticAttenuationParam = new Qt3DRender::QParameter;
    ls->quadraticAttenuationParam->setName(QLatin1String("quadraticAttenuation"));
    ls->quadraticAttenuationParam->setValue(qBound(0.0f, light3DS->expFade(), 1000.0f) * 0.0000001f);

    // having non-zero values in either width or hight properties
    // determines that this is an area light in shader logic
    if (!ls->widthParam)
        ls->widthParam = new Qt3DRender::QParameter;
    ls->widthParam->setName(QLatin1String("width"));
    if (light3DS->lightType() == Q3DSLightNode::Area)
        ls->widthParam->setValue(light3DS->areaWidth());
    else
        ls->widthParam->setValue(0.0f);

    if (!ls->heightParam)
        ls->heightParam = new Qt3DRender::QParameter;
    ls->heightParam->setName(QLatin1String("height"));
    if (light3DS->lightType() == Q3DSLightNode::Area)
        ls->heightParam->setValue(light3DS->areaHeight());
    else
        ls->heightParam->setValue(0.0f);

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

Qt3DCore::QEntity *Q3DSSceneManager::buildModel(Q3DSModelNode *model3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
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

    model3DS->addPropertyChangeObserver(std::bind(&Q3DSSceneManager::handlePropertyChange, this,
                                                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    return entity;
}

static void addShadowSsaoParams(Q3DSLayerAttached *layerData, QVector<Qt3DRender::QParameter *> *params)
{
    for (const Q3DSLayerAttached::PerLightShadowMapData &sd : qAsConst(layerData->shadowMapData.shadowCasters)) {
        if (sd.materialParams.shadowSampler)
            params->append(sd.materialParams.shadowSampler);
        if (sd.materialParams.shadowMatrixParam)
            params->append(sd.materialParams.shadowMatrixParam);
        if (sd.materialParams.shadowControlParam)
            params->append(sd.materialParams.shadowControlParam);
    }

    if (layerData->ssaoTextureData.enabled && layerData->ssaoTextureData.ssaoTextureSampler)
        params->append(layerData->ssaoTextureData.ssaoTextureSampler);
}

void Q3DSSceneManager::buildModelMaterial(Q3DSModelNode *model3DS)
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
                    layerData->allLightsConstantBuffer = new Qt3DRender::QBuffer;
                    layerData->allLightsConstantBuffer->setObjectName(QLatin1String("all lights constant buffer"));
                    updateLightsBuffer(layerData->allLights, layerData->allLightsConstantBuffer);
                }
                if (!layerData->allLightsParam)
                    layerData->allLightsParam = new Qt3DRender::QParameter;
                layerData->allLightsParam->setName(QLatin1String("cbBufferLights"));
                layerData->allLightsParam->setValue(QVariant::fromValue(layerData->allLightsConstantBuffer));
                params.append(layerData->allLightsParam);

                addShadowSsaoParams(layerData, &params);

                // Do not let the QMaterial own the (potentially not yet
                // parented) QParameters. Parent them to some other QNode. This
                // is important here due to rebuildModelMaterial() where
                // sm.materialComponent may get destroyed afterwards.
                for (Qt3DRender::QParameter *param : params)
                    param->setParent(sm.entity);

                sm.materialComponent = m_matGen->generateMaterial(defaultMaterial, params, layerData->lightNodes, modelData->layer3DS);
                sm.entity->addComponent(sm.materialComponent);
            } else if (sm.resolvedMaterial->type() == Q3DSGraphObject::CustomMaterial) {
                Q3DSCustomMaterialInstance *customMaterial = static_cast<Q3DSCustomMaterialInstance *>(sm.resolvedMaterial);
                QVector<Qt3DRender::QParameter *> params = prepareCustomMaterial(customMaterial, model3DS);
                Q3DSCustomMaterialAttached *custMatData = static_cast<Q3DSCustomMaterialAttached *>(customMaterial->attached());
                custMatData->opacity = modelData->globalOpacity;
                updateCustomMaterial(customMaterial);

                // Here lights are provided in two separate buffers.
                if (!layerData->nonAreaLightsConstantBuffer) {
                    layerData->nonAreaLightsConstantBuffer = new Qt3DRender::QBuffer;
                    layerData->nonAreaLightsConstantBuffer->setObjectName(QLatin1String("non-area lights constant buffer"));
                    updateLightsBuffer(layerData->nonAreaLights, layerData->nonAreaLightsConstantBuffer);
                }
                if (!layerData->nonAreaLightsParam)
                    layerData->nonAreaLightsParam = new Qt3DRender::QParameter;
                layerData->nonAreaLightsParam->setName(QLatin1String("cbBufferLights")); // i.e. this cannot be combined with allLightsParam
                layerData->nonAreaLightsParam->setValue(QVariant::fromValue(layerData->nonAreaLightsConstantBuffer));
                params.append(layerData->nonAreaLightsParam);

                if (!layerData->areaLightsConstantBuffer) {
                    layerData->areaLightsConstantBuffer = new Qt3DRender::QBuffer;
                    layerData->areaLightsConstantBuffer->setObjectName(QLatin1String("area lights constant buffer"));
                    updateLightsBuffer(layerData->areaLights, layerData->areaLightsConstantBuffer);
                }
                if (!layerData->areaLightsParam)
                    layerData->areaLightsParam = new Qt3DRender::QParameter;
                layerData->areaLightsParam->setName(QLatin1String("cbBufferAreaLights"));
                layerData->areaLightsParam->setValue(QVariant::fromValue(layerData->areaLightsConstantBuffer));
                params.append(layerData->areaLightsParam);

                addShadowSsaoParams(layerData, &params);

                // Like for the default material, be careful with the parent.
                for (Qt3DRender::QParameter *param : params)
                    param->setParent(sm.entity);

                // ### TODO support more than one pass
                auto pass = customMaterial->material()->passes().first();
                sm.materialComponent = m_customMaterialGen->generateMaterial(customMaterial, params, layerData->lightNodes, modelData->layer3DS, pass);
                sm.entity->addComponent(sm.materialComponent);
            }
        }
    }
}

void Q3DSSceneManager::rebuildModelMaterial(Q3DSModelNode *model3DS)
{
    // After the initial phase 2 of scene building, materials will sometimes
    // need to be recreated due to certain property changes (shadow casters, SSAO).

    Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(model3DS->attached());
    if (!modelData)
        return;

    for (Q3DSModelAttached::SubMesh &sm : modelData->subMeshes) {
        if (sm.resolvedMaterial && sm.materialComponent) {
            qCDebug(lcScene, "Rebuilding material for %s (entity %p)", model3DS->id().constData(), sm.entity);
            delete sm.materialComponent;
            sm.materialComponent = nullptr;
        }
    }

    buildModelMaterial(model3DS);
}

void Q3DSSceneManager::retagSubMeshes(Q3DSModelNode *model3DS)
{
    Q3DSModelAttached *data = static_cast<Q3DSModelAttached *>(model3DS->attached());
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(data->layer3DS->attached());

    for (Q3DSModelAttached::SubMesh &sm : data->subMeshes) {
        float opacity = data->globalOpacity;
        bool hasTransparency = false;
        // Note: cannot rely on sm.resolvedMaterial->attached() since it may
        // not yet be created. This is not a problem in practice.
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
            sm.hasTransparency = opacity < 1.0f || hasTransparency;
        } else if (sm.resolvedMaterial->type() == Q3DSGraphObject::CustomMaterial) {
            auto customMaterial = static_cast<Q3DSCustomMaterialInstance *>(sm.resolvedMaterial);
            const Q3DSCustomMaterial *matDesc = customMaterial->material();
            sm.hasTransparency = matDesc->materialHasTransparency() || matDesc->materialHasRefraction();
        }

        Qt3DRender::QLayer *newTag = sm.hasTransparency ? layerData->transparentTag : layerData->opaqueTag;
        if (!sm.entity->components().contains(newTag)) {
            Qt3DRender::QLayer *prevTag = newTag == layerData->transparentTag ? layerData->opaqueTag : layerData->transparentTag;
            sm.entity->removeComponent(prevTag);
            sm.entity->addComponent(newTag);
        }
    }
}

void Q3DSSceneManager::prepareTextureParameters(Q3DSTextureParameters &textureParameters, const QString &name, Q3DSImage *image3DS)
{
    textureParameters.sampler = new Qt3DRender::QParameter;
    textureParameters.sampler->setName(name + QLatin1String("_sampler"));

    textureParameters.offsets = new Qt3DRender::QParameter;
    textureParameters.offsets->setName(name + QLatin1String("_offsets"));

    textureParameters.rotations = new Qt3DRender::QParameter;
    textureParameters.rotations->setName(name + QLatin1String("_rotations"));

    // Prefer QTextureLoader since this is simpler and leads to updated width,
    // height, etc. values on the texture.
    textureParameters.texture = new Qt3DRender::QTextureLoader(m_rootEntity);
    m_profiler->trackNewObject(textureParameters.texture, Q3DSProfiler::TextureLoaderObject,
                               "Texture for image %s", image3DS->id().constData());
}

void Q3DSSceneManager::updateTextureParameters(Q3DSTextureParameters &textureParameters, Q3DSImage *image)
{
    if (!image->subPresentation().isEmpty()) {
        if (textureParameters.subPresId != image->subPresentation()) {
            textureParameters.subPresId = image->subPresentation();
            // won't yet have the subpresentations if this is still during the building of the main one
            if (m_subPresentations.isEmpty()) {
                textureParameters.sampler->setValue(QVariant::fromValue(dummyTexture()));
                m_subPresImages.append(qMakePair(textureParameters.sampler, image));
            } else {
                setImageTextureFromSubPresentation(textureParameters.sampler, image);
            }
        }
    } else if (!image->sourcePath().isEmpty()) {
        textureParameters.texture->setSource(QUrl::fromLocalFile(image->sourcePath()));
        textureParameters.sampler->setValue(QVariant::fromValue(textureParameters.texture));
    } else {
        textureParameters.sampler->setValue(QVariant::fromValue(dummyTexture()));
    }

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

    Qt3DRender::QAbstractTexture *texture = textureParameters.sampler->value().value<Qt3DRender::QAbstractTexture *>();
    Q_ASSERT(texture);
    texture->setGenerateMipMaps(true);
    texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    texture->setMinificationFilter(Qt3DRender::QAbstractTexture::LinearMipMapLinear);
    texture->setWrapMode(wrapMode);

    const QMatrix4x4 &textureTransform = image->textureTransform();
    const float *m = textureTransform.constData();

    QVector3D offsets(m[12], m[13], 0.0f); // ### z = isPremultiplied?
    textureParameters.offsets->setValue(offsets);

    QVector4D rotations(m[0], m[4], m[1], m[5]);
    textureParameters.rotations->setValue(rotations);
}

void Q3DSSceneManager::setImageTextureFromSubPresentation(Qt3DRender::QParameter *sampler, Q3DSImage *image)
{
    auto it = std::find_if(m_subPresentations.cbegin(), m_subPresentations.cend(),
                           [image](const Q3DSSubPresentation &sp) { return sp.id == image->subPresentation(); });
    if (it != m_subPresentations.cend()) {
        qCDebug(lcScene, "Directing subpresentation %s to image %s",
                qPrintable(image->subPresentation()), image->id().constData());
        sampler->setValue(QVariant::fromValue(it->colorTex));
    } else {
        qCDebug(lcScene, "Subpresentation %s for image %s not found",
                qPrintable(image->subPresentation()), image->id().constData());
        sampler->setValue(QVariant::fromValue(dummyTexture()));
    }
}

QVector<Qt3DRender::QParameter *> Q3DSSceneManager::prepareDefaultMaterial(Q3DSDefaultMaterial *m, Q3DSModelNode *model3DS)
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

    if (m->diffuseMap()) {
        prepareTextureParameters(data->diffuseMapParams, QLatin1String("diffuseMap"), m->diffuseMap());
        params.append(data->diffuseMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->diffuseMap()->attached())->referencingDefaultMaterials.insert(m);
    }

    if (m->diffuseMap2()) {
        prepareTextureParameters(data->diffuseMap2Params, QLatin1String("diffuseMap2"), m->diffuseMap2());
        params.append(data->diffuseMap2Params.parameters());
        static_cast<Q3DSImageAttached *>(m->diffuseMap2()->attached())->referencingDefaultMaterials.insert(m);
    }

    if (m->diffuseMap3()) {
        prepareTextureParameters(data->diffuseMap3Params, QLatin1String("diffuseMap3"), m->diffuseMap3());
        params.append(data->diffuseMap3Params.parameters());
        static_cast<Q3DSImageAttached *>(m->diffuseMap3()->attached())->referencingDefaultMaterials.insert(m);
    }

    if (m->specularReflection()) {
        prepareTextureParameters(data->specularReflectionParams, QLatin1String("specularreflection"), m->specularReflection());
        params.append(data->specularReflectionParams.parameters());
        static_cast<Q3DSImageAttached *>(m->specularReflection()->attached())->referencingDefaultMaterials.insert(m);
    }

    if (m->specularMap()) {
        prepareTextureParameters(data->specularMapParams, QLatin1String("specularMap"), m->specularMap());
        params.append(data->specularMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->specularMap()->attached())->referencingDefaultMaterials.insert(m);
    }

    if (m->bumpMap()) {
        prepareTextureParameters(data->bumpMapParams, QLatin1String("bumpMap"), m->bumpMap());
        params.append(data->bumpMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->bumpMap()->attached())->referencingDefaultMaterials.insert(m);
    }

    if (m->normalMap()) {
        prepareTextureParameters(data->normalMapParams, QLatin1String("normalMap"), m->normalMap());
        params.append(data->normalMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->normalMap()->attached())->referencingDefaultMaterials.insert(m);
    }

    if (m->displacementmap()) {
        prepareTextureParameters(data->displacementMapParams, QLatin1String("displacementMap"), m->displacementmap());
        params.append(data->displacementMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->displacementmap()->attached())->referencingDefaultMaterials.insert(m);
    }

    if (m->opacityMap()) {
        prepareTextureParameters(data->opacityMapParams, QLatin1String("opacityMap"), m->opacityMap());
        params.append(data->opacityMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->opacityMap()->attached())->referencingDefaultMaterials.insert(m);
    }

    if (m->emissiveMap()) {
        prepareTextureParameters(data->emissiveMapParams, QLatin1String("emissiveMap"), m->emissiveMap());
        params.append(data->emissiveMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->emissiveMap()->attached())->referencingDefaultMaterials.insert(m);
    }

    if (m->emissiveMap2()) {
        prepareTextureParameters(data->emissiveMap2Params, QLatin1String("emissiveMap2"), m->emissiveMap2());
        params.append(data->emissiveMap2Params.parameters());
        static_cast<Q3DSImageAttached *>(m->emissiveMap2()->attached())->referencingDefaultMaterials.insert(m);
    }

    if (m->translucencyMap()) {
        prepareTextureParameters(data->translucencyMapParams, QLatin1String("translucencyMap"), m->translucencyMap());
        params.append(data->translucencyMapParams.parameters());
        static_cast<Q3DSImageAttached *>(m->translucencyMap()->attached())->referencingDefaultMaterials.insert(m);
    }

    return params;
}

void Q3DSSceneManager::updateDefaultMaterial(Q3DSDefaultMaterial *m)
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
                                        emissivePower,
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

typedef std::function<void(const QString &, const QVariant &, const Q3DSMaterial::PropertyElement &)> CustomPropertyCallback;

static void iterateCustomProperties(const QVariantMap *properties,
                                    const QMap<QString, Q3DSMaterial::PropertyElement> &propertiesMeta,
                                    CustomPropertyCallback callback)
{
    for (auto it = properties->cbegin(), itEnd = properties->cend(); it != itEnd; ++it) {
        const QString &propName(it.key());
        const QVariant &propValue(it.value());
        const Q3DSMaterial::PropertyElement &propMeta(propertiesMeta[propName]);
        callback(propName, propValue, propMeta);
    }
}

static inline void forAllCustomProperties(Q3DSCustomMaterialInstance *m, CustomPropertyCallback callback)
{
    iterateCustomProperties(m->materialPropertyValues(), m->material()->properties(), callback);
}

static inline void forAllCustomProperties(Q3DSEffectInstance *eff3DS, CustomPropertyCallback callback)
{
    iterateCustomProperties(eff3DS->effectPropertyValues(), eff3DS->effect()->properties(), callback);
}

Qt3DRender::QAbstractTexture *Q3DSSceneManager::createCustomPropertyTexture(const Q3DSCustomPropertyParameter &p)
{
    const QString source = p.inputValue.toString();
    // Using QTextureLoader has the benefit of getting updated width and height
    // values from the texture later on which is pretty important for some
    // effect uniforms for instance.
    Qt3DRender::QTextureLoader *texture = new Qt3DRender::QTextureLoader(m_rootEntity);
    m_profiler->trackNewObject(texture, Q3DSProfiler::TextureLoaderObject,
                               "Custom property texture %s", qPrintable(source));
    if (!source.isEmpty()) {
        qCDebug(lcScene, "Creating custom property texture %s", qPrintable(source));
        texture->setSource(QUrl::fromLocalFile(source));
    }

    switch (p.meta.magFilterType) {
    case Q3DSMaterial::Nearest:
        texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Nearest);
        break;
    default:
        texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
        break;
    }

    switch (p.meta.minFilterType) {
    case Q3DSMaterial::Nearest:
        texture->setMinificationFilter(Qt3DRender::QAbstractTexture::Nearest);
        break;
    case Q3DSMaterial::Linear:
        texture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
        break;
    case Q3DSMaterial::NearestMipmapNearest:
        texture->setMinificationFilter(Qt3DRender::QAbstractTexture::NearestMipMapNearest);
        texture->setGenerateMipMaps(true);
        break;
    case Q3DSMaterial::NearestMipmapLinear:
        texture->setMinificationFilter(Qt3DRender::QAbstractTexture::NearestMipMapLinear);
        texture->setGenerateMipMaps(true);
        break;
    case Q3DSMaterial::LinearMipmapNearest:
        texture->setMinificationFilter(Qt3DRender::QAbstractTexture::LinearMipMapNearest);
        texture->setGenerateMipMaps(true);
        break;
    default:
        texture->setMinificationFilter(Qt3DRender::QAbstractTexture::LinearMipMapLinear);
        texture->setGenerateMipMaps(true);
        break;

    }

    Qt3DRender::QTextureWrapMode wrapMode;
    switch (p.meta.clampType) {
    case Q3DSMaterial::Repeat:
        wrapMode.setX(Qt3DRender::QTextureWrapMode::Repeat);
        wrapMode.setY(Qt3DRender::QTextureWrapMode::Repeat);
        break;
    default:
        wrapMode.setX(Qt3DRender::QTextureWrapMode::ClampToEdge);
        wrapMode.setY(Qt3DRender::QTextureWrapMode::ClampToEdge);
        break;
    }
    texture->setWrapMode(wrapMode);

    return texture;
}

QVector<Qt3DRender::QParameter *> Q3DSSceneManager::prepareCustomMaterial(Q3DSCustomMaterialInstance *m, Q3DSModelNode *model3DS)
{
    if (!m->attached())
        m->setAttached(new Q3DSCustomMaterialAttached);

    Q3DSCustomMaterialAttached *data = static_cast<Q3DSCustomMaterialAttached *>(m->attached());
    Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(model3DS->attached());
    data->entity = modelData->entity;
    data->model3DS = model3DS;

    // Generate QParameters
    QVector<Qt3DRender::QParameter *> paramList;
    forAllCustomProperties(m, [&paramList, data](const QString &propKey, const QVariant &, const Q3DSMaterial::PropertyElement &propMeta) {
        QVariant v(0); // initial value is something dummy, ignore propValue for now
        Qt3DRender::QParameter *param = new Qt3DRender::QParameter;
        param->setName(propKey);
        param->setValue(v);
        paramList.append(param);
        data->params.insert(propKey, Q3DSCustomPropertyParameter(param, v, propMeta));
    });

    return paramList;
}

void Q3DSSceneManager::updateCustomMaterial(Q3DSCustomMaterialInstance *m)
{
    Q3DSCustomMaterialAttached *data = static_cast<Q3DSCustomMaterialAttached *>(m->attached());

    // set all dynamic property values to the corresponding QParameters
    forAllCustomProperties(m, [data, this](const QString &propKey, const QVariant &propValue, const Q3DSMaterial::PropertyElement &) {
        if (!data->params.contains(propKey)) // we do not currently support new dynamic properties appearing out of nowhere
            return;

        Q3DSCustomPropertyParameter &p(data->params[propKey]);
        if (propValue == p.inputValue)
            return;

        p.inputValue = propValue;

        // Floats, vectors, etc. should be good already. Other types need
        // mapping, for instance Texture is only a filename string at this
        // point whereas we need a proper Qt 3D texture.
        switch (p.meta.type) {
        case Q3DS::Texture:
            p.param->setValue(QVariant::fromValue(createCustomPropertyTexture(p)));
            break;

            // Buffer, Image2D, etc. are not used for custom materials

        default:
            p.param->setValue(p.inputValue);
            break;
        }
    });
}

void Q3DSSceneManager::buildEffect(Q3DSEffectInstance *eff3DS, Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q3DSEffectAttached *effData = new Q3DSEffectAttached;
    effData->entity = layerData->entity;
    effData->layer3DS = layer3DS;
    eff3DS->setAttached(effData);
    layerData->effectData.effects.append(eff3DS);

    if (!layerData->effLayerTexture) {
        const QSize sz = safeLayerPixelSize(layerData);
        layerData->effLayerTexture = newColorBuffer(sz, layerData->msaaSampleCount);
        m_profiler->trackNewObject(layerData->effLayerTexture, Q3DSProfiler::Texture2DObject,
                                   "Effect buffer for layer %s", layer3DS->id().constData());
        layerData->sizeManagedTextures.append(layerData->effLayerTexture);
    }
}

static inline void setTextureInfoUniform(Qt3DRender::QParameter *param, Qt3DRender::QAbstractTexture *texture)
{
    param->setValue(QVector4D(texture->width(), texture->height(), 0, 0)); // ### 3rd value is isPremultiplied, is that correct?
}

static inline Qt3DRender::QParameter *makePropertyUniform(const QString &name, const QString &value, const Q3DSMaterial::PropertyElement &propMeta)
{
    QScopedPointer<Qt3DRender::QParameter> param(new Qt3DRender::QParameter);
    param->setName(name);

    switch (propMeta.type) {
    case Q3DS::Float:
    {
        float v;
        if (Q3DS::convertToFloat(&value, &v)) {
            param->setValue(v);
            return param.take();
        } else {
            qWarning("Invalid value %s for custom property %s", qPrintable(value), qPrintable(name));
        }
    }
        break;
    case Q3DS::Long:
    {
        int v;
        if (Q3DS::convertToInt(&value, &v)) {
            param->setValue(v);
            return param.take();
        } else {
            qWarning("Invalid value %s for custom property %s", qPrintable(value), qPrintable(name));
        }
    }
        break;
    case Q3DS::Float2:
    {
        QVector2D v;
        if (Q3DS::convertToVector2D(&value, &v)) {
            param->setValue(v);
            return param.take();
        } else {
            qWarning("Invalid value %s for custom property %s", qPrintable(value), qPrintable(name));
        }
    }
        break;
    case Q3DS::Vector:
    case Q3DS::Scale:
    case Q3DS::Rotation:
    case Q3DS::Color:
    {
        QVector3D v;
        if (Q3DS::convertToVector3D(&value, &v)) {
            param->setValue(v);
            return param.take();
        } else {
            qWarning("Invalid value %s for custom property %s", qPrintable(value), qPrintable(name));
        }
    }
        break;
    default:
        qWarning("Unknown uniform mapping for custom property %s with type %d", qPrintable(name), propMeta.type);
        break;
    }

    return nullptr;
}

static inline Qt3DRender::QStencilTestArguments::StencilFunction convertToQt3DStencilFunc(Q3DSMaterial::BoolOp func)
{
    switch (func) {
    case Q3DSMaterial::Never:
        return Qt3DRender::QStencilTestArguments::Never;
    case Q3DSMaterial::AlwaysTrue:
        return Qt3DRender::QStencilTestArguments::Always;
    case Q3DSMaterial::Less:
        return Qt3DRender::QStencilTestArguments::Less;
    case Q3DSMaterial::LessThanOrEqual:
        return Qt3DRender::QStencilTestArguments::LessOrEqual;
    case Q3DSMaterial::Equal:
        return Qt3DRender::QStencilTestArguments::Equal;
    case Q3DSMaterial::GreaterThanOrEqual:
        return Qt3DRender::QStencilTestArguments::GreaterOrEqual;
    case Q3DSMaterial::Greater:
        return Qt3DRender::QStencilTestArguments::Greater;
    case Q3DSMaterial::NotEqual:
        return Qt3DRender::QStencilTestArguments::NotEqual;
    default:
        return Qt3DRender::QStencilTestArguments::Never;
    }
}

static inline Qt3DRender::QStencilOperationArguments::Operation convertToQt3DStencilOp(Q3DSMaterial::StencilOp op)
{
    switch (op) {
    case Q3DSMaterial::Keep:
        return Qt3DRender::QStencilOperationArguments::Keep;
    case Q3DSMaterial::Zero:
        return Qt3DRender::QStencilOperationArguments::Zero;
    case Q3DSMaterial::Replace:
        return Qt3DRender::QStencilOperationArguments::Replace;
    case Q3DSMaterial::Increment:
        return Qt3DRender::QStencilOperationArguments::Increment;
    case Q3DSMaterial::IncrementWrap:
        return Qt3DRender::QStencilOperationArguments::IncrementWrap;
    case Q3DSMaterial::Decrement:
        return Qt3DRender::QStencilOperationArguments::Decrement;
    case Q3DSMaterial::DecrementWrap:
        return Qt3DRender::QStencilOperationArguments::DecrementWrap;
    case Q3DSMaterial::Invert:
        return Qt3DRender::QStencilOperationArguments::Invert;
    default:
        return Qt3DRender::QStencilOperationArguments::Keep;
    }
}

static inline void setupStencilTest(Qt3DRender::QStencilTest *stencilTest, const Q3DSMaterial::PassCommand &cmd)
{
    auto d = cmd.data();
    for (auto s : { stencilTest->front(), stencilTest->back() }) {
        s->setComparisonMask(d->mask);
        s->setReferenceValue(d->stencilValue);
        s->setStencilFunction(convertToQt3DStencilFunc(d->stencilFunction));
    }
}

void Q3DSSceneManager::finalizeEffects(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    if (layerData->effectData.effects.isEmpty())
        return;

    for (Q3DSEffectInstance *eff3DS : layerData->effectData.effects) {
        Q3DSEffectAttached *effData = static_cast<Q3DSEffectAttached *>(eff3DS->attached());
        qCDebug(lcScene, "Applying post-processing effect %s to layer %s",
                eff3DS->id().constData(), layer3DS->id().constData());

        // Set up textures for Buffers
        createEffectBuffers(eff3DS);

        QVector<Qt3DRender::QParameter *> commonParamList;

        // Create QParameters for built-in uniforms (see effect.glsllib).
        // Texture0 and friends depend on the input so those are per pass.
        effData->appFrameParam = new Qt3DRender::QParameter;
        effData->appFrameParam->setName(QLatin1String("AppFrame"));
        commonParamList.append(effData->appFrameParam);

        effData->fpsParam = new Qt3DRender::QParameter;
        effData->fpsParam->setName(QLatin1String("FPS"));
        commonParamList.append(effData->fpsParam);

        effData->cameraClipRangeParam = new Qt3DRender::QParameter;
        effData->cameraClipRangeParam->setName(QLatin1String("CameraClipRange"));
        commonParamList.append(effData->cameraClipRangeParam);

        // Create QParameters for custom properties.
        forAllCustomProperties(eff3DS, [&commonParamList, effData](const QString &propKey, const QVariant &propValue, const Q3DSMaterial::PropertyElement &propMeta) {
            // textures with no filename are effectively samplers (to be used in a BufferInput f.ex.)
            if (propMeta.type == Q3DS::Texture && propValue.toString().isEmpty())
                return;

            Qt3DRender::QParameter *param = new Qt3DRender::QParameter;
            param->setName(propKey);
            commonParamList.append(param);
            Qt3DRender::QParameter *infoParam = nullptr;
            Qt3DRender::QParameter *flagParam = nullptr;
            // textures do not just get a sampler uniform but also an additional vec4
            if (propMeta.type == Q3DS::Texture) {
                infoParam = new Qt3DRender::QParameter;
                infoParam->setName(propKey + QLatin1String("Info"));
                commonParamList.append(infoParam);
                flagParam = new Qt3DRender::QParameter;
                flagParam->setName(QLatin1String("flag") + propKey);
                flagParam->setValue(0); // will change to 1 when loading something
                commonParamList.append(flagParam);
            }
            Q3DSCustomPropertyParameter pp(param, QVariant(), propMeta);
            pp.texInfoParam = infoParam;
            pp.texFlagParam = flagParam;
            effData->params.insert(propKey, pp);
        });

        // Set initial QParameter (uniform) values.
        updateEffect(eff3DS);

        const Q3DSEffect *effDesc = eff3DS->effect();
        const QMap<QString, Q3DSMaterial::Shader> &shaderPrograms = effDesc->shaders();
        const QMap<QString, Q3DSMaterial::PropertyElement> &propMeta = effDesc->properties();

        // Each pass leads to creating a new framegraph subtree parented to effectRoot.
        auto passes = effDesc->passes();
        const bool implicitPass = passes.isEmpty();
        const int passCount = implicitPass ? 1 : passes.count();
        for (int passIdx = 0; passIdx < passCount; ++passIdx) {
            Q3DSMaterial::Pass pass;
            if (!implicitPass) {
                pass = passes[passIdx];
                if (!shaderPrograms.contains(pass.shaderName)) {
                    qWarning("Effect %s: Unknown shader program %s; pass ignored",
                             eff3DS->id().constData(), qPrintable(pass.shaderName));
                    continue;
                }
                qCDebug(lcScene, "  Registered effect pass with shader program %s input %s output %s %d extra commands",
                        qPrintable(pass.shaderName), qPrintable(pass.input), qPrintable(pass.output), pass.commands.count());
            } else {
                // Not having any passes is valid too. This should use the first vertex/fragment shader (no name matching).
                if (shaderPrograms.isEmpty()) {
                    qWarning("Effect %s: No shader program; pass ignored", eff3DS->id().constData());
                    continue;
                }
                // Leave pass.input and output as the default [source] and [dest].
                qCDebug(lcScene, "  Registered implicit effect pass with input %s output %s",
                        qPrintable(pass.input), qPrintable(pass.output));
            }

            QVector<Qt3DRender::QParameter *> paramList;
            paramList << commonParamList;

            Qt3DRender::QBlendEquation *blendFunc = nullptr;
            Qt3DRender::QBlendEquationArguments *blendArgs = nullptr;
            Qt3DRender::QStencilTest *stencilTest = nullptr;
            Qt3DRender::QStencilOperation *stencilOp = nullptr;
            bool depthNeedsClear = false;
            bool stencilNeedsClear = false;
            QString depthStencilBufferName;

            for (const Q3DSMaterial::PassCommand &cmd : pass.commands) {
                switch (cmd.type()) {
                case Q3DSMaterial::PassCommand::BufferInputType:
                {
                    bool valid = true;
                    const QString bufferName = cmd.data()->value;
                    if (bufferName == QStringLiteral("[source]")) {
                        Qt3DRender::QParameter *texParam = new Qt3DRender::QParameter;
                        texParam->setName(cmd.data()->param);
                        texParam->setValue(QVariant::fromValue(layerData->layerTexture));
                        paramList.append(texParam);
                        Qt3DRender::QParameter *texInfoParam = new Qt3DRender::QParameter;
                        texInfoParam->setName(cmd.data()->param + QLatin1String("Info"));
                        setTextureInfoUniform(texInfoParam, layerData->layerTexture);
                        effData->sourceDepTextureInfoParams.append(texInfoParam);
                        paramList.append(texInfoParam);
                    } else if (effData->textureBuffers.contains(bufferName)) {
                        Q3DSEffectAttached::TextureBuffer &tb(effData->textureBuffers[bufferName]);
                        Qt3DRender::QParameter *texParam = new Qt3DRender::QParameter;
                        texParam->setName(cmd.data()->param);
                        texParam->setValue(QVariant::fromValue(tb.texture));
                        paramList.append(texParam);
                        Qt3DRender::QParameter *texInfoParam = new Qt3DRender::QParameter;
                        texInfoParam->setName(cmd.data()->param + QLatin1String("Info"));
                        setTextureInfoUniform(texInfoParam, tb.texture);
                        tb.textureInfoParams.append(texInfoParam);
                        paramList.append(texInfoParam);
                    } else {
                        qWarning("Effect %s: Unknown buffer %s", eff3DS->id().constData(), qPrintable(bufferName));
                        valid = false;
                    }
                    Qt3DRender::QParameter *texFlagParam = new Qt3DRender::QParameter;
                    texFlagParam->setName(QLatin1String("flag") + cmd.data()->param);
                    texFlagParam->setValue(valid ? 1 : 0);
                    paramList.append(texFlagParam);
                }
                    break;

                case Q3DSMaterial::PassCommand::DepthInputType:
                {
                    setDepthTextureEnabled(layer3DS, true);
                    // param cannot be anything else but a buffer name (with no
                    // source, hence being mapped to a plain sampler).
                    const QString samplerName = cmd.data()->param;
                    const bool valid = propMeta.contains(samplerName);
                    if (valid) {
                        Qt3DRender::QParameter *texParam = new Qt3DRender::QParameter;
                        texParam->setName(samplerName);
                        texParam->setValue(QVariant::fromValue(layerData->depthTextureData.depthTexture));
                        paramList.append(texParam);
                        // Have the usual Info and flag uniforms.
                        Qt3DRender::QParameter *texInfoParam = new Qt3DRender::QParameter;
                        texInfoParam->setName(samplerName + QLatin1String("Info"));
                        // Can conveniently use the layer texture since the
                        // sizes must match. This is very handy esp. with
                        // sourceDepTextureInfoParams since we get size updates
                        // via the same code path.
                        setTextureInfoUniform(texInfoParam, layerData->layerTexture);
                        effData->sourceDepTextureInfoParams.append(texInfoParam);
                        paramList.append(texInfoParam);
                    } else {
                        qWarning("Effect %s: Unknown depth texture sampler %s",
                                 eff3DS->id().constData(), qPrintable(samplerName));
                    }
                    Qt3DRender::QParameter *texFlagParam = new Qt3DRender::QParameter;
                    texFlagParam->setName(QLatin1String("flag") + samplerName);
                    texFlagParam->setValue(valid ? 1 : 0);
                    paramList.append(texFlagParam);
                }
                    break;

                case Q3DSMaterial::PassCommand::SetParamType:
                {
                    auto cmdData = cmd.data();
                    if (propMeta.contains(cmdData->name)) {
                        const Q3DSMaterial::PropertyElement &propDesc(propMeta.value(cmdData->name));
                        Qt3DRender::QParameter *param = makePropertyUniform(cmdData->name, cmdData->value, propDesc);
                        if (param)
                            paramList.append(param);
                    } else {
                        qWarning("Effect %s: SetParam for unknown property %s",
                                 eff3DS->id().constData(), qPrintable(cmd.data()->name));
                    }
                }
                    break;

                case Q3DSMaterial::PassCommand::BlendingType:
                {
                    if (!blendFunc)
                        blendFunc = new Qt3DRender::QBlendEquation;
                    blendFunc->setBlendFunction(Qt3DRender::QBlendEquation::Add);
                    if (!blendArgs)
                        blendArgs = new Qt3DRender::QBlendEquationArguments;
                    switch (cmd.data()->blendSource) {
                    case Q3DSMaterial::SrcAlpha:
                        blendArgs->setSourceRgba(Qt3DRender::QBlendEquationArguments::SourceAlpha);
                        break;
                    case Q3DSMaterial::OneMinusSrcAlpha:
                        blendArgs->setSourceRgba(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
                        break;
                    case Q3DSMaterial::One:
                        blendArgs->setSourceRgba(Qt3DRender::QBlendEquationArguments::One);
                        break;
                    default:
                        break;
                    }
                    switch (cmd.data()->blendDestination) {
                    case Q3DSMaterial::SrcAlpha:
                        blendArgs->setDestinationRgba(Qt3DRender::QBlendEquationArguments::SourceAlpha);
                        break;
                    case Q3DSMaterial::OneMinusSrcAlpha:
                        blendArgs->setDestinationRgba(Qt3DRender::QBlendEquationArguments::OneMinusSourceAlpha);
                        break;
                    case Q3DSMaterial::One:
                        blendArgs->setDestinationRgba(Qt3DRender::QBlendEquationArguments::One);
                        break;
                    default:
                        break;
                    }
                }
                    break;

                case Q3DSMaterial::PassCommand::RenderStateType:
                {
                    if (cmd.data()->name == QStringLiteral("Stencil")) {
                        bool enabled = false;
                        if (Q3DS::convertToBool(&cmd.data()->value, &enabled)) {
                            if (enabled) {
                                if (!stencilTest)
                                    stencilTest = new Qt3DRender::QStencilTest;
                                Q3DSMaterial::PassCommand dummy; // with defaults
                                setupStencilTest(stencilTest, dummy);
                            } else {
                                delete stencilTest;
                                stencilTest = nullptr;
                                delete stencilOp;
                                stencilOp = nullptr;
                            }
                        }
                    } else {
                        qWarning("Effect %s: Unsupported render state %s", eff3DS->id().constData(), qPrintable(cmd.data()->name));
                    }
                }
                    break;

                case Q3DSMaterial::PassCommand::DepthStencilType:
                {
                    auto d = cmd.data();
                    depthStencilBufferName = d->bufferName;
                    depthNeedsClear = d->flags.testFlag(Q3DSMaterial::ClearDepth);
                    stencilNeedsClear = d->flags.testFlag(Q3DSMaterial::ClearStencil);

                    if (!stencilTest)
                        stencilTest = new Qt3DRender::QStencilTest;

                    setupStencilTest(stencilTest, cmd);

                    if (!stencilOp)
                        stencilOp = new Qt3DRender::QStencilOperation;

                    for (auto s : { stencilOp->front(), stencilOp->back() }) {
                        s->setStencilTestFailureOperation(convertToQt3DStencilOp(d->stencilFail));
                        s->setDepthTestFailureOperation(convertToQt3DStencilOp(d->depthFail));
                        s->setAllTestsPassOperation(convertToQt3DStencilOp(d->depthPass));
                    }
                }
                    break;

                default:
                    qWarning("Effect %s: Unhandled command %d", eff3DS->id().constData(), cmd.type());
                    break;
                }
            }

            Qt3DRender::QAbstractTexture *passInput = nullptr;
            if (pass.input == QStringLiteral("[source]")) {
                passInput = layerData->layerTexture;
            } else {
                if (effData->textureBuffers.contains(pass.input)) {
                    passInput = effData->textureBuffers.value(pass.input).texture;
                } else {
                    qWarning("Effect %s: Unknown input buffer %s; pass ignored",
                             eff3DS->id().constData(), qPrintable(pass.input));
                    continue;
                }
            }

            bool outputNeedsClear = false;
            Qt3DRender::QAbstractTexture *passOutput = nullptr;
            if (pass.output == QStringLiteral("[dest]")) {
                passOutput = layerData->effLayerTexture;
                outputNeedsClear = true;
            } else {
                if (effData->textureBuffers.contains(pass.output)) {
                    auto tb = effData->textureBuffers.value(pass.output);
                    passOutput = tb.texture;
                    outputNeedsClear = tb.hasSceneLifetime;
                } else {
                    qWarning("Effect %s: Unknown output buffer %s; pass ignored",
                             eff3DS->id().constData(), qPrintable(pass.output));
                    continue;
                }
            }

            Qt3DRender::QParameter *texture0Param = new Qt3DRender::QParameter;
            texture0Param->setName(QLatin1String("Texture0"));
            texture0Param->setValue(QVariant::fromValue(passInput));
            paramList.append(texture0Param);

            Qt3DRender::QParameter *texture0InfoParam = new Qt3DRender::QParameter;
            texture0InfoParam->setName(QLatin1String("Texture0Info"));
            paramList.append(texture0InfoParam);

            Qt3DRender::QParameter *texture0FlagParam = new Qt3DRender::QParameter;
            texture0FlagParam->setName(QLatin1String("Texture0Flags")); // this is not a mistake, it's not flagTexture0 but Texture0Flags. go figure.
            texture0FlagParam->setValue(1);
            paramList.append(texture0FlagParam);

            Qt3DRender::QParameter *fragColorAlphaParam = new Qt3DRender::QParameter;
            fragColorAlphaParam->setName(QLatin1String("FragColorAlphaSettings"));
            fragColorAlphaParam->setValue(QVector2D(1.0f, 0.0f));
            paramList.append(fragColorAlphaParam);

            Qt3DRender::QParameter *destSizeParam = new Qt3DRender::QParameter;
            destSizeParam->setName(QLatin1String("DestSize"));
            paramList.append(destSizeParam);

            Q3DSEffectAttached::PassData pd;
            pd.passInput = passInput;
            pd.texture0InfoParam = texture0InfoParam;
            pd.passOutput = passOutput;
            pd.destSizeParam = destSizeParam;
            effData->passData.append(pd);

            const Q3DSMaterial::Shader &shaderProgram = !implicitPass ? shaderPrograms.value(pass.shaderName) : shaderPrograms.first();
            const QString decoratedShaderName = QString::fromUtf8(eff3DS->id()) + QLatin1Char('_') + pass.shaderName;
            const QString decoratedVertexShader = effDesc->addPropertyUniforms(shaderProgram.vertexShader);
            const QString decoratedFragmentShader = effDesc->addPropertyUniforms(shaderProgram.fragmentShader);
            Qt3DRender::QShaderProgram *prog = Q3DSShaderManager::instance().getEffectShader(m_rootEntity,
                                                                                             decoratedShaderName,
                                                                                             decoratedVertexShader,
                                                                                             decoratedFragmentShader);

            effData->quadEntityTag = new Qt3DRender::QLayer;

            FsQuadParams quadInfo;
            quadInfo.parentEntity = m_rootEntity;
            quadInfo.passNames << QLatin1String("eff");
            quadInfo.passProgs << prog;
            quadInfo.tag = effData->quadEntityTag;
            quadInfo.params = paramList;
            if (blendFunc)
                quadInfo.renderStates << blendFunc;
            if (blendArgs)
                quadInfo.renderStates << blendArgs;
            if (stencilTest)
                quadInfo.renderStates << stencilTest;
            if (stencilOp)
                quadInfo.renderStates << stencilOp;

            buildFsQuad(quadInfo);

            Qt3DRender::QRenderTargetSelector *rtSel = new Qt3DRender::QRenderTargetSelector(layerData->effectData.effectRoot);
            Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
            m_profiler->trackNewObject(rt, Q3DSProfiler::RenderTargetObject, "RT for effect %s pass %d",
                                       eff3DS->id().constData(), passIdx + 1);
            Qt3DRender::QRenderTargetOutput *color = new Qt3DRender::QRenderTargetOutput;
            color->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
            color->setTexture(passOutput);
            rt->addOutput(color);
            if (!depthStencilBufferName.isEmpty()) {
                if (effData->textureBuffers.contains(depthStencilBufferName)) {
                    qCDebug(lcScene, "    Binding buffer %s for depth-stencil", qPrintable(depthStencilBufferName));
                    Qt3DRender::QRenderTargetOutput *ds = new Qt3DRender::QRenderTargetOutput;
                    ds->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::DepthStencil);
                    ds->setTexture(effData->textureBuffers[depthStencilBufferName].texture);
                    rt->addOutput(ds);
                } else {
                    qWarning("Effect %s: Unknown depth-stencil buffer %s",
                             eff3DS->id().constData(), qPrintable(depthStencilBufferName));
                }
            }
            rtSel->setTarget(rt);

            Qt3DRender::QClearBuffers *clearBuf = new Qt3DRender::QClearBuffers(rtSel);
            clearBuf->setClearColor(Qt::transparent);
            int buffersToClear = Qt3DRender::QClearBuffers::None;
            if (outputNeedsClear)
                buffersToClear |= Qt3DRender::QClearBuffers::ColorBuffer;
            if (depthNeedsClear)
                buffersToClear |= Qt3DRender::QClearBuffers::DepthBuffer;
            if (stencilNeedsClear)
                buffersToClear |= Qt3DRender::QClearBuffers::StencilBuffer;
            clearBuf->setBuffers(Qt3DRender::QClearBuffers::BufferType(buffersToClear));

            Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(clearBuf);
            layerFilter->addLayer(effData->quadEntityTag);
        }

        // set initial values for per-frame uniforms
        // (or the ones that depend on pass input/output size and are easier to handle this way)
        updateEffectForNextFrame(eff3DS, 0);
    }

    // The layer compositor must use the output of the effect passes from now on.
    layerData->compositorSourceParam->setValue(QVariant::fromValue(layerData->effLayerTexture));
}

void Q3DSSceneManager::setupEffectTextureBuffer(Q3DSEffectAttached::TextureBuffer *tb,
                                                const Q3DSMaterial::PassBuffer &bufDesc,
                                                Q3DSLayerNode *layer3DS)
{
    Qt3DRender::QAbstractTexture *texture = new Qt3DRender::QTexture2D(m_rootEntity);
    m_profiler->trackNewObject(texture, Q3DSProfiler::Texture2DObject,
                               "Effect buffer %s", qPrintable(bufDesc.name()));
    tb->texture = texture;

    switch (bufDesc.filter()) {
    case Q3DSMaterial::Nearest:
        texture->setMinificationFilter(Qt3DRender::QAbstractTexture::Nearest);
        texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Nearest);
        break;
    default:
        texture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
        texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
        break;
    }

    Qt3DRender::QTextureWrapMode wrapMode;
    switch (bufDesc.wrap()) {
    case Q3DSMaterial::Repeat:
        wrapMode.setX(Qt3DRender::QTextureWrapMode::Repeat);
        wrapMode.setY(Qt3DRender::QTextureWrapMode::Repeat);
        break;
    default:
        wrapMode.setX(Qt3DRender::QTextureWrapMode::ClampToEdge);
        wrapMode.setY(Qt3DRender::QTextureWrapMode::ClampToEdge);
        break;
    }
    texture->setWrapMode(wrapMode);

    switch (bufDesc.textureFormat()) {
    case Q3DSMaterial::Depth24Stencil8:
        texture->setFormat(Qt3DRender::QAbstractTexture::D24S8);
        break;
    case Q3DSMaterial::RGB8:
        texture->setFormat(Qt3DRender::QAbstractTexture::RGB8_UNorm);
        break;
    case Q3DSMaterial::Alpha8:
        texture->setFormat(Qt3DRender::QAbstractTexture::AlphaFormat);
        break;
    case Q3DSMaterial::Luminance8:
        texture->setFormat(Qt3DRender::QAbstractTexture::LuminanceFormat);
        break;
    case Q3DSMaterial::LuminanceAlpha8:
        texture->setFormat(Qt3DRender::QAbstractTexture::LuminanceAlphaFormat);
        break;
    case Q3DSMaterial::RG8:
        texture->setFormat(Qt3DRender::QAbstractTexture::RG8_UNorm);
        break;
    case Q3DSMaterial::RGB565:
        texture->setFormat(Qt3DRender::QAbstractTexture::R5G6B5);
        break;
    case Q3DSMaterial::RGBA5551:
        texture->setFormat(Qt3DRender::QAbstractTexture::RGB5A1);
        break;
    case Q3DSMaterial::RGBA16F:
        texture->setFormat(Qt3DRender::QAbstractTexture::RGBA16F);
        break;
    case Q3DSMaterial::RG16F:
        texture->setFormat(Qt3DRender::QAbstractTexture::RG16F);
        break;
    case Q3DSMaterial::RGBA32F:
        texture->setFormat(Qt3DRender::QAbstractTexture::RGBA32F);
        break;
    case Q3DSMaterial::RG32F:
        texture->setFormat(Qt3DRender::QAbstractTexture::RG32F);
        break;
    default: // incl. Unknown that is set for "source"
        texture->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
        break;
    }

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    float sizeMultiplier = bufDesc.size();
    auto sizeCalc = [texture, layerData, sizeMultiplier](Q3DSLayerNode*) {
        const QSize sz = safeLayerPixelSize(layerData);
        texture->setWidth(int(sz.width() * sizeMultiplier));
        texture->setHeight(int(sz.height() * sizeMultiplier));
    };
    prepareSizeDependentTexture(texture, layer3DS, sizeCalc, Q3DSLayerAttached::SizeManagedTexture::CustomSizeCalculation);
    sizeCalc(layer3DS);
}

void Q3DSSceneManager::createEffectBuffers(Q3DSEffectInstance *eff3DS)
{
    Q3DSEffectAttached *effData = static_cast<Q3DSEffectAttached *>(eff3DS->attached());
    const Q3DSEffect *effDesc = eff3DS->effect();
    const QMap<QString, Q3DSMaterial::PassBuffer> &bufDescs = effDesc->buffers();
    for (const Q3DSMaterial::PassBuffer &bufDesc : bufDescs) {
        if (bufDesc.passBufferType() == Q3DSMaterial::BufferType) {
            if (effData->textureBuffers.contains(bufDesc.name())) {
                qWarning("Texture Buffer name %s reused", qPrintable(bufDesc.name()));
                continue;
            }
            Q3DSEffectAttached::TextureBuffer tb;
            tb.hasSceneLifetime = bufDesc.hasSceneLifetime();
            setupEffectTextureBuffer(&tb, bufDesc, effData->layer3DS);
            effData->textureBuffers.insert(bufDesc.name(), tb);
        } else {
            qWarning("Effect %s: Unsupported buffer type", eff3DS->id().constData());
        }
    }
}

// called once on load from finalizeEffect, and then every time an effect property has changed
void Q3DSSceneManager::updateEffect(Q3DSEffectInstance *eff3DS)
{
    Q3DSEffectAttached *effData = static_cast<Q3DSEffectAttached *>(eff3DS->attached());
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(effData->layer3DS->attached());

    QVector2D cameraClipRange(0, 5000);
    if (layerData->cam3DS) {
        cameraClipRange.setX(layerData->cam3DS->clipNear());
        cameraClipRange.setY(layerData->cam3DS->clipFar());
    }
    effData->cameraClipRangeParam->setValue(cameraClipRange);

    forAllCustomProperties(eff3DS, [effData, this](const QString &propKey, const QVariant &propValue, const Q3DSMaterial::PropertyElement &) {
        if (!effData->params.contains(propKey))
            return;

        Q3DSCustomPropertyParameter &p(effData->params[propKey]);
        if (propValue == p.inputValue)
            return;

        p.inputValue = propValue;

        switch (p.meta.type) {
        case Q3DS::Texture:
        {
            Qt3DRender::QAbstractTexture *tex = createCustomPropertyTexture(p);
            p.param->setValue(QVariant::fromValue(tex));
            setTextureInfoUniform(p.texInfoParam, tex);
            p.texFlagParam->setValue(1);
        }
            break;

        // ### others?

        default:
            p.param->setValue(p.inputValue);
            break;
        }
    });
}

// called after each frame
void Q3DSSceneManager::updateEffectForNextFrame(Q3DSEffectInstance *eff3DS, qint64 nextFrameNo)
{
    Q3DSEffectAttached *effData = static_cast<Q3DSEffectAttached *>(eff3DS->attached());
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(effData->layer3DS->attached());

    effData->appFrameParam->setValue(float(nextFrameNo));
    effData->fpsParam->setValue(60.0f); // heh
    for (const auto &pd : effData->passData) {
        pd.texture0InfoParam->setValue(QVector4D(pd.passInput->width(),
                                                 pd.passInput->height(),
                                                 0, // ### isPremultiplied?
                                                 0));
        pd.destSizeParam->setValue(QVector2D(pd.passOutput->width(), pd.passOutput->height()));
    }
    for (const auto &tb : effData->textureBuffers) {
        for (Qt3DRender::QParameter *param : tb.textureInfoParams)
            setTextureInfoUniform(param, tb.texture);
    }
    for (Qt3DRender::QParameter *param : effData->sourceDepTextureInfoParams)
        setTextureInfoUniform(param, layerData->layerTexture);
}

void Q3DSSceneManager::gatherLights(Q3DSGraphObject *root,
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

void Q3DSSceneManager::updateLightsBuffer(const QVector<Q3DSLightSource> &lights, Qt3DRender::QBuffer *uniformBuffer)
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
        lightData[i].m_position = lights[i].positionParam->value().value<QVector4D>();
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
        lightData[i].m_diffuse = lights[i].diffuseParam->value().value<QVector4D>();
        // ambient
        lightData[i].m_ambient = lights[i].ambientParam->value().value<QVector4D>();
        // specular
        lightData[i].m_specular = lights[i].specularParam->value().value<QVector4D>();
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

void Q3DSSceneManager::updateModel(Q3DSModelNode *model3DS)
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
            }
        }
    }
}

// when entering a slide, or when animating a property
void Q3DSSceneManager::handlePropertyChange(Q3DSGraphObject *obj, const QSet<QString> &keys, int changeFlags)
{
    Q_UNUSED(keys);
    Q3DSGraphObjectAttached *data = obj->attached();
    if (!data) // Qt3D stuff not yet built for this object -> nothing to do
        return;

    // 'keys' is not used in general, rely rather on Q3DSPropertyChangeList's
    // pre-baked flags to determine certain special cases. For most other cases
    // it is enough to know that _something_ has changed.
    const Q3DSPropertyChangeList::Flags cf = Q3DSPropertyChangeList::Flags(changeFlags);

    // all actual processing must be deferred to updateSubTreeRecursive()
    switch (obj->type()) {
    case Q3DSGraphObject::Layer:
    {
        data->dirty |= Q3DSGraphObjectAttached::LayerDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Camera:
    {
        data->dirty |= Q3DSGraphObjectAttached::CameraDirty;
        data->changeFlags |= cf;
    }
        break;

    case Q3DSGraphObject::DefaultMaterial:
    {
        data->dirty |= Q3DSGraphObjectAttached::DefaultMaterialDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::CustomMaterial:
    {
        data->dirty |= Q3DSGraphObjectAttached::CustomMaterialDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Effect:
    {
        data->dirty |= Q3DSGraphObjectAttached::EffectDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Image:
    {
        data->dirty |= Q3DSGraphObjectAttached::ImageDirty;
        data->changeFlags |= cf;
    }
        break;

    case Q3DSGraphObject::Group:
    {
        data->dirty |= Q3DSGraphObjectAttached::GroupDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Component:
    {
        data->dirty |= Q3DSGraphObjectAttached::ComponentDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Light:
    {
        data->dirty |= Q3DSGraphObjectAttached::LightDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Model:
    {
        data->dirty |= Q3DSGraphObjectAttached::ModelDirty;
        data->changeFlags |= cf;
    }
        break;
    case Q3DSGraphObject::Text:
    {
        data->dirty |= Q3DSGraphObjectAttached::TextDirty;
        data->changeFlags |= cf;
    }
        break;

    default:
        break;
    }

    // Note the lack of call to updateSubTree(). That happens in a QFrameAction once per frame.
}

void Q3DSSceneManager::updateSubTree(Q3DSGraphObject *obj)
{
    m_layersWithDirtyLights.clear();
    m_pendingDefMatRebuild.clear();

    updateSubTreeRecursive(obj);

    for (Q3DSLayerNode *layer3DS : m_layersWithDirtyLights) {
        Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
        // Attempt to update all buffers, if some do not exist (null) that's fine too.
        updateLightsBuffer(layerData->allLights, layerData->allLightsConstantBuffer);
        updateLightsBuffer(layerData->nonAreaLights, layerData->nonAreaLightsConstantBuffer);
        updateLightsBuffer(layerData->areaLights, layerData->areaLightsConstantBuffer);
        bool smDidChange = false;
        updateShadowMapStatus(layer3DS, &smDidChange);
        if (smDidChange) {
            Q3DSPresentation::forAllModels(layer3DS->firstChild(),
                                           [this](Q3DSModelNode *model3DS) { rebuildModelMaterial(model3DS); },
                                           true); // include hidden ones too
        }
    }

    for (Q3DSDefaultMaterial *mat3DS : m_pendingDefMatRebuild) {
        if (Q3DSDefaultMaterialAttached *matData = static_cast<Q3DSDefaultMaterialAttached *>(mat3DS->attached()))
            rebuildModelMaterial(matData->model3DS);
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

static Q3DSComponentNode *findNextComponentParent(Q3DSComponentNode *component)
{
    auto target = component->parent();
    while (target) {
        Q3DSGraphObject *parent = component->parent();
        if (parent->type() == Q3DSGraphObject::Component)
            return static_cast<Q3DSComponentNode *>(parent);
        target = target->parent();
    }
    return nullptr;
}

bool Q3DSSceneManager::isComponentVisible(Q3DSComponentNode *component)
{
    bool visible = true;
    if (component) {
        auto targetComponent = component;
        do {
            auto parentComponent = findNextComponentParent(targetComponent);
            Q3DSSlide *parentMaster = parentComponent ? parentComponent->masterSlide () : m_masterSlide;
            Q3DSSlide *parentCurrentSlide = parentComponent ? parentComponent->currentSlide() : m_currentSlide;
            if (!parentMaster->objects()->contains(targetComponent) && !parentCurrentSlide->objects()->contains(targetComponent)) {
                visible = false;
                break;
            }
            targetComponent = parentComponent;
        } while (targetComponent);
    }

    return visible;
}

void Q3DSSceneManager::handleSlideChange(Q3DSSlide *prevSlide, Q3DSSlide *currentSlide, Q3DSSlide *masterSlide, Q3DSComponentNode *component)
{
    if (!currentSlide->attached())
        currentSlide->setAttached(new Q3DSSlideAttached);

    // Reset properties like eyeball to the master slide's value.
    auto slideData = static_cast<Q3DSSlideAttached *>(prevSlide->attached());
    for (Q3DSNode *node : qAsConst(slideData->needsMasterRollback)) {
        const Q3DSPropertyChangeList *changeList = node->masterRollbackList();
        if (changeList) {
            qCDebug(lcScene, "Rolling back %d changes to master for %s", changeList->count(), node->id().constData());
            node->applyPropertyChanges(changeList);
            if (isComponentVisible(component)) {
                node->notifyPropertyChanges(changeList);
                updateSubTreeRecursive(node);
            }
        }
    }
    slideData->needsMasterRollback.clear();

    m_presentation->applySlidePropertyChanges(currentSlide);

    updateSlideObjectVisibilities(prevSlide, component);
    updateSlideObjectVisibilities(currentSlide, component);

    updateAnimations(masterSlide, nullptr, currentSlide);
    updateAnimations(currentSlide, prevSlide, currentSlide);
}

void Q3DSSceneManager::prepareNextFrame()
{
    m_wasDirty = false;
    Q3DSPresentation::forAllLayers(m_scene, [this](Q3DSLayerNode *layer3DS) {
        static_cast<Q3DSLayerAttached *>(layer3DS->attached())->wasDirty = false;
    });

    updateSubTree(m_scene);

    qint64 nextFrameNo = m_frameUpdater->frameCounter() + 1;
    Q3DSPresentation::forAllLayers(m_scene, [this, nextFrameNo](Q3DSLayerNode *layer3DS) {
        // Dirty flags now up-to-date -> update progressive AA status
        if (layer3DS->progressiveAA() != Q3DSLayerNode::NoPAA)
            updateProgressiveAA(layer3DS);

        // Post-processing effects have uniforms that need to be updated on every frame.
        Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
        if (layerData) {
            for (Q3DSEffectInstance *eff3DS : qAsConst(layerData->effectData.effects))
                updateEffectForNextFrame(eff3DS, nextFrameNo);
        }
    });
}

// Now to the nightmare of maintaining per-layer dirty flags. The scene-wide
// m_wasDirty flag is simple but ultimately of little value since techniques
// like progressive or temporal AA need to know the status on a per-layer basis.

// For nodes there is a link to the layer, but no such thing for non-node
// objects (e.g. Image, DefaultMaterial), and there really cannot be due to
// references. So e.g. for Image, the correct approach is to go through its
// referencingMaterials set, follow the model3DS link in those materials, and
// take the layer3DS from that.

static void markLayerForObjectDirty(Q3DSGraphObject *obj)
{
    auto findLayerForMat = [](Q3DSGraphObject *obj) {
        Q3DSLayerNode *layer3DS = nullptr;
        Q3DSMaterialAttached *data = static_cast<Q3DSMaterialAttached *>(obj->attached());
        if (data && data->model3DS) {
            Q3DSNodeAttached *ndata = static_cast<Q3DSNodeAttached *>(data->model3DS->attached());
            layer3DS = ndata->layer3DS;
        }
        return layer3DS;
    };
    auto markLayerDirty = [](Q3DSLayerNode *layer3DS) {
        static_cast<Q3DSLayerAttached *>(layer3DS->attached())->wasDirty = true;
    };

    if (obj->type() == Q3DSGraphObject::Layer) {
        markLayerDirty(static_cast<Q3DSLayerNode *>(obj));
    } else if (obj->isNode()) {
        Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(obj->attached());
        if (data && data->layer3DS)
            markLayerDirty(data->layer3DS);
    } else if (obj->type() == Q3DSGraphObject::Image) {
        Q3DSImageAttached *data = static_cast<Q3DSImageAttached *>(obj->attached());
        for (Q3DSDefaultMaterial *mat3DS : qAsConst(data->referencingDefaultMaterials)) {
            Q3DSLayerNode *layer3DS = findLayerForMat(mat3DS);
            if (layer3DS)
                markLayerDirty(layer3DS);
        }
    } else if (obj->type() == Q3DSGraphObject::DefaultMaterial || obj->type() == Q3DSGraphObject::CustomMaterial) {
        Q3DSLayerNode *layer3DS = findLayerForMat(obj);
        if (layer3DS)
            markLayerDirty(layer3DS);
    } else if (obj->type() == Q3DSGraphObject::Effect) {
        Q3DSEffectAttached *data = static_cast<Q3DSEffectAttached *>(obj->attached());
        if (data && data->layer3DS)
            markLayerDirty(data->layer3DS);
    }
}

void Q3DSSceneManager::updateSubTreeRecursive(Q3DSGraphObject *obj)
{
    switch (obj->type()) {
    case Q3DSGraphObject::Group:
        Q_FALLTHROUGH();
    case Q3DSGraphObject::Component:
    {
        // Group and Component inherit all interesting properties from Node
        Q3DSNode *node = static_cast<Q3DSNode *>(obj);
        Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(obj->attached());
        if (data)
            updateNodeFromChangeFlags(node, data->transform, data->changeFlags);
    }
        break;
    case Q3DSGraphObject::Text:
    {
        Q3DSTextNode *text3DS = static_cast<Q3DSTextNode *>(obj);
        Q3DSTextAttached *data = static_cast<Q3DSTextAttached *>(text3DS->attached());
        if (data) {
            updateNodeFromChangeFlags(text3DS, data->transform, data->changeFlags);
            if (data->dirty & (Q3DSGraphObjectAttached::TextDirty | Q3DSGraphObjectAttached::GlobalOpacityDirty)) {
                const bool needsNewImage = data->changeFlags.testFlag(Q3DSPropertyChangeList::TextTextureImageDepChanges);
                updateText(text3DS, needsNewImage);
                m_wasDirty = true;
                markLayerForObjectDirty(text3DS);
            }
        }
    }
        break;
    case Q3DSGraphObject::Light:
    {
        Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(obj);
        Q3DSLightAttached *data = static_cast<Q3DSLightAttached *>(light3DS->attached());
        if (data) {
            updateNodeFromChangeFlags(light3DS, data->transform, data->changeFlags);
            if (data->dirty & (Q3DSGraphObjectAttached::LightDirty | Q3DSGraphObjectAttached::GlobalTransformDirty)) {
                setLightProperties(light3DS);
                if (!data->changeFlags.testFlag(Q3DSPropertyChangeList::EyeballChanges)) // already done if eyeball changed
                    m_layersWithDirtyLights.insert(data->layer3DS);
                m_wasDirty = true;
                markLayerForObjectDirty(light3DS);
            }
        }
    }
        break;
    case Q3DSGraphObject::Model:
    {
        Q3DSModelNode *model3DS = static_cast<Q3DSModelNode *>(obj);
        Q3DSModelAttached *data = static_cast<Q3DSModelAttached *>(model3DS->attached());
        if (data) {
            updateNodeFromChangeFlags(model3DS, data->transform, data->changeFlags);
            if (data->dirty & (Q3DSGraphObjectAttached::ModelDirty | Q3DSGraphObjectAttached::GlobalOpacityDirty)) {
                updateModel(model3DS);
                m_wasDirty = true;
                markLayerForObjectDirty(model3DS);
            }
        }
    }
        break;
    case Q3DSGraphObject::Camera:
    {
        Q3DSCameraNode *cam3DS = static_cast<Q3DSCameraNode *>(obj);
        Q3DSCameraAttached *data = static_cast<Q3DSCameraAttached *>(cam3DS->attached());
        if (data) {
            updateNodeFromChangeFlags(cam3DS, data->transform, data->changeFlags);
            if (data->dirty & Q3DSGraphObjectAttached::CameraDirty) {
                // Change the camera if necessary
                if (data->changeFlags.testFlag(Q3DSPropertyChangeList::EyeballChanges)) {
                    updateLayerCamera(data->layer3DS);
                }
                setCameraProperties(cam3DS, data->changeFlags); // handles both Node- and Camera-level properties
                setLayerCameraSizeProperties(data->layer3DS);
                m_wasDirty = true;
                markLayerForObjectDirty(cam3DS);
            }
        }
    }
        break;
    case Q3DSGraphObject::Layer:
    {
        Q3DSLayerNode *layer3DS = static_cast<Q3DSLayerNode *>(obj);
        Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
        if (data && (data->dirty & Q3DSGraphObjectAttached::LayerDirty)) {
            updateSizesForLayer(layer3DS, data->parentSize);
            setLayerProperties(layer3DS);
            if (data->changeFlags.testFlag(Q3DSPropertyChangeList::AoOrShadowChanges)) {
                bool aoDidChange = false;
                updateSsaoStatus(layer3DS, &aoDidChange);
                if (aoDidChange) {
                    Q3DSPresentation::forAllModels(layer3DS->firstChild(),
                                                   [this](Q3DSModelNode *model3DS) { rebuildModelMaterial(model3DS); },
                    true); // include hidden ones too
                }
            }
            m_wasDirty = true;
            markLayerForObjectDirty(layer3DS);
        }
    }
        break;
    case Q3DSGraphObject::DefaultMaterial:
    {
        Q3DSDefaultMaterial *mat3DS = static_cast<Q3DSDefaultMaterial *>(obj);
        Q3DSDefaultMaterialAttached *data = static_cast<Q3DSDefaultMaterialAttached *>(mat3DS->attached());
        if (data && (data->dirty & Q3DSGraphObjectAttached::DefaultMaterialDirty)) {
            Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(data->model3DS->attached());
            data->opacity = modelData->globalOpacity * mat3DS->opacity();
            updateDefaultMaterial(mat3DS);
            m_wasDirty = true;
            markLayerForObjectDirty(mat3DS);
            if (data->changeFlags.testFlag(Q3DSPropertyChangeList::BlendModeChanges))
                m_pendingDefMatRebuild.insert(mat3DS);
        }
    }
        break;
    case Q3DSGraphObject::CustomMaterial:
    {
        Q3DSCustomMaterialInstance *mat3DS = static_cast<Q3DSCustomMaterialInstance *>(obj);
        Q3DSCustomMaterialAttached *data = static_cast<Q3DSCustomMaterialAttached *>(mat3DS->attached());
        if (data && (data->dirty & Q3DSGraphObjectAttached::CustomMaterialDirty)) {
            Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(data->model3DS->attached());
            data->opacity = modelData->globalOpacity;
            updateCustomMaterial(mat3DS);
            m_wasDirty = true;
            markLayerForObjectDirty(mat3DS);
        }
    }
        break;
    case Q3DSGraphObject::Effect:
    {
        Q3DSEffectInstance *eff3DS = static_cast<Q3DSEffectInstance *>(obj);
        Q3DSEffectAttached *data = static_cast<Q3DSEffectAttached *>(eff3DS->attached());
        if (data && (data->dirty & Q3DSGraphObjectAttached::EffectDirty)) {
            updateEffect(eff3DS);
            m_wasDirty = true;
            markLayerForObjectDirty(eff3DS);
        }
    }
        break;
    case Q3DSGraphObject::Image:
    {
        Q3DSImage *image3DS = static_cast<Q3DSImage *>(obj);
        Q3DSImageAttached *data = static_cast<Q3DSImageAttached *>(image3DS->attached());
        if (data && (data->dirty & Q3DSGraphObjectAttached::ImageDirty)) {
            image3DS->calculateTextureTransform();
            for (Q3DSDefaultMaterial *m : data->referencingDefaultMaterials)
                updateDefaultMaterial(m);
            m_wasDirty = true;
            markLayerForObjectDirty(image3DS);
        }
    }
        break;

    default:
        break;
    }

    Q3DSGraphObjectAttached *data = obj->attached();
    if (data) {
        data->dirty = 0;
        data->changeFlags = 0;
    }

    obj = obj->firstChild();
    while (obj) {
        updateSubTreeRecursive(obj);
        obj = obj->nextSibling();
    }
}

void Q3DSSceneManager::updateNodeFromChangeFlags(Q3DSNode *node, Qt3DCore::QTransform *transform, int changeFlags)
{
    const Q3DSPropertyChangeList::Flags cf = Q3DSPropertyChangeList::Flags(changeFlags);
    if (cf.testFlag(Q3DSPropertyChangeList::NodeTransformChanges)
            || cf.testFlag(Q3DSPropertyChangeList::NodeOpacityChanges))
    {
        setNodeProperties(node, nullptr, transform, NodePropUpdateGlobalsRecursively);
        m_wasDirty = true;
        markLayerForObjectDirty(node);
    }

    if (cf.testFlag(Q3DSPropertyChangeList::EyeballChanges)) {
        // Special case: objects on master slide that get an eyeball change in
        // a subslide. These must be tracked so that obj->masterRollbackList()
        // can be applied since otherwise there's nothing ensuring the
        // visibility is reset when moving to another slide afterwards.
        auto nodeData = static_cast<Q3DSNodeAttached*>(node->attached());
        auto currentSlide = nodeData->component ? nodeData->component->currentSlide() : m_currentSlide;
        auto masterSlide = nodeData->component ? nodeData->component->masterSlide() : m_masterSlide;
        if (currentSlide && masterSlide->objects()->contains(node)) {
            Q3DSSlideAttached *data = static_cast<Q3DSSlideAttached *>(currentSlide->attached());
            Q_ASSERT(data);
            data->needsMasterRollback.insert(node);
        }

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
        m_wasDirty = true;
        markLayerForObjectDirty(node);
    }
}

void Q3DSSceneManager::updateSlideObjectVisibilities(Q3DSSlide *slide, Q3DSComponentNode *component)
{
    for (Q3DSGraphObject *obj : *slide->objects()) {
        const bool visible = scheduleNodeVisibilityUpdate(obj, component);
        if (obj->type() == Q3DSGraphObject::Component) {
            // objects on the Component's current (or master) slide
            Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(obj);
            // Recursively update any component slides
            Q3DSGraphObject *n = comp->masterSlide()->firstChild();
            while (n) {
                updateSlideObjectVisibilities(static_cast<Q3DSSlide *>(n), comp);
                n = n->nextSibling();
            }
            for (Q3DSGraphObject *cobj : *comp->masterSlide()->objects())
                scheduleNodeVisibilityUpdate(cobj, comp);
            for (Q3DSGraphObject *cobj : *comp->currentSlide()->objects())
                scheduleNodeVisibilityUpdate(cobj, comp);
            if (visible) { // if Component is not on the current slide, then don't care for now
                updateAnimations(comp->masterSlide(), nullptr, comp->currentSlide());
                updateAnimations(comp->currentSlide(), nullptr, comp->currentSlide());
            }
        }
    }
}

void Q3DSSceneManager::setNodeVisibility(Q3DSNode *node, bool visible)
{
    Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(node->attached());
    Q_ASSERT(data);
    if (!data->entity)
        return;

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(data->layer3DS->attached());
    if (!layerData->opaqueTag || !layerData->transparentTag) // bail out for subpresentation layers
        return;

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
bool Q3DSSceneManager::scheduleNodeVisibilityUpdate(Q3DSGraphObject *obj, Q3DSComponentNode *component)
{
    if (obj->isNode() && obj->type() != Q3DSGraphObject::Camera) {
        Q3DSNode *node = static_cast<Q3DSNode *>(obj);
        Q3DSNodeAttached *ndata = static_cast<Q3DSNodeAttached *>(node->attached());
        if (ndata) {
            bool visible = ndata->globalVisibility;
            // Check that object exists current slide scope (master + current)
            Q3DSSlide *master = component ? component->masterSlide () : m_masterSlide;
            Q3DSSlide *currentSlide = component ? component->currentSlide() : m_currentSlide;
            if (!master->objects()->contains(node) && !currentSlide->objects()->contains(node))
                visible = false;
            // Check if component is visible in parent slide hierarchy
            if (!isComponentVisible(component))
                visible = false;
            QSet<Q3DSNode *> *targetSet = visible ? &m_pendingNodeShow : &m_pendingNodeHide;
            targetSet->insert(node);
            return visible;
        }
    }
    return false;
}

void Q3DSSceneManager::updateAnimations(Q3DSSlide *animSourceSlide, Q3DSSlide *prevAnimSourceSlide, Q3DSSlide *playModeSourceSlide)
{
    // Called when entering a slide. Go through the slide's animations and add
    // Animator components for the affected entities (after removing existing ones).
    if (prevAnimSourceSlide) {
        setAnimationsRunning(prevAnimSourceSlide, false);
    }
    m_animationManager->updateAnimations(animSourceSlide, prevAnimSourceSlide, playModeSourceSlide);
}

void Q3DSSceneManager::setAnimationsRunning(Q3DSSlide *slide, bool running)
{
    Q3DSSlideAttached *data = static_cast<Q3DSSlideAttached *>(slide->attached());
    for (Qt3DAnimation::QClipAnimator *animator : data->animators)
        animator->setRunning(running);
}

void Q3DSFrameUpdater::frameAction(float dt)
{
    static const bool animDebug = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 3;
    if (Q_UNLIKELY(animDebug))
        qDebug().nospace() << "frame action " << m_frameCounter << ", delta=" << dt << ", applying animations and updating nodes";

    // Record new frame event.
    m_sceneManager->profiler()->reportNewFrame(dt * 1000.0f);
    // Set and notify the value changes queued by animations.
    m_sceneManager->animationManager()->applyChanges();
    // Recursively check dirty flags and update inherited values, execute
    // pending visibility changes, update light cbuffers, etc.
    m_sceneManager->prepareNextFrame();
    // Update profiling statistics for this frame.
    m_sceneManager->profiler()->updateFrameStats(m_frameCounter);
    ++m_frameCounter;
}

void Q3DSSceneManager::setProfileUiVisible(bool visible)
{
    if (m_profileUi)
        m_profileUi->setVisible(visible);
}

bool Q3DSSceneManager::isProfileUiVisible() const
{
    return m_profileUi ? m_profileUi->visible() : false;
}

void Q3DSSceneManager::setProfileUiInputEventSource(QObject *obj)
{
    if (m_profileUi)
        m_profileUi->setInputEventSource(obj);
}

QT_END_NAMESPACE
