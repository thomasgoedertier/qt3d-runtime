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

#include "q3dsscenemanager_p.h"
#include "q3dsengine_p.h"
#include "q3dsdefaultmaterialgenerator_p.h"
#include "q3dscustommaterialgenerator_p.h"
#include "q3dstextmaterialgenerator_p.h"
#include "q3dstextrenderer_p.h"
#include "q3dsutils_p.h"
#include "q3dsprofiler_p.h"
#include "shadergenerator/q3dsshadermanager_p.h"
#include "q3dsslideplayer_p.h"
#include "q3dsimagemanager_p.h"
#include "q3dslogging_p.h"
#if QT_CONFIG(q3ds_profileui)
#include "profileui/q3dsprofileui_p.h"
#include "q3dsconsolecommands_p.h"
#endif

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
#include <Qt3DRender/QRenderStateSet>
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
#include <Qt3DRender/QScissorTest>
#include <Qt3DRender/QRayCaster>

#include <Qt3DRender/private/qpaintedtextureimage_p.h>

#include <Qt3DAnimation/QClipAnimator>
#include <Qt3DAnimation/qclock.h>

#include <Qt3DExtras/QPlaneMesh>

#include <Qt3DLogic/QFrameAction>

QT_BEGIN_NAMESPACE

static const int LAYER_CACHING_THRESHOLD = 4;

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

                    FrameGraphNode { // layerContainerFg
                        // Due to layer caching children may appear and disappear at any time, and while the order relative
                        // to each other does not matter, they must never be placed after the compositor subtree. Hence the need
                        // for a "container" node here too.
                        NoDraw { }

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

                        ... // layer #3, #4, etc.
                    }

                    // compositor framegraph
                    LayerFilter {
                        Layer { id: compositorTag }
                        layers: [ compositorTag ]
                        ... // see buildCompositor()
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

    Compute pass for texture prefiltering is provided as a separate technique with "type" == "bsdfPrefilter". [not implemented]

*/

Q3DSSceneManager::Q3DSSceneManager()
    : m_gfxLimits(Q3DS::graphicsLimits()),
      m_matGen(new Q3DSDefaultMaterialGenerator),
      m_customMaterialGen(new Q3DSCustomMaterialGenerator),
      m_textMatGen(new Q3DSTextMaterialGenerator),
      m_textRenderer(new Q3DSTextRenderer(this)),
      m_profiler(new Q3DSProfiler),
      m_slidePlayer(new Q3DSSlidePlayer(this)),
      m_inputManager(new Q3DSInputManager(this))
{
    const QString fontDir = Q3DSUtils::resourcePrefix() + QLatin1String("res/Font");
    m_textRenderer->registerFonts({ fontDir });

    qRegisterMetaType<Qt3DRender::QRenderTarget *>("Qt3DRender::QRenderTarget*"); // wtf??
}

Q3DSSceneManager::~Q3DSSceneManager()
{
    m_logMutex.lock();
    m_inDestructor = true;
    m_logMutex.unlock();

#if QT_CONFIG(q3ds_profileui)
    delete m_consoleCommands;
    delete m_profileUi;
#endif
    delete m_slidePlayer;
    delete m_textRenderer;
    delete m_textMatGen;
    delete m_matGen;
    delete m_frameUpdater;
    delete m_profiler;
    delete m_inputManager;

    if (m_scene && m_sceneChangeObserverIndex >= 0)
        m_scene->removeSceneChangeObserver(m_sceneChangeObserverIndex);
    if (m_masterSlide && m_slideGraphChangeObserverIndex >= 0)
        m_masterSlide->removeSlideGraphChangeObserver(m_slideGraphChangeObserverIndex);
}

void Q3DSSceneManager::updateSizes(const QSize &size, qreal dpr, const QRect &viewport, bool forceSynchronous)
{
    if (!m_scene)
        return;

    // Setup the matte and scene viewport
    QRectF normalizedViewport;
    if (viewport.isNull())
        normalizedViewport = QRectF(0, 0, 1, 1);
    else
        normalizedViewport = QRectF(viewport.x() / qreal(size.width()),
                                    viewport.y() / qreal(size.height()),
                                    viewport.width() / qreal(size.width()),
                                    viewport.height() / qreal(size.height()));

    if (m_viewportData.viewport)
       m_viewportData.viewport->setNormalizedRect(normalizedViewport);

    m_viewportData.viewportRect = viewport;
    m_viewportData.viewportDpr = dpr;
    if (m_viewportData.matteScissorTest) {
        m_viewportData.matteScissorTest->setBottom(qFloor(-(viewport.height() + viewport.y() - size.height()) * dpr));
        m_viewportData.matteScissorTest->setLeft(qFloor(viewport.x() * dpr));
        m_viewportData.matteScissorTest->setWidth(qFloor(viewport.width() * dpr));
        m_viewportData.matteScissorTest->setHeight(qFloor(viewport.height() * dpr));
    }

    qCDebug(lcScene) << "Resize to" << size << "with viewport" << viewport << "device pixel ratio" << dpr;

    m_outputPixelSize = viewport.size() * dpr;

    // m_guiData uses the full surface size (not viewport)
    m_guiData.outputSize = size;
    m_guiData.outputDpr = dpr;

    if (m_guiData.camera) {
        m_guiData.camera->setRight(size.width() * float(dpr));
        m_guiData.camera->setBottom(size.height() * float(dpr));
    }

    for (auto callback : m_compositorOutputSizeChangeCallbacks)
        callback();

    bool forceTreeVisit = forceSynchronous;
    Q3DSUipPresentation::forAllObjects(m_scene, [this, &forceTreeVisit](Q3DSGraphObject *obj) {
        if (obj->type() == Q3DSGraphObject::Layer) {
            Q3DSLayerAttached *data = obj->attached<Q3DSLayerAttached>();
            if (data) {
                data->parentSize = m_outputPixelSize;
                data->frameDirty |= Q3DSGraphObjectAttached::LayerDirty;
                // do it right away if there was no size set yet
                if (data->parentSize.isEmpty())
                    forceTreeVisit = true;
                // Defer otherwise, like it is done for other property changes.
            }
        } else if (obj->type() == Q3DSGraphObject::Text) {
            // Text nodes depend on the device pixel ratio and so
            // may need to be updated.
            Q3DSTextAttached *data = obj->attached<Q3DSTextAttached>();
            if (data) {
                data->frameDirty = Q3DSGraphObjectAttached::TextDirty;
                data->frameChangeFlags |= Q3DSTextNode::TextureImageDepChanges;
            }
        }
    });
    if (forceTreeVisit)
        syncScene();
}

void Q3DSSceneManager::setCurrentSlide(Q3DSSlide *newSlide, bool flush)
{
    // NOTE: m_currentSlide is update from the slide player...
    if (m_currentSlide == newSlide)
        return;

    const int index = m_slidePlayer->slideDeck()->indexOfSlide(newSlide);
    if (index == -1 && !newSlide->parent()) // silently refuse to change to the master slide
        return;
    Q_ASSERT(index != -1);
    m_slidePlayer->slideDeck()->setCurrentSlide(index);
    if (flush || m_slidePlayer->state() != Q3DSSlidePlayer::PlayerState::Playing) {
        m_slidePlayer->setSlideTime(newSlide, 0.0f);
        syncScene();
    }
}

void Q3DSSceneManager::setComponentCurrentSlide(Q3DSSlide *newSlide, bool flush)
{
    Q3DSSlideAttached *data = newSlide->attached<Q3DSSlideAttached>();
    const int index = data->slidePlayer->slideDeck()->indexOfSlide(newSlide);
    if (index == -1 && !newSlide->parent()) // silently refuse to change to the master slide
        return;
    Q_ASSERT(index != -1);
    data->slidePlayer->slideDeck()->setCurrentSlide(index);
    if (flush || data->slidePlayer->state() != Q3DSSlidePlayer::PlayerState::Playing) {
        data->slidePlayer->setSlideTime(newSlide, 0.0f);
        syncScene();
    }
}

void Q3DSSceneManager::setLayerCaching(bool enabled)
{
    if (m_layerCaching == enabled)
        return;

    qCDebug(lcScene, "Layer caching enabled = %d", enabled);
    m_layerCaching = enabled;

    m_layerUncachePending = !m_layerCaching;
}

void Q3DSSceneManager::prepareAnimators()
{
    m_slidePlayer->sceneReady();
}

QDebug operator<<(QDebug dbg, const Q3DSSceneManager::SceneBuilderParams &p)
{
    QDebugStateSaver saver(dbg);
    dbg << "SceneBuilderParams(" << p.flags << p.outputSize << p.outputDpr << p.surface << ")";
    return dbg;
}

void Q3DSSceneManager::prepareEngineReset()
{
    qCDebug(lcScene, "prepareEngineReset on scenemanager %p", this);

    delete m_frameUpdater;
    m_frameUpdater = nullptr;

    delete m_slidePlayer;
    m_slidePlayer = nullptr;

#if QT_CONFIG(q3ds_profileui)
    if (m_profileUi)
        m_profileUi->releaseResources();
#endif
}

void Q3DSSceneManager::prepareEngineResetGlobal()
{
    qCDebug(lcScene, "prepareEngineResetGlobal");

    Q3DSImageManager::instance().invalidate();
    Q3DSShaderManager::instance().invalidate();
}

/*
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
Q3DSSceneManager::Scene Q3DSSceneManager::buildScene(Q3DSUipPresentation *presentation, const SceneBuilderParams &params)
{
    if (!presentation->scene()) {
        qWarning("Q3DSSceneBuilder: No scene?");
        return Scene();
    }

    qCDebug(lcScene) << "Building scene for" << presentation->sourceFile() << params; // NB params.outputSize==(0,0) is acceptable

    const QString projectFontDir = QFileInfo(presentation->sourceFile()).canonicalPath() + QLatin1Char('/') + QLatin1String("fonts");
    if (QDir(projectFontDir).exists())
        m_textRenderer->registerFonts({ projectFontDir });

    m_engine = params.engine;
    m_flags = params.flags;

    m_presentation = presentation;
    m_presentationSize = QSize(m_presentation->presentationWidth(), m_presentation->presentationHeight());
    m_scene = m_presentation->scene();
    m_masterSlide = m_presentation->masterSlide();
    m_currentSlide = nullptr;
    m_pendingObjectVisibility.clear();
    m_pendingSubPresLayers.clear();
    m_pendingSubPresImages.clear();
    m_subPresentations.clear();
    m_profiler->resetForNewScene(this);

    m_profiler->setEnabled(m_flags.testFlag(EnableProfiling));

    // All shader program info goes to the main presentation's profiler,
    // including programs from subpresentations.
    if (!m_flags.testFlag(SubPresentation))
        Q3DSShaderManager::instance().setProfiler(m_profiler);

    if (!m_scene) {
        qWarning("Q3DSSceneManager: No scene?");
        return Scene();
    }
    if (!m_masterSlide) {
        qWarning("Q3DSSceneManager: No master slide?");
        return Scene();
    }
    if (!m_masterSlide->firstChild()) {
        qWarning("Q3DSSceneManager: No slides?");
        return Scene();
    }

    // Kick off the Qt3D scene.
    m_rootEntity = new Qt3DCore::QEntity;
    m_rootEntity->setObjectName(QString(QLatin1String("non-layer root for presentation %1")).arg(m_presentation->name()));
    m_profiler->reportQt3DSceneGraphRoot(m_rootEntity);

    static const auto createSlideAttached = [](Q3DSSlide *slide, Qt3DCore::QEntity *entity) {
        if (!slide->attached()) {
            Q3DSSlideAttached *data = new Q3DSSlideAttached;
            data->entity = entity;
            slide->setAttached(data);
        }
    };

    // Create the attached data object(s) and register for change notifications
    createSlideAttached(m_masterSlide, m_rootEntity);
    m_masterSlide->attached<Q3DSSlideAttached>()->slideObjectChangeObserverIndex = m_masterSlide->addSlideObjectChangeObserver(
                std::bind(&Q3DSSceneManager::handleSlideObjectChange, this, std::placeholders::_1, std::placeholders::_2));
    Q3DSSlide *subslide = static_cast<Q3DSSlide *>(m_masterSlide->firstChild());
    while (subslide) {
        createSlideAttached(subslide, m_rootEntity);
        subslide->attached<Q3DSSlideAttached>()->slideObjectChangeObserverIndex = subslide->addSlideObjectChangeObserver(
                    std::bind(&Q3DSSceneManager::handleSlideObjectChange, this, std::placeholders::_1, std::placeholders::_2));
        subslide = static_cast<Q3DSSlide *>(subslide->nextSibling());
    }

    Q3DSSlideDeck *slideDeck = new Q3DSSlideDeck(m_masterSlide);
    m_currentSlide = slideDeck->currentSlide();

    // Property change processing happens in a frame action
    Qt3DLogic::QFrameAction *nodeUpdater = new Qt3DLogic::QFrameAction;
    m_frameUpdater = new Q3DSFrameUpdater(this);
    QObject::connect(nodeUpdater, &Qt3DLogic::QFrameAction::triggered, m_frameUpdater, &Q3DSFrameUpdater::frameAction);
    m_rootEntity->addComponent(nodeUpdater);

    Qt3DRender::QRenderSettings *frameGraphComponent;
    Qt3DRender::QFrameGraphNode *frameGraphRoot;
    Qt3DRender::QFrameGraphNode *subPresFrameGraphRoot;
    if (params.surface) {
        Q_ASSERT(!m_flags.testFlag(SubPresentation));
        frameGraphComponent = new Qt3DRender::QRenderSettings(m_rootEntity);
        frameGraphRoot = new Qt3DRender::QRenderSurfaceSelector;
        m_profiler->reportFrameGraphRoot(frameGraphRoot);
        // a node under which subpresentation framegraphs can be added
        subPresFrameGraphRoot = new Qt3DRender::QFrameGraphNode(frameGraphRoot);
        m_profiler->reportFrameGraphStopNode(subPresFrameGraphRoot);
        // but do nothing there when there are no subpresentations
        new Qt3DRender::QNoDraw(subPresFrameGraphRoot);
    } else {
        Q_ASSERT(m_flags.testFlag(SubPresentation));
        frameGraphComponent = nullptr;
        frameGraphRoot = params.frameGraphRoot;
        m_profiler->reportFrameGraphRoot(frameGraphRoot);
        subPresFrameGraphRoot = nullptr;
        Q_ASSERT(frameGraphRoot);
    }
    if (params.viewport.isNull())
        m_outputPixelSize = params.outputSize * params.outputDpr;
    else
        m_outputPixelSize = params.viewport.size() * params.outputDpr;
    m_guiData.outputSize = params.outputSize;
    m_guiData.outputDpr = params.outputDpr;

    // Parent it to something long-living (note: cannot be the framegraph).
    // Cannot leave globally used components unparented since that would mean
    // they get parented to the first node they get added to - and that might
    // be something that gets destroyed over time, e.g. in a framegraph subtree
    // that gets removed or replaced at some point.
    m_fsQuadTag = new Qt3DRender::QLayer(m_rootEntity);
    m_fsQuadTag->setObjectName(QLatin1String("Fullscreen quad pass"));

    // Build the (offscreen) Qt3D scene
    m_layerContainerFg = new Qt3DRender::QFrameGraphNode(frameGraphRoot);
    new Qt3DRender::QNoDraw(m_layerContainerFg); // in case there are no layers at all
    Q3DSUipPresentation::forAllLayers(m_scene, [=](Q3DSLayerNode *layer3DS) {
        if (layer3DS->sourcePath().isEmpty())
            buildLayer(layer3DS, m_layerContainerFg, m_outputPixelSize);
        else
            buildSubPresentationLayer(layer3DS, m_outputPixelSize);
    });

    // The Scene object may have non-layer children.
    Q3DSGraphObject *sceneChild = m_scene->firstChild();
    while (sceneChild) {
        if (sceneChild->type() == Q3DSGraphObject::Behavior)
            initBehaviorInstance(static_cast<Q3DSBehaviorInstance *>(sceneChild));
        sceneChild = sceneChild->nextSibling();
    }

    // Onscreen (or not) compositor (still offscreen when this is a subpresentation)
    buildCompositor(frameGraphRoot, m_rootEntity);

    // Profiling UI (main presentation only)
#if QT_CONFIG(q3ds_profileui)
    if (!m_flags.testFlag(SubPresentation)) {
        buildGuiPass(frameGraphRoot, m_rootEntity);
        m_consoleCommands = new Q3DSConsoleCommands(this);
        m_profileUi = new Q3DSProfileUi(&m_guiData, m_profiler,
                                        std::bind(&Q3DSConsoleCommands::setupConsole, m_consoleCommands, std::placeholders::_1));
    }
#endif

    // Fullscreen quad for bluring the shadow map/cubemap
    Q3DSShaderManager &sm(Q3DSShaderManager::instance());
    QStringList fsQuadPassNames { QLatin1String("shadowOrthoBlurX"), QLatin1String("shadowOrthoBlurY") };
    QVector<Qt3DRender::QShaderProgram *> fsQuadPassProgs { sm.getOrthoShadowBlurXShader(m_rootEntity), sm.getOrthoShadowBlurYShader(m_rootEntity) };

    if (!m_gfxLimits.useGles2Path) {
        if (m_gfxLimits.maxDrawBuffers >= 6) { // ###
            fsQuadPassNames << QLatin1String("shadowCubeBlurX") << QLatin1String("shadowCubeBlurY");
            fsQuadPassProgs << sm.getCubeShadowBlurXShader(m_rootEntity, m_gfxLimits) << sm.getCubeShadowBlurYShader(m_rootEntity, m_gfxLimits);
        }
        fsQuadPassNames << QLatin1String("ssao") << QLatin1String("progaa");
        fsQuadPassProgs << sm.getSsaoTextureShader(m_rootEntity) << sm.getProgAABlendShader(m_rootEntity);
    }
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

    if (params.surface) {
        // Ready to go (except that the sizes calculated from params.outputSize are
        // likely bogus when it is derived from QWindow::size() during app startup;
        // will get updated in updateSizes()).
        static_cast<Qt3DRender::QRenderSurfaceSelector *>(frameGraphRoot)->setSurface(params.surface);
        frameGraphComponent->setActiveFrameGraph(frameGraphRoot);
        m_rootEntity->addComponent(frameGraphComponent);
        sc.renderSettings = frameGraphComponent;
    }

    // We now set of slide handling, updating object visibility etc.
    qCDebug(lcScene, "Kicking off slide system");
    m_slidePlayer->setSlideDeck(slideDeck);

    // Force processing the initial set of object visibility changes generated
    // by the slideplayer as we need this now, do not wait until the first frame action.
    setPendingVisibilities();

    // Listen to future changes to the scene graph.
    m_sceneChangeObserverIndex = m_scene->addSceneChangeObserver(
                std::bind(&Q3DSSceneManager::handleSceneChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // And the "slide graph" (more like slide list).
    m_slideGraphChangeObserverIndex = m_masterSlide->addSlideGraphChangeObserver(
                std::bind(&Q3DSSceneManager::handleSlideGraphChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // And for events too.
    m_scene->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));

    // measure the time from the end of scene building to the invocation of the first frame action
    m_frameUpdater->startTimeFirstFrame();

    qCDebug(lcScene, "buildScene done");
    return sc;
}

/*
    To be called on the scenemanager corresponding to the main presentation
    once after all subpresentation buildScene() calls have succeeded. This is
    where the association of textures and subpresentation layers happens. That
    cannot be done in the first buildScene since the textures for
    subpresentations (and the corresponding framegraph subtrees) are not yet
    generated at that stage.
 */
void Q3DSSceneManager::finalizeMainScene(const QVector<Q3DSSubPresentation> &subPresentations)
{
    Q_ASSERT(!m_flags.testFlag(SubPresentation));
    qCDebug(lcScene, "finalizeMainScene");

    m_subPresentations = subPresentations;

    for (Q3DSLayerNode *layer3DS : m_pendingSubPresLayers) {
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

    for (auto p : m_pendingSubPresImages)
        setImageTextureFromSubPresentation(p.first, p.second);

    m_pendingSubPresLayers.clear();
    m_pendingSubPresImages.clear();

    for (const Q3DSSubPresentation &subPres : m_subPresentations) {
        if (!subPres.sceneManager)
            continue;
        m_profiler->registerSubPresentationProfiler(subPres.sceneManager->m_profiler);
    }

#if QT_CONFIG(q3ds_profileui)
    if (!m_flags.testFlag(SubPresentation))
        m_consoleCommands->runBootScript();
#endif
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

static Qt3DRender::QAbstractTexture *newDepthStencilBuffer(const QSize &layerPixelSize, int msaaSampleCount, Qt3DRender::QAbstractTexture::TextureFormat format)
{
    // GLES <= 3.1 does not have glFramebufferTexture and support for combined
    // depth-stencil textures. Here we rely on the fact the Qt3D will
    // transparently switch to using a renderbuffer in place of a texture. Using
    // separate textures is problematic for stencil since the suitable texture
    // type is an ES 3.1 extension, so that is not an option either for <= 3.1.
    //
    // The internal difference (renderbuffer vs. texture) won't matter as long
    // as a custom material or other thing does not need the depth texture.
    // When that happens, we will be in trouble when running on GLES < 3.2.
    //
    // Therefore, on GLES 2.0, 3.0 and 3.1 we expect to get called with D16 (or
    // D24 or D32) and no stencil. (whereas GLES 3.2 or desktop GL will use D24S8)

    Qt3DRender::QAbstractTexture *dsTexOrRb;
    if (msaaSampleCount > 1) {
        dsTexOrRb = new Qt3DRender::QTexture2DMultisample;
        dsTexOrRb->setSamples(msaaSampleCount);
    } else {
        dsTexOrRb = new Qt3DRender::QTexture2D;
    }
    dsTexOrRb->setFormat(format);
    dsTexOrRb->setWidth(layerPixelSize.width());
    dsTexOrRb->setHeight(layerPixelSize.height());
    dsTexOrRb->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    dsTexOrRb->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    return dsTexOrRb;
}

Qt3DRender::QRenderTarget *Q3DSSceneManager::newLayerRenderTarget(const QSize &layerPixelSize, int msaaSampleCount,
                                                                  Qt3DRender::QAbstractTexture **colorTex, Qt3DRender::QAbstractTexture **dsTexOrRb,
                                                                  Qt3DCore::QNode *textureParentNode, Q3DSLayerNode *layer3DS,
                                                                  Qt3DRender::QAbstractTexture *existingDS)
{
    Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;

    Qt3DRender::QRenderTargetOutput *color = new Qt3DRender::QRenderTargetOutput;
    color->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
    *colorTex = newColorBuffer(layerPixelSize, msaaSampleCount);
    m_profiler->trackNewObject(*colorTex, Q3DSProfiler::Texture2DObject,
                               "Color buffer for layer %s", layer3DS->id().constData());
    (*colorTex)->setParent(textureParentNode);
    color->setTexture(*colorTex);
    rt->addOutput(color);

    Qt3DRender::QRenderTargetOutput *ds = new Qt3DRender::QRenderTargetOutput;
    bool noStencil = !m_gfxLimits.packedDepthStencilBufferSupported; // GLES 2.0
    // see newDepthStencilBuffer for a detailed description of this mess
    noStencil |= m_gfxLimits.format.renderableType() == QSurfaceFormat::OpenGLES
            && Q3DS::graphicsLimits().format.version() <= qMakePair(3, 1);
    Qt3DRender::QAbstractTexture::TextureFormat textureFormat =
            noStencil ? Qt3DRender::QAbstractTexture::D16 : Qt3DRender::QAbstractTexture::D24S8;
    ds->setAttachmentPoint(noStencil ? Qt3DRender::QRenderTargetOutput::Depth
                                     : Qt3DRender::QRenderTargetOutput::DepthStencil);
    if (!existingDS) {
        if (noStencil)
            qCDebug(lcScene, "Render target depth-stencil attachment uses D16 (no stencil)");
        else
            qCDebug(lcScene, "Render target depth-stencil attachment uses D24S8");
        *dsTexOrRb = newDepthStencilBuffer(layerPixelSize, msaaSampleCount, textureFormat);
        m_profiler->trackNewObject(*dsTexOrRb, Q3DSProfiler::Texture2DObject,
                                   "Depth-stencil buffer for layer %s", layer3DS->id().constData());
        (*dsTexOrRb)->setParent(textureParentNode);
    } else {
        *dsTexOrRb = existingDS;
    }

    ds->setTexture(*dsTexOrRb);

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
        qCDebug(lcPerf, "Layer %s uses %dx SSAA", layer3DS->id().constData(), ssaaScaleFactor);
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
        qCDebug(lcPerf, "Layer %s uses multisample texture", layer3DS->id().constData());

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

    Qt3DRender::QLayer *opaqueTag = new Qt3DRender::QLayer(m_rootEntity);
    opaqueTag->setObjectName(QLatin1String("Opaque pass"));
    Qt3DRender::QLayer *transparentTag = new Qt3DRender::QLayer(m_rootEntity);
    transparentTag->setObjectName(QLatin1String("Transparent pass"));

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

    Qt3DCore::QEntity *layerSceneRootEntity = new Qt3DCore::QEntity;
    layerSceneRootEntity->setObjectName(QObject::tr("root for %1").arg(QString::fromUtf8(layer3DS->id())));
    layerSceneRootEntity->setParent(rtSelector); // the separate setParent() call is intentional here, see QTBUG-69352
    m_profiler->reportQt3DSceneGraphRoot(layerSceneRootEntity);

    Q3DSLayerAttached *layerData = new Q3DSLayerAttached;
    // Must set an entity to make Q3DSLayerNode properties animatable.
    // also, this must be the general purpose "root" (### why?)
    layerData->entity = m_rootEntity;
    // The entity tree corresponding to 3DS nodes lives under a separate,
    // per-layer "root" entity, to allow easy cleanup in case the layer goes
    // away later on.
    layerData->layerSceneRootEntity = layerSceneRootEntity;
    layerData->layer3DS = layer3DS;
    layerData->layerFgRoot = layerFgRoot;
    layerData->layerFgRootParent = layerFgRoot->parentNode();
    layerData->layerFgDummyParent = new Qt3DCore::QNode(layerData->entity);
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

    // Prepare image objects. This must be done up-front.
    Q3DSUipPresentation::forAllObjectsInSubTree(layer3DS, [this](Q3DSGraphObject *obj) {
        if (obj->type() == Q3DSGraphObject::Image)
            initImage(static_cast<Q3DSImage *>(obj));
    });

    // Now add the scene contents.
    Q3DSGraphObject *obj = layer3DS->firstChild();
    m_componentNodeStack.clear();
    m_componentNodeStack.push(nullptr);
    while (obj) {
        buildLayerScene(obj, layer3DS, layerSceneRootEntity);
        obj = obj->nextSibling();
    }

    // Setup picking for layer
    if (layer3DS->firstChild()) {
        auto rayCaster = new Qt3DRender::QRayCaster(layerSceneRootEntity);
        rayCaster->setFilterMode(Qt3DRender::QAbstractRayCaster::AcceptAnyMatchingLayers);
        rayCaster->addLayer(layerData->opaqueTag);
        rayCaster->addLayer(layerData->transparentTag);
        rayCaster->setRunMode(Qt3DRender::QAbstractRayCaster::SingleShot);
        layerSceneRootEntity->addComponent(rayCaster);
        layerData->layerRayCaster = rayCaster;
    }

    // Find the active camera for this layer and set it up (but note that there
    // may be none if this is the initial scene build where the slideplayer has
    // not yet been initialized).
    setActiveLayerCamera(findFirstCamera(layer3DS), layer3DS);

    setLayerProperties(layer3DS);

    layerData->propertyChangeObserverIndex = layer3DS->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    layerData->eventObserverIndex = layer3DS->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));

    // Phase 2: deferred stuff

    // Gather lights for this layer.
    gatherLights(layer3DS);

    updateShadowMapStatus(layer3DS); // must be done before generating materials below

    // Enable SSAO (and depth texture generation) when needed. Must be done
    // before generating materials below.
    updateSsaoStatus(layer3DS);

    // Generate Qt3D material components.
    Q3DSUipPresentation::forAllModels(layer3DS->firstChild(),
                                      [this](Q3DSModelNode *model3DS) { buildModelMaterial(model3DS); },
                                      true); // include hidden ones too

    // Set up effects.
    updateEffectStatus(layer3DS);
}

Qt3DRender::QAbstractTexture *Q3DSSceneManager::dummyTexture()
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
    Q3DSLayerAttached *data = new Q3DSLayerAttached;
    data->entity = m_rootEntity; // must set an entity to to make Q3DSLayerNode properties animatable, just use the root
    data->layer3DS = layer3DS;
    data->layerSize = calculateLayerSize(layer3DS, parentSize);
    data->parentSize = parentSize;

    // camera and stuff stays null, no such thing for subpresentation layers

    // leave compositorSourceParam dummy for now, we don't know the actual texture yet
    data->compositorSourceParam = new Qt3DRender::QParameter(QLatin1String("tex"), dummyTexture());
    m_pendingSubPresLayers.insert(layer3DS);

    // subpresentations associated with layers follow the size of the layer
    data->updateSubPresentationSize = [this, layer3DS, data]() {
        const QSize sz = data->layerSize;
        if (sz.isEmpty())
            return;
        const QSize layerPixelSize = safeLayerPixelSize(data);
        auto it = std::find_if(m_subPresentations.cbegin(), m_subPresentations.cend(),
                               [layer3DS](const Q3DSSubPresentation &sp) { return sp.id == layer3DS->sourcePath(); });
        if (it != m_subPresentations.cend()) {
            qCDebug(lcScene, "Resizing subpresentation %s for layer %s to %dx%d",
                    qPrintable(layer3DS->sourcePath()), layer3DS->id().constData(), sz.width(), sz.height());
            // Resize the offscreen subpresentation buffers
            it->colorTex->setWidth(layerPixelSize.width());
            it->colorTex->setHeight(layerPixelSize.height());
            if (it->depthOrDepthStencilTex) {
                it->depthOrDepthStencilTex->setWidth(layerPixelSize.width());
                it->depthOrDepthStencilTex->setHeight(layerPixelSize.height());
            }
            if (it->stencilTex) {
                it->stencilTex->setWidth(layerPixelSize.width());
                it->stencilTex->setHeight(layerPixelSize.height());
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
#if 0
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
#endif
    Q_UNUSED(layer3DS);
    return {};
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

void Q3DSSceneManager::setLayerCameraSizeProperties(Q3DSLayerNode *layer3DS, const QVector2D &offset)
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
            if (!offset.isNull()) {
                QMatrix4x4 proj = camera->projectionMatrix();
                float *projData = proj.data();
                projData[12] += offset.x();
                projData[13] += offset.y();
                // Mode changes to Custom.
                camera->setProjectionMatrix(proj);
            }
        } else {
            camera->setAspectRatio(presentationAspect);
            QMatrix4x4 proj = camera->projectionMatrix();
            float *projData = proj.data();
            if (!qFuzzyIsNull(presentationAspect))
                projData[5] *= aspect / presentationAspect;
            if (!offset.isNull()) {
                projData[12] += offset.x();
                projData[13] += offset.y();
            }
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

namespace  {
qreal qLog2(qreal x) {
    return qLn(x) / 0.693147180559945309417;
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
        data->compositorEntity->setEnabled(layer3DS->flags().testFlag(Q3DSNode::Active) && (data->visibilityTag == Q3DSGraphObjectAttached::Visible));

    // IBL Probes
    if (!data->iblProbeData.lightProbeProperties) {
        data->iblProbeData.lightProbeProperties = new Qt3DRender::QParameter;
        data->iblProbeData.lightProbeProperties->setName(QLatin1String("light_probe_props"));
    }
    data->iblProbeData.lightProbeProperties->setValue(QVector4D(0.0f, 0.0f, layer3DS->probeHorizon(), layer3DS->probeBright() * 0.01f));

    if (!data->iblProbeData.lightProbe2Properties) {
        data->iblProbeData.lightProbe2Properties = new Qt3DRender::QParameter;
        data->iblProbeData.lightProbe2Properties->setName(QLatin1String("light_probe2_props"));
    }
    data->iblProbeData.lightProbe2Properties->setValue(QVector4D(0.0f, 0.0f, 0.0f, 0.0f));

    if (!data->iblProbeData.lightProbeOptions) {
        data->iblProbeData.lightProbeOptions = new Qt3DRender::QParameter;
        data->iblProbeData.lightProbeOptions->setName(QLatin1String("light_probe_opts"));

    }
    data->iblProbeData.lightProbeOptions->setValue(QVector4D(0.01745329251994329547f * layer3DS->probeFov(), 0.0f, 0.0f, 0.0f));

    if (layer3DS->lightProbe()) {
        // initialize light probe parameters if necessary
        if (!data->iblProbeData.lightProbeTexture) {
            data->iblProbeData.lightProbeTexture = Q3DSImageManager::instance().newTextureForImageFile(
                        m_rootEntity, Q3DSImageManager::GenerateMipMapsForIBL,
                        m_profiler, "iblProbe texture for image %s", layer3DS->lightProbe()->id().constData());
        }
        if (!data->iblProbeData.lightProbeSampler) {
            data->iblProbeData.lightProbeSampler = new Qt3DRender::QParameter;
            data->iblProbeData.lightProbeSampler->setName(QLatin1String("light_probe"));
        }

        if (!data->iblProbeData.lightProbeOffset) {
            data->iblProbeData.lightProbeOffset = new Qt3DRender::QParameter;
            data->iblProbeData.lightProbeOffset->setName(QLatin1String("light_probe_offset"));
        }

        if (!data->iblProbeData.lightProbeRotation) {
            data->iblProbeData.lightProbeRotation = new Qt3DRender::QParameter;
            data->iblProbeData.lightProbeRotation->setName(QLatin1String("light_probe_rotation"));
        }

        // Update light probe parameter values

        // also sets min/mag and generates mipmaps
        Q3DSImageManager::instance().setSource(data->iblProbeData.lightProbeTexture,
                                               QUrl::fromLocalFile(layer3DS->lightProbe()->sourcePath()));
        data->iblProbeData.lightProbeSampler->setValue(QVariant::fromValue(data->iblProbeData.lightProbeTexture));

        // Image probes force repeat for horizontal repeat
        Qt3DRender::QTextureWrapMode wrapMode;
        wrapMode.setX(Qt3DRender::QTextureWrapMode::Repeat);

        switch (layer3DS->lightProbe()->verticalTiling()) {
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

        Qt3DRender::QAbstractTexture *texture = data->iblProbeData.lightProbeTexture;
        texture->setWrapMode(wrapMode);

        const QMatrix4x4 &textureTransform = layer3DS->lightProbe()->textureTransform();
        const float *m = textureTransform.constData();

        // offsets.w = max mip level
        const QSize texSize = Q3DSImageManager::instance().size(texture);
        float mipLevels = float(qCeil(qLog2(qMax(texSize.width(), texSize.height()))));
        QVector4D offsets(m[12], m[13], 0.0f, mipLevels);
        data->iblProbeData.lightProbeOffset->setValue(offsets);

        QVector4D rotations(m[0], m[4], m[1], m[5]);
        data->iblProbeData.lightProbeRotation->setValue(rotations);

        if (layer3DS->lightProbe2()) {
            // Initialize light probe 2 parameters
            if (!data->iblProbeData.lightProbe2Texture) {
                data->iblProbeData.lightProbe2Texture = Q3DSImageManager::instance().newTextureForImageFile(
                            m_rootEntity, Q3DSImageManager::GenerateMipMapsForIBL,
                            m_profiler, "iblProbe2 texture for image %s", layer3DS->lightProbe2()->id().constData());
            }

            if (!data->iblProbeData.lightProbe2Sampler) {
                data->iblProbeData.lightProbe2Sampler = new Qt3DRender::QParameter;
                data->iblProbeData.lightProbe2Sampler->setName(QLatin1String("light_probe2"));
            }

            // Update light probe 2 parameter values

            // also sets min/mag and generates mipmaps
            Q3DSImageManager::instance().setSource(data->iblProbeData.lightProbe2Texture,
                                                   QUrl::fromLocalFile(layer3DS->lightProbe2()->sourcePath()));
            data->iblProbeData.lightProbe2Sampler->setValue(QVariant::fromValue(data->iblProbeData.lightProbe2Texture));

            data->iblProbeData.lightProbe2Texture->setWrapMode(wrapMode);

            QVector4D probe2Props(layer3DS->probe2Window(), layer3DS->probe2Pos(), layer3DS->probe2Fade(), 1.0f);
            data->iblProbeData.lightProbe2Properties->setValue(probe2Props);
            const QMatrix4x4 &textureTransform = layer3DS->lightProbe2()->textureTransform();
            const float *m = textureTransform.constData();
            QVector4D probeProps(m[12], m[13], layer3DS->probeHorizon(), layer3DS->probeBright() * 0.01f);
            data->iblProbeData.lightProbeProperties->setValue(probeProps);
        } else {
            data->iblProbeData.lightProbe2Properties->setValue(QVector4D(0.0f, 0.0f, 0.0f, 0.0f));
            data->iblProbeData.lightProbeProperties->setValue(QVector4D(0.0f, 0.0f, layer3DS->probeHorizon(), layer3DS->probeBright() * 0.01f));
        }
    }
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
    // Make sure data->entity, globalTransform, etc. are usable. Note however
    // that during the initial scene building globalVisibility will typically
    // be false at this stage due to the slideplayer only generating visibility
    // changes later on.
    setNodeProperties(cam3DS, camera, data->transform, NodePropUpdateAttached | NodePropUpdateGlobalsRecursively);
    setCameraProperties(cam3DS, Q3DSNode::TransformChanges);
    data->propertyChangeObserverIndex = cam3DS->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    data->eventObserverIndex = cam3DS->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));
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

    if (!(changeFlags & Q3DSNode::TransformChanges))
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
bool Q3DSSceneManager::setActiveLayerCamera(Q3DSCameraNode *cam3DS, Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    if (layerData->cam3DS != cam3DS) {
        layerData->cam3DS = cam3DS;
        if (cam3DS) {
            Q3DSCameraAttached *activeCameraData = static_cast<Q3DSCameraAttached *>(cam3DS->attached());
            layerData->cameraSelector->setCamera(activeCameraData->camera);
        } else {
            layerData->cameraSelector->setCamera(nullptr);
        }

        if (cam3DS) {
            setCameraProperties(cam3DS, Q3DSNode::TransformChanges);
            layerData->cameraPropertiesParam->setValue(QVector2D(cam3DS->clipNear(), cam3DS->clipFar()));
        }

        // may not have a valid size yet
        if (!layerData->layerSize.isEmpty()) {
            setLayerCameraSizeProperties(layer3DS);
            setLayerSizeProperties(layer3DS);
        }

        layerData->wasDirty = true;

        qCDebug(lcScene, "Layer %s uses camera %s", layer3DS->id().constData(), cam3DS ? cam3DS->id().constData() : "null");
        return true;
    }
    return false;
}

// layers use the first active camera for rendering
Q3DSCameraNode *Q3DSSceneManager::findFirstCamera(Q3DSLayerNode *layer3DS)
{
    // Pick the first active camera encountered when walking depth-first.
    std::function<Q3DSCameraNode *(Q3DSGraphObject *)> f;
    f = [&f,this](Q3DSGraphObject *obj) -> Q3DSCameraNode* {
        while (obj) {
            if (obj->type() == Q3DSGraphObject::Camera) {
                Q3DSCameraNode *cam = static_cast<Q3DSCameraNode *>(obj);
                const bool active = cam->attached() && cam->attached<Q3DSCameraAttached>()->globalEffectiveVisibility;
                if (active) {
                    // Check if camera is on the current slide
                    Q3DSComponentNode *component = cam->attached() ? cam->attached()->component : nullptr;
                    // Check that object exists current slide scope (master + current)
                    Q3DSSlide *master = component ? component->masterSlide () : m_masterSlide;
                    Q3DSSlide *currentSlide = component ? component->currentSlide() : m_currentSlide;
                    if ((master->objects().contains(cam) || currentSlide->objects().contains(cam)) && isComponentVisible(component))
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

    const int scale = !flags.testFlag(Q3DSLayerAttached::SizeManagedTexture::IgnoreSSAA) ? data->ssaaScaleFactor : 1;
    const QSize layerPixelSize = safeLayerPixelSize(data->layerSize, scale);
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
    qCDebug(lcPerf, "Depth texture enabled for layer %s is now %d", layer3DS->id().constData(), enabled);
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
    qCDebug(lcPerf, "SSAO enabled for layer %s is now %d", layer3DS->id().constData(), enabled);

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

            data->ssaoTextureData.aoDataBuf = new Qt3DRender::QBuffer(data->entity);
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

void Q3DSSceneManager::updateCubeShadowMapParams(Q3DSLayerAttached::PerLightShadowMapData *d,
                                                 Q3DSLightNode *light3DS,
                                                 const QString &lightIndexStr)
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

void Q3DSSceneManager::updateOrthoShadowMapParams(Q3DSLayerAttached::PerLightShadowMapData *d,
                                                  Q3DSLightNode *light3DS,
                                                  const QString &lightIndexStr)
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

    if (!layerData->cam3DS) // ok to do nothing assuming there will be another updateShadowMapStatus call once the camera is known
        return;

    Q3DSCameraAttached *sceneCamData = static_cast<Q3DSCameraAttached *>(layerData->cam3DS->attached());
    Qt3DRender::QCamera *sceneCamera = sceneCamData->camera;
    QVector3D camX = sceneCamData->globalTransform.column(0).toVector3D();
    QVector3D camY = sceneCamData->globalTransform.column(1).toVector3D();
    QVector3D camZ = sceneCamData->globalTransform.column(2).toVector3D();
    float tanFOV = qTan(qDegreesToRadians(sceneCamera->fieldOfView()) * 0.5f);
    float asTanFOV = tanFOV * sceneCamera->aspectRatio();
    QVector3D camEdges[4];
    camEdges[0] = -asTanFOV * camX + tanFOV * camY + camZ;
    camEdges[1] = asTanFOV * camX + tanFOV * camY + camZ;
    camEdges[2] = asTanFOV * camX - tanFOV * camY + camZ;
    camEdges[3] = -asTanFOV * camX - tanFOV * camY + camZ;

    for (int i = 0; i < 4; ++i) {
        camEdges[i].setX(-camEdges[i].x());
        camEdges[i].setY(-camEdges[i].y());
    }

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
    const QVector3D right = QVector3D::crossProduct(lightDir, QVector3D(0, 1, 0)).normalized();
    const QVector3D up = QVector3D::crossProduct(right, lightDir).normalized();

    // Calculate bounding box of the scene camera frustum
    float minDistanceZ = std::numeric_limits<float>::max();
    float maxDistanceZ = -std::numeric_limits<float>::max();
    float minDistanceY = std::numeric_limits<float>::max();
    float maxDistanceY = -std::numeric_limits<float>::max();
    float minDistanceX = std::numeric_limits<float>::max();
    float maxDistanceX = -std::numeric_limits<float>::max();
    for (int i = 0; i < 8; ++i) {
        float distanceZ = QVector3D::dotProduct(camVerts[i], lightDir);
        if (distanceZ < minDistanceZ)
            minDistanceZ = distanceZ;
        if (distanceZ > maxDistanceZ)
            maxDistanceZ = distanceZ;
        float distanceY = QVector3D::dotProduct(camVerts[i], up);
        if (distanceY < minDistanceY)
            minDistanceY = distanceY;
        if (distanceY > maxDistanceY)
            maxDistanceY = distanceY;
        float distanceX = QVector3D::dotProduct(camVerts[i], right);
        if (distanceX < minDistanceX)
            minDistanceX = distanceX;
        if (distanceX > maxDistanceX)
            maxDistanceX = distanceX;
    }

    float clipNear = -qFabs(maxDistanceZ - minDistanceZ);
    float clipFar = qFabs(maxDistanceZ - minDistanceZ);

    d->shadowCamOrtho->setNearPlane(clipNear);
    d->shadowCamOrtho->setFarPlane(clipFar);

    // The shadow camera's projection is (more or less?) an ordinary orthographic projection.
    float deltaZ = clipFar - clipNear;
    float halfWidth = qFabs(maxDistanceX - minDistanceX) / 2;
    float halfHeight = qFabs(maxDistanceY - minDistanceY) / 2;
    if (deltaZ != 0) {
        QMatrix4x4 proj;
        float *writePtr = proj.data();
        writePtr[0] = 1.0f / halfWidth;
        writePtr[5] = 1.0f / halfHeight;
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
    int orthoCasterIdx = 0;
    int cubeCasterIdx = 0;
    QVariantList custMatCubeShadowMaps;
    QVariantList custMatOrthoShadowMaps;

    for (Q3DSLayerAttached::PerLightShadowMapData &d : layerData->shadowMapData.shadowCasters)
        d.active = false;

    // Go through the list of visible (eyeball==true) lights and pick the ones with castshadow==true.
    for (Q3DSLightNode *light3DS : qAsConst(layerData->lightsData->lightNodes)) {
        Q_ASSERT(light3DS->flags().testFlag(Q3DSNode::Active));
        // Note the global indexing for shadow map uniforms too: a casting, a
        // noncasting, and another shadow casting point light leads to uniforms
        // like shadowcube1 and shadowcube3. (with the default material)
        const QString lightIndexStr = QString::number(lightIdx++);
        // On the other hand custom materials need a shadowIdx to index a
        // shadowMaps or shadowCubes sampler uniform array. Hence ortho/cubeCasterIdx.

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
                qCDebug(lcPerf, "Slow path! Recreating shadow map textures for light %s", light3DS->id().constData());
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
                    dsTexOrRb->setFormat(Qt3DRender::QAbstractTexture::D16);
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

                Qt3DRender::QAbstractTexture::TextureFormat format = Qt3DRender::QAbstractTexture::R16_UNorm;
                // GL_R16 does not seem to exist in OpenGL ES unless GL_EXT_texture_norm16
                // is present.  Fall back to the GL_R8 when not supported (but this leads
                // to uglier output with more artifacts).
                if (!m_gfxLimits.norm16TexturesSupported)
                    format = Qt3DRender::QAbstractTexture::R8_UNorm;

                d->shadowMapTexture->setFormat(format);
                d->shadowMapTextureTemp->setFormat(format);

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

                // for custom materials
                if (!layerData->shadowMapData.custMatParams.numShadowCubesParam) {
                    layerData->shadowMapData.custMatParams.numShadowCubesParam = new Qt3DRender::QParameter(layerData->entity);
                    layerData->shadowMapData.custMatParams.numShadowCubesParam->setName(QLatin1String("uNumShadowCubes"));
                }
                if (!layerData->shadowMapData.custMatParams.numShadowMapsParam) {
                    layerData->shadowMapData.custMatParams.numShadowMapsParam = new Qt3DRender::QParameter(layerData->entity);
                    layerData->shadowMapData.custMatParams.numShadowMapsParam->setName(QLatin1String("uNumShadowMaps"));
                }
                if (!layerData->shadowMapData.custMatParams.shadowMapSamplersParam) {
                    layerData->shadowMapData.custMatParams.shadowMapSamplersParam = new Qt3DRender::QParameter(layerData->entity);
                    layerData->shadowMapData.custMatParams.shadowMapSamplersParam->setName(QLatin1String("shadowMaps[0]")); // will pass all array elems as one param
                }
                if (!layerData->shadowMapData.custMatParams.shadowCubeSamplersParam) {
                    layerData->shadowMapData.custMatParams.shadowCubeSamplersParam = new Qt3DRender::QParameter(layerData->entity);
                    layerData->shadowMapData.custMatParams.shadowCubeSamplersParam->setName(QLatin1String("shadowCubes[0]")); // will pass all array elems as one param
                }

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
                        shadowRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Depth);
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
                    custMatCubeShadowMaps.append(QVariant::fromValue(d->shadowMapTexture));
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
                    shadowRtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Depth);
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
                    custMatOrthoShadowMaps.append(QVariant::fromValue(d->shadowMapTexture));
                } // if isCube
            } else {
                // !needsFramegraph -> update the values that could have changed
                if (isCube) {
                    for (int faceIdx = 0; faceIdx < 6; ++faceIdx)
                        updateCubeShadowCam(d, faceIdx, light3DS);

                    updateCubeShadowMapParams(d, light3DS, lightIndexStr);
                    custMatCubeShadowMaps.append(QVariant::fromValue(d->shadowMapTexture));
                } else {
                    updateOrthoShadowCam(d, light3DS, layerData);
                    updateOrthoShadowMapParams(d, light3DS, lightIndexStr);
                    custMatOrthoShadowMaps.append(QVariant::fromValue(d->shadowMapTexture));
                }
            }

            if (isCube) {
                light3DS->attached<Q3DSLightAttached>()->lightSource.shadowIdxParam->setValue(cubeCasterIdx);
                ++cubeCasterIdx;
                Q_ASSERT(custMatCubeShadowMaps.count() == cubeCasterIdx);
            } else {
                light3DS->attached<Q3DSLightAttached>()->lightSource.shadowIdxParam->setValue(orthoCasterIdx);
                ++orthoCasterIdx;
                Q_ASSERT(custMatOrthoShadowMaps.count() == orthoCasterIdx);
            }
        } else {
            // non-shadow-casting light
            light3DS->attached<Q3DSLightAttached>()->lightSource.shadowIdxParam->setValue(-1);
        }
    }

    if (layerData->shadowMapData.custMatParams.numShadowCubesParam)
        layerData->shadowMapData.custMatParams.numShadowCubesParam->setValue(cubeCasterIdx);
    if (layerData->shadowMapData.custMatParams.numShadowMapsParam)
        layerData->shadowMapData.custMatParams.numShadowMapsParam->setValue(orthoCasterIdx);
    if (layerData->shadowMapData.custMatParams.shadowMapSamplersParam)
        layerData->shadowMapData.custMatParams.shadowMapSamplersParam->setValue(custMatOrthoShadowMaps);
    if (layerData->shadowMapData.custMatParams.shadowCubeSamplersParam)
        layerData->shadowMapData.custMatParams.shadowCubeSamplersParam->setValue(custMatCubeShadowMaps);

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
        qCDebug(lcPerf, "Layer %s has %d shadow casting lights", layer3DS->id().constData(), layerData->shadowMapData.shadowCasters.count());
}

static Qt3DRender::QRenderTargetSelector *createProgressiveTemporalAAFramegraph(Qt3DCore::QNode *parent,
                                                                                Qt3DRender::QRenderTarget *rt,
                                                                                Qt3DRender::QLayer *tag,
                                                                                Qt3DRender::QLayerFilter **layerFilter,
                                                                                Qt3DRender::QParameter **accumTexParam,
                                                                                Qt3DRender::QParameter **lastTexParam,
                                                                                Qt3DRender::QParameter **blendFactorsParam)
{
    Qt3DRender::QRenderTargetSelector *rtSel = new Qt3DRender::QRenderTargetSelector(parent);
    rtSel->setTarget(rt);

    Qt3DRender::QViewport *viewport = new Qt3DRender::QViewport(rtSel);
    viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

    Qt3DRender::QFilterKey *filterKey = new Qt3DRender::QFilterKey;
    filterKey->setName(QLatin1String("pass"));
    filterKey->setValue(QLatin1String("progaa"));
    Qt3DRender::QRenderPassFilter *filter = new Qt3DRender::QRenderPassFilter(viewport);
    filter->addMatch(filterKey);

    *layerFilter = new Qt3DRender::QLayerFilter(filter);
    (*layerFilter)->addLayer(tag);

    *accumTexParam = new Qt3DRender::QParameter;
    (*accumTexParam)->setName(QLatin1String("accumulator"));
    *lastTexParam = new Qt3DRender::QParameter;
    (*lastTexParam)->setName(QLatin1String("last_frame"));
    *blendFactorsParam = new Qt3DRender::QParameter;
    (*blendFactorsParam)->setName(QLatin1String("blend_factors"));

    filter->addParameter(*accumTexParam);
    filter->addParameter(*lastTexParam);
    filter->addParameter(*blendFactorsParam);

    return rtSel;
}

static QVector2D adjustedProgressiveTemporalAAVertexOffset(const QVector2D &vertexOffset, Q3DSLayerAttached *data)
{
    const QSize layerPixelSize = safeLayerPixelSize(data);
    const bool leftHanded = data->cam3DS->orientation() == Q3DSNode::LeftHanded;
    const float lhFactor = leftHanded ? -1.0f : 1.0f;
    const float camZ = data->cam3DS->position().z() * lhFactor;
    return QVector2D(vertexOffset.x() / (layerPixelSize.width() / 2.0f) * camZ,
                     vertexOffset.y() / (layerPixelSize.height() / 2.0f) * camZ);
}

void Q3DSSceneManager::stealLayerRenderTarget(Qt3DRender::QAbstractTexture **stolenColorBuf, Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    if (*stolenColorBuf) {
        // the previous stolen color buffer it not needed anymore
        data->sizeManagedTextures.removeOne(*stolenColorBuf);
        delete *stolenColorBuf;
    }
    *stolenColorBuf = data->effectActive ? data->effLayerTexture : data->layerTexture;
    // create a whole new render target for the layer
    data->sizeManagedTextures.removeOne(*stolenColorBuf);
    const QSize layerPixelSize = safeLayerPixelSize(data);
    const int msaaSampleCount = 0;
    Qt3DRender::QAbstractTexture *colorTex;
    Qt3DRender::QAbstractTexture *dsTexOrRb;
    Qt3DRender::QRenderTarget *rt = newLayerRenderTarget(layerPixelSize, msaaSampleCount,
                                                         &colorTex, &dsTexOrRb,
                                                         data->layerFgRoot,
                                                         layer3DS,
                                                         data->layerDS); // keep using the existing depth-stencil buffer
    Qt3DRender::QRenderTarget *oldRt = data->rtSelector->target();
    data->rtSelector->setTarget(rt);
    delete oldRt;
    if (data->effectActive) {
        data->sizeManagedTextures.append(colorTex);
        data->effLayerTexture = colorTex;
    } else {
        data->sizeManagedTextures.insert(0, colorTex);
        data->layerTexture = colorTex;
    }

    // update descriptions for profiler
    m_profiler->updateObjectInfo(*stolenColorBuf, Q3DSProfiler::Texture2DObject,
                                 "PAA/TAA color buffer for layer %s", layer3DS->id().constData());
}

Qt3DRender::QAbstractTexture *Q3DSSceneManager::createProgressiveTemporalAAExtraBuffer(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Qt3DRender::QAbstractTexture *tex = new Qt3DRender::QTexture2D(data->layerFgRoot);
    m_profiler->trackNewObject(tex, Q3DSProfiler::Texture2DObject,
                               "PAA/TAA work buffer for layer %s", layer3DS->id().constData());
    tex->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
    tex->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
    tex->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
    prepareSizeDependentTexture(tex, layer3DS);
    return tex;
}

//#define PAA_ALWAYS_ON

static const int MAX_AA_LEVELS = 8;

// Called once per frame (in the preparation step from the frame action) for
// each progressive AA enabled layer.
bool Q3DSSceneManager::updateProgressiveAA(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    if (!data || !data->cam3DS)
        return false;

    // No prog.aa for msaa/ssaa layers - 3DS1 supports this (would need the
    // usual resolve step with blitframebuffer) but we'll live with this
    // limitation for now.
    if (data->msaaSampleCount > 1 || data->ssaaScaleFactor > 1)
        return false;

    // When a frame applies an offset to the camera's matrix, the next frame
    // must reset it. This must happen regardless of having PAA active in the
    // next frame.
    if (data->progAA.cameraAltered) {
        data->progAA.cameraAltered = false;
        setLayerCameraSizeProperties(layer3DS);
    }

    if (data->layerSize.isEmpty())
        return false;

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
        // State is Idle. Keep displaying the output in currentOutputTexture until
        // wasDirty becomes true and so pass gets reset.
        if (data->progAA.layerFilter->layers().contains(m_fsQuadTag))
            data->progAA.layerFilter->removeLayer(m_fsQuadTag);
#ifdef PAA_ALWAYS_ON
        data->progAA.pass = 0;
#endif
        return true;
    }

    if (data->progAA.pass < 1 + PROGAA_FRAME_DELAY) {
        // State is Inactive. Must make sure no progAA output is shown.
        ++data->progAA.pass;
        if (data->progAA.enabled) {
            qCDebug(lcScene, "Stopping progressive AA for layer %s", layer3DS->id().constData());
            data->progAA.enabled = false;
            data->compositorSourceParam->setValue(QVariant::fromValue(data->effectActive ? data->effLayerTexture : data->layerTexture));

            // Do not delete and then recreate the framegraph subtree since
            // that is likely way too expensive. Keep it around instead and
            // disable it by making sure it has no renderable entities. This
            // could have already been done in the > maxPass branch above but
            // won't hurt to repeat since we may get a dirty frame before
            // reaching the maximum accumulation level.
            data->progAA.layerFilter->removeLayer(m_fsQuadTag);
        }
        return false;
    }

    // State is Active.

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

    const int factorsIdx = data->progAA.pass - (1 + PROGAA_FRAME_DELAY);
    const QVector2D vertexOffset = adjustedProgressiveTemporalAAVertexOffset(vertexOffsets[factorsIdx], data);
    const QVector2D blendFactor = blendFactors[factorsIdx];

    if (!data->progAA.enabled && data->progAA.fg && data->progAA.layerFilter)
        data->progAA.layerFilter->addLayer(m_fsQuadTag);

    data->progAA.enabled = true;
    if (factorsIdx == 0)
        qCDebug(lcPerf, "Kicking off progressive AA for layer %s", layer3DS->id().constData());

    // Alter the camera's matrix by a little movement based on the current
    // vertexOffset. This applies to the camera used by the main layer passes.
    setLayerCameraSizeProperties(layer3DS, vertexOffset);
    data->progAA.cameraAltered = true;

    // data->layerTexture is the original contents (albeit with jiggled
    // camera), generated by the main layer passes. Have two additional
    // textures, and swap them every frame (the one that was used as input will
    // be used as output in the next frame). The layer compositor will be
    // switched over to use always the one that is the (blended) output.

    if (!data->progAA.extraColorBuf)
        data->progAA.extraColorBuf = createProgressiveTemporalAAExtraBuffer(layer3DS);

    // ### depth, ssao, shadow passes in the main layer framegraph subtree should be disabled when pass > 0

    // For the other buffer there is no new texture needed - instead,
    // steal data->(eff)layerTexture. (note: already in sizeManagedTextures)
    if (factorsIdx == 0) {
        stealLayerRenderTarget(&data->progAA.stolenColorBuf, layer3DS);
        // start with the existing color buffer as the accumulator
        data->progAA.currentAccumulatorTexture = data->progAA.stolenColorBuf;
        data->progAA.currentOutputTexture = data->progAA.extraColorBuf;
    }

    // ### have to do this on every new PAA run since accumulatorTexture changes above.
    // It is an overkill though since the framegraph could be generated just once.
    if (factorsIdx == 0) {
        delete data->progAA.fg;

        // set up a framegraph subtree to render a quad

        // Due to QTBUG-64757 we will need to switch QRenderTargets, not just
        // the texture in QRenderTargetOutput. Hence the need for multiple
        // ones. This is probably more efficient anyways (since it results in
        // binding a different FBO, without altering attachments).

        data->progAA.curTarget = 0;
        for (Qt3DRender::QAbstractTexture *t : { data->progAA.currentOutputTexture, data->progAA.currentAccumulatorTexture }) {
            Qt3DRender::QRenderTargetOutput *rtOutput = new Qt3DRender::QRenderTargetOutput;
            rtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
            rtOutput->setTexture(t);
            Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
            m_profiler->trackNewObject(rt, Q3DSProfiler::RenderTargetObject,
                                       "Progressive AA RT for layer %s", layer3DS->id().constData());
            rt->addOutput(rtOutput);
            data->progAA.rts[data->progAA.curTarget++] = rt;
        }

        data->progAA.curTarget = 0;
        auto rtSel = createProgressiveTemporalAAFramegraph(data->layerFgRoot,
                                                           data->progAA.rts[data->progAA.curTarget],
                                                           m_fsQuadTag,
                                                           &data->progAA.layerFilter,
                                                           &data->progAA.accumTexParam,
                                                           &data->progAA.lastTexParam,
                                                           &data->progAA.blendFactorsParam);
        data->progAA.fg = data->progAA.rtSel = rtSel;
    }

    // Input
    data->progAA.accumTexParam->setValue(QVariant::fromValue(data->progAA.currentAccumulatorTexture));
    data->progAA.lastTexParam->setValue(QVariant::fromValue(data->effectActive ? data->effLayerTexture : data->layerTexture));
    data->progAA.blendFactorsParam->setValue(blendFactor);

    // Output
    data->progAA.rtSel->setTarget(data->progAA.rts[data->progAA.curTarget]);

    // have the compositor use the blended results instead of the layer texture
    data->compositorSourceParam->setValue(QVariant::fromValue(data->progAA.currentOutputTexture));

    // In the next PAA round (i.e. the frame after the next one) reuse accumTex
    // as the output and the current output as accumTex...
    std::swap(data->progAA.currentAccumulatorTexture, data->progAA.currentOutputTexture);
    // ...whereas the output of the next frame will be used as input, so flip the
    // index to use the correct render target.
    data->progAA.curTarget = 1 - data->progAA.curTarget;

    ++data->progAA.pass;

    // Make sure layer caching does not interfere and the layer's framegraph subtree
    // is still active as long as PAA accumulation is active.
    m_layerUncachePending = true;

    return true;
}

void Q3DSSceneManager::updateTemporalAA(Q3DSLayerNode *layer3DS)
{
    static const int MAX_TEMPORAL_AA_LEVELS = 2;

    Q3DSLayerAttached *data = layer3DS->attached<Q3DSLayerAttached>();
    if (!data || !data->cam3DS)
        return;

    // No TempAA for MSAA/SSAA layers for now.
    if (data->msaaSampleCount > 1 || data->ssaaScaleFactor > 1)
        return;

    // Temporal AA is like a 2x progressive AA while the layer has movement (is
    // dirty). Accumulation stops after 2 non-dirty frames but apart from that
    // it is "always on".

    if (data->wasDirty)
        data->tempAA.nonDirtyPass = 0;

    if (data->tempAA.nonDirtyPass >= MAX_TEMPORAL_AA_LEVELS)
        return;

    static const QVector2D vertexOffsets[MAX_TEMPORAL_AA_LEVELS] = {
        QVector2D(0.3f, 0.3f),
        QVector2D(-0.3f, -0.3f)
    };
    static const QVector2D blendFactor(0.5f, 0.5f);

    const QVector2D vertexOffset = adjustedProgressiveTemporalAAVertexOffset(vertexOffsets[data->tempAA.passIndex], data);

    if (!data->tempAA.fg) {
        data->tempAA.extraColorBuf = createProgressiveTemporalAAExtraBuffer(layer3DS);

        stealLayerRenderTarget(&data->tempAA.stolenColorBuf, layer3DS);
        // start with the existing color buffer as the accumulator
        data->tempAA.currentAccumulatorTexture = data->tempAA.stolenColorBuf;
        data->tempAA.currentOutputTexture = data->tempAA.extraColorBuf;

        int i = 0;
        for (Qt3DRender::QAbstractTexture *t : { data->tempAA.currentOutputTexture, data->tempAA.currentAccumulatorTexture }) {
            Qt3DRender::QRenderTargetOutput *rtOutput = new Qt3DRender::QRenderTargetOutput;
            rtOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
            rtOutput->setTexture(t);
            Qt3DRender::QRenderTarget *rt = new Qt3DRender::QRenderTarget;
            m_profiler->trackNewObject(rt, Q3DSProfiler::RenderTargetObject,
                                       "Temporal AA RT for layer %s", layer3DS->id().constData());
            rt->addOutput(rtOutput);
            data->tempAA.rts[i++] = rt;
        }

        auto rtSel = createProgressiveTemporalAAFramegraph(data->layerFgRoot,
                                                           data->tempAA.rts[0],
                                                           m_fsQuadTag,
                                                           &data->tempAA.layerFilter,
                                                           &data->tempAA.accumTexParam,
                                                           &data->tempAA.lastTexParam,
                                                           &data->tempAA.blendFactorsParam);
        data->tempAA.fg = data->tempAA.rtSel = rtSel;

        data->tempAA.layerFilter->addLayer(m_fsQuadTag);
    }

    // Jiggle
    setLayerCameraSizeProperties(layer3DS, vertexOffset);

    // Input
    data->tempAA.accumTexParam->setValue(QVariant::fromValue(data->tempAA.currentAccumulatorTexture));
    data->tempAA.lastTexParam->setValue(QVariant::fromValue(data->effectActive ? data->effLayerTexture : data->layerTexture));
    data->tempAA.blendFactorsParam->setValue(blendFactor);

    // Output
    data->tempAA.rtSel->setTarget(data->tempAA.rts[data->tempAA.passIndex]);

    // have the compositor use the blended results instead of the layer texture
    data->compositorSourceParam->setValue(QVariant::fromValue(data->tempAA.currentOutputTexture));

    // play ping-pong with the buffers
    std::swap(data->tempAA.currentAccumulatorTexture, data->tempAA.currentOutputTexture);

    // Make sure layer caching does not interfere and the layer's framegraph subtree
    // is still active as long as TAA accumulation is active.
    m_layerUncachePending = true;

    data->tempAA.passIndex = (data->tempAA.passIndex + 1) % MAX_TEMPORAL_AA_LEVELS;
    ++data->tempAA.nonDirtyPass;
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
                                            Qt3DRender::QLayer *tag, BuildLayerQuadFlags flags, int layerDepth)
{
    // Generates the layer's compositorEntity and the components on it.

    // Watch out for parenting: this entity will get destroyed and then
    // recreated eventually, therefore it and its components must not take
    // ownership of longer living objects.

    qCDebug(lcScene, "Generating compositor quad for %s with logical depth %d", layer3DS->id().constData(), layerDepth);

    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    Q_ASSERT(data);
    Qt3DCore::QEntity *layerQuadEntity = new Qt3DCore::QEntity;
    layerQuadEntity->setObjectName(QObject::tr("compositor quad for %1").arg(QString::fromUtf8(layer3DS->id())));
    layerQuadEntity->setParent(parentEntity); // the separate setParent() call is intentional here, see QTBUG-69352
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
    data->updateCompositorCalculations = [data, layerQuadEntity, tag, mesh, transform, layerDepth]() {
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
                                            0 - layerDepth * 0.0001f));
    };

    Qt3DRender::QMaterial *material = new Qt3DRender::QMaterial;
    Qt3DRender::QEffect *effect = new Qt3DRender::QEffect;
    Qt3DRender::QTechnique *technique = new Qt3DRender::QTechnique;
    Q3DSDefaultMaterialGenerator::addDefaultApiFilter(technique);

    Qt3DRender::QRenderPass *renderPass = new Qt3DRender::QRenderPass;

    if (flags.testFlag(LayerQuadBlend)) {
        Qt3DRender::QBlendEquation *blendFunc = new Qt3DRender::QBlendEquation;
        Qt3DRender::QBlendEquationArguments *blendArgs = new Qt3DRender::QBlendEquationArguments;
        setLayerBlending(blendFunc, blendArgs, layer3DS);
        renderPass->addRenderState(blendFunc);
        renderPass->addRenderState(blendArgs);
    }

    Qt3DRender::QDepthTest *depthTest = new Qt3DRender::QDepthTest;
    depthTest->setDepthFunction(Qt3DRender::QDepthTest::Less);
    renderPass->addRenderState(depthTest);

    data->compositorRenderPass = renderPass;

    if (!flags.testFlag(LayerQuadCustomShader)) {
        updateLayerCompositorProgram(layer3DS);

        if (!data->compositorSourceParam->parent())
            data->compositorSourceParam->setParent(data->layerSceneRootEntity);

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

// to support switching between msaa and non-msaa composition at runtime the
// program is (re)set in this separate function
void Q3DSSceneManager::updateLayerCompositorProgram(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    if (data && data->compositorRenderPass && data->usesDefaultCompositorProgram) {
        Qt3DRender::QShaderProgram *shaderProgram = data->compositorRenderPass->shaderProgram();
        const bool programIsNew = !shaderProgram;
        if (programIsNew)
            shaderProgram = new Qt3DRender::QShaderProgram;

        const bool gles = m_gfxLimits.format.renderableType() == QSurfaceFormat::OpenGLES;
        const bool msaa = data->msaaSampleCount > 1 && !data->effectActive;

        // 1. compositor.vert + compositor.frag -> GLSL 100
        // 2. compositor_core.vert + compositor_core.frag -> 330 core
        // 3. compositor_ms.vert + compositor_ms2|ms4.frag -> GLSL 310 (MSAA)
        // 4. compositor_core.vert + compositor_ms2|ms4_core.frag -> 330 core (MSAA)
        //
        // (3. uses a separate vertex shader only to avoid linking together 100 and 310 which is not supported as per spec)

        QString vertSuffix;
        QString fragSuffix;
        if (gles) {
            vertSuffix = msaa ? QLatin1String("_ms.vert") : QLatin1String(".vert");
            fragSuffix = QLatin1String(".frag");
        } else {
            vertSuffix = QLatin1String("_core.vert");
            fragSuffix = QLatin1String("_core.frag");
        }

        const QString vertSrc = QLatin1String("qrc:/q3ds/shaders/compositor") + vertSuffix;
        shaderProgram->setVertexShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl(vertSrc)));

        QString fragSrc;
        if (msaa)
            fragSrc = QLatin1String("qrc:/q3ds/shaders/compositor_ms") + QString::number(data->msaaSampleCount) + fragSuffix;
        else
            fragSrc = QLatin1String("qrc:/q3ds/shaders/compositor") + fragSuffix;

        shaderProgram->setFragmentShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl(fragSrc)));

        if (programIsNew)
            data->compositorRenderPass->setShaderProgram(shaderProgram);
    }
}

void Q3DSSceneManager::buildCompositor(Qt3DRender::QFrameGraphNode *parent, Qt3DCore::QEntity *parentEntity)
{
    // Simplified view (excluding advanced blending-specific nodes):
    // MatteRoot - NoDraw
    //           - [Clear - NoDraw]
    // Viewport - CameraSelector - [Scissor] - ClearBuffers - NoDraw
    //                           - "m_compositorRoot" (destroyed and recreated on-the-fly when needed)
    //                               - LayerFilter for layer quad entity 0
    //                               - LayerFilter for layer quad entity 1
    //                                 ...
    // With standard blend modes only the list is simplified to a single LayerFilter and QLayer tag.

    // If Matte is enabled in the ViewerSettings, then we need clear with the matte color first
    // then later restrict the scene clear color to the vieport rect with a scissor test
    m_viewportData.drawMatteNode = new Qt3DRender::QFrameGraphNode(parent);
    new Qt3DRender::QNoDraw(m_viewportData.drawMatteNode);
    m_viewportData.dummyMatteRoot = new Qt3DCore::QNode(m_rootEntity);
    m_viewportData.matteClearBuffers = new Qt3DRender::QClearBuffers(m_viewportData.dummyMatteRoot);
    m_viewportData.matteClearBuffers->setBuffers(Qt3DRender::QClearBuffers::ColorDepthStencilBuffer);
    m_viewportData.matteClearBuffers->setClearColor(Qt::black);
    new Qt3DRender::QNoDraw(m_viewportData.matteClearBuffers);

    m_viewportData.viewport = new Qt3DRender::QViewport(parent);
    m_viewportData.viewport->setNormalizedRect(QRectF(0, 0, 1, 1));

    Qt3DRender::QCamera *camera = new Qt3DRender::QCamera;
    camera->setObjectName(QLatin1String("compositor camera"));
    camera->setProjectionType(Qt3DRender::QCameraLens::OrthographicProjection);
    camera->setLeft(-1);
    camera->setRight(1);
    camera->setTop(1);
    camera->setBottom(-1);
    camera->setPosition(QVector3D(0, 0, 1));
    camera->setViewCenter(QVector3D(0, 0, 0));

    Qt3DRender::QCameraSelector *cameraSelector = new Qt3DRender::QCameraSelector(m_viewportData.viewport);
    cameraSelector->setCamera(camera);

    m_viewportData.matteRenderState = new Qt3DRender::QRenderStateSet(cameraSelector);
    m_viewportData.matteRenderState->setEnabled(false);
    m_viewportData.matteScissorTest = new Qt3DRender::QScissorTest(m_viewportData.matteRenderState);
    m_viewportData.matteRenderState->addRenderState(m_viewportData.matteScissorTest);

    Qt3DRender::QClearBuffers *clearBuffers = new Qt3DRender::QClearBuffers(m_viewportData.matteRenderState);
    clearBuffers->setBuffers(Qt3DRender::QClearBuffers::ColorDepthStencilBuffer);
    QColor clearColor = Qt::transparent;
    if (m_scene->useClearColor()) {
        // Alpha is 1 here even for subpresentations. Otherwise there would
        // be no way to get the background visible when used as a texture later on.
        clearColor = m_scene->clearColor();
    }
    clearBuffers->setClearColor(clearColor);

    new Qt3DRender::QNoDraw(clearBuffers);

    m_compositorFgContainer = cameraSelector;
    m_compositorParentEntity = parentEntity;
    rebuildCompositorLayerChain();
}

void Q3DSSceneManager::rebuildCompositorLayerChain()
{
    // The order of layers in the object tree is front-to-back which is somewhat
    // unintuitive. Anyways, while it might seem that the Z value in the quad's
    // transform (based on a unique layerDepth) is sufficient to ensure the
    // correct ordering during composition (and so a full rebuild of the frame
    // and scene graph for the compositor quads is not necessary after the
    // initial buildCompositor() run), that isn't true for advanced blend mode
    // emulation because that depends heavily on back-to-front order in the
    // framegraph. Therefore we'll keep rebuilding the layer quads and
    // corresponding framegraph entries whenever the list of layers change.

    delete m_compositorRoot;
    qDeleteAll(m_compositorEntities);
    m_compositorEntities.clear();

    m_compositorRoot = new Qt3DRender::QFrameGraphNode(m_compositorFgContainer);

    QVarLengthArray<Q3DSLayerNode *, 16> layers;
    Q3DSUipPresentation::forAllLayers(m_scene, [&layers](Q3DSLayerNode *layer3DS) {
        layers.append(layer3DS);
    }, true); // process layers in reverse order

    qCDebug(lcPerf, "Composing %d layers in presentation %s",
            layers.count(), qPrintable(m_profiler->presentationName()));

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
        // standard case: 1 LayerFilter leaf to the framegraph, N entities (with quad mesh) to the scenegraph
        Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(m_compositorRoot);
        Qt3DRender::QLayer *tag = new Qt3DRender::QLayer; // just let it be parented to the LayerFilter
        tag->setObjectName(QLatin1String("Compositor quad pass"));
        layerFilter->addLayer(tag);
        // This works also when 'layers' is empty since we have a single leaf
        // (the LayerFilter) that will get no match for its filter.

        int layerDepth = 1;
        for (Q3DSLayerNode *layer3DS : layers) {
            BuildLayerQuadFlags flags = 0;
            if (layer3DS->layerBackground() == Q3DSLayerNode::Transparent)
                flags |= LayerQuadBlend;

            buildLayerQuadEntity(layer3DS, m_compositorParentEntity, tag, flags, layerDepth++);
            m_compositorEntities.append(layer3DS->attached<Q3DSLayerAttached>()->compositorEntity);
        }
    } else {
        qCDebug(lcPerf, "Some layers use an advanced blend mode in presentation %s. "
                        "This is slower due to an extra blit and a custom blend shader.",
                qPrintable(m_profiler->presentationName()));

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
        int layerDepth = 1;
        for (Q3DSLayerNode *layer3DS : layers) {
            Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
            Q_ASSERT(data);

            if (layerNeedsAdvancedBlending(layer3DS)) {
                data->usesDefaultCompositorProgram = false;

                if (!data->advBlend.tempTexture) {
                    data->advBlend.tempTexture = new Qt3DRender::QTexture2D(data->entity);
                    m_profiler->trackNewObject(data->advBlend.tempTexture, Q3DSProfiler::Texture2DObject,
                                               "Advanced blend texture for layer %s", layer3DS->id().constData());
                    data->advBlend.tempTexture->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
                    data->advBlend.tempTexture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
                    data->advBlend.tempTexture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
                    // must track layer size, but without the SSAA scale
                    prepareSizeDependentTexture(data->advBlend.tempTexture, layer3DS, nullptr, Q3DSLayerAttached::SizeManagedTexture::IgnoreSSAA);
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

                Qt3DRender::QBlitFramebuffer *bgBlit = new Qt3DRender::QBlitFramebuffer(m_compositorRoot);
                bgBlit->setDestination(data->advBlend.tempRt);
                new Qt3DRender::QNoDraw(bgBlit);

                // Layer size dependent properties have to be updated dynamically.
                auto setSizeDependentValues = [bgBlit, layer3DS, data, this](Q3DSLayerNode *changedLayer) {
                    if (changedLayer != layer3DS)
                        return;

                    if (data->layerSize.isEmpty())
                        return;

                    const QPointF pos = data->layerPos + m_viewportData.viewportRect.topLeft() * m_viewportData.viewportDpr;

                    // this assumes QTBUG-65123 is fixed
                    QRectF srcRect(pos, data->layerSize);
                    bgBlit->setSourceRect(srcRect);
                    QRectF dstRect(QPointF(0, 0), data->layerSize);
                    bgBlit->setDestinationRect(dstRect);
                };
                data->layerSizeChangeCallbacks.append(setSizeDependentValues);
                // Set some initial sizes.
                setSizeDependentValues(layer3DS);

                Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(m_compositorRoot);
                Qt3DRender::QLayer *tag = new Qt3DRender::QLayer;
                tag->setObjectName(QLatin1String("Adv. blend mode compositor quad pass"));
                layerFilter->addLayer(tag);

                // Now we do not need normal blending and will provide a custom shader program.
                BuildLayerQuadFlags flags = LayerQuadCustomShader;
                buildLayerQuadEntity(layer3DS, m_compositorParentEntity, tag, flags, layerDepth++);
                m_compositorEntities.append(data->compositorEntity);
                Qt3DRender::QRenderPass *renderPass = data->compositorRenderPass;

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
                blendLayerParam->setValue(QVariant::fromValue(data->effectActive ? data->effLayerTexture : data->layerTexture));
                renderPass->addParameter(baseLayerParam);
                renderPass->addParameter(blendLayerParam);
            } else {
                data->usesDefaultCompositorProgram = true;

                Qt3DRender::QLayerFilter *layerFilter = new Qt3DRender::QLayerFilter(m_compositorRoot);
                Qt3DRender::QLayer *tag = new Qt3DRender::QLayer;
                tag->setObjectName(QLatin1String("Compositor quad pass (adv.blend path)"));
                layerFilter->addLayer(tag);

                BuildLayerQuadFlags flags = 0;
                if (layer3DS->layerBackground() == Q3DSLayerNode::Transparent)
                    flags |= LayerQuadBlend;

                buildLayerQuadEntity(layer3DS, m_compositorParentEntity, tag, flags, layerDepth++);
                m_compositorEntities.append(data->compositorEntity);
            }
        }
    }

    // When dynamically introducing a new layer, it is essential to re-run the
    // transform calculations for all layers because the logical depth
    // (layerDepth) based on which Z is generated may change for some of them.
    for (Q3DSLayerNode *layer3DS : layers)
        layer3DS->attached<Q3DSLayerAttached>()->updateCompositorCalculations();
}

void Q3DSSceneManager::buildGuiPass(Qt3DRender::QFrameGraphNode *parent, Qt3DCore::QEntity *parentEntity)
{
    // No dependencies to the actual gui renderer here. Isolate that to
    // profileui. The interface consists of tags and filter keys. profileui can
    // then assume the framegraph has the necessary LayerFilters both for
    // including and excluding.

    m_guiData.tag = new Qt3DRender::QLayer(parentEntity); // all gui entities are tagged with this
    m_guiData.activeTag = new Qt3DRender::QLayer(parentEntity); // active gui entities - added/removed to entities dynamically by imguimanager
    m_guiData.techniqueFilterKey = new Qt3DRender::QFilterKey;
    m_guiData.techniqueFilterKey->setName(QLatin1String("type"));
    m_guiData.techniqueFilterKey->setValue(QLatin1String("gui"));
    m_guiData.rootEntity = parentEntity;

    Qt3DRender::QTechniqueFilter *tfilter = new Qt3DRender::QTechniqueFilter(parent);
    m_profiler->reportFrameGraphStopNode(tfilter);
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

Qt3DCore::QEntity *Q3DSSceneManager::buildFsQuad(const FsQuadParams &info)
{
    Q_ASSERT(info.tag);
    Q_ASSERT(info.passNames.count() == info.passProgs.count());

    Qt3DCore::QEntity *fsQuadEntity = new Qt3DCore::QEntity;
    fsQuadEntity->setObjectName(QLatin1String("fullscreen quad"));
    fsQuadEntity->setParent(info.parentEntity); // the separate setParent() call is intentional here, see QTBUG-69352

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

    return fsQuadEntity;
}

void Q3DSSceneManager::initBehaviorInstance(Q3DSBehaviorInstance *behaviorInstance)
{
    Q3DSBehaviorAttached *data = new Q3DSBehaviorAttached;
    behaviorInstance->setAttached(data);

    data->propertyChangeObserverIndex = behaviorInstance->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    data->eventObserverIndex = behaviorInstance->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));
}

void Q3DSSceneManager::initImage(Q3DSImage *image)
{
    Q3DSImageAttached *data = new Q3DSImageAttached;
    data->entity = m_rootEntity; // must set an entity to to make Q3DSImage properties animatable, just use the root
    image->setAttached(data);

    data->propertyChangeObserverIndex = image->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    data->eventObserverIndex = image->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));
}

void Q3DSSceneManager::initDefaultMaterial(Q3DSDefaultMaterial *m)
{
    Q3DSDefaultMaterialAttached *data = new Q3DSDefaultMaterialAttached;
    // the real work is deferred to prepareDefaultMaterial()
    m->setAttached(data);

    data->propertyChangeObserverIndex = m->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    data->eventObserverIndex = m->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));
}

void Q3DSSceneManager::initCustomMaterial(Q3DSCustomMaterialInstance *m)
{
    Q3DSCustomMaterialAttached *data = new Q3DSCustomMaterialAttached;
    // the real work is deferred to prepareCustomMaterial()
    m->setAttached(data);

    data->propertyChangeObserverIndex = m->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    data->eventObserverIndex = m->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));
}

// Recursively builds the Qt3D scene for the given node tree under a layer. (or
// sub-tree, when called in response to a dynamic scene change with DirtyNodeAdded).
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
        switch (obj->type()) {
        case Q3DSGraphObject::Image:
            // already done, nothing to do here
            break;
        case Q3DSGraphObject::Effect:
            buildEffect(static_cast<Q3DSEffectInstance *>(obj), layer3DS);
            break;
        case Q3DSGraphObject::Behavior:
            initBehaviorInstance(static_cast<Q3DSBehaviorInstance *>(obj));
            break;
        case Q3DSGraphObject::DefaultMaterial:
            initDefaultMaterial(static_cast<Q3DSDefaultMaterial *>(obj));
            break;
        case Q3DSGraphObject::CustomMaterial:
            initCustomMaterial(static_cast<Q3DSCustomMaterialInstance *>(obj));
            break;
        default:
            break;
        }

        if (obj->attached())
            obj->attached()->component = m_componentNodeStack.top();

        return;
    }

    Qt3DCore::QEntity *newEntity = nullptr;

    switch (obj->type()) {
    case Q3DSGraphObject::Light:
        newEntity = buildLight(static_cast<Q3DSLightNode *>(obj), layer3DS, parent);
        addChildren(obj, newEntity);
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
    case Q3DSGraphObject::Alias:
    {
        auto alias = static_cast<Q3DSAliasNode *>(obj);
        newEntity = buildAlias(alias, layer3DS, parent);
        addChildren(obj, newEntity);
    }
        break;
    default:
        break;
    }

    if (obj->attached())
        obj->attached()->component = m_componentNodeStack.top();
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

float clampOpacity(float f)
{
    f = qBound(0.0f, f, 1.0f);
    // make sure 1 is 1 so we won't end up with opacity < 1 == false
    if (qFuzzyCompare(f, 1.0f))
        f = 1.0f;
    return f;
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
    bool globalLogicalVisibility;
    bool globalEffectiveVisibility;

    if (parentData) {
        if (!flags.testFlag(UpdateGlobalsSkipTransform)) {
            // cache the global transform in the Q3DSNode for easy access
            globalTransform = parentData->globalTransform * data->transform->matrix();
        }
        // update the global, inherited opacity
        globalOpacity = clampOpacity(parentData->globalOpacity * (node->localOpacity() / 100.0f));
        // update inherited visibility
        globalLogicalVisibility = node->flags().testFlag(Q3DSNode::Active) && parentData->globalLogicalVisibility;
        globalEffectiveVisibility = node->flags().testFlag(Q3DSNode::Active)
                && data->visibilityTag == Q3DSGraphObjectAttached::Visible
                && parentData->globalEffectiveVisibility;
    } else {
        if (!flags.testFlag(UpdateGlobalsSkipTransform))
            globalTransform = data->transform->matrix();
        globalOpacity = clampOpacity(node->localOpacity() / 100.0f);
        globalLogicalVisibility = node->flags().testFlag(Q3DSNode::Active);
        globalEffectiveVisibility = node->flags().testFlag(Q3DSNode::Active)
                && data->visibilityTag == Q3DSGraphObjectAttached::Visible;
    }

    if (!flags.testFlag(UpdateGlobalsSkipTransform) && globalTransform != data->globalTransform) {
        data->globalTransform = globalTransform;
        data->frameDirty.setFlag(Q3DSGraphObjectAttached::GlobalTransformDirty, true);
    }
    if (globalOpacity != data->globalOpacity) {
        data->globalOpacity = globalOpacity;
        data->frameDirty.setFlag(Q3DSGraphObjectAttached::GlobalOpacityDirty, true);
    }
    if (globalLogicalVisibility != data->globalLogicalVisibility
            || globalEffectiveVisibility != data->globalEffectiveVisibility)
    {
        data->globalLogicalVisibility = globalLogicalVisibility;
        data->globalEffectiveVisibility = globalEffectiveVisibility;
        data->frameDirty.setFlag(Q3DSGraphObjectAttached::GlobalVisibilityDirty, true);
    }

    // Now frameDirty has the relevant bits set only when the corresponding
    // value has actually changed. Based on this, it is time to put the rolling
    // effect of these inherited values into action (e.g. when an ancestor goes
    // invisible, the children should go invisible as well).
    if (data->frameDirty.testFlag(Q3DSGraphObjectAttached::GlobalTransformDirty)
            || data->frameDirty.testFlag(Q3DSGraphObjectAttached::GlobalOpacityDirty)
            || data->frameDirty.testFlag(Q3DSGraphObjectAttached::GlobalVisibilityDirty))
    {
        handleNodeGlobalChange(node);
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

    Qt3DCore::QEntity *group = new Qt3DCore::QEntity;
    group->setObjectName(QObject::tr("group %1").arg(QString::fromUtf8(group3DS->id())));
    group->setParent(parent); // the separate setParent() call is intentional here, see QTBUG-69352
    initEntityForNode(group, group3DS, layer3DS);

    data->propertyChangeObserverIndex = group3DS->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    data->eventObserverIndex = group3DS->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));

    return group;
}

Qt3DCore::QEntity *Q3DSSceneManager::buildComponent(Q3DSComponentNode *comp3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSComponentAttached *data = new Q3DSComponentAttached;
    comp3DS->setAttached(data);

    Qt3DCore::QEntity *comp = new Qt3DCore::QEntity;
    comp->setObjectName(QObject::tr("component %1").arg(QString::fromUtf8(comp3DS->id())));
    comp->setParent(parent); // the separate setParent() call is intentional here, see QTBUG-69352
    initEntityForNode(comp, comp3DS, layer3DS);

    data->propertyChangeObserverIndex = comp3DS->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    data->eventObserverIndex = comp3DS->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));

    return comp;
}

Qt3DCore::QEntity *Q3DSSceneManager::buildAlias(Q3DSAliasNode *alias3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSAliasAttached *data = new Q3DSAliasAttached;
    alias3DS->setAttached(data);

    auto aliasEntity = new Qt3DCore::QEntity;
    aliasEntity->setObjectName(QObject::tr("alias %1").arg(QString::fromUtf8(alias3DS->id())));
    aliasEntity->setParent(parent); // the separate setParent() call is intentional here, see QTBUG-69352
    initEntityForNode(aliasEntity, alias3DS, layer3DS);

    data->propertyChangeObserverIndex = alias3DS->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    data->eventObserverIndex = alias3DS->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));

    return aliasEntity;
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

    Qt3DCore::QEntity *entity = new Qt3DCore::QEntity;
    entity->setObjectName(QObject::tr("text %1").arg(QString::fromUtf8(text3DS->id())));
    entity->setParent(parent); // the separate setParent() call is intentional here, see QTBUG-69352
    initEntityForNode(entity, text3DS, layer3DS);

    data->propertyChangeObserverIndex = text3DS->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    data->eventObserverIndex = text3DS->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));

    QSize sz = m_textRenderer->textImageSize(text3DS);
    if (sz.isEmpty())
        return entity;

    data->mesh = new Qt3DExtras::QPlaneMesh;
    data->mesh->setWidth(sz.width());
    data->mesh->setHeight(sz.height());
    data->mesh->setMirrored(true);
    entity->addComponent(data->mesh);

    data->opacityParam = new Qt3DRender::QParameter;
    data->opacityParam->setName(QLatin1String("opacity"));
    data->opacityParam->setValue(data->globalOpacity);

    data->colorParam = new Qt3DRender::QParameter;
    data->colorParam->setName(QLatin1String("color"));
    data->colorParam->setValue(text3DS->color());

    data->texture = new Qt3DRender::QTexture2D;
    m_profiler->trackNewObject(data->texture, Q3DSProfiler::Texture2DObject,
                               "Texture for text item %s", text3DS->id().constData());
    data->texture->setGenerateMipMaps(true);
    data->texture->setMinificationFilter(Qt3DRender::QAbstractTexture::LinearMipMapLinear);
    data->texture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);

    data->textureImage = new Q3DSTextImage(text3DS, m_textRenderer);
#if QT_VERSION >= QT_VERSION_CHECK(5,11,1)
    auto texImageD = static_cast<Qt3DRender::QPaintedTextureImagePrivate *>(
                Qt3DRender::QPaintedTextureImagePrivate::get(data->textureImage));
    texImageD->m_devicePixelRatio = m_viewportData.viewportDpr;
    data->textureImage->setSize(sz * m_viewportData.viewportDpr);
#else
    data->textureImage->setSize(sz);
#endif
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

    if (data->frameDirty.testFlag(Q3DSGraphObjectAttached::GlobalOpacityDirty))
        data->opacityParam->setValue(data->globalOpacity);

    data->colorParam->setValue(text3DS->color());

    if (needsNewImage) {
        // textstring, leading, tracking, ...
        const QSize sz = m_textRenderer->textImageSize(text3DS);
        if (!sz.isEmpty()) {
            data->mesh->setWidth(sz.width());
            data->mesh->setHeight(sz.height());
#if QT_VERSION >= QT_VERSION_CHECK(5,11,1)
            const QSize pixelSize = sz * m_viewportData.viewportDpr;
            auto texImageD = static_cast<Qt3DRender::QPaintedTextureImagePrivate *>(
                        Qt3DRender::QPaintedTextureImagePrivate::get(data->textureImage));
            if (data->textureImage->size() != pixelSize
                    || texImageD->m_devicePixelRatio != m_viewportData.viewportDpr)
            {
                texImageD->m_devicePixelRatio = m_viewportData.viewportDpr;
                // this repaints, no need for update() afterwards
                data->textureImage->setSize(pixelSize);
            } else {
                data->textureImage->update();
            }
#else
            if (data->textureImage->size() != sz)
                data->textureImage->setSize(sz); // this repaints, no need for update() afterwards
            else
                data->textureImage->update();
#endif
        }
    }
}

Qt3DCore::QEntity *Q3DSSceneManager::buildLight(Q3DSLightNode *light3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSLightAttached *data = new Q3DSLightAttached;
    light3DS->setAttached(data);

    Qt3DCore::QEntity *entity = new Qt3DCore::QEntity;
    entity->setObjectName(QObject::tr("light %1").arg(QString::fromUtf8(light3DS->id())));
    entity->setParent(parent); // the separate setParent() call is intentional here, see QTBUG-69352
    initEntityForNode(entity, light3DS, layer3DS);

    setLightProperties(light3DS, true);

    data->propertyChangeObserverIndex = light3DS->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    data->eventObserverIndex = light3DS->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));

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

    if (forceUpdate || data->frameDirty.testFlag(Q3DSGraphObjectAttached::GlobalTransformDirty)) {
        if (!ls->positionParam) {
            ls->positionParam = new Qt3DRender::QParameter;
            ls->positionParam->setName(QLatin1String("position"));
        }

        if (light3DS->lightType() == Q3DSLightNode::Directional) {
            // directional lights have a w value of 0 in position
            // position is used for direction in the custom material shader
            // custom material shader also wants the inverse handedness
            ls->positionParam->setValue(QVector4D(directionFromTransform(data->globalTransform, !leftHanded), 0.0f));
        } else {
            // point and area lights have w value of not-zero in position
            ls->positionParam->setValue(QVector4D(data->globalTransform.column(3).toVector3D(), 1.0f));
        }

        if (!ls->directionParam) {
            ls->directionParam = new Qt3DRender::QParameter;
            ls->directionParam->setName(QLatin1String("direction"));
        }
        ls->directionParam->setValue(directionFromTransform(data->globalTransform, leftHanded));
    }

    if (!ls->upParam) {
        ls->upParam = new Qt3DRender::QParameter;
        ls->upParam->setName(QLatin1String("up"));
    }
    if (!ls->rightParam) {
        ls->rightParam = new Qt3DRender::QParameter;
        ls->rightParam->setName(QLatin1String("right"));
    }
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

    if (!ls->diffuseParam) {
        ls->diffuseParam = new Qt3DRender::QParameter;
        ls->diffuseParam->setName(QLatin1String("diffuse"));
    }
    auto diffuseColor = QVector4D(float(light3DS->diffuse().redF()) * normalizedBrightness,
                                  float(light3DS->diffuse().greenF()) * normalizedBrightness,
                                  float(light3DS->diffuse().blueF()) * normalizedBrightness,
                                  float(light3DS->diffuse().alphaF()));
    ls->diffuseParam->setValue(diffuseColor);

    if (!ls->ambientParam) {
        ls->ambientParam = new Qt3DRender::QParameter;
        ls->ambientParam->setName(QLatin1String("ambient"));
    }
    auto ambientColor = QVector4D(float(light3DS->ambient().redF()),
                                  float(light3DS->ambient().greenF()),
                                  float(light3DS->ambient().blueF()),
                                  float(light3DS->ambient().alphaF()));
    ls->ambientParam->setValue(ambientColor);

    if (!ls->specularParam) {
        ls->specularParam = new Qt3DRender::QParameter;
        ls->specularParam->setName(QLatin1String("specular"));
    }
    auto specularColor = QVector4D(float(light3DS->specular().redF()) * normalizedBrightness,
                                   float(light3DS->specular().greenF()) * normalizedBrightness,
                                   float(light3DS->specular().blueF()) * normalizedBrightness,
                                   float(light3DS->specular().alphaF()));
    ls->specularParam->setValue(specularColor);

/*
    if (!ls->spotExponentParam) {
        ls->spotExponentParam = new Qt3DRender::QParameter;
        ls->spotExponentParam->setName(QLatin1String("spotExponent"));
    }
    ls->spotExponentParam->setValue(1.0f);

    if (!ls->spotCutoffParam) {
        ls->spotCutoffParam = new Qt3DRender::QParameter;
        ls->spotCutoffParam->setName(QLatin1String("spotCutoff"));
    }
    ls->spotCutoffParam->setValue(180.0f);
*/
    if (!ls->constantAttenuationParam) {
        ls->constantAttenuationParam = new Qt3DRender::QParameter;
        ls->constantAttenuationParam->setName(QLatin1String("constantAttenuation"));
    }
    ls->constantAttenuationParam->setValue(1.0f);

    if (!ls->linearAttenuationParam) {
        ls->linearAttenuationParam = new Qt3DRender::QParameter;
        ls->linearAttenuationParam->setName(QLatin1String("linearAttenuation"));
    }
    ls->linearAttenuationParam->setValue(qBound(0.0f, light3DS->linearFade(), 1000.0f) * 0.0001f);

    if (!ls->quadraticAttenuationParam) {
        ls->quadraticAttenuationParam = new Qt3DRender::QParameter;
        ls->quadraticAttenuationParam->setName(QLatin1String("quadraticAttenuation"));
    }
    ls->quadraticAttenuationParam->setValue(qBound(0.0f, light3DS->expFade(), 1000.0f) * 0.0000001f);

    // having non-zero values in either width or hight properties
    // determines that this is an area light in shader logic
    if (!ls->widthParam) {
        ls->widthParam = new Qt3DRender::QParameter;
        ls->widthParam->setName(QLatin1String("width"));
    }
    if (light3DS->lightType() == Q3DSLightNode::Area)
        ls->widthParam->setValue(light3DS->areaWidth());
    else
        ls->widthParam->setValue(0.0f);

    if (!ls->heightParam) {
        ls->heightParam = new Qt3DRender::QParameter;
        ls->heightParam->setName(QLatin1String("height"));
    }
    if (light3DS->lightType() == Q3DSLightNode::Area)
        ls->heightParam->setValue(light3DS->areaHeight());
    else
        ls->heightParam->setValue(0.0f);

    // For custom materials. The default material does not use these as it
    // generates the shader code dynamically and can therefore rely on uniforms
    // named like shadowcube_<index> and shadowmap_<index>_control. Custom
    // materials cannot do this and will instead use the shadow control and
    // matrix from the lights array and use shadowIdx to index the array
    // uniforms for the samplers.
    if (!ls->shadowControlsParam) {
        ls->shadowControlsParam = new Qt3DRender::QParameter;
        ls->shadowControlsParam->setName(QLatin1String("shadowControls"));
    }
    ls->shadowControlsParam->setValue(QVector4D(light3DS->shadowBias(), light3DS->shadowFactor(), light3DS->shadowMapFar(), 0));

    if (!ls->shadowViewParam) {
        ls->shadowViewParam = new Qt3DRender::QParameter;
        ls->shadowViewParam->setName(QLatin1String("shadowView"));
    }
    if (light3DS->lightType() == Q3DSLightNode::Point)
        ls->shadowViewParam->setValue(QMatrix4x4());
    else
        ls->shadowViewParam->setValue(data->globalTransform);

    if (!ls->shadowIdxParam) {
        ls->shadowIdxParam = new Qt3DRender::QParameter;
        ls->shadowIdxParam->setName(QLatin1String("shadowIdx"));
        ls->shadowIdxParam->setValue(-1); // must be -1 for non-shadow-casting lights
    }
}

Qt3DCore::QEntity *Q3DSSceneManager::buildModel(Q3DSModelNode *model3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent)
{
    Q3DSModelAttached *data = new Q3DSModelAttached;
    model3DS->setAttached(data);

    data->propertyChangeObserverIndex = model3DS->addPropertyChangeObserver(
                std::bind(&Q3DSSceneManager::handlePropertyChange, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    data->eventObserverIndex = model3DS->addEventHandler(QString(), std::bind(&Q3DSSceneManager::handleEvent, this, std::placeholders::_1));

    Qt3DCore::QEntity *entity = new Qt3DCore::QEntity;
    entity->setObjectName(QObject::tr("model %1").arg(QString::fromUtf8(model3DS->id())));
    entity->setParent(parent); // the separate setParent() call is intentional here, see QTBUG-69352
    initEntityForNode(entity, model3DS, layer3DS);

    const MeshList meshList = model3DS->mesh();
    if (meshList.isEmpty()) // e.g. a default constructed Q3DSModelNode, do nothing else for now
        return entity;

    rebuildModelSubMeshes(model3DS);

    return entity;
}

void Q3DSSceneManager::rebuildModelSubMeshes(Q3DSModelNode *model3DS)
{
    Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(model3DS->attached());

    if (!modelData->subMeshes.isEmpty()) {
        for (const Q3DSModelAttached::SubMesh &sm : modelData->subMeshes)
            delete sm.entity;
        modelData->subMeshes.clear();
    }

    QVector<Q3DSGraphObject *> materials;
    for (int i = 0; i < model3DS->childCount(); ++i) {
        Q3DSGraphObject *child = model3DS->childAtIndex(i);
        if (child->type() == Q3DSGraphObject::DefaultMaterial
                || child->type() == Q3DSGraphObject::CustomMaterial
                || child->type() == Q3DSGraphObject::ReferencedMaterial)
            materials.append(child);
    }

    const MeshList meshList = model3DS->mesh();
    int meshCount = meshList.count();
    if (materials.count() != meshCount) {
        const int actualCount = qMin(meshCount, materials.count());
        qCDebug(lcScene, "Model %s has %d submeshes but %d materials, this is wrong. Creating %d submeshes.",
                model3DS->id().constData(),
                meshCount,
                materials.count(),
                actualCount);
        meshCount = actualCount;
    }
    modelData->subMeshes.reserve(meshCount);

    for (int i = 0; i < meshCount; ++i) {
        auto material = materials.at(i);
        auto mesh = meshList.at(i);
        // Now this is tricky. The presentation caches the meshes which means
        // we cannot just let the mesh to be parented to the entity (in the
        // addComponent). For now keep meshes alive by parenting them to
        // something else. May need something more sophisticated later
        // (reference counting? proper LRU cache behavior?).
        mesh->setParent(m_rootEntity);
        m_profiler->trackNewObject(mesh, Q3DSProfiler::MeshObject,
                                   "Mesh %d for model %s", i, model3DS->id().constData());

        Q3DSModelAttached::SubMesh sm;
        sm.mesh = mesh;
        sm.entity = new Qt3DCore::QEntity;
        sm.entity->setObjectName(QObject::tr("model %1 submesh #%2").arg(QString::fromUtf8(model3DS->id())).arg(i));
        sm.entity->setParent(modelData->entity); // the separate setParent() call is intentional here, see QTBUG-69352
        sm.entity->addComponent(mesh);
        sm.material = material;
        sm.resolvedMaterial = material;
        // follow the reference for ReferencedMaterial
        if (sm.material->type() == Q3DSGraphObject::ReferencedMaterial) {
            Q3DSReferencedMaterial *matRef = static_cast<Q3DSReferencedMaterial *>(sm.material);
            sm.referencingMaterial = matRef;
            if (matRef->referencedMaterial())
                sm.resolvedMaterial = matRef->referencedMaterial();
        }
        // leave sm.materialComponent unset for now -> defer until the scene is processed once and so all lights are known
        modelData->subMeshes.append(sm);
    }

    // update submesh entities wrt opaque vs transparent
    retagSubMeshes(model3DS);
}

static void addShadowSsaoParams(Q3DSLayerAttached *layerData, QVector<Qt3DRender::QParameter *> *params, bool isCustomMaterial)
{
    for (const Q3DSLayerAttached::PerLightShadowMapData &sd : qAsConst(layerData->shadowMapData.shadowCasters)) {
        if (!isCustomMaterial) {
            if (sd.materialParams.shadowSampler)
                params->append(sd.materialParams.shadowSampler);
            if (sd.materialParams.shadowMatrixParam)
                params->append(sd.materialParams.shadowMatrixParam);
            if (sd.materialParams.shadowControlParam)
                params->append(sd.materialParams.shadowControlParam);
        }
    }

    if (layerData->ssaoTextureData.enabled && layerData->ssaoTextureData.ssaoTextureSampler)
        params->append(layerData->ssaoTextureData.ssaoTextureSampler);

    if (isCustomMaterial) {
        if (layerData->shadowMapData.custMatParams.numShadowCubesParam)
            params->append(layerData->shadowMapData.custMatParams.numShadowCubesParam);
        if (layerData->shadowMapData.custMatParams.numShadowMapsParam)
            params->append(layerData->shadowMapData.custMatParams.numShadowMapsParam);
        if (layerData->shadowMapData.custMatParams.shadowMapSamplersParam)
            params->append(layerData->shadowMapData.custMatParams.shadowMapSamplersParam);
        if (layerData->shadowMapData.custMatParams.shadowCubeSamplersParam)
            params->append(layerData->shadowMapData.custMatParams.shadowCubeSamplersParam);
    }
}

void Q3DSSceneManager::buildModelMaterial(Q3DSModelNode *model3DS)
{
    // Scene building phase 2: all lights are known -> generate actual Qt3D materials

    Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(model3DS->attached());
    if (!modelData)
        return;

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(modelData->layer3DS->attached());
    Q_ASSERT(layerData);

    QVector<Q3DSNodeAttached::LightsData *> lights = getLightsDataForNode(model3DS);

    // The first node in the list is current scope lightData
    Q3DSNodeAttached::LightsData *lightsData = nullptr;
    if (!lights.isEmpty())
        lightsData = lights.first();

    // Get a full list of light nodes covered by the current scope
    QVector<Q3DSLightNode *> lightNodes;
    for (auto light : lights)
        lightNodes.append(light->lightNodes);

    if (lightNodes.count() > m_gfxLimits.maxLightsPerLayer) {
        qCWarning(lcPerf, "Default material for model %s got %d lights, shader input truncated to %d",
                  model3DS->id().constData(), lightNodes.count(), m_gfxLimits.maxLightsPerLayer);
        // This is what the shader generator will see so truncate this. That
        // some calculations below may still use all the lights does not
        // matters so much.
        lightNodes.resize(m_gfxLimits.maxLightsPerLayer);
    }

    for (Q3DSModelAttached::SubMesh &sm : modelData->subMeshes) {
        if (sm.resolvedMaterial && !sm.materialComponent) {
            if (sm.resolvedMaterial->type() == Q3DSGraphObject::DefaultMaterial) {
                Q3DSDefaultMaterial *defaultMaterial = static_cast<Q3DSDefaultMaterial *>(sm.resolvedMaterial);

                // Create parameters to the shader.
                QVector<Qt3DRender::QParameter *> params = prepareDefaultMaterial(defaultMaterial, sm.referencingMaterial, model3DS);

                // Update parameter values.
                Q3DSDefaultMaterialAttached *defMatData = static_cast<Q3DSDefaultMaterialAttached *>(defaultMaterial->attached());
                const float opacity = clampOpacity(modelData->globalOpacity * (defaultMaterial->opacity() / 100.0f));
                defMatData->perModelData[model3DS].combinedOpacity = opacity;
                updateDefaultMaterial(defaultMaterial, sm.referencingMaterial, model3DS);

                // Setup camera properties
                if (layerData->cameraPropertiesParam != nullptr)
                    params.append(layerData->cameraPropertiesParam);

                // Setup ambient light total

                if (lightsData) {
                    updateLightsParams(lights, lightsData);
                    params.append(lightsData->lightAmbientTotalParamenter);

                    if (!m_gfxLimits.useGles2Path) {
                        // Setup lights, use combined buffer for the default material.
                        if (!lightsData->allLightsConstantBuffer) {
                            lightsData->allLightsConstantBuffer = new Qt3DRender::QBuffer(layerData->entity);
                            lightsData->allLightsConstantBuffer->setObjectName(QLatin1String("all lights constant buffer"));
                            // make sure we pick up all inherited lights in scope
                            QVector<Q3DSLightSource> allLights;
                            for (auto light : lights)
                                allLights.append(light->allLights);
                            updateLightsBuffer(allLights, lightsData->allLightsConstantBuffer);
                        }
                        if (!lightsData->allLightsParam)
                            lightsData->allLightsParam = new Qt3DRender::QParameter;
                        lightsData->allLightsParam->setName(QLatin1String("cbBufferLights"));
                        lightsData->allLightsParam->setValue(QVariant::fromValue(lightsData->allLightsConstantBuffer));
                        params.append(lightsData->allLightsParam);
                    } else {
                        // ubo not supported, create uniforms for each light
                        params.append(prepareSeparateLightUniforms(lightsData->allLights, QStringLiteral("qLights")));
                    }
                }

                addShadowSsaoParams(layerData, &params, false);

                // Setup light probe parameters
                if (defMatData->lightProbeOverrideTexture || modelData->layer3DS->lightProbe()) {
                    // Always include probe properties
                    params.append(layerData->iblProbeData.lightProbeProperties);
                    params.append(layerData->iblProbeData.lightProbe2Properties);
                    params.append(layerData->iblProbeData.lightProbeOptions);

                    if (!defMatData->lightProbeOverrideTexture) {
                        // There is no override (so provide everything)
                        params.append(layerData->iblProbeData.lightProbeSampler);
                        params.append(layerData->iblProbeData.lightProbeOffset);
                        params.append(layerData->iblProbeData.lightProbeRotation);
                    } else {
                        params.append(defMatData->lightProbeSampler);
                        params.append(defMatData->lightProbeOffset);
                        params.append(defMatData->lightProbeRotation);
                    }

                    if (modelData->layer3DS->lightProbe2()) {
                        params.append(layerData->iblProbeData.lightProbe2Sampler);
                    }
                }

                // Do not let the QMaterial own the (potentially not yet
                // parented) QParameters. Parent them to some other QNode. This
                // is important here due to rebuildModelMaterial() where
                // sm.materialComponent may get destroyed afterwards.
                for (Qt3DRender::QParameter *param : params) {
                    // sm.entity is not suitable either since nodes and so their entities may come and go
                    param->setParent(layerData->entity);
                }

                sm.materialComponent = m_matGen->generateMaterial(defaultMaterial, sm.referencingMaterial, params, lightNodes, modelData->layer3DS);
                sm.entity->addComponent(sm.materialComponent);
            } else if (sm.resolvedMaterial->type() == Q3DSGraphObject::CustomMaterial) {
                Q3DSCustomMaterialInstance *customMaterial = static_cast<Q3DSCustomMaterialInstance *>(sm.resolvedMaterial);
                QVector<Qt3DRender::QParameter *> params = prepareCustomMaterial(customMaterial, sm.referencingMaterial, model3DS);
                Q3DSCustomMaterialAttached *custMatData = static_cast<Q3DSCustomMaterialAttached *>(customMaterial->attached());
                custMatData->perModelData[model3DS].combinedOpacity = modelData->globalOpacity;
                updateCustomMaterial(customMaterial, sm.referencingMaterial, model3DS);

                if (lightsData) {
                    if (!m_gfxLimits.useGles2Path) {
                        // Here lights are provided in two separate buffers.
                        if (!lightsData->nonAreaLightsConstantBuffer) {
                            lightsData->nonAreaLightsConstantBuffer = new Qt3DRender::QBuffer(layerData->entity);
                            lightsData->nonAreaLightsConstantBuffer->setObjectName(QLatin1String("non-area lights constant buffer"));
                            QVector<Q3DSLightSource> nonAreaLights;
                            for (auto light : lights)
                                nonAreaLights.append(light->nonAreaLights);
                            updateLightsBuffer(nonAreaLights, lightsData->nonAreaLightsConstantBuffer);
                        }
                        if (!lightsData->nonAreaLightsParam)
                            lightsData->nonAreaLightsParam = new Qt3DRender::QParameter;
                        lightsData->nonAreaLightsParam->setName(QLatin1String("cbBufferLights")); // i.e. this cannot be combined with allLightsParam
                        lightsData->nonAreaLightsParam->setValue(QVariant::fromValue(lightsData->nonAreaLightsConstantBuffer));
                        params.append(lightsData->nonAreaLightsParam);

                        if (!lightsData->areaLightsConstantBuffer) {
                            lightsData->areaLightsConstantBuffer = new Qt3DRender::QBuffer(layerData->entity);
                            lightsData->areaLightsConstantBuffer->setObjectName(QLatin1String("area lights constant buffer"));
                            QVector<Q3DSLightSource> areaLights;
                            for (auto light : lights)
                                areaLights.append(light->areaLights);
                            updateLightsBuffer(areaLights, lightsData->areaLightsConstantBuffer);
                        }
                        if (!lightsData->areaLightsParam)
                            lightsData->areaLightsParam = new Qt3DRender::QParameter;
                        lightsData->areaLightsParam->setName(QLatin1String("cbBufferAreaLights"));
                        lightsData->areaLightsParam->setValue(QVariant::fromValue(lightsData->areaLightsConstantBuffer));
                        params.append(lightsData->areaLightsParam);
                    } else {
                        // ubo not supported, create uniforms for each light
                        params.append(prepareSeparateLightUniforms(lightsData->nonAreaLights, QStringLiteral("qLights")));
                        params.append(prepareSeparateLightUniforms(lightsData->areaLights, QStringLiteral("areaLights")));
                    }
                }

                addShadowSsaoParams(layerData, &params, true);

                // Setup light probe parameters
                if (custMatData->lightProbeOverrideTexture || modelData->layer3DS->lightProbe()) {
                    // Always include probe properties
                    params.append(layerData->iblProbeData.lightProbeProperties);
                    params.append(layerData->iblProbeData.lightProbe2Properties);
                    params.append(layerData->iblProbeData.lightProbeOptions);

                    if (!custMatData->lightProbeOverrideTexture) {
                        // There is no override (so provide everything)
                        params.append(layerData->iblProbeData.lightProbeSampler);
                        params.append(layerData->iblProbeData.lightProbeOffset);
                        params.append(layerData->iblProbeData.lightProbeRotation);
                    } else {
                        params.append(custMatData->lightProbeSampler);
                        params.append(custMatData->lightProbeOffset);
                        params.append(custMatData->lightProbeRotation);
                    }

                    if (modelData->layer3DS->lightProbe2()) {
                        params.append(layerData->iblProbeData.lightProbe2Sampler);
                    }
                }

                // Like for the default material, be careful with the parent.
                for (Qt3DRender::QParameter *param : params) {
                    // sm.entity is not suitable either since nodes and so their entities may come and go
                    param->setParent(layerData->entity);
                }

                // ### TODO support more than one pass
                auto pass = customMaterial->material()->passes().first();
                sm.materialComponent = m_customMaterialGen->generateMaterial(customMaterial, sm.referencingMaterial, params, lightNodes, modelData->layer3DS, pass);
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
            qCDebug(lcPerf, "Rebuilding material for %s (entity %p)", model3DS->id().constData(), sm.entity);
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

    Q3DSProfiler::SubMeshData profData;
    for (Q3DSModelAttached::SubMesh &sm : data->subMeshes) {
        float opacity = data->globalOpacity;
        bool hasTransparency = false;
        // Note: cannot rely on sm.resolvedMaterial->attached() since it may
        // not yet be created. This is not a problem in practice.
        if (sm.resolvedMaterial->type() == Q3DSGraphObject::DefaultMaterial) {
            auto defaultMaterial = static_cast<Q3DSDefaultMaterial *>(sm.resolvedMaterial);
            opacity = clampOpacity(opacity * defaultMaterial->opacity() / 100.0f);
            // Check maps for transparency as well
            hasTransparency = ((defaultMaterial->diffuseMap() && defaultMaterial->diffuseMap()->hasTransparency(m_presentation)) ||
                               (defaultMaterial->diffuseMap2() && defaultMaterial->diffuseMap2()->hasTransparency(m_presentation)) ||
                               (defaultMaterial->diffuseMap3() && defaultMaterial->diffuseMap3()->hasTransparency(m_presentation)) ||
                               defaultMaterial->opacityMap() ||
                               defaultMaterial->translucencyMap() ||
                               defaultMaterial->displacementMap() ||
                               defaultMaterial->blendMode() != Q3DSDefaultMaterial::Normal);
            sm.hasTransparency = opacity < 1.0f || hasTransparency;
        } else if (sm.resolvedMaterial->type() == Q3DSGraphObject::CustomMaterial) {
            auto customMaterial = static_cast<Q3DSCustomMaterialInstance *>(sm.resolvedMaterial);
            const Q3DSCustomMaterial *matDesc = customMaterial->material();
            sm.hasTransparency = opacity < 1.0f || matDesc->materialHasTransparency() || matDesc->materialHasRefraction();
        }

        if (data->globalEffectiveVisibility) {
            Qt3DRender::QLayer *newTag = sm.hasTransparency ? layerData->transparentTag : layerData->opaqueTag;
            if (!sm.entity->components().contains(newTag)) {
                Qt3DRender::QLayer *prevTag = newTag == layerData->transparentTag ? layerData->opaqueTag : layerData->transparentTag;
                sm.entity->removeComponent(prevTag);
                sm.entity->addComponent(newTag);
            }
        }

        profData.needsBlending = sm.hasTransparency;
        m_profiler->reportSubMeshData(sm.mesh, profData);
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

    textureParameters.size = new Qt3DRender::QParameter;
    textureParameters.size->setName(name + QLatin1String("_size"));

    textureParameters.texture = Q3DSImageManager::instance().newTextureForImageFile(
                m_rootEntity, 0, m_profiler, "Texture for image %s", image3DS->id().constData());
}

void Q3DSSceneManager::updateTextureParameters(Q3DSTextureParameters &textureParameters, Q3DSImage *image)
{
    if (!image->subPresentation().isEmpty()) {
        if (textureParameters.subPresId != image->subPresentation()) {
            textureParameters.subPresId = image->subPresentation();
            // won't yet have the subpresentations if this is still during the building of the main one
            if (m_subPresentations.isEmpty()) {
                textureParameters.sampler->setValue(QVariant::fromValue(dummyTexture()));
                m_pendingSubPresImages.append(qMakePair(textureParameters.sampler, image));
            } else {
                setImageTextureFromSubPresentation(textureParameters.sampler, image);
            }
        }
    } else if (!image->sourcePath().isEmpty()) {
        Q3DSImageManager::instance().setSource(textureParameters.texture, QUrl::fromLocalFile(image->sourcePath()));
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
    texture->setWrapMode(wrapMode);
    // min/mag are already set at this point

    const QMatrix4x4 &textureTransform = image->textureTransform();
    const float *m = textureTransform.constData();

    QVector3D offsets(m[12], m[13], image->hasPremultipliedAlpha() ? 1 : 0);
    textureParameters.offsets->setValue(offsets);

    QVector4D rotations(m[0], m[4], m[1], m[5]);
    textureParameters.rotations->setValue(rotations);

    textureParameters.size->setValue(QVector2D(texture->width(), texture->height()));
}

void Q3DSSceneManager::setImageTextureFromSubPresentation(Qt3DRender::QParameter *sampler, Q3DSImage *image)
{
    auto it = std::find_if(m_subPresentations.cbegin(), m_subPresentations.cend(),
                           [image](const Q3DSSubPresentation &sp) { return sp.id == image->subPresentation(); });
    if (it != m_subPresentations.cend()) {
        qCDebug(lcScene, "Directing subpresentation %s to image %s",
                qPrintable(image->subPresentation()), image->id().constData());
        sampler->setValue(QVariant::fromValue(it->colorTex));
        qCDebug(lcPerf, "Using a subpresentation as texture map (not as layer) makes layer caching in main presentation less efficient");
        // QML subpresentations will not have a scenemanager
        if (it->sceneManager)
            m_layerCacheDeps.insert(it->sceneManager);
        else
            m_hasQmlSubPresAsTextureMap = true;
    } else {
        qCDebug(lcScene, "Subpresentation %s for image %s not found",
                qPrintable(image->subPresentation()), image->id().constData());
        sampler->setValue(QVariant::fromValue(dummyTexture()));
    }
}

QVector<Qt3DRender::QParameter *> Q3DSSceneManager::prepareDefaultMaterial(Q3DSDefaultMaterial *m, Q3DSReferencedMaterial *rm, Q3DSModelNode *model3DS)
{
    // This function can be called more than once for the same 'm' (with a
    // different 'model3DS' each time). The returned QParameter list must be
    // complete in any case, but returning QParameters that were returned
    // before for a different model3DS is fine.

    Q3DSDefaultMaterialAttached *data = static_cast<Q3DSDefaultMaterialAttached *>(m->attached());
    const bool firstRun = !data->prepared;
    if (firstRun)
        data->prepared = true;

    Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(model3DS->attached());
    QVector<Qt3DRender::QParameter *> params;

    if (firstRun)
        data->entity = modelData->entity;

    if (firstRun) {
        data->diffuseParam = new Qt3DRender::QParameter;
        data->diffuseParam->setName(QLatin1String("diffuse_color"));
    }
    params.append(data->diffuseParam);

    Q3DSMaterialAttached::PerModelData md;
    md.materialDiffuseParam = new Qt3DRender::QParameter;
    md.materialDiffuseParam->setName(QLatin1String("material_diffuse"));
    data->perModelData.insert(model3DS, md);
    params.append(md.materialDiffuseParam);
    data->perModelData.insert(model3DS, md);

    if (firstRun) {
        data->specularParam = new Qt3DRender::QParameter;
        data->specularParam->setName(QLatin1String("material_specular"));
    }
    params.append(data->specularParam);

    if (firstRun) {
        data->fresnelPowerParam = new Qt3DRender::QParameter;
        data->fresnelPowerParam->setName(QLatin1String("fresnelPower"));
    }
    params.append(data->fresnelPowerParam);

    if (firstRun) {
        data->bumpAmountParam = new Qt3DRender::QParameter;
        data->bumpAmountParam->setName(QLatin1String("bumpAmount"));
    }
    params.append(data->bumpAmountParam);

    if (firstRun) {
        data->materialPropertiesParam = new Qt3DRender::QParameter;
        data->materialPropertiesParam->setName(QLatin1String("material_properties"));
    }
    params.append(data->materialPropertiesParam);

    if (firstRun) {
        data->translucentFalloffParam = new Qt3DRender::QParameter;
        data->translucentFalloffParam->setName(QLatin1String("translucentFalloff"));
    }
    params.append(data->translucentFalloffParam);

    if (firstRun) {
        data->diffuseLightWrapParam = new Qt3DRender::QParameter;
        data->diffuseLightWrapParam->setName(QLatin1String("diffuseLightWrap"));
    }
    params.append(data->diffuseLightWrapParam);

    if (firstRun) {
        data->displaceAmountParam = new Qt3DRender::QParameter;
        data->displaceAmountParam->setName(QLatin1String("displaceAmount"));
    }
    params.append(data->displaceAmountParam);

    if (m->diffuseMap()) {
        if (firstRun) {
            prepareTextureParameters(data->diffuseMapParams, QLatin1String("diffuseMap"), m->diffuseMap());
            static_cast<Q3DSImageAttached *>(m->diffuseMap()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->diffuseMapParams.parameters());
    }

    if (m->diffuseMap2()) {
        if (firstRun) {
            prepareTextureParameters(data->diffuseMap2Params, QLatin1String("diffuseMap2"), m->diffuseMap2());
            static_cast<Q3DSImageAttached *>(m->diffuseMap2()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->diffuseMap2Params.parameters());
    }

    if (m->diffuseMap3()) {
        if (firstRun) {
            prepareTextureParameters(data->diffuseMap3Params, QLatin1String("diffuseMap3"), m->diffuseMap3());
            static_cast<Q3DSImageAttached *>(m->diffuseMap3()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->diffuseMap3Params.parameters());
    }

    if (m->specularReflection()) {
        if (firstRun) {
            prepareTextureParameters(data->specularReflectionParams, QLatin1String("specularreflection"), m->specularReflection());
            static_cast<Q3DSImageAttached *>(m->specularReflection()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->specularReflectionParams.parameters());
    }

    if (m->specularMap()) {
        if (firstRun) {
            prepareTextureParameters(data->specularMapParams, QLatin1String("specularMap"), m->specularMap());
            static_cast<Q3DSImageAttached *>(m->specularMap()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->specularMapParams.parameters());
    }

    if (m->roughnessMap()) {
        if (firstRun) {
            prepareTextureParameters(data->roughnessMapParams, QLatin1String("roughnessMap"), m->roughnessMap());
            static_cast<Q3DSImageAttached *>(m->roughnessMap()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->roughnessMapParams.parameters());
    }

    if (m->bumpMap()) {
        if (firstRun) {
            prepareTextureParameters(data->bumpMapParams, QLatin1String("bumpMap"), m->bumpMap());
            static_cast<Q3DSImageAttached *>(m->bumpMap()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->bumpMapParams.parameters());
    }

    if (m->normalMap()) {
        if (firstRun) {
            prepareTextureParameters(data->normalMapParams, QLatin1String("normalMap"), m->normalMap());
            static_cast<Q3DSImageAttached *>(m->normalMap()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->normalMapParams.parameters());
    }

    if (m->displacementMap()) {
        if (firstRun) {
            prepareTextureParameters(data->displacementMapParams, QLatin1String("displacementMap"), m->displacementMap());
            static_cast<Q3DSImageAttached *>(m->displacementMap()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->displacementMapParams.parameters());
    }

    if (m->opacityMap()) {
        if (firstRun) {
            prepareTextureParameters(data->opacityMapParams, QLatin1String("opacityMap"), m->opacityMap());
            static_cast<Q3DSImageAttached *>(m->opacityMap()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->opacityMapParams.parameters());
    }

    if (m->emissiveMap()) {
        if (firstRun) {
            prepareTextureParameters(data->emissiveMapParams, QLatin1String("emissiveMap"), m->emissiveMap());
            static_cast<Q3DSImageAttached *>(m->emissiveMap()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->emissiveMapParams.parameters());
    }

    if (m->emissiveMap2()) {
        if (firstRun) {
            prepareTextureParameters(data->emissiveMap2Params, QLatin1String("emissiveMap2"), m->emissiveMap2());
            static_cast<Q3DSImageAttached *>(m->emissiveMap2()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->emissiveMap2Params.parameters());
    }

    if (m->translucencyMap()) {
        if (firstRun) {
            prepareTextureParameters(data->translucencyMapParams, QLatin1String("translucencyMap"), m->translucencyMap());
            static_cast<Q3DSImageAttached *>(m->translucencyMap()->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->translucencyMapParams.parameters());
    }

    // Lightmaps
    // check for referencedMaterial Overrides
    // Note all the below must ignore firstRun since there's nothing saying the
    // first call for a certain 'm' is the one with rm != null.
    Q3DSImage *lightmapIndirect = nullptr;
    if (rm && rm->lightmapIndirectMap())
        lightmapIndirect = rm->lightmapIndirectMap();
    else if (m->lightmapIndirectMap())
        lightmapIndirect = m->lightmapIndirectMap();
    if (lightmapIndirect) {
        if (!data->lightmapIndirectParams.sampler) {
            prepareTextureParameters(data->lightmapIndirectParams, QLatin1String("lightmapIndirect"), lightmapIndirect);
            static_cast<Q3DSImageAttached *>(lightmapIndirect->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->lightmapIndirectParams.parameters());
    }

    Q3DSImage *lightmapRadiosity = nullptr;
    if (rm && rm->lightmapRadiosityMap())
        lightmapRadiosity = rm->lightmapRadiosityMap();
    else if (m->lightmapRadiosityMap())
        lightmapRadiosity = m->lightmapRadiosityMap();
    if (lightmapRadiosity) {
        if (!data->lightmapRadiosityParams.sampler) {
            prepareTextureParameters(data->lightmapRadiosityParams, QLatin1String("lightmapRadiosity"), lightmapRadiosity);
            static_cast<Q3DSImageAttached *>(lightmapRadiosity->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->lightmapRadiosityParams.parameters());
    }

    Q3DSImage *lightmapShadow = nullptr;
    if (rm && rm->lightmapShadowMap())
        lightmapShadow = rm->lightmapShadowMap();
    else if (m->lightmapShadowMap())
        lightmapShadow = m->lightmapShadowMap();
    if (lightmapShadow) {
        if (!data->lightmapShadowParams.sampler) {
            prepareTextureParameters(data->lightmapShadowParams, QLatin1String("lightmapShadow"), lightmapShadow);
            static_cast<Q3DSImageAttached *>(lightmapShadow->attached())->referencingDefaultMaterials.insert(m);
        }
        params.append(data->lightmapShadowParams.parameters());
    }

    // IBL
    Q3DSImage *iblOverrideImage = nullptr;
    if (rm && rm->lightProbe())
        iblOverrideImage = rm->lightProbe();
    else if (m->lightProbe())
        iblOverrideImage = m->lightProbe();
    if (iblOverrideImage) {
        if (!data->lightProbeOverrideTexture) {
            data->lightProbeOverrideTexture = Q3DSImageManager::instance().newTextureForImageFile(
                        m_rootEntity, Q3DSImageManager::GenerateMipMapsForIBL,
                        m_profiler, "Texture for image %s", iblOverrideImage->id().constData());
            data->lightProbeSampler = new Qt3DRender::QParameter;
            data->lightProbeSampler->setName(QLatin1String("light_probe"));

            data->lightProbeRotation = new Qt3DRender::QParameter;
            data->lightProbeRotation->setName(QLatin1String("light_probe_rotation"));

            data->lightProbeOffset = new Qt3DRender::QParameter;
            data->lightProbeOffset->setName(QLatin1String("light_probe_offset"));
        }
        params.append(data->lightProbeSampler);
        params.append(data->lightProbeRotation);
        params.append(data->lightProbeOffset);
    }

    return params;
}

void Q3DSSceneManager::updateDefaultMaterial(Q3DSDefaultMaterial *m, Q3DSReferencedMaterial *rm, Q3DSModelNode *model3DS)
{
    // This function is typically called in a loop for each Q3DSModelNode that
    // shares the same Q3DSDefaultMaterial. Having more than one model can only
    // happen when a ReferencedMaterial is used. Currently we are somewhat
    // wasteful in this case since setting the QParameter values over and over
    // again is not really needed, with the exception of the one model-specific
    // one. We'll live with this for the time being.

    Q3DSDefaultMaterialAttached *data = static_cast<Q3DSDefaultMaterialAttached *>(m->attached());
    Q_ASSERT(data && data->diffuseParam);

    data->diffuseParam->setValue(m->diffuse());

    float emissivePower = 1.0f;
    bool hasLighting = m->shaderLighting() != Q3DSDefaultMaterial::ShaderLighting::NoShaderLighting;
    if (hasLighting)
        emissivePower = m->emissivePower() / 100.0f;

    // This one depends on the combinedOpacity which depends on the
    // localOpacity from the model. Hence its placement in the perModelData map.
    auto perModelDataIt = data->perModelData.find(model3DS);
    Q_ASSERT(perModelDataIt != data->perModelData.end());
    QVector4D material_diffuse(m->emissiveColor().redF() * emissivePower,
                               m->emissiveColor().greenF() * emissivePower,
                               m->emissiveColor().blueF() * emissivePower,
                               perModelDataIt->combinedOpacity);
    perModelDataIt->materialDiffuseParam->setValue(material_diffuse);

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

    if (m->roughnessMap())
        updateTextureParameters(data->roughnessMapParams, m->roughnessMap());

    if (m->bumpMap())
        updateTextureParameters(data->bumpMapParams, m->bumpMap());

    if (m->normalMap())
        updateTextureParameters(data->normalMapParams, m->normalMap());

    if (m->displacementMap())
        updateTextureParameters(data->displacementMapParams, m->displacementMap());

    if (m->opacityMap())
        updateTextureParameters(data->opacityMapParams, m->opacityMap());

    if (m->emissiveMap())
        updateTextureParameters(data->emissiveMapParams, m->emissiveMap());

    if (m->emissiveMap2())
        updateTextureParameters(data->emissiveMap2Params, m->emissiveMap2());

    if (m->translucencyMap())
        updateTextureParameters(data->translucencyMapParams, m->translucencyMap());

    // Lightmaps
    Q3DSImage *lightmapIndirect = nullptr;
    if (rm && rm->lightmapIndirectMap())
        lightmapIndirect = rm->lightmapIndirectMap();
    else if (m->lightmapIndirectMap())
        lightmapIndirect = m->lightmapIndirectMap();
    if (lightmapIndirect)
        updateTextureParameters(data->lightmapIndirectParams, lightmapIndirect);

    Q3DSImage *lightmapRadiosity = nullptr;
    if (rm && rm->lightmapRadiosityMap())
        lightmapRadiosity = rm->lightmapRadiosityMap();
    else if (m->lightmapRadiosityMap())
        lightmapRadiosity = m->lightmapRadiosityMap();
    if (lightmapRadiosity)
        updateTextureParameters(data->lightmapRadiosityParams, lightmapRadiosity);

    Q3DSImage *lightmapShadow = nullptr;
    if (rm && rm->lightmapShadowMap())
        lightmapShadow = rm->lightmapShadowMap();
    else if (m->lightmapShadowMap())
        lightmapShadow = m->lightmapShadowMap();
    if (lightmapShadow)
        updateTextureParameters(data->lightmapShadowParams, lightmapShadow);

    // IBL Override
    Q3DSImage *iblOverride = nullptr;
    if (rm && rm->lightProbe())
        iblOverride = rm->lightProbe();
    else if (m->lightProbe())
        iblOverride = m->lightProbe();

    // IBL
    if (iblOverride) {
        // also sets min/mag and generates mipmaps
        Q3DSImageManager::instance().setSource(data->lightProbeOverrideTexture,
                                               QUrl::fromLocalFile(iblOverride->sourcePath()));
        data->lightProbeSampler->setValue(QVariant::fromValue(data->lightProbeOverrideTexture));

        Qt3DRender::QTextureWrapMode wrapMode;
        wrapMode.setX(Qt3DRender::QTextureWrapMode::Repeat);
        switch (iblOverride->verticalTiling()) {
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

        Q_ASSERT(data->lightProbeOverrideTexture);
        data->lightProbeOverrideTexture->setWrapMode(wrapMode);

        const QMatrix4x4 &textureTransform = iblOverride->textureTransform();
        const float *m = textureTransform.constData();

        // offsets.w = max mip level
        const QSize texSize = Q3DSImageManager::instance().size(data->lightProbeOverrideTexture);
        float mipLevels = float(qCeil(qLog2(qMax(texSize.width(), texSize.height()))));
        QVector4D offsets(m[12], m[13], 0.0f, mipLevels);
        data->lightProbeOffset->setValue(offsets);

        QVector4D rotations(m[0], m[4], m[1], m[5]);
        data->lightProbeRotation->setValue(rotations);
    }
}

typedef std::function<void(const QString &, const QVariant &, const Q3DSMaterial::PropertyElement &)> CustomPropertyCallback;

static void iterateCustomProperties(const QVariantMap &properties,
                                    const QMap<QString, Q3DSMaterial::PropertyElement> &propertiesMeta,
                                    CustomPropertyCallback callback)
{
    for (auto it = properties.cbegin(), itEnd = properties.cend(); it != itEnd; ++it) {
        const QString &propName(it.key());
        const QVariant &propValue(it.value());
        const Q3DSMaterial::PropertyElement &propMeta(propertiesMeta[propName]);
        callback(propName, propValue, propMeta);
    }
}

static inline void forAllCustomProperties(Q3DSCustomMaterialInstance *m, CustomPropertyCallback callback)
{
    iterateCustomProperties(m->dynamicProperties(), m->material()->properties(), callback);
}

static inline void forAllCustomProperties(Q3DSEffectInstance *eff3DS, CustomPropertyCallback callback)
{
    iterateCustomProperties(eff3DS->dynamicProperties(), eff3DS->effect()->properties(), callback);
}

Qt3DRender::QAbstractTexture *Q3DSSceneManager::createCustomPropertyTexture(const Q3DSCustomPropertyParameter &p)
{
    const QString source = p.inputValue.toString();
    Qt3DRender::QAbstractTexture *texture;
    if (source.isEmpty()) {
        texture = dummyTexture();
    } else {
        texture = Q3DSImageManager::instance().newTextureForImageFile(m_rootEntity, 0, m_profiler,
                                                                      "Custom property texture %s", qPrintable(source));
        qCDebug(lcScene, "Creating custom property texture %s", qPrintable(source));
        Q3DSImageManager::instance().setSource(texture, QUrl::fromLocalFile(source));
    }

    // now override the defaults set in setSource() with whatever the metadata specifies

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

QVector<Qt3DRender::QParameter *> Q3DSSceneManager::prepareCustomMaterial(Q3DSCustomMaterialInstance *m, Q3DSReferencedMaterial *rm, Q3DSModelNode *model3DS)
{
    // this function can be called more than once for the same 'm' (with a different 'model3DS' each time)

    Q3DSCustomMaterialAttached *data = static_cast<Q3DSCustomMaterialAttached *>(m->attached());
    const bool firstRun = !data->prepared;
    if (firstRun)
        data->prepared = true;

    Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(model3DS->attached());

    if (firstRun)
        data->entity = modelData->entity;

    // Generate QParameters
    QVector<Qt3DRender::QParameter *> paramList;

    if (firstRun) {
        forAllCustomProperties(m, [&paramList, data](const QString &propKey, const QVariant &, const Q3DSMaterial::PropertyElement &propMeta) {
            QVariant v(0); // initial value is something dummy, ignore propValue for now
            Qt3DRender::QParameter *param = new Qt3DRender::QParameter;
            param->setName(propKey);
            param->setValue(v);
            paramList.append(param);
            data->params.insert(propKey, Q3DSCustomPropertyParameter(param, v, propMeta));
        });
    } else {
        for (auto it = data->params.cbegin(), itEnd = data->params.cend(); it != itEnd; ++it)
            paramList.append(it->param);
    }

    Q3DSMaterialAttached::PerModelData md;
    md.objectOpacityParam = new Qt3DRender::QParameter;
    md.objectOpacityParam->setName(QLatin1String("object_opacity"));
    data->perModelData.insert(model3DS, md);
    paramList.append(md.objectOpacityParam);
    data->perModelData.insert(model3DS, md);

    // Lightmaps
    // check for referencedMaterial Overrides
    // Note all the below must ignore firstRun since there's nothing saying the
    // first call for a certain 'm' is the one with rm != null.
    Q3DSImage *lightmapIndirect = nullptr;
    if (rm && rm->lightmapIndirectMap())
        lightmapIndirect = rm->lightmapIndirectMap();
    else if (m->lightmapIndirectMap())
        lightmapIndirect = m->lightmapIndirectMap();
    if (lightmapIndirect) {
        if (!data->lightmapIndirectParams.sampler)
            prepareTextureParameters(data->lightmapIndirectParams, QLatin1String("lightmapIndirect"), lightmapIndirect);
        paramList.append(data->lightmapIndirectParams.parameters());
    }

    Q3DSImage *lightmapRadiosity = nullptr;
    if (rm && rm->lightmapRadiosityMap())
        lightmapRadiosity = rm->lightmapRadiosityMap();
    else if (m->lightmapRadiosityMap())
        lightmapRadiosity = m->lightmapRadiosityMap();
    if (lightmapRadiosity) {
        if (!data->lightmapRadiosityParams.sampler)
            prepareTextureParameters(data->lightmapRadiosityParams, QLatin1String("lightmapRadiosity"), lightmapRadiosity);
        paramList.append(data->lightmapRadiosityParams.parameters());
    }

    Q3DSImage *lightmapShadow = nullptr;
    if (rm && rm->lightmapShadowMap())
        lightmapShadow = rm->lightmapShadowMap();
    else if (m->lightmapShadowMap())
        lightmapShadow = m->lightmapShadowMap();
    if (lightmapShadow) {
        if (!data->lightmapShadowParams.sampler)
            prepareTextureParameters(data->lightmapShadowParams, QLatin1String("lightmapShadow"), lightmapShadow);
        paramList.append(data->lightmapShadowParams.parameters());
    }

    // IBL
    Q3DSImage *iblOverrideImage = nullptr;
    if (rm && rm->lightProbe())
        iblOverrideImage = rm->lightProbe();
    else if (m->lightProbe())
        iblOverrideImage = m->lightProbe();
    if (iblOverrideImage) {
        if (!data->lightProbeOverrideTexture) {
            data->lightProbeOverrideTexture = Q3DSImageManager::instance().newTextureForImageFile(
                        m_rootEntity, Q3DSImageManager::GenerateMipMapsForIBL,
                        m_profiler, "Texture for image %s", iblOverrideImage->id().constData());
            data->lightProbeSampler = new Qt3DRender::QParameter;
            data->lightProbeSampler->setName(QLatin1String("light_probe"));

            data->lightProbeRotation = new Qt3DRender::QParameter;
            data->lightProbeRotation->setName(QLatin1String("light_probe_rotation"));

            data->lightProbeOffset = new Qt3DRender::QParameter;
            data->lightProbeOffset->setName(QLatin1String("light_probe_offset"));
        }
        paramList.append(data->lightProbeSampler);
        paramList.append(data->lightProbeRotation);
        paramList.append(data->lightProbeOffset);
    }

    return paramList;
}

void Q3DSSceneManager::updateCustomMaterial(Q3DSCustomMaterialInstance *m, Q3DSReferencedMaterial *rm, Q3DSModelNode *model3DS)
{
    // This function is typically called in a loop for each Q3DSModelNode that
    // shares the same Q3DSCustomMaterial(Instance). Having more than one model
    // can only happen when a ReferencedMaterial is used. Currently we are
    // somewhat wasteful in this case since setting the QParameter values over
    // and over again is not really needed, with the exception of the one
    // model-specific one. We'll live with this for the time being.

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

    auto perModelDataIt = data->perModelData.find(model3DS);
    Q_ASSERT(perModelDataIt != data->perModelData.end());
    perModelDataIt->objectOpacityParam->setValue(perModelDataIt->combinedOpacity);

    // Lightmaps
    Q3DSImage *lightmapIndirect = nullptr;
    if (rm && rm->lightmapIndirectMap())
        lightmapIndirect = rm->lightmapIndirectMap();
    else if (m->lightmapIndirectMap())
        lightmapIndirect = m->lightmapIndirectMap();
    if (lightmapIndirect)
        updateTextureParameters(data->lightmapIndirectParams, lightmapIndirect);

    Q3DSImage *lightmapRadiosity = nullptr;
    if (rm && rm->lightmapRadiosityMap())
        lightmapRadiosity = rm->lightmapRadiosityMap();
    else if (m->lightmapRadiosityMap())
        lightmapRadiosity = m->lightmapRadiosityMap();
    if (lightmapRadiosity)
        updateTextureParameters(data->lightmapRadiosityParams, lightmapRadiosity);

    Q3DSImage *lightmapShadow = nullptr;
    if (rm && rm->lightmapShadowMap())
        lightmapShadow = rm->lightmapShadowMap();
    else if (m->lightmapShadowMap())
        lightmapShadow = m->lightmapShadowMap();
    if (lightmapShadow)
        updateTextureParameters(data->lightmapShadowParams, lightmapShadow);

    // IBL Override
    Q3DSImage *iblOverride = nullptr;
    if (rm && rm->lightProbe())
        iblOverride = rm->lightProbe();
    else if (m->lightProbe())
        iblOverride = m->lightProbe();

    // IBL
    if (iblOverride) {
        // also sets min/mag and generates mipmaps
        Q3DSImageManager::instance().setSource(data->lightProbeOverrideTexture,
                                               QUrl::fromLocalFile(iblOverride->sourcePath()));
        data->lightProbeSampler->setValue(QVariant::fromValue(data->lightProbeOverrideTexture));

        Qt3DRender::QTextureWrapMode wrapMode;
        wrapMode.setX(Qt3DRender::QTextureWrapMode::Repeat);

        switch (iblOverride->verticalTiling()) {
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

        Q_ASSERT(data->lightProbeOverrideTexture);
        data->lightProbeOverrideTexture->setWrapMode(wrapMode);

        const QMatrix4x4 &textureTransform = iblOverride->textureTransform();
        const float *m = textureTransform.constData();

        // offsets.w = max mip level
        const QSize texSize = Q3DSImageManager::instance().size(data->lightProbeOverrideTexture);
        float mipLevels = float(qCeil(qLog2(qMax(texSize.width(), texSize.height()))));
        QVector4D offsets(m[12], m[13], 0.0f, mipLevels);
        data->lightProbeOffset->setValue(offsets);

        QVector4D rotations(m[0], m[4], m[1], m[5]);
        data->lightProbeRotation->setValue(rotations);
    }
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
        // The effect's output texture is never multisampled. If the layer
        // (i.e. the effect's input) uses MSAA or SSAA then an extra resolve
        // step will be applied via BlitFramebuffer. From that point on
        // everything behaves like non-MSAA.
        const QSize sz = safeLayerPixelSize(layerData);
        layerData->effLayerTexture = newColorBuffer(sz, 1);
        layerData->effLayerTexture->setParent(layerData->entity);
        m_profiler->trackNewObject(layerData->effLayerTexture, Q3DSProfiler::Texture2DObject,
                                   "Effect buffer for layer %s", layer3DS->id().constData());
        layerData->sizeManagedTextures.append(layerData->effLayerTexture);
    }
}

static inline void setTextureInfoUniform(Qt3DRender::QParameter *param, Qt3DRender::QAbstractTexture *texture)
{
    const QSize size = Q3DSImageManager::instance().size(texture);
    const bool isPremultiplied = false;
    param->setValue(QVector4D(size.width(), size.height(), isPremultiplied ? 1 : 0, 0));
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

// The main entry point for activating/deactivating effects on a layer. Called
// upon scene building and every time an effect gets hidden/shown (eyeball).
void Q3DSSceneManager::updateEffectStatus(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    const int count = layerData->effectData.effects.count();
    int activeEffectCount = 0;
    int firstActiveIndex = 0;
    int lastActiveIndex = 0;
    bool change = false;

    for (int i = 0; i < count; ++i) {
        Q3DSEffectInstance *eff3DS = layerData->effectData.effects[i];
        Q3DSEffectAttached *effData = static_cast<Q3DSEffectAttached *>(eff3DS->attached());
        if (eff3DS->eyeballEnabled() && effData->visibilityTag == Q3DSGraphObjectAttached::Visible) {
            ++activeEffectCount;
            if (activeEffectCount == 1)
                firstActiveIndex = i;
            lastActiveIndex = i;
            if (!effData->active)
                change = true;
        } else {
            if (effData->active)
                change = true;
        }
    }

    if (!change)
        return; // nothing has changed

    // Activating/deactivating an effect in the chain needs resetting
    // source/output related settings in the other effects so it is simpler to
    // reactivate everything (considering also that multiple effects (like more
    // than two) are rare).

    qCDebug(lcScene, "Reinitializing effect chain (%d of %d active) on layer %s",
            activeEffectCount, count, layer3DS->id().constData());

    for (int i = 0; i < count; ++i) {
        Q3DSEffectInstance *eff3DS = layerData->effectData.effects[i];
        deactivateEffect(eff3DS, layer3DS);
    }

    cleanupEffectSource(layer3DS);

    if (activeEffectCount) {
        ensureEffectSource(layer3DS);

        Qt3DRender::QAbstractTexture *prevOutput = nullptr;
        for (int i = 0; i < count; ++i) {
            Q3DSEffectInstance *eff3DS = layerData->effectData.effects[i];
            if (eff3DS->eyeballEnabled() && eff3DS->attached()->visibilityTag == Q3DSGraphObjectAttached::Visible) {
                Q3DSSceneManager::EffectActivationFlags flags = 0;
                if (i == firstActiveIndex)
                    flags |= Q3DSSceneManager::EffIsFirst;
                if (i == lastActiveIndex)
                    flags |= Q3DSSceneManager::EffIsLast;
                activateEffect(eff3DS, layer3DS, flags, prevOutput);
                prevOutput = eff3DS->attached<Q3DSEffectAttached>()->outputTexture;
            }
        }
    }

    const bool wasActive = layerData->effectActive;
    if (activeEffectCount) {
        // The layer compositor must use the output of the effect passes from now on.
        layerData->compositorSourceParam->setValue(QVariant::fromValue(layerData->effLayerTexture));
        layerData->effectActive = true;
        if (!wasActive && layerData->msaaSampleCount > 1)
            updateLayerCompositorProgram(layer3DS);
    } else {
        layerData->compositorSourceParam->setValue(QVariant::fromValue(layerData->layerTexture));
        layerData->effectActive = false;
        if (wasActive && layerData->msaaSampleCount > 1)
            updateLayerCompositorProgram(layer3DS);
    }
}

void Q3DSSceneManager::ensureEffectSource(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    // MSAA/SSAA layers need an additional resolve step since effects can only
    // work with non-MSAA (and 1:1 sized) textures as input.
    const bool needsResolve = layerData->msaaSampleCount > 1 || layerData->ssaaScaleFactor > 1;
    if (needsResolve) {
        layerData->effectData.sourceTexture = new Qt3DRender::QTexture2D(m_rootEntity);
        m_profiler->trackNewObject(layerData->effectData.sourceTexture, Q3DSProfiler::Texture2DObject,
                                   "Resolve buffer for effects");
        layerData->effectData.sourceTexture->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
        layerData->effectData.sourceTexture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
        layerData->effectData.sourceTexture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
        layerData->effectData.ownsSourceTexture = true;

        Qt3DRender::QRenderTarget *rtSrc = new Qt3DRender::QRenderTarget;
        m_profiler->trackNewObject(rtSrc, Q3DSProfiler::RenderTargetObject,
                                   "Src resolve RT for effects");
        Qt3DRender::QRenderTargetOutput *rtSrcOutput = new Qt3DRender::QRenderTargetOutput;
        rtSrcOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
        rtSrcOutput->setTexture(layerData->layerTexture);
        rtSrc->addOutput(rtSrcOutput);

        Qt3DRender::QRenderTarget *rtDst = new Qt3DRender::QRenderTarget;
        m_profiler->trackNewObject(rtDst, Q3DSProfiler::RenderTargetObject,
                                   "Dst resolve RT for effects");
        Qt3DRender::QRenderTargetOutput *rtDstOutput = new Qt3DRender::QRenderTargetOutput;
        rtDstOutput->setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color0);
        rtDstOutput->setTexture(layerData->effectData.sourceTexture);
        rtDst->addOutput(rtDstOutput);

        Qt3DRender::QBlitFramebuffer *resolve = new Qt3DRender::QBlitFramebuffer(layerData->effectData.effectRoot);
        layerData->effectData.resolve = resolve;
        new Qt3DRender::QNoDraw(resolve);
        resolve->setSource(rtSrc);
        resolve->setDestination(rtDst);

        auto blitResizer = [resolve, layerData](Q3DSLayerNode *) {
            resolve->setSourceRect(QRectF(QPointF(0, 0), layerData->layerSize * layerData->ssaaScaleFactor));
            resolve->setDestinationRect(QRectF(QPointF(0, 0), layerData->layerSize));
        };

        // must track layer size, but without the SSAA scale
        prepareSizeDependentTexture(layerData->effectData.sourceTexture, layer3DS, blitResizer,
                                    Q3DSLayerAttached::SizeManagedTexture::IgnoreSSAA);
        // set initial blit rects
        blitResizer(layer3DS);
    } else {
        layerData->effectData.sourceTexture = layerData->layerTexture;
        layerData->effectData.ownsSourceTexture = false;
        layerData->effectData.resolve = nullptr;
    }
}

void Q3DSSceneManager::cleanupEffectSource(Q3DSLayerNode *layer3DS)
{
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    delete layerData->effectData.resolve;
    layerData->effectData.resolve = nullptr;
    if (layerData->effectData.ownsSourceTexture) {
        layerData->effectData.ownsSourceTexture = false;
        layerData->sizeManagedTextures.removeOne(layerData->effectData.sourceTexture);
        delete layerData->effectData.sourceTexture;
    }
}

void Q3DSSceneManager::activateEffect(Q3DSEffectInstance *eff3DS,
                                      Q3DSLayerNode *layer3DS,
                                      EffectActivationFlags flags,
                                      Qt3DRender::QAbstractTexture *prevOutput)
{
    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());

    Q3DSEffectAttached *effData = static_cast<Q3DSEffectAttached *>(eff3DS->attached());
    if (effData->active)
        return;

    qCDebug(lcPerf, "Applying post-processing effect %s to layer %s",
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

    // Mark the effect as created (even though it's still in progress) since
    // the updateEffect* functions need this flag.
    effData->active = true;

    // Set initial QParameter (uniform) values.
    updateEffect(eff3DS);

    // The first effect takes either the layer's output texture or a resolved
    // (if MSAA/SSAA) version of it as its input. The last effect outputs to
    // effLayerTexture. Effects in-between have their own output textures while
    // the source refers to the output of the previous effect.
    if (flags.testFlag(EffIsFirst)) {
        Q_ASSERT(layerData->effectData.sourceTexture);
        effData->sourceTexture = layerData->effectData.sourceTexture;
    } else {
        Q_ASSERT(prevOutput);
        effData->sourceTexture = prevOutput;
    }
    if (flags.testFlag(EffIsLast)) {
        Q_ASSERT(layerData->effLayerTexture);
        effData->outputTexture = layerData->effLayerTexture;
        effData->ownsOutputTexture = false;
    } else {
        effData->outputTexture = new Qt3DRender::QTexture2D(m_rootEntity);
        m_profiler->trackNewObject(effData->outputTexture, Q3DSProfiler::Texture2DObject,
                                   "Output texture for effect %s", eff3DS->id().constData());
        effData->outputTexture->setFormat(Qt3DRender::QAbstractTexture::RGBA8_UNorm);
        effData->outputTexture->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
        effData->outputTexture->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);
        prepareSizeDependentTexture(effData->outputTexture, layer3DS);
        effData->ownsOutputTexture = true;
    }

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

        // figure out what the effect wants as shader inputs or when it comes to render states
        for (const Q3DSMaterial::PassCommand &cmd : pass.commands) {
            switch (cmd.type()) {
            case Q3DSMaterial::PassCommand::BufferInputType:
            {
                bool valid = true;
                const QString bufferName = cmd.data()->value;
                if (bufferName == QStringLiteral("[source]")) {
                    Qt3DRender::QParameter *texParam = new Qt3DRender::QParameter;
                    texParam->setName(cmd.data()->param);
                    texParam->setValue(QVariant::fromValue(effData->sourceTexture));
                    paramList.append(texParam);
                    Qt3DRender::QParameter *texInfoParam = new Qt3DRender::QParameter;
                    texInfoParam->setName(cmd.data()->param + QLatin1String("Info"));
                    setTextureInfoUniform(texInfoParam, effData->sourceTexture);
                    effData->sourceDepTextureInfoParams.append(qMakePair(texInfoParam, effData->sourceTexture));
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
                    // Can conveniently use the source texture since the
                    // sizes must match. This is very handy esp. with
                    // sourceDepTextureInfoParams since we get size updates
                    // via the same code path.
                    setTextureInfoUniform(texInfoParam, effData->sourceTexture);
                    effData->sourceDepTextureInfoParams.append(qMakePair(texInfoParam, effData->sourceTexture));
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

        // input
        Qt3DRender::QAbstractTexture *passInput = nullptr;
        if (pass.input == QStringLiteral("[source]")) {
            passInput = effData->sourceTexture;
        } else {
            if (effData->textureBuffers.contains(pass.input)) {
                passInput = effData->textureBuffers.value(pass.input).texture;
            } else {
                qWarning("Effect %s: Unknown input buffer %s; pass ignored",
                         eff3DS->id().constData(), qPrintable(pass.input));
                continue;
            }
        }

        // output
        bool outputNeedsClear = false;
        Qt3DRender::QAbstractTexture *passOutput = nullptr;
        if (pass.output == QStringLiteral("[dest]")) {
            passOutput = effData->outputTexture;
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

        // shader uniforms
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

        // get the shader program
        const Q3DSMaterial::Shader &shaderProgram = !implicitPass ? shaderPrograms.value(pass.shaderName) : shaderPrograms.first();
        const QString decoratedShaderName = QString::fromUtf8(eff3DS->id()) + QLatin1Char('_') + pass.shaderName;
        const QString decoratedVertexShader = effDesc->addPropertyUniforms(shaderProgram.vertexShader);
        const QString decoratedFragmentShader = effDesc->addPropertyUniforms(shaderProgram.fragmentShader);
        Qt3DRender::QShaderProgram *prog = Q3DSShaderManager::instance().getEffectShader(m_rootEntity,
                                                                                         decoratedShaderName,
                                                                                         decoratedVertexShader,
                                                                                         decoratedFragmentShader);

        effData->quadEntityTag = new Qt3DRender::QLayer(layerData->entity);
        effData->quadEntityTag->setObjectName(QLatin1String("Effect quad pass"));

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

        effData->quadEntity = buildFsQuad(quadInfo);

        // framegraph
        Qt3DRender::QRenderTargetSelector *rtSel = new Qt3DRender::QRenderTargetSelector(layerData->effectData.effectRoot);
        effData->passFgRoots.append(rtSel);
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

void Q3DSSceneManager::deactivateEffect(Q3DSEffectInstance *eff3DS, Q3DSLayerNode *layer3DS)
{
    Q3DSEffectAttached *effData = static_cast<Q3DSEffectAttached *>(eff3DS->attached());
    if (!effData->active)
        return;

    qCDebug(lcPerf, "Disabling post-processing effect %s on layer %s",
            eff3DS->id().constData(), layer3DS->id().constData());

    effData->active = false;

    // Kill the quad entity, the framegraph additions that were made under
    // effectRoot, and any textures that were created by activateEffect.

    delete effData->quadEntity;

    qDeleteAll(effData->passFgRoots);

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
    for (const Q3DSEffectAttached::TextureBuffer &tb : effData->textureBuffers) {
        layerData->sizeManagedTextures.removeOne(tb.texture);
        delete tb.texture;
    }

    if (effData->ownsOutputTexture) {
        layerData->sizeManagedTextures.removeOne(effData->outputTexture);
        delete effData->outputTexture;
    }

    effData->reset();
    effData->layer3DS = layer3DS;
}

void Q3DSEffectAttached::reset()
{
    active = false;
    layer3DS = nullptr;
    quadEntity = nullptr;
    quadEntityTag = nullptr;
    params.clear();
    appFrameParam = fpsParam = cameraClipRangeParam = nullptr;
    textureBuffers.clear();
    passData.clear();
    sourceDepTextureInfoParams.clear();
    passFgRoots.clear();
    sourceTexture = outputTexture = nullptr;
    ownsOutputTexture = false;
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
        const QSize sz = safeLayerPixelSize(layerData->layerSize, 1); // SSAA scale factor no longer plays a role
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

// called once on load from applyEffect, and then every time an effect property has changed
void Q3DSSceneManager::updateEffect(Q3DSEffectInstance *eff3DS)
{
    Q3DSEffectAttached *effData = static_cast<Q3DSEffectAttached *>(eff3DS->attached());
    if (!effData->active)
        return;

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
    if (!effData->active)
        return;

    effData->appFrameParam->setValue(float(nextFrameNo));
    effData->fpsParam->setValue(60.0f); // heh
    for (const auto &pd : effData->passData) {
        const bool isPremultiplied = false;
        pd.texture0InfoParam->setValue(QVector4D(pd.passInput->width(),
                                                 pd.passInput->height(),
                                                 isPremultiplied ? 1 : 0,
                                                 0));
        pd.destSizeParam->setValue(QVector2D(pd.passOutput->width(), pd.passOutput->height()));
    }
    for (const auto &tb : effData->textureBuffers) {
        for (Qt3DRender::QParameter *param : tb.textureInfoParams)
            setTextureInfoUniform(param, tb.texture);
    }
    for (const auto &p : effData->sourceDepTextureInfoParams)
        setTextureInfoUniform(p.first, p.second);
}

namespace {
Q3DSLayerNode *getLayerForObject(Q3DSGraphObject *object) {
    Q3DSGraphObject *target = object;
    while (target) {
        if (target->type() == Q3DSGraphObject::Layer)
            return static_cast<Q3DSLayerNode *>(target);
        target = target->parent();
    }
    return nullptr;
}

bool isLightScopeValid(Q3DSGraphObject *target, Q3DSLayerNode *layer) {
    if (target->type() == Q3DSGraphObject::Scene)
        return false;
    if (getLayerForObject(target) == layer)
        return true;
    return false;
}
}

void Q3DSSceneManager::gatherLights(Q3DSLayerNode *layer)
{
    if (!layer)
        return;

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer->attached());
    QSet<Q3DSNodeAttached::LightsData*> resetMap;

    Q3DSUipPresentation::forAllObjectsInSubTree(layer, [layer,layerData,&resetMap](Q3DSGraphObject *object){
        if (object->type() == Q3DSGraphObject::Light) {
            Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(object);
            Q3DSNodeAttached::LightsData *lightData = layerData->lightsData.data();
            // If this light is a scoped light, setup the scoped node with lightsData
            if (light3DS->scope() && isLightScopeValid(light3DS->scope(), layer)) {
                Q3DSNodeAttached *scopeData = static_cast<Q3DSNodeAttached *>(light3DS->scope()->attached());
                if (!scopeData->lightsData) {
                    scopeData->lightsData.reset(new Q3DSNodeAttached::LightsData);
                    // new lightData doesn't need to be reset
                    resetMap.insert(scopeData->lightsData.data());
                }
                lightData = scopeData->lightsData.data();
            }
            // reset lightData if necessary
            if (!resetMap.contains(lightData)) {
                lightData->lightNodes.clear();
                lightData->allLights.clear();
                lightData->nonAreaLights.clear();
                lightData->areaLights.clear();
                resetMap.insert(lightData);
            }

            if (light3DS->flags().testFlag(Q3DSNode::Active)) {
                Q3DSLightAttached *data = static_cast<Q3DSLightAttached *>(light3DS->attached());
                Q_ASSERT(data);
                lightData->lightNodes.append(light3DS);
                lightData->allLights.append(data->lightSource);
                if (light3DS->lightType() == Q3DSLightNode::Area)
                    lightData->areaLights.append(data->lightSource);
                else
                    lightData->nonAreaLights.append(data->lightSource);
            }
        }
    });
}

QVector<Qt3DRender::QParameter*> Q3DSSceneManager::prepareSeparateLightUniforms(const QVector<Q3DSLightSource> &allLights, const QString &lightsUniformName)
{
    QVector<Qt3DRender::QParameter*> params;
    for (int i = 0; i < allLights.size(); ++i) {
        if (i >= m_gfxLimits.maxLightsPerLayer)
            break;

        const QString uniformPrefix = lightsUniformName + QStringLiteral("[") + QString::number(i) + QStringLiteral("].");

        // position
        allLights[i].positionParam->setName(uniformPrefix + QStringLiteral("position"));
        params.append(allLights[i].positionParam);

        // direction
        allLights[i].directionParam->setName(uniformPrefix + QStringLiteral("direction"));
        params.append(allLights[i].directionParam);

        // up
        allLights[i].upParam->setName(uniformPrefix + QStringLiteral("up"));
        params.append(allLights[i].upParam);

        // right
        allLights[i].rightParam->setName(uniformPrefix + QStringLiteral("right"));
        params.append(allLights[i].rightParam);

        // diffuse
        allLights[i].diffuseParam->setName(uniformPrefix + QStringLiteral("diffuse"));
        params.append(allLights[i].diffuseParam);

        // ambient
        allLights[i].ambientParam->setName(uniformPrefix + QStringLiteral("ambient"));
        params.append(allLights[i].ambientParam);

        // specular
        allLights[i].specularParam->setName(uniformPrefix + QStringLiteral("specular"));
        params.append(allLights[i].specularParam);

        // spotExponent (not used)
        //allLights[i].spotExponentParam->setName(uniformPrefix + QStringLiteral("spotExponent"));
        //params.append(allLights[i].spotExponentParam);

        // spotCutoff (not used)
        //allLights[i].spotCutoffParam->setName(uniformPrefix + QStringLiteral("spotCutoff"));
        //params.append(allLights[i].spotCutoffParam);

        // constantAttenuation
        allLights[i].constantAttenuationParam->setName(uniformPrefix + QStringLiteral("constantAttenuation"));
        params.append(allLights[i].constantAttenuationParam);

        // linearAttenuation
        allLights[i].linearAttenuationParam->setName(uniformPrefix + QStringLiteral("linearAttenuation"));
        params.append(allLights[i].linearAttenuationParam);

        // quadraticAttenuation
        allLights[i].quadraticAttenuationParam->setName(uniformPrefix + QStringLiteral("quadraticAttenuation"));
        params.append(allLights[i].quadraticAttenuationParam);

        // width
        allLights[i].widthParam->setName(uniformPrefix + QStringLiteral("width"));
        params.append(allLights[i].widthParam);

        // height
        allLights[i].heightParam->setName(uniformPrefix + QStringLiteral("height"));
        params.append(allLights[i].heightParam);

        // shadowControls
        allLights[i].shadowControlsParam->setName(uniformPrefix + QStringLiteral("shadowControls"));
        params.append(allLights[i].shadowControlsParam);

        // shadowView
        allLights[i].shadowViewParam->setName(uniformPrefix + QStringLiteral("shadowView"));
        params.append(allLights[i].shadowViewParam);

        // shadowIdx
        allLights[i].shadowIdxParam->setName(uniformPrefix + QStringLiteral("shadowIdx"));
        params.append(allLights[i].shadowIdxParam);
    }
    return params;
}

void Q3DSSceneManager::updateLightsParams(const QVector<Q3DSNodeAttached::LightsData *> &lights, Q3DSNodeAttached::LightsData *dst)
{
    QVector3D lightAmbientTotal;
    for (auto lightsDataSub : lights) {
        for (auto light : lightsDataSub->lightNodes) {
            lightAmbientTotal += QVector3D(light->ambient().redF(),
                                           light->ambient().greenF(),
                                           light->ambient().blueF());
        }
    }

    if (!dst->lightAmbientTotalParamenter) {
        dst->lightAmbientTotalParamenter = new Qt3DRender::QParameter;
        dst->lightAmbientTotalParamenter->setName(QLatin1String("light_ambient_total"));
    }

    dst->lightAmbientTotalParamenter->setValue(lightAmbientTotal);
}

void Q3DSSceneManager::updateLightsBuffer(const QVector<Q3DSLightSource> &lights, Qt3DRender::QBuffer *uniformBuffer)
{
    if (!uniformBuffer) // no models in the layer -> no buffers -> handle gracefully
        return; // can also get here when no custom material-specific buffers exist for a given layer because it only uses default material, this is normal

    Q_ASSERT(sizeof(Q3DSLightSourceData) == 240);
    // uNumLights takes 4 * sizeof(qint32) since the next member must be 4N (16 bytes) aligned
    const int uNumLightsSize = 4 * sizeof(qint32);
    QByteArray lightBufferData((sizeof(Q3DSLightSourceData) * m_gfxLimits.maxLightsPerLayer) + uNumLightsSize, '\0');
    // Set the number of lights
    qint32 *numLights = reinterpret_cast<qint32 *>(lightBufferData.data());
    *numLights = lights.count();
    // Set the lightData
    Q3DSLightSourceData *lightData = reinterpret_cast<Q3DSLightSourceData *>(lightBufferData.data() + uNumLightsSize);
    for (int i = 0; i < lights.count(); ++i) {
        if (i >= m_gfxLimits.maxLightsPerLayer)
            break;
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
        memcpy(lightData[i].m_shadowView, lights[i].shadowViewParam->value().value<QMatrix4x4>().constData(), 16 * sizeof(float));
        // shadowIdx
        lightData[i].m_shadowIdx = lights[i].shadowIdxParam->value().toInt();
    }
    uniformBuffer->setData(lightBufferData);
}

void Q3DSSceneManager::updateModel(Q3DSModelNode *model3DS)
{
    Q3DSModelAttached *data = static_cast<Q3DSModelAttached *>(model3DS->attached());
    Q_ASSERT(data);

    if (data->frameDirty.testFlag(Q3DSGraphObjectAttached::GlobalOpacityDirty)) {
        // Apply opaque or transparent pass tag to the submeshes.
        retagSubMeshes(model3DS);
        // The model's and material's opacity are both used to determine the final
        // opacity that goes to the shader. Update this.
        Q3DSModelAttached *data = static_cast<Q3DSModelAttached *>(model3DS->attached());
        for (Q3DSModelAttached::SubMesh &sm : data->subMeshes) {
            if (sm.resolvedMaterial->type() == Q3DSGraphObject::DefaultMaterial) {
                auto m = static_cast<Q3DSDefaultMaterial *>(sm.resolvedMaterial);
                auto d = static_cast<Q3DSDefaultMaterialAttached *>(m->attached());
                if (d) {
                    auto perModelDataIt = d->perModelData.find(model3DS);
                    if (perModelDataIt != d->perModelData.end()) {
                        perModelDataIt->combinedOpacity = clampOpacity(data->globalOpacity * (m->opacity() / 100.0f));
                        QVector4D c = perModelDataIt->materialDiffuseParam->value().value<QVector4D>();
                        c.setW(perModelDataIt->combinedOpacity);
                        perModelDataIt->materialDiffuseParam->setValue(c);
                    }
                }
            }
        }
    }

    if (data->frameChangeFlags & Q3DSModelNode::MeshChanges) {
        model3DS->resolveReferences(*m_presentation); // make mesh() return the new submesh list
        qCDebug(lcPerf, "Rebuilding submeshes for %s", model3DS->id().constData());
        rebuildModelSubMeshes(model3DS);
        rebuildModelMaterial(model3DS);
    }
}

// This method is used to get the lights based on the scope of current object
QVector<Q3DSNodeAttached::LightsData *> Q3DSSceneManager::getLightsDataForNode(Q3DSGraphObject *object)
{
    QVector<Q3DSNodeAttached::LightsData *> lightsDatas;
    Q3DSGraphObject *target = object;
    while (target) {
        auto targetData = static_cast<Q3DSNodeAttached*>(target->attached());
        if (targetData)
            if (targetData->lightsData)
                lightsDatas.append(targetData->lightsData.data());
        target = target->parent();
    }
    return lightsDatas;
}

// when entering a slide, or when animating a property
void Q3DSSceneManager::handlePropertyChange(Q3DSGraphObject *obj, const QSet<QString> &keys, int changeFlags)
{
    // 'keys' is not used here, rely rather on the pre-baked changeFlags to
    // determine certain special cases. For others it is enough to
    // know that _something_ has changed.
    Q_UNUSED(keys);

    Q3DSGraphObjectAttached *data = obj->attached();
    if (!data) // Qt3D stuff not yet built for this object -> nothing to do
        return;

    // all actual processing must be deferred to updateSubTreeRecursive()
    switch (obj->type()) {
    case Q3DSGraphObject::Layer:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::LayerDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;
    case Q3DSGraphObject::Camera:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::CameraDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;

    case Q3DSGraphObject::DefaultMaterial:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::DefaultMaterialDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;
    case Q3DSGraphObject::CustomMaterial:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::CustomMaterialDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;
    case Q3DSGraphObject::Effect:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::EffectDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;
    case Q3DSGraphObject::Behavior:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::BehaviorDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;
    case Q3DSGraphObject::Image:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::ImageDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;

    case Q3DSGraphObject::Group:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::GroupDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;
    case Q3DSGraphObject::Component:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::ComponentDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;
    case Q3DSGraphObject::Light:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::LightDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;
    case Q3DSGraphObject::Model:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::ModelDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;
    case Q3DSGraphObject::Text:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::TextDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;
    case Q3DSGraphObject::Alias:
    {
        data->frameDirty |= Q3DSGraphObjectAttached::AliasDirty;
        data->frameChangeFlags |= changeFlags;
    }
        break;
    default:
        break;
    }

    // Note the lack of call to syncScene(). That happens in a QFrameAction once per frame.
}

void Q3DSSceneManager::syncScene()
{
    m_subTreesWithDirtyLights.clear();
    m_pendingDefMatRebuild.clear();

    updateSubTreeRecursive(m_scene);

    QSet<Q3DSModelNode *> needsRebuild;

    for (const SubTreeWithDirtyLight &dl : m_subTreesWithDirtyLights) {
        Q3DSGraphObject *subtreeObject = dl.first;
        const bool lightVisibilityChanged = dl.second;
        // Attempt to update all buffers, if some do not exist (null) that's fine too.
        auto lights = getLightsDataForNode(subtreeObject);

        QVector<Q3DSLightSource> allLights;
        QVector<Q3DSLightSource> nonAreaLights;
        QVector<Q3DSLightSource> areaLights;

        if (!lights.isEmpty()) {
            Q3DSNodeAttached::LightsData *lightsData = lights.first();
            for (auto light : lights) {
                allLights.append(light->allLights);
                nonAreaLights.append(light->nonAreaLights);
                areaLights.append(light->areaLights);
            }
            updateLightsBuffer(allLights, lightsData->allLightsConstantBuffer);
            updateLightsBuffer(nonAreaLights, lightsData->nonAreaLightsConstantBuffer);
            updateLightsBuffer(areaLights, lightsData->areaLightsConstantBuffer);
        }

        bool smDidChange = false;
        auto layer3DS = getLayerForObject(subtreeObject);
        updateShadowMapStatus(layer3DS, &smDidChange);
        if (smDidChange) {
            Q3DSUipPresentation::forAllModels(layer3DS->firstChild(),
                                           [&needsRebuild](Q3DSModelNode *model3DS) { needsRebuild.insert(model3DS); },
                                           true); // include hidden ones too
        }

        bool hasDefaultMaterial = false;
        Q3DSUipPresentation::forAllObjectsInSubTree(subtreeObject, [&hasDefaultMaterial, &needsRebuild, lightVisibilityChanged](Q3DSGraphObject *obj) {
            if (obj->type() == Q3DSGraphObject::DefaultMaterial) {
                hasDefaultMaterial = true;
                // in addition to shadow or ssao changes, the default material's shader
                // code is dependent on the number of lights as well, so may need to rebuild materials...
                if (lightVisibilityChanged) { // ...but only when it's sure the number of lights has changed
                    if (obj->parent() && obj->parent()->type() == Q3DSGraphObject::Model) // this could probably have been an assert
                        needsRebuild.insert(static_cast<Q3DSModelNode *>(obj->parent()));
                }
            }
        });
        if (hasDefaultMaterial) {
            // update uniforms like ambient total
            if (!lights.isEmpty())
                updateLightsParams(lights, lights.first());
        }
    }

    for (Q3DSDefaultMaterial *mat3DS : m_pendingDefMatRebuild) {
        if (Q3DSDefaultMaterialAttached *matData = static_cast<Q3DSDefaultMaterialAttached *>(mat3DS->attached())) {
            for (auto it = matData->perModelData.cbegin(), itEnd = matData->perModelData.cend(); it != itEnd; ++it)
                needsRebuild.insert(it.key());
        }
    }

    for (Q3DSModelNode *model3DS : needsRebuild)
        rebuildModelMaterial(model3DS);

    setPendingVisibilities();
}

void Q3DSSceneManager::setPendingVisibilities()
{
    for (auto it = m_pendingObjectVisibility.constBegin(); it != m_pendingObjectVisibility.constEnd(); ++it) {
        if (it.key()->isNode() && it.key()->type() != Q3DSGraphObject::Layer && it.key()->type() != Q3DSGraphObject::Camera) {
            Q3DSNode *node = static_cast<Q3DSNode *>(it.key());
            const bool visible = (it.value() && node->flags().testFlag(Q3DSNode::Active));
            it.key()->attached()->visibilityTag = visible ? Q3DSGraphObjectAttached::Visible
                                                          : Q3DSGraphObjectAttached::Hidden;
            setNodeVisibility(node, visible); // will call updateGlobals() as well
        } else if (it.key()->type() == Q3DSGraphObject::Effect){
            Q3DSEffectInstance *effect = static_cast<Q3DSEffectInstance *>(it.key());
            Q3DSEffectAttached *data = effect->attached<Q3DSEffectAttached>();
            if (data) {
                data->visibilityTag = (it.value() && effect->eyeballEnabled()) ? Q3DSGraphObjectAttached::Visible
                                                                               : Q3DSGraphObjectAttached::Hidden;
                updateEffectStatus(effect->attached<Q3DSEffectAttached>()->layer3DS);
            }
        } else if (it.key()->type() == Q3DSGraphObject::Layer) {
            Q3DSLayerNode *layer3DS = static_cast<Q3DSLayerNode *>(it.key());
            Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
            if (data) {
                const bool enabled = it.value() && layer3DS->flags().testFlag(Q3DSNode::Active);
                data->visibilityTag = enabled ? Q3DSGraphObjectAttached::Visible
                                              : Q3DSGraphObjectAttached::Hidden;
                updateGlobals(layer3DS, UpdateGlobalsRecursively | UpdateGlobalsSkipTransform);
                if (data->compositorEntity) // may not exist if this is still buildLayer()
                    data->compositorEntity->setEnabled(enabled);
            }
        } else if (it.key()->type() == Q3DSGraphObject::Camera) {
            Q3DSCameraNode *cam3DS = static_cast<Q3DSCameraNode *>(it.key());
            Q3DSCameraAttached *data = static_cast<Q3DSCameraAttached *>(cam3DS->attached());
            if (data) {
                const bool visible = (it.value() && cam3DS->flags().testFlag(Q3DSNode::Active));
                data->visibilityTag = visible ? Q3DSGraphObjectAttached::Visible
                                              : Q3DSGraphObjectAttached::Hidden;
                updateGlobals(cam3DS, UpdateGlobalsRecursively | UpdateGlobalsSkipTransform);
                // leave rechoosing the camera to handleNodeGlobalChange()
            }
        } else {
            // nothing to do for materials, the slideplayer may try to manage
            // visibility of those as well (since they are typically in the
            // slides' objects() lists) but on the scene level that is
            // meaningless.
        }
    }

    m_pendingObjectVisibility.clear();
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
            if (!parentMaster->objects().contains(targetComponent) && !parentCurrentSlide->objects().contains(targetComponent)) {
                visible = false;
                break;
            }
            targetComponent = parentComponent;
        } while (targetComponent);
    }

    return visible;
}

void Q3DSSceneManager::prepareNextFrame()
{
    m_wasDirty = false;
    Q3DSUipPresentation::forAllLayers(m_scene, [](Q3DSLayerNode *layer3DS) {
        static_cast<Q3DSLayerAttached *>(layer3DS->attached())->wasDirty = false;
    });

    syncScene();

    qint64 nextFrameNo = m_frameUpdater->frameCounter() + 1;
    Q3DSUipPresentation::forAllLayers(m_scene, [this, nextFrameNo](Q3DSLayerNode *layer3DS) {
        // Dirty flags now up-to-date -> update progressive AA status
        bool paaActive = false;
        if (layer3DS->progressiveAA() != Q3DSLayerNode::NoPAA)
            paaActive = updateProgressiveAA(layer3DS);
        // if progAA is not in progress then maybe we want temporal AA
        if (layer3DS->layerFlags().testFlag(Q3DSLayerNode::TemporalAA) && !paaActive)
            updateTemporalAA(layer3DS);

        // Post-processing effects have uniforms that need to be updated on every frame.
        Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
        if (layerData) {
            for (Q3DSEffectInstance *eff3DS : qAsConst(layerData->effectData.effects))
                updateEffectForNextFrame(eff3DS, nextFrameNo);
        }
    });

    static const bool layerCacheDebug = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 2;
    Q3DSUipPresentation::forAllLayers(m_scene, [this](Q3DSLayerNode *layer3DS) {
        Q3DSLayerAttached *layerData = layer3DS->attached<Q3DSLayerAttached>();
        if (!layerData->layerFgRoot) // layers with a subpresentation won't have this
            return;

        // m_layerCacheDeps holds scenemanagers for subpresentations whose
        // result is used as texture maps by us. Best we can do is to check the
        // global dirty flag and prevent any layer caching to kick in.
        bool subDirty = false;
        for (Q3DSSceneManager *subSceneManager : m_layerCacheDeps) {
            if (subSceneManager->m_wasDirty) {
                subDirty = true;
                break;
            }
        }
        // Any QML subpresentations used as texture map disables layer caching
        // altogether since we have no idea when their content changes.
        if (m_hasQmlSubPresAsTextureMap)
            subDirty = true;

        if (!layerData->wasDirty && !m_layerUncachePending && !subDirty) {
            ++layerData->nonDirtyRenderCount;
            if (layerData->nonDirtyRenderCount > LAYER_CACHING_THRESHOLD) {
                layerData->nonDirtyRenderCount = 0;
                if (m_layerCaching && layerData->layerFgRoot->parentNode() != layerData->layerFgDummyParent) {
                    if (layerCacheDebug)
                        qCDebug(lcScene, "Switching %s to cached", layer3DS->id().constData());
                    layerData->layerFgRoot->setParent(layerData->layerFgDummyParent);
                }
            }
        } else {
            layerData->nonDirtyRenderCount = 0;
            if (layerData->layerFgRoot->parentNode() != layerData->layerFgRootParent) {
                if (layerCacheDebug)
                    qCDebug(lcScene, "Switching %s to non-cached", layer3DS->id().constData());
                layerData->layerFgRoot->setParent(layerData->layerFgRootParent);
            }
        }
    });
    m_layerUncachePending = false;
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
    auto findLayersForMat = [](Q3DSGraphObject *obj) {
        QVector<Q3DSLayerNode *> layers;
        Q3DSMaterialAttached *data = static_cast<Q3DSMaterialAttached *>(obj->attached());
        if (data) {
            for (auto it = data->perModelData.cbegin(), itEnd = data->perModelData.cend(); it != itEnd; ++it) {
                Q3DSModelNode *model3DS = it.key();
                Q3DSNodeAttached *ndata = static_cast<Q3DSNodeAttached *>(model3DS->attached());
                layers.append(ndata->layer3DS);
            }
        }
        return layers;
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
            for (Q3DSLayerNode *layer3DS : findLayersForMat(mat3DS))
                markLayerDirty(layer3DS);
        }
    } else if (obj->type() == Q3DSGraphObject::DefaultMaterial || obj->type() == Q3DSGraphObject::CustomMaterial) {
        for (Q3DSLayerNode *layer3DS : findLayersForMat(obj))
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
    case Q3DSGraphObject::Alias:
        Q_FALLTHROUGH();
    case Q3DSGraphObject::Component:
    {
        // Group, Alias, and Component inherit all interesting properties from Node
        Q3DSNode *node = static_cast<Q3DSNode *>(obj);
        Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(obj->attached());
        if (data)
            updateNodeFromChangeFlags(node, data->transform, data->frameChangeFlags);
    }
        break;
    case Q3DSGraphObject::Text:
    {
        Q3DSTextNode *text3DS = static_cast<Q3DSTextNode *>(obj);
        Q3DSTextAttached *data = static_cast<Q3DSTextAttached *>(text3DS->attached());
        if (data) {
            updateNodeFromChangeFlags(text3DS, data->transform, data->frameChangeFlags);
            if (data->frameDirty & (Q3DSGraphObjectAttached::TextDirty | Q3DSGraphObjectAttached::GlobalOpacityDirty)) {
                const bool needsNewImage = data->frameChangeFlags & Q3DSTextNode::TextureImageDepChanges;
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
            updateNodeFromChangeFlags(light3DS, data->transform, data->frameChangeFlags);
            if (data->frameDirty & (Q3DSGraphObjectAttached::LightDirty | Q3DSGraphObjectAttached::GlobalTransformDirty)) {
                setLightProperties(light3DS);
                if (!(data->frameChangeFlags & Q3DSNode::EyeballChanges)) {// already done if eyeball changed
                    if (light3DS->scope() && isLightScopeValid(light3DS->scope(), data->layer3DS))
                        m_subTreesWithDirtyLights.insert(SubTreeWithDirtyLight(light3DS->scope(), false));
                    else
                        m_subTreesWithDirtyLights.insert(SubTreeWithDirtyLight(data->layer3DS, false));
                }
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
            updateNodeFromChangeFlags(model3DS, data->transform, data->frameChangeFlags);
            if (data->frameDirty & (Q3DSGraphObjectAttached::ModelDirty | Q3DSGraphObjectAttached::GlobalOpacityDirty)) {
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
            updateNodeFromChangeFlags(cam3DS, data->transform, data->frameChangeFlags);
            // no need for special EyeballChanges handling, updateNodeFromChangeFlags took care of that
            if (data->frameDirty & Q3DSGraphObjectAttached::CameraDirty) {
                setCameraProperties(cam3DS, data->frameChangeFlags); // handles both Node- and Camera-level properties
                setLayerCameraSizeProperties(data->layer3DS);
                // (orthographic) shadow maps, ssao texture, effects all rely on camera properties like clip range
                updateShadowMapStatus(data->layer3DS);
                updateSsaoStatus(data->layer3DS);
                for (Q3DSEffectInstance *eff3DS : data->layer3DS->attached<Q3DSLayerAttached>()->effectData.effects)
                    updateEffect(eff3DS);
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
        if (data && (data->frameDirty & Q3DSGraphObjectAttached::LayerDirty)) {
            updateSizesForLayer(layer3DS, data->parentSize);
            setLayerProperties(layer3DS);
            if (data->frameChangeFlags & Q3DSLayerNode::AoOrShadowChanges) {
                bool aoDidChange = false;
                updateSsaoStatus(layer3DS, &aoDidChange);
                if (aoDidChange) {
                    Q3DSUipPresentation::forAllModels(layer3DS->firstChild(),
                                                   [this](Q3DSModelNode *model3DS) { rebuildModelMaterial(model3DS); },
                    true); // include hidden ones too
                }
            }
            // regen light/shadow/materials when a light was added/removed under the layer
            if (data->frameChangeFlags & Q3DSLayerNode::LayerContentSubTreeLightsChange)
                m_subTreesWithDirtyLights.insert(SubTreeWithDirtyLight(layer3DS, true));
            // LayerContentSubTreeChanges needs no special handling here, just set the dirty like for everything else
            m_wasDirty = true;
            markLayerForObjectDirty(layer3DS);
        }
    }
        break;
    case Q3DSGraphObject::DefaultMaterial:
    {
        Q3DSDefaultMaterial *mat3DS = static_cast<Q3DSDefaultMaterial *>(obj);
        Q3DSDefaultMaterialAttached *data = static_cast<Q3DSDefaultMaterialAttached *>(mat3DS->attached());
        if (data && (data->frameDirty & Q3DSGraphObjectAttached::DefaultMaterialDirty)) {
            for (auto it = data->perModelData.begin(), itEnd = data->perModelData.end(); it != itEnd; ++it) {
                Q3DSModelNode *model3DS = it.key();
                Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(model3DS->attached());
                it->combinedOpacity = clampOpacity(modelData->globalOpacity * (mat3DS->opacity() / 100.0f));
                updateDefaultMaterial(mat3DS, nullptr, model3DS);
                retagSubMeshes(model3DS);
            }
            m_wasDirty = true;
            markLayerForObjectDirty(mat3DS);
            if (data->frameChangeFlags & Q3DSDefaultMaterial::BlendModeChanges)
                m_pendingDefMatRebuild.insert(mat3DS);
        }
    }
        break;
    case Q3DSGraphObject::CustomMaterial:
    {
        Q3DSCustomMaterialInstance *mat3DS = static_cast<Q3DSCustomMaterialInstance *>(obj);
        Q3DSCustomMaterialAttached *data = static_cast<Q3DSCustomMaterialAttached *>(mat3DS->attached());
        if (data && (data->frameDirty & Q3DSGraphObjectAttached::CustomMaterialDirty)) {
            for (auto it = data->perModelData.begin(), itEnd = data->perModelData.end(); it != itEnd; ++it) {
                Q3DSModelNode *model3DS = it.key();
                Q3DSModelAttached *modelData = static_cast<Q3DSModelAttached *>(model3DS->attached());
                it->combinedOpacity = modelData->globalOpacity;
                updateCustomMaterial(mat3DS, nullptr, model3DS);
                retagSubMeshes(model3DS);
            }
            m_wasDirty = true;
            markLayerForObjectDirty(mat3DS);
        }
    }
        break;
    case Q3DSGraphObject::Effect:
    {
        Q3DSEffectInstance *eff3DS = static_cast<Q3DSEffectInstance *>(obj);
        Q3DSEffectAttached *data = static_cast<Q3DSEffectAttached *>(eff3DS->attached());
        if (data && (data->frameDirty & Q3DSGraphObjectAttached::EffectDirty)) {
            const bool activeFlagChanges = data->frameChangeFlags & Q3DSEffectInstance::EyeBallChanges;
            if (activeFlagChanges) // active/deactivate effects
                updateEffectStatus(eff3DS->attached<Q3DSEffectAttached>()->layer3DS);
            // send changed parameter values to the shader
            updateEffect(eff3DS);
            m_wasDirty = true;
            markLayerForObjectDirty(eff3DS);
        }
    }
        break;
    case Q3DSGraphObject::Behavior:
    {
        Q3DSBehaviorInstance *behaviorInstance = static_cast<Q3DSBehaviorInstance *>(obj);
        Q3DSBehaviorAttached *data = static_cast<Q3DSBehaviorAttached *>(behaviorInstance->attached());
        if (data && (data->frameDirty & Q3DSGraphObjectAttached::BehaviorDirty)) {
            const bool activeFlagChanges = data->frameChangeFlags & Q3DSBehaviorInstance::EyeBallChanges;
            if (activeFlagChanges) {
                if (behaviorInstance->eyeballEnabled())
                    m_engine->loadBehaviorInstance(behaviorInstance, m_presentation);
                else
                    m_engine->unloadBehaviorInstance(behaviorInstance);
            }
            // ignore layer dirty flags
        }
    }
        break;
    case Q3DSGraphObject::Image:
    {
        Q3DSImage *image3DS = static_cast<Q3DSImage *>(obj);
        Q3DSImageAttached *data = static_cast<Q3DSImageAttached *>(image3DS->attached());
        if (data && (data->frameDirty & Q3DSGraphObjectAttached::ImageDirty)) {
            image3DS->calculateTextureTransform();
            for (Q3DSDefaultMaterial *m : data->referencingDefaultMaterials) {
                Q3DSDefaultMaterialAttached *matData = m->attached<Q3DSDefaultMaterialAttached>();
                for (auto it = matData->perModelData.cbegin(), itEnd = matData->perModelData.cend(); it != itEnd; ++it)
                    updateDefaultMaterial(m, nullptr, it.key());
            }
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
        data->frameDirty = 0;
        data->frameChangeFlags = 0;
    }

    obj = obj->firstChild();
    while (obj) {
        updateSubTreeRecursive(obj);
        obj = obj->nextSibling();
    }
}

void Q3DSSceneManager::updateNodeFromChangeFlags(Q3DSNode *node, Qt3DCore::QTransform *transform, int changeFlags)
{
    bool didUpdateGlobals = false;
    if ((changeFlags & Q3DSNode::TransformChanges)
            || (changeFlags & Q3DSNode::OpacityChanges))
    {
        setNodeProperties(node, nullptr, transform, NodePropUpdateGlobalsRecursively);
        didUpdateGlobals = true;
        m_wasDirty = true;
        markLayerForObjectDirty(node);
    }

    if (changeFlags & Q3DSNode::EyeballChanges) {
        if (node->type() == Q3DSGraphObject::Light) {
            Q3DSLightAttached *lightData = static_cast<Q3DSLightAttached *>(node->attached());
            Q3DSGraphObject *rootObject = lightData->layer3DS;
            // if this is a scoped light, use the scope node instead of layer
            Q3DSLightNode *lightNode = static_cast<Q3DSLightNode*>(node);
            if (lightNode->scope() && isLightScopeValid(lightNode->scope(), lightData->layer3DS))
                rootObject = lightNode->scope();
            gatherLights(lightData->layer3DS);
            m_subTreesWithDirtyLights.insert(SubTreeWithDirtyLight(rootObject, true));
            if (!didUpdateGlobals)
                updateGlobals(node, UpdateGlobalsRecursively | UpdateGlobalsSkipTransform);
        } else if (node->type() == Q3DSGraphObject::Camera) {
            if (!didUpdateGlobals) {
                // also triggers rechoosing the layer's camera when calling back handleNodeGlobalChange()
                updateGlobals(node, UpdateGlobalsRecursively | UpdateGlobalsSkipTransform);
            }
        } else {
            // Drop whatever is queued since that was based on now-invalid
            // input. (important when entering slides, where eyball property
            // changes get processed after an initial visit of all objects)
            m_pendingObjectVisibility.remove(node);
            // This is an explicit eyeball change via the API or the console,
            // so update both visibilityTag and the QLayer tags based on
            // eyeball (and nothing else). The slide/animation system does not
            // use this path, that queues visibilityTag changes via
            // m_pendingObjectVisibility instead.
            setNodeVisibility(node, node->flags().testFlag(Q3DSNode::Active)); // will call updateGlobals() as well
        }
        m_wasDirty = true;
        markLayerForObjectDirty(node);
    }
}

void Q3DSSceneManager::setNodeVisibility(Q3DSNode *node, bool visible)
{
    Q3DSNodeAttached *data = static_cast<Q3DSNodeAttached *>(node->attached());
    Q_ASSERT(data);
    if (!data->entity)
        return;

    // Attempting to manage visibility by QLayer tags for nodes that are not
    // backed by a dedicated entity (3DS layers for instance) is an error and
    // cannot happen. The concept of QLayer tags is only suitable for "normal" nodes.
    Q_ASSERT_X(data->entity != m_rootEntity,"Q3DSSceneManager::setNodeVisibility",
               "Attempted to manage visibility of a node with non-dedicated entity by QLayer. This should not happen.");

    Q3DSLayerAttached *layerData = static_cast<Q3DSLayerAttached *>(data->layer3DS->attached());
    if (!layerData->opaqueTag || !layerData->transparentTag) // bail out for subpresentation layers
        return;

    if (!visible) {
        data->visibilityTag = Q3DSGraphObjectAttached::Hidden;
        data->entity->removeComponent(layerData->opaqueTag);
        data->entity->removeComponent(layerData->transparentTag);
    } else {
        data->visibilityTag = Q3DSGraphObjectAttached::Visible;
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
            if (!visible) {
                sm.entity->removeComponent(layerData->opaqueTag);
                sm.entity->removeComponent(layerData->transparentTag);
            } else {
                sm.entity->addComponent(tag);
            }
        }
    }

    updateGlobals(node, UpdateGlobalsRecursively | UpdateGlobalsSkipTransform);
}

void Q3DSFrameUpdater::frameAction(float dt)
{
    if (m_firstFrameAction) {
        m_firstFrameAction = false;
        m_sceneManager->m_firstFrameActionTime = m_firstFrameActionTimer.elapsed();
        qCDebug(lcPerf, "Presentation %s: Time from the end of Qt3D scene building until first frame action: %lld ms",
                qPrintable(m_sceneManager->m_profiler->presentationName()), m_sceneManager->m_firstFrameActionTime);
        // Now it's time to push all timing data to the profiler.
        m_sceneManager->m_profiler->reportTimeAfterBuildUntilFirstFrameAction(m_sceneManager->m_firstFrameActionTime);
        if (!m_sceneManager->m_flags.testFlag(Q3DSSceneManager::SubPresentation)) {
            // total parse/build time is taken from the main presentation (since
            // the time includes all subpresentations too)
            m_sceneManager->m_profiler->reportTotalParseBuildTime(m_sceneManager->m_engine->totalLoadTimeMsecs());
        }
    }
    m_sceneManager->m_profiler->reportBehaviorStats(m_sceneManager->m_engine->behaviorLoadTimeMsecs(),
                                                    m_sceneManager->m_engine->behaviorHandles().count());

    static const bool animDebug = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 3;
    if (Q_UNLIKELY(animDebug))
        qDebug().nospace() << "frame action " << m_frameCounter << ", delta=" << dt << ", applying animations and updating nodes";

    // Record new frame event.
    m_sceneManager->profiler()->reportNewFrame(dt * 1000.0f);
    // Process input. In an ideal world this would be synchronous so that we
    // get results, process events from those, and then process property
    // changes from those events in one single frame action. Our world is not
    // ideal (but we can always pretend...).
    m_sceneManager->m_inputManager->runPicks();
    // Process queued object events.
    m_sceneManager->flushEventQueue();
    // Set and notify the value changes queued by animations.
    m_sceneManager->slidePlayer()->advanceFrame();
    // Recursively check dirty flags and update inherited values, execute
    // pending visibility changes, update light cbuffers, etc.
    m_sceneManager->prepareNextFrame();
    // Update profiling statistics for this frame.
    m_sceneManager->profiler()->updateFrameStats(m_frameCounter);
    ++m_frameCounter;
}

void Q3DSSceneManager::setProfileUiVisible(bool visible, bool openLogAndConsole)
{
#if QT_CONFIG(q3ds_profileui)
    if (m_profileUi) {
        m_profileUi->setVisible(visible);
        if (visible && openLogAndConsole)
            m_profileUi->openLogAndConsole();
    }
#else
    Q_UNUSED(visible);
    Q_UNUSED(openLogAndConsole);
#endif
}

bool Q3DSSceneManager::isProfileUiVisible() const
{
#if QT_CONFIG(q3ds_profileui)
    return m_profileUi ? m_profileUi->visible() : false;
#else
    return false;
#endif
}

void Q3DSSceneManager::setProfileUiInputEventSource(QObject *obj)
{
#if QT_CONFIG(q3ds_profileui)
    if (m_profileUi)
        m_profileUi->setInputEventSource(obj);
#else
    Q_UNUSED(obj);
#endif
}

void Q3DSSceneManager::configureProfileUi(float scale)
{
#if QT_CONFIG(q3ds_profileui)
    if (m_profileUi)
        m_profileUi->configure(scale);
#else
    Q_UNUSED(scale);
#endif
}

void Q3DSSceneManager::setMatteEnabled(bool isEnabled)
{
    if (!m_viewportData.drawMatteNode)
        return;

    if (isEnabled) {
        // Attached the matteClearBuffers to the drawMatteNode placeholder
        m_viewportData.matteClearBuffers->setParent(m_viewportData.drawMatteNode);
        m_viewportData.matteRenderState->setEnabled(true);
    } else {
        m_viewportData.matteClearBuffers->setParent(m_viewportData.dummyMatteRoot);
        m_viewportData.matteRenderState->setEnabled(false);
    }
}

void Q3DSSceneManager::setMatteColor(const QColor &color)
{
    if (m_viewportData.matteClearBuffers)
        m_viewportData.matteClearBuffers->setClearColor(color);
}

void Q3DSSceneManager::addLog(const QString &msg)
{
#if QT_CONFIG(q3ds_profileui)
    QMutexLocker locker(&m_logMutex);
    if (m_inDestructor)
        return;
    // the log is maintained by the main presentation's profiler; route to that
    // even if this is a subpresentation
    m_profiler->mainPresentationProfiler()->addLog(msg);
#else
    Q_UNUSED(msg);
#endif
}

void Q3DSSceneManager::addLog(const char *fmt, ...)
{
#if QT_CONFIG(q3ds_profileui)
    va_list ap;
    va_start(ap, fmt);
    const QString msg = QString::vasprintf(fmt, ap);
    va_end(ap);
    addLog(msg);
#else
    Q_UNUSED(fmt);
#endif
}

void Q3DSSceneManager::changeSlideByName(Q3DSGraphObject *sceneOrComponent, const QString &name)
{
    if (!sceneOrComponent)
        return;

    if (sceneOrComponent->type() != Q3DSGraphObject::Scene && sceneOrComponent->type() != Q3DSGraphObject::Component) {
        qWarning("changeSlideByName: Object %s is not Scene or Component", sceneOrComponent->id().constData());
        return;
    }

    Q3DSComponentNode *component = sceneOrComponent->type() == Q3DSGraphObject::Component
            ? static_cast<Q3DSComponentNode *>(sceneOrComponent) : nullptr;
    Q3DSSlide *root = component ? component->masterSlide() : m_masterSlide;
    if (!root)
        return;

    Q3DSSlide *targetSlide = nullptr;
    Q3DSUipPresentation::forAllObjectsOfType(root, Q3DSGraphObject::Slide,
                                             [name, &targetSlide](Q3DSGraphObject *obj) {
        Q3DSSlide *slide = static_cast<Q3DSSlide *>(obj);
        if (slide->name() == name)
            targetSlide = slide;
    });

    if (targetSlide) {
        if (component) {
            if (m_currentSlide->objects().contains(component) || m_masterSlide->objects().contains(component))
                setComponentCurrentSlide(targetSlide);
            else
                component->setCurrentSlide(targetSlide);
        } else {
            setCurrentSlide(targetSlide);
        }
    } else {
        qWarning("changeSlideByName: Slide %s not found on %s", qPrintable(name), sceneOrComponent->id().constData());
    }
}

void Q3DSSceneManager::changeSlideByIndex(Q3DSGraphObject *sceneOrComponent, int index)
{
    Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
    if (sceneOrComponent->type() == Q3DSGraphObject::Component) {
        slidePlayer = static_cast<Q3DSComponentNode *>(sceneOrComponent)->masterSlide()
                ->attached<Q3DSSlideAttached>()->slidePlayer;
    }

    slidePlayer->slideDeck()->setCurrentSlide(index);
}

void Q3DSSceneManager::changeSlideByDirection(Q3DSGraphObject *sceneOrComponent, bool next, bool wrap)
{
    Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
    if (sceneOrComponent->type() == Q3DSGraphObject::Component) {
        slidePlayer = static_cast<Q3DSComponentNode *>(sceneOrComponent)->masterSlide()
                ->attached<Q3DSSlideAttached>()->slidePlayer;
    }

    if (next)
        slidePlayer->slideDeck()->nextSlide(wrap);
    else
        slidePlayer->slideDeck()->previousSlide(wrap);

    slidePlayer->reload();
}

Q3DSSlide *Q3DSSceneManager::currentSlideForSceneOrComponent(Q3DSGraphObject *sceneOrComponent) const
{
    Q3DSSlide *slide = currentSlide();
    if (sceneOrComponent->type() == Q3DSGraphObject::Component)
        slide = static_cast<Q3DSComponentNode *>(sceneOrComponent)->currentSlide();

    return slide;
}

void Q3DSSceneManager::goToTime(Q3DSGraphObject *sceneOrComponent, float milliseconds)
{
    Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
    if (sceneOrComponent->type() == Q3DSGraphObject::Component) {
        slidePlayer = static_cast<Q3DSComponentNode *>(sceneOrComponent)->masterSlide()
                ->attached<Q3DSSlideAttached>()->slidePlayer;
    }

    slidePlayer->seek(milliseconds);
}

void Q3DSSceneManager::goToTimeAction(Q3DSGraphObject *sceneOrComponent, float milliseconds, bool pause)
{
    Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
    if (sceneOrComponent->type() == Q3DSGraphObject::Component) {
        slidePlayer = static_cast<Q3DSComponentNode *>(sceneOrComponent)->masterSlide()
                ->attached<Q3DSSlideAttached>()->slidePlayer;
    }

    if (pause)
        slidePlayer->pause();

    slidePlayer->seek(milliseconds);

    if (!pause)
        slidePlayer->play();
}

void Q3DSSceneManager::setDataInputValue(const QString &dataInputName, const QVariant &value)
{
    auto dataInputMap = m_presentation->dataInputMap();
    auto it = dataInputMap->constFind(dataInputName);
    auto dataInputEntries = m_presentation->dataInputEntries();
    Q_ASSERT(dataInputEntries);
    const Q3DSDataInputEntry &meta((*dataInputEntries)[dataInputName]);

    while (it != dataInputMap->constEnd() && it.key() == dataInputName) {
        Q3DSGraphObject *obj = it.value();
        Q3DS::PropertyType type = Q3DS::String;
        if (dataInputEntries) {
            switch (meta.type) {
            case Q3DSDataInputEntry::TypeString:
                type = Q3DS::String;
                break;
            case Q3DSDataInputEntry::TypeRangedNumber:
                type = Q3DS::Float;
                break;
            case Q3DSDataInputEntry::TypeVec2:
                type = Q3DS::Float2;
                break;
            case Q3DSDataInputEntry::TypeVec3:
                type = Q3DS::Vector;
                break;
            // for Variant datainput the target type is unknown
            // but values are passed on from this function anyway
            // as QVariants.
            case Q3DSDataInputEntry::TypeVariant:
                type = Q3DS::Unknown;
                break;
            default:
                break;
            }
        }

        Q3DSPropertyChangeList changeList;
        // Remember that we have QMultiHash everywhere since one data input
        // entry can control multiple properties, on the same object even.
        for (const QString &propName : obj->dataInputControlledProperties()->values(dataInputName)) {
            qCDebug(lcUipProp, "Data input: object %s property %s value %s",
                    obj->id().constData(), qPrintable(propName), qPrintable(value.toString()));
            if (propName.startsWith(QLatin1Char('@'))) {
                if (propName == QStringLiteral("@slide")) {
                    changeSlideByName(obj, value.toString());
                } else if (propName == QStringLiteral("@timeline")) {
                    if (obj->type() == Q3DSGraphObject::Scene || obj->type() == Q3DSGraphObject::Component) {
                        // Normalize the datainput min-max range between scene or component
                        // timeline length and map the incoming value on it (just because 3DS1
                        // does it). If min-max is not specified, interpret value directly as
                        // timeline point in milliseconds
                        float seekTimeMs = 0.0f;
                        if (meta.hasMinMax()) {
                            Q_ASSERT(!qFuzzyIsNull(meta.maxValue));
                            const float normalized = qBound(0.0f, (value.toFloat() / (meta.maxValue - meta.minValue)), 1.0f);
                            Q3DSSlide *slide = (obj->type() == Q3DSGraphObject::Component) ? static_cast<Q3DSComponentNode *>(obj)->currentSlide()
                                                                                           : m_currentSlide;
                            qint32 startTime = 0;
                            qint32 endTime = 0;
                            Q3DSSlideUtils::getStartAndEndTime(slide, &startTime, &endTime);
                            seekTimeMs = normalized * (endTime - startTime);
                        } else {
                            seekTimeMs = value.toFloat();
                        }
                        goToTime(obj, seekTimeMs);
                    } else {
                        qWarning("Object %s with timeline data input is not Scene or Component", obj->id().constData());
                    }
                }
            } else {
                if (type == Q3DS::Float && meta.hasMinMax()) {
                    // Keep value between min&max
                    float val = value.toFloat();
                    val = std::min(meta.maxValue, std::max(meta.minValue, val));
                    changeList.append(Q3DSPropertyChange::fromVariant(propName, val));
                } else {
                    changeList.append(Q3DSPropertyChange::fromVariant(propName, value));
                }
            }
        }

        if (!changeList.isEmpty()) {
            obj->applyPropertyChanges(changeList);
            obj->notifyPropertyChanges(changeList);
        }
        ++it;
    }
}

void Q3DSSceneManager::handleEvent(const Q3DSGraphObject::Event &e)
{
    Q3DSSlide *slide = currentSlide();
    Q3DSComponentNode *component = e.target->attached() ? e.target->attached()->component : nullptr;
    if (component)
        slide = component->currentSlide();

    const auto runSlideAction = [this, &e](Q3DSSlide *slide) {
        if (slide) {
            for (const Q3DSAction &action : slide->actions()) {
                if (!action.eyeball || action.triggerObject != e.target)
                    continue;
                if (action.event == e.event)
                    runAction(action);
            }
        }
    };

    // Run on both the current slide and its parent (the master slide).
    runSlideAction(slide);
    runSlideAction(static_cast<Q3DSSlide *>(slide->parent()));

    const bool isSlideEnter = e.event == Q3DSGraphObjectEvents::slideEnterEvent();
    const bool isSlideExit = e.event == Q3DSGraphObjectEvents::slideExitEvent();
    if (isSlideEnter || isSlideExit) {
        slide = e.args.at(0).value<Q3DSSlide *>();
        const int index = e.args.at(1).toInt();
        if (slide) {
            Q3DSGraphObject *sceneOrComponent = e.target;
            // the slide name is provided as-is since the editor does unique
            // names for them by default
            QString name = slide->name();
            if (name.isEmpty())
                name = QString::fromUtf8(slide->id());
            if (isSlideEnter)
                emit m_engine->slideEntered(sceneOrComponent, index, name);
            else
                emit m_engine->slideExited(sceneOrComponent, index, name);
        }

        // Slide changes that do not cause any property changes and start no
        // animations would not change the involved layers' dirty flag and so
        // could potentially lead to bad rendering. Uncache all layers to be
        // safe. Caching will kick in again after a few frames anyway.
        m_layerUncachePending = true;
    }
}

void Q3DSSceneManager::queueEvent(const Q3DSGraphObject::Event &e)
{
    m_eventQueue.append(e);
}

void Q3DSSceneManager::flushEventQueue()
{
    const QVector<Q3DSGraphObject::Event> q = std::move(m_eventQueue);
    for (const Q3DSGraphObject::Event &e : q) {
        if (e.target && !e.event.isEmpty())
            e.target->processEvent(e);
    }
}

void Q3DSSceneManager::runAction(const Q3DSAction &action)
{
    switch (action.handler) {
    case Q3DSAction::SetProperty:
    {
        Q3DSAction::HandlerArgument propName = action.handlerWithArgType(Q3DSAction::HandlerArgument::Property);
        Q3DSAction::HandlerArgument propValue = action.handlerWithArgType(Q3DSAction::HandlerArgument::Dependent);
        if (propName.isValid() && propValue.isValid()) {
            Q3DSGraphObject *target = action.targetObject;
            const QString name = propName.value;
            const QString value = propValue.value;
            if (target && !name.isEmpty()) {
                Q3DSPropertyChangeList cl { Q3DSPropertyChange(name, value) };
                target->applyPropertyChanges(cl);
                target->notifyPropertyChanges(cl);
            } else {
                qWarning("SetProperty action %s has invalid target", action.id.constData());
            }
        } else {
            qWarning("SetProperty action %s has invalid arguments", action.id.constData());
        }
    }
        break;
    case Q3DSAction::FireEvent:
    {
        Q3DSAction::HandlerArgument event = action.handlerWithArgType(Q3DSAction::HandlerArgument::Event);
        if (event.isValid()) {
            Q3DSGraphObject *target = action.targetObject;
            if (target && !event.value.isEmpty())
                queueEvent(Q3DSGraphObject::Event(target, event.value));
        }
    }
        break;
    case Q3DSAction::EmitSignal:
        // Here we have a target object and a string (a "signal" name). Just
        // expose this in the public API somehow, no further actions are needed
        // on the runtime's side.
    {
        Q3DSAction::HandlerArgument signalName = action.handlerWithArgType(Q3DSAction::HandlerArgument::Signal);
        if (signalName.isValid()) {
            Q3DSGraphObject *target = action.targetObject;
            if (target && !signalName.value.isEmpty())
                emit m_engine->customSignalEmitted(target, signalName.value);
        }
    }
        break;
    case Q3DSAction::GoToSlide:
    {
        Q3DSAction::HandlerArgument slideName = action.handlerWithArgType(Q3DSAction::HandlerArgument::Slide);
        if (slideName.isValid())
            changeSlideByName(action.targetObject, slideName.value);
        else
            qWarning("GoToSlide action %s has no valid slide name argument", action.id.constData());
    }
        break;
    case Q3DSAction::NextSlide:
        if (action.targetObject) {
            Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
            if (action.targetObject->type() == Q3DSGraphObject::Component) {
                slidePlayer = static_cast<Q3DSComponentNode *>(action.targetObject)->masterSlide()
                        ->attached<Q3DSSlideAttached>()->slidePlayer;
            }
            slidePlayer->nextSlide();
        }
        break;
    case Q3DSAction::PreviousSlide:
        if (action.targetObject) {
            Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
            if (action.targetObject->type() == Q3DSGraphObject::Component) {
                slidePlayer = static_cast<Q3DSComponentNode *>(action.targetObject)->masterSlide()
                        ->attached<Q3DSSlideAttached>()->slidePlayer;
            }
            slidePlayer->previousSlide();
        }
        break;
    case Q3DSAction::PrecedingSlide:
        if (action.targetObject) {
            Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
            if (action.targetObject->type() == Q3DSGraphObject::Component) {
                slidePlayer = static_cast<Q3DSComponentNode *>(action.targetObject)->masterSlide()
                        ->attached<Q3DSSlideAttached>()->slidePlayer;
            }
            slidePlayer->precedingSlide();
        }
        break;
    case Q3DSAction::Play:
        if (action.targetObject) {
            Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
            if (action.targetObject->type() == Q3DSGraphObject::Component) {
                slidePlayer = static_cast<Q3DSComponentNode *>(action.targetObject)->masterSlide()
                        ->attached<Q3DSSlideAttached>()->slidePlayer;
            }
            slidePlayer->play();
        }
        break;
    case Q3DSAction::Pause:
        if (action.targetObject) {
            Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
            if (action.targetObject->type() == Q3DSGraphObject::Component) {
                slidePlayer = static_cast<Q3DSComponentNode *>(action.targetObject)->masterSlide()
                        ->attached<Q3DSSlideAttached>()->slidePlayer;
            }
            slidePlayer->pause();
        }
        break;
    case Q3DSAction::GoToTime:
    {
        Q3DSAction::HandlerArgument newTime = action.handlerWithName(QLatin1String("Time"));
        Q3DSAction::HandlerArgument shouldPause = action.handlerWithName(QLatin1String("Pause"));
        if (action.targetObject && newTime.isValid()) {
            const float seekTimeMs = newTime.value.toFloat(); // input value is assumed to be in milliseconds
            bool pause = false;
            if (shouldPause.isValid())
                Q3DS::convertToBool(&shouldPause.value, &pause);
            goToTimeAction(action.targetObject, seekTimeMs, pause);
        }
    }
        break;
    case Q3DSAction::BehaviorHandler:
    {
        if (action.targetObject && action.targetObject->type() == Q3DSGraphObject::Behavior) {
            Q3DSBehaviorInstance *bi = static_cast<Q3DSBehaviorInstance *>(action.targetObject);
            auto handles = m_engine->behaviorHandles();
            Q3DSBehaviorObject *qmlObj = nullptr;
            if (handles.contains(bi))
                qmlObj = handles[bi].object;
            if (qmlObj)
                qmlObj->call(action.behaviorHandler);
            // else the behavior instance is not loaded (QML object is not
            // active) - this is fine and not an error when the behavior instance
            // has active (eyeball) == false
        } else {
            qWarning("BehaviorHandler: Invalid target object");
        }
    }
        break;
    default:
        Q_UNREACHABLE();
        break;
    }
}

void Q3DSSceneManager::handleNodeGlobalChange(Q3DSNode *node)
{
    // called when updateGlobals changes a global* value, giving a chance to act

    Q3DSGraphObjectAttached::FrameDirtyFlags dirty = node->attached()->frameDirty;

    if (dirty.testFlag(Q3DSGraphObjectAttached::GlobalVisibilityDirty)) {
        if (node->type() == Q3DSGraphObject::Camera) {
            Q3DSCameraAttached *data = node->attached<Q3DSCameraAttached>();
            if (setActiveLayerCamera(findFirstCamera(data->layer3DS), data->layer3DS)) {
                // (orthographic) shadow maps, ssao texture, effects all rely on camera properties like clip range
                updateShadowMapStatus(data->layer3DS);
                updateSsaoStatus(data->layer3DS);
                for (Q3DSEffectInstance *eff3DS : data->layer3DS->attached<Q3DSLayerAttached>()->effectData.effects)
                    updateEffect(eff3DS);
            }
        } else if (node->type() != Q3DSGraphObject::Layer) {
            const bool isVisible = node->attached<Q3DSNodeAttached>()->globalEffectiveVisibility;
            m_pendingObjectVisibility.insert(node, isVisible);
        }
    }
}

static Q3DSLayerNode *findLayerForObjectInScene(Q3DSGraphObject *obj)
{
    // This does not rely on attached->layer3DS since the attached object is
    // simply not built yet when adding an object to the scene.

    Q3DSGraphObject *p = obj->parent();
    while (p && p->type() != Q3DSGraphObject::Layer)
        p = p->parent();

    return static_cast<Q3DSLayerNode *>(p);
}

void Q3DSSceneManager::handleSceneChange(Q3DSScene *, Q3DSGraphObject::DirtyFlag change, Q3DSGraphObject *obj)
{
    if (change == Q3DSGraphObject::DirtyNodeAdded) {
        if (obj->isNode()) {
            if (obj->type() != Q3DSGraphObject::Layer && obj->type() != Q3DSGraphObject::Camera) {
                Q_ASSERT(obj->parent() && obj->parent()->attached());
                Q3DSLayerNode *layer3DS = findLayerForObjectInScene(obj);
                if (layer3DS)
                    addLayerContent(obj, obj->parent(), layer3DS);
            } else if (obj->type() == Q3DSGraphObject::Layer) {
                addLayer(static_cast<Q3DSLayerNode *>(obj));
            }
        }
    } else if (change == Q3DSGraphObject::DirtyNodeRemoved) {
        if (obj->isNode()) {
            Q_ASSERT(obj->parent());
            const bool isLayer = obj->type() == Q3DSGraphObject::Layer;
            if (isLayer)
                qCDebug(lcScene) << "Dyn.removing layer" << obj->id();
            else
                qCDebug(lcScene) << "Dyn.removing" << obj->attached()->entity;
            Q3DSUipPresentation::forAllObjectsInSubTree(obj, [this](Q3DSGraphObject *objOrChild) {
                Q3DSGraphObjectAttached *data = objOrChild->attached();
                if (!data)
                    return;
                Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
                if (data->component)
                    slidePlayer = data->component->masterSlide()->attached<Q3DSSlideAttached>()->slidePlayer;
                slidePlayer->objectAboutToBeRemovedFromScene(objOrChild);
                if (data->propertyChangeObserverIndex >= 0) {
                    objOrChild->removePropertyChangeObserver(data->propertyChangeObserverIndex);
                    data->propertyChangeObserverIndex = -1;
                }
                if (data->eventObserverIndex >= 0) {
                    objOrChild->removeEventHandler(QString(), data->eventObserverIndex);
                    data->eventObserverIndex = -1;
                }
                m_pendingObjectVisibility.remove(objOrChild);
            });
            if (!isLayer) {
                delete obj->attached()->entity;
                Q3DSLayerNode *layer3DS = findLayerForObjectInScene(obj);
                if (layer3DS)
                    removeLayerContent(obj, layer3DS);
            } else {
                rebuildCompositorLayerChain();
            }
        }
        // bye bye attached; it will get recreated in case obj gets added back later on
        if (obj->attached()) {
            delete obj->attached();
            obj->setAttached(nullptr);
        }
    }
}

static void markLayerAsContentChanged(Q3DSLayerNode *layer3DS, int change = Q3DSLayerNode::LayerContentSubTreeChanges)
{
    Q3DSLayerAttached *data = layer3DS->attached<Q3DSLayerAttached>();
    data->frameDirty |= Q3DSGraphObjectAttached::LayerDirty;
    data->frameChangeFlags |= change;
}

void Q3DSSceneManager::addLayerContent(Q3DSGraphObject *obj, Q3DSGraphObject *parent, Q3DSLayerNode *layer3DS)
{
    // obj can have children, process the entire subtree recursively

    // When parenting to a node like model, text, or group, use the corresponding entity as the parent.
    Qt3DCore::QEntity *parentEntity = parent->attached()->entity;
    // However, in some cases 'entity' is merely m_rootEntity. Use a more appropriate one.
    if (parent->type() == Q3DSGraphObject::Layer)
        parentEntity = parent->attached<Q3DSLayerAttached>()->layerSceneRootEntity;

    // First make sure all properties are resolved; f.ex. a model with
    // sourcepath changed before attaching the node to a scene will not
    // generate a MeshChanges prop.change, and so the actual mesh will not
    // be up-to-date without a resolve. Avoid this.
    Q3DSUipPresentation::forAllObjectsInSubTree(obj, [this](Q3DSGraphObject *objOrChild) {
        objOrChild->resolveReferences(*m_presentation);

        // Image objects must be prepared in advance.
        if (objOrChild->type() == Q3DSGraphObject::Image)
            initImage(static_cast<Q3DSImage *>(objOrChild));
    });

    // phase 1
    buildLayerScene(obj, layer3DS, parentEntity);
    qCDebug(lcScene) << "Dyn.added" << obj->attached()->entity;

    // phase 2
    bool needsEffectUpdate = false;
    Q3DSUipPresentation::forAllObjectsInSubTree(obj, [this, layer3DS, &needsEffectUpdate](Q3DSGraphObject *objOrChild) {
        switch (objOrChild->type()) {
        case Q3DSGraphObject::Model:
            buildModelMaterial(static_cast<Q3DSModelNode *>(objOrChild));
            break;
        case Q3DSGraphObject::Light:
            markLayerAsContentChanged(layer3DS, Q3DSLayerNode::LayerContentSubTreeLightsChange);
            break;
        case Q3DSGraphObject::Effect:
            needsEffectUpdate = true;
            break;

        case Q3DSGraphObject::Scene:
            Q_FALLTHROUGH();
        case Q3DSGraphObject::Layer:
            qWarning("addLayerContent: Invalid child object %s", objOrChild->id().constData());
            return;

        default:
            break;
        }

        Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
        if (objOrChild->attached() && objOrChild->attached()->component)
            slidePlayer = objOrChild->attached()->component->masterSlide()->attached<Q3DSSlideAttached>()->slidePlayer;

        slidePlayer->objectAboutToBeAddedToScene(objOrChild);
    });
    if (needsEffectUpdate)
        updateEffectStatus(layer3DS);

    // ensure layer gets dirtied
    markLayerAsContentChanged(layer3DS);
}

void Q3DSSceneManager::removeLayerContent(Q3DSGraphObject *obj, Q3DSLayerNode *layer3DS)
{
    markLayerAsContentChanged(layer3DS);
    if (obj->type() == Q3DSGraphObject::Light)
        markLayerAsContentChanged(layer3DS, Q3DSLayerNode::LayerContentSubTreeLightsChange);
}

void Q3DSSceneManager::addLayer(Q3DSLayerNode *layer3DS)
{
    // Simply build the layer like buildScene() would. The order in the
    // framegraph does not matter here (unlike in the composition step), so we
    // can just append the new subtree root to m_LayerContainerFg.

    if (layer3DS->sourcePath().isEmpty())
        buildLayer(layer3DS, m_layerContainerFg, m_outputPixelSize);
    else
        buildSubPresentationLayer(layer3DS, m_outputPixelSize);

    Q3DSUipPresentation::forAllObjectsInSubTree(layer3DS, [this, layer3DS](Q3DSGraphObject *objOrChild) {
        Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
        if (objOrChild->attached() && objOrChild->attached()->component)
            slidePlayer = objOrChild->attached()->component->masterSlide()->attached<Q3DSSlideAttached>()->slidePlayer;

        slidePlayer->objectAboutToBeAddedToScene(objOrChild);
    });

    rebuildCompositorLayerChain();
}

void Q3DSSceneManager::handleSlideGraphChange(Q3DSSlide *master, Q3DSGraphObject::DirtyFlag change, Q3DSSlide *slide)
{
    // called when a slide (a child of the master slide) is added or removed

    Q_UNUSED(master);

    if (change == Q3DSGraphObject::DirtyNodeAdded) {
        // ###
    } else if (change == Q3DSGraphObject::DirtyNodeRemoved) {
        Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
        if (data->slideObjectChangeObserverIndex >= 0) {
            slide->removeSlideObjectChangeObserver(data->slideObjectChangeObserverIndex);
            data->slideObjectChangeObserverIndex = -1;
        }
        delete data;
        slide->setAttached(nullptr);
    }
}

void Q3DSSceneManager::handleSlideObjectChange(Q3DSSlide *slide, const Q3DSSlide::SlideObjectChange &change)
{
    auto findSlidePlayer = [this](Q3DSGraphObject *obj) {
        Q3DSSlidePlayer *slidePlayer = m_slidePlayer;
        if (obj->attached() && obj->attached()->component)
            slidePlayer = obj->attached()->component->masterSlide()->attached<Q3DSSlideAttached>()->slidePlayer;
        return slidePlayer;
    };

    switch (change.type) {
    case Q3DSSlide::SlideObjectAdded: // addObject() was called
    {
        findSlidePlayer(change.obj)->objectAddedToSlide(change.obj, slide);
        // ensure the layer gets dirtied because if the object becomes visible
        // ( = is on the current or master slide), then layer caching needs to
        // recognize that the layer needs to re-render
        Q3DSLayerNode *layer3DS = findLayerForObjectInScene(change.obj);
        if (layer3DS)
            markLayerAsContentChanged(layer3DS);
    }
        break;
    case Q3DSSlide::SlideObjectRemoved: // removeObject() was called
    {
        findSlidePlayer(change.obj)->objectRemovedFromSlide(change.obj, slide);
        Q3DSLayerNode *layer3DS = findLayerForObjectInScene(change.obj);
        if (layer3DS)
            markLayerAsContentChanged(layer3DS);
    }
        break;

    case Q3DSSlide::SlidePropertyChangesAdded: // addPropertyChanges() was called
        break;
    case Q3DSSlide::SlidePropertyChangesRemoved: // removePropertyChanges() was called
        break;

    case Q3DSSlide::SlideAnimationAdded: // addAnimation() was called
        break;
    case Q3DSSlide::SlideAnimationRemoved: // removeAnimation() was called
        break;

    case Q3DSSlide::SlideActionAdded: // addAction() was called
        break;
    case Q3DSSlide::SlideActionRemoved: // removeAction() was called
        break;

    default:
        break;
    }
}

QT_END_NAMESPACE
