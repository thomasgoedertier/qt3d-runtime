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

#ifndef Q3DSSCENEBUILDER_H
#define Q3DSSCENEBUILDER_H

#include <Qt3DStudioRuntime2/q3dspresentation.h>
#include <Qt3DStudioRuntime2/q3dsgraphicslimits.h>
#include <QWindow>

QT_BEGIN_NAMESPACE

class Q3DSAnimationManager;
class Q3DSFrameUpdater;
class Q3DSTextRenderer;
class Q3DSDefaultMaterialGenerator;
class Q3DSTextMaterialGenerator;

namespace Qt3DCore {
class QEntity;
class QTransform;
}

namespace Qt3DRender {
class QFrameGraphNode;
class QRenderSettings;
class QCamera;
class QCameraSelector;
class QAbstractTexture;
class QClearBuffers;
class QLayer;
class QParameter;
class QRenderPass;
class QShaderProgram;
class QBuffer;
class QTexture2D;
class QTextureImage;
class QPaintedTextureImage;
class QLayerFilter;
class QRenderTargetSelector;
class QTechnique;
}

namespace Qt3DInput {
class QInputSettings;
}

namespace Qt3DExtras {
class QPlaneMesh;
}

// The Qt 3D scene, once built, still has to react to property changes.
// Therefore some extra bookkeeping is needed, so that the Qt 3D objects to
// update are accessible in a sane manner. This is done via the *Attached
// objects, with the pointers stored in the Q3DSNodes themselves.

struct Q3DSLightSource
{
    Qt3DRender::QParameter *positionParam = nullptr;
    Qt3DRender::QParameter *directionParam = nullptr;
    Qt3DRender::QParameter *upParam = nullptr;
    Qt3DRender::QParameter *rightParam = nullptr;
    Qt3DRender::QParameter *diffuseParam = nullptr;
    Qt3DRender::QParameter *ambientParam = nullptr;
    Qt3DRender::QParameter *specularParam = nullptr;
    //Qt3DRender::QParameter *spotExponentParam = nullptr;
    //Qt3DRender::QParameter *spotCutoffParam = nullptr;
    Qt3DRender::QParameter *constantAttenuationParam = nullptr;
    Qt3DRender::QParameter *linearAttenuationParam = nullptr;
    Qt3DRender::QParameter *quadraticAttenuationParam = nullptr;
    //Qt3DRender::QParameter *rangeParam = nullptr;
    Qt3DRender::QParameter *widthParam = nullptr;
    Qt3DRender::QParameter *heightParam = nullptr;
    Qt3DRender::QParameter *shadowControlsParam = nullptr;
    Qt3DRender::QParameter *shadowViewParam = nullptr;
    Qt3DRender::QParameter *shadowIdxParam = nullptr;
};

Q_DECLARE_TYPEINFO(Q3DSLightSource, Q_MOVABLE_TYPE);

// these are our current shader limits
#define Q3DS_MAX_NUM_LIGHTS 16
#define Q3DS_MAX_NUM_SHADOWS 8

// note this struct must exactly match the memory layout of the
// struct sampleLight.glsllib and sampleArea.glsllib. If you make changes here you need
// to adjust the code in sampleLight.glsllib and sampleArea.glsllib as well
struct Q3DSLightSourceData
{
    QVector4D m_position;
    QVector4D m_direction; // Specifies the light direction in world coordinates.
    QVector4D m_up;
    QVector4D m_right;
    QVector4D m_diffuse;
    QVector4D m_ambient;
    QVector4D m_specular;
    float m_spotExponent; // Specifies the intensity distribution of the light.
    float m_spotCutoff; // Specifies the maximum spread angle of the light.
    float m_constantAttenuation; // Specifies the constant light attenuation factor.
    float m_linearAttenuation; // Specifies the linear light attenuation factor.
    float m_quadraticAttenuation; // Specifies the quadratic light attenuation factor.
    float m_range; // Specifies the maximum distance of the light influence
    float m_width; // Specifies the width of the area light surface.
    float m_height; // Specifies the height of the area light surface;
    QVector4D m_shadowControls;
    QMatrix4x4 m_shadowView;
    qint32 m_shadowIdx;
    float m_padding1[2];
};

Q_DECLARE_TYPEINFO(Q3DSLightSourceData, Q_MOVABLE_TYPE);

// must match cbAoShadowParam in Q3DSShaderManager::getSsaoTextureShader()
struct Q3DSAmbientOcclusionData
{
    QVector4D aoProperties;
    QVector4D aoProperties2;
    QVector4D shadowProperties;
    QVector4D aoScreenConst;
    QVector4D uvToEyeConst;
};

Q_DECLARE_TYPEINFO(Q3DSAmbientOcclusionData, Q_MOVABLE_TYPE);

class Q3DSLayerAttached : public Q3DSNodeAttached
{
public:
    Qt3DCore::QEntity *compositorEntity = nullptr;
    Q3DSCameraNode *cam3DS = nullptr;
    Qt3DRender::QCameraSelector *cameraSelector = nullptr;
    Qt3DRender::QClearBuffers *clearBuffers = nullptr;
    struct SizeManagedTexture {
        typedef std::function<void(Q3DSLayerNode *)> SizeChangeCallback;
        SizeManagedTexture() { }
        SizeManagedTexture(Qt3DRender::QAbstractTexture *t, SizeChangeCallback c = nullptr)
            : texture(t), sizeChangeCallback(c)
        { }
        Qt3DRender::QAbstractTexture *texture = nullptr;
        SizeChangeCallback sizeChangeCallback = nullptr;
    };
    QVector<SizeManagedTexture> sizeManagedTextures;
    std::function<void()> updateCompositorCalculations = nullptr;
    QSize layerSize;
    QSize parentSize;
    QPointF layerPos;
    QVector<Q3DSLightSource> allLights;
    QVector<Q3DSLightSource> nonAreaLights;
    QVector<Q3DSLightSource> areaLights;
    QVector<Q3DSLightNode *> lightNodes;
    Qt3DRender::QLayer *opaqueTag = nullptr;
    Qt3DRender::QLayer *transparentTag = nullptr;
    Qt3DRender::QParameter *cameraPropertiesParam = nullptr;
    Qt3DRender::QParameter *allLightsParam = nullptr; // for default material
    Qt3DRender::QParameter *nonAreaLightsParam = nullptr; // split, for custom materials
    Qt3DRender::QParameter *areaLightsParam = nullptr; // split, for custom materials
    Qt3DRender::QBuffer *allLightsConstantBuffer = nullptr;
    Qt3DRender::QBuffer *nonAreaLightsConstantBuffer = nullptr;
    Qt3DRender::QBuffer *areaLightsConstantBuffer = nullptr;
    Qt3DRender::QParameter *lightAmbientTotalParamenter = nullptr;

    struct DepthTextureData {
        bool enabled = false;
        Qt3DRender::QRenderTargetSelector *rtSelector = nullptr;
        Qt3DRender::QTexture2D *depthTexture = nullptr;
        Qt3DRender::QClearBuffers *clearBuffers = nullptr;
        Qt3DRender::QLayerFilter *layerFilterOpaque = nullptr;
        Qt3DRender::QLayerFilter *layerFilterTransparent = nullptr;
    } depthTextureData;

    struct SsaoTextureData {
        bool enabled = false;
        Qt3DRender::QRenderTargetSelector *rtSelector = nullptr;
        Qt3DRender::QTexture2D *ssaoTexture = nullptr;
        Qt3DRender::QClearBuffers *clearBuffers = nullptr;
        Qt3DRender::QLayerFilter *layerFilter = nullptr;
        Qt3DRender::QParameter *depthSampler = nullptr;
        Qt3DRender::QBuffer *aoDataBuf = nullptr;
        Qt3DRender::QParameter *aoDataBufParam = nullptr;
        Qt3DRender::QParameter *ssaoTextureSampler = nullptr;
    } ssaoTextureData;

    struct PerLightShadowMapData {
        Q3DSLightNode *lightNode = nullptr;
        Qt3DRender::QAbstractTexture *shadowMapTextureTemp = nullptr;
        Qt3DRender::QAbstractTexture *shadowMapTexture = nullptr;
        Qt3DRender::QParameter *cameraPositionParam = nullptr;
        Qt3DRender::QParameter *shadowSampler = nullptr;
        Qt3DRender::QParameter *shadowMatrixParam = nullptr;
        Qt3DRender::QParameter *shadowControlParam = nullptr;
    };
    struct ShadowMapData {
        Qt3DRender::QFrameGraphNode *shadowRoot = nullptr;
        QVector<PerLightShadowMapData> shadowCasters;
        Qt3DRender::QTexture2D *shadowDS = nullptr;
    } shadowMapData;
};

Q_DECLARE_TYPEINFO(Q3DSLayerAttached::SizeManagedTexture, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSLayerAttached::PerLightShadowMapData, Q_MOVABLE_TYPE);

class Q3DSCameraAttached : public Q3DSNodeAttached
{
public:
    Qt3DRender::QCamera *camera = nullptr;
};

class Q3DSGroupAttached : public Q3DSNodeAttached
{
public:
};

class Q3DSComponentAttached : public Q3DSNodeAttached
{
public:
};

class Q3DSTextAttached : public Q3DSNodeAttached
{
public:
    Qt3DRender::QParameter *opacityParam = nullptr;
    Qt3DRender::QParameter *colorParam = nullptr;
    Qt3DRender::QParameter *textureParam = nullptr;
    Qt3DRender::QTexture2D *texture = nullptr;
    Qt3DRender::QPaintedTextureImage *textureImage = nullptr;
};

class Q3DSLightAttached : public Q3DSNodeAttached
{
public:
    Q3DSLightSource lightSource;
};

class Q3DSModelAttached : public Q3DSNodeAttached
{
public:
    struct SubMesh {
        Qt3DRender::QMaterial *materialComponent = nullptr;
        Q3DSGraphObject *material = nullptr; // Default, Custom, Referenced
        Q3DSGraphObject *resolvedMaterial = nullptr; // Default, Custom normally, but can still be Referenced for invalid refs
        Qt3DCore::QEntity *entity = nullptr;
        bool hasTransparency = false;
    };
    QVector<SubMesh> subMeshes;
};

Q_DECLARE_TYPEINFO(Q3DSModelAttached::SubMesh, Q_MOVABLE_TYPE);

struct Q3DSTextureParameters
{
    Qt3DRender::QParameter *sampler = nullptr;
    Qt3DRender::QParameter *offsets = nullptr;
    Qt3DRender::QParameter *rotations = nullptr;

    QVector<Qt3DRender::QParameter *> parameters() const { return { sampler, offsets, rotations }; }

    Qt3DRender::QTexture2D *texture = nullptr;
    Qt3DRender::QTextureImage *textureImage = nullptr;
};

class Q3DSDefaultMaterialAttached : public Q3DSGraphObjectAttached
{
public:
    Qt3DCore::QEntity *entity = nullptr; // model3DS->attached->entity, req'd separately here for templated animation stuff
    Q3DSModelNode *model3DS = nullptr;
    float opacity = 1.0f;
    Qt3DRender::QParameter *diffuseParam = nullptr;
    Qt3DRender::QParameter *materialDiffuseParam = nullptr;
    Qt3DRender::QParameter *materialPropertiesParam = nullptr;
    Qt3DRender::QParameter *specularParam = nullptr;
    Qt3DRender::QParameter *fresnelPowerParam = nullptr;
    Qt3DRender::QParameter *bumpAmountParam = nullptr;
    Qt3DRender::QParameter *diffuseLightWrapParam = nullptr;
    Qt3DRender::QParameter *translucentFalloffParam = nullptr;
    Qt3DRender::QParameter *displaceAmountParam = nullptr;
    // Textures
    Q3DSTextureParameters diffuseMapParams;
    Q3DSTextureParameters diffuseMap2Params;
    Q3DSTextureParameters diffuseMap3Params;
    Q3DSTextureParameters specularReflectionParams;
    Q3DSTextureParameters specularMapParams;
    Q3DSTextureParameters bumpMapParams;
    Q3DSTextureParameters normalMapParams;
    Q3DSTextureParameters displacementMapParams;
    Q3DSTextureParameters opacityMapParams;
    Q3DSTextureParameters emissiveMapParams;
    Q3DSTextureParameters emissiveMap2Params;
    Q3DSTextureParameters translucencyMapParams;
};

class Q3DSSlideAttached : public Q3DSGraphObjectAttached
{
public:
    QVector<Qt3DAnimation::QClipAnimator *> animators;
};

class Q3DSImageAttached : public Q3DSGraphObjectAttached
{
public:
    Qt3DCore::QEntity *entity = nullptr; // dummy, for animation
    QSet<Q3DSDefaultMaterial *> referencingMaterials;
};

class Q3DSV_EXPORT Q3DSSceneManager
{
public:
    enum SceneBuilderFlag {
        LayerMSAA4x = 0x01
    };
    Q_DECLARE_FLAGS(SceneBuilderFlags, SceneBuilderFlag)

    struct Scene {
        Qt3DCore::QEntity *rootEntity = nullptr;
        Qt3DRender::QRenderSettings *renderSettings = nullptr;
        Qt3DInput::QInputSettings *inputSettings = nullptr;
    };

    Q3DSSceneManager(const Q3DSGraphicsLimits &limits);
    ~Q3DSSceneManager();

    void prepareSceneChange();
    Scene buildScene(Q3DSPresentation *presentation, QWindow *window, SceneBuilderFlags flags);
    void updateSizes(QWindow *window);

    Q3DSSlide *currentSlide() const { return m_currentSlide; }
    Q3DSSlide *masterSlide() const { return m_masterSlide; }
    void setCurrentSlide(Q3DSSlide *newSlide);

    void updateAnimations(Q3DSSlide *animSourceSlide, Q3DSSlide *playModeSourceSlide);
    void setAnimationsRunning(Q3DSSlide *slide, bool running);

    void prepareNextFrame();
    Q3DSAnimationManager *animationBuilder() { return m_animationManager; }

    enum SetNodePropFlag {
        NodePropUpdateGlobalsRecursively = 0x01,
        NodePropUpdateAttached = 0x02
    };
    Q_DECLARE_FLAGS(SetNodePropFlags, SetNodePropFlag)

    enum UpdateGlobalFlag {
        UpdateGlobalsRecursively = 0x01,
        UpdateGlobalsSkipTransform = 0x02
    };
    Q_DECLARE_FLAGS(UpdateGlobalFlags, UpdateGlobalFlag)

    static QVector<Qt3DRender::QRenderPass *> standardRenderPasses(Qt3DRender::QShaderProgram *program,
                                                                   Q3DSLayerNode *layer3DS,
                                                                   Q3DSDefaultMaterial::BlendMode blendMode = Q3DSDefaultMaterial::Normal,
                                                                   bool hasDisplacement = false);
    static QVector<Qt3DRender::QTechnique *> computeTechniques(Q3DSLayerNode *layer3DS);
    static void markAsMainTechnique(Qt3DRender::QTechnique *technique);

    // for testing from the viewer - to be moved private later
    void setDepthTextureEnabled(Q3DSLayerNode *layer3DS, bool enabled);
    void rebuildModelMaterial(Q3DSModelNode *model3DS);

private:
    Qt3DRender::QFrameGraphNode *buildLayer(Q3DSLayerNode *layer3DS, Qt3DRender::QFrameGraphNode *parent, const QSize &parentSize);
    QSize calculateLayerSize(Q3DSLayerNode *layer3DS, const QSize &parentSize);
    QPointF calculateLayerPos(Q3DSLayerNode *layer3DS, const QSize &parentSize);
    void updateSizesForLayer(Q3DSLayerNode *layer3DS, const QSize &newParentSize);
    void setLayerCameraSizeProperties(Q3DSLayerNode *layer3DS);
    void setLayerSizeProperties(Q3DSLayerNode *layer3DS);
    void setLayerProperties(Q3DSLayerNode *layer3DS);
    void buildLayerScene(Q3DSGraphObject *obj, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    void reparentCamera(Q3DSLayerNode *layer3DS);
    void setSsaoTextureEnabled(Q3DSLayerNode *layer3DS, bool enabled);
    void updateAoParameters(Q3DSLayerNode *layer3DS);
    void updateSsaoStatus(Q3DSLayerNode *layer3DS, bool *aoDidChange = nullptr);
    void updateShadowMapStatus(Q3DSLayerNode *layer3DS);

    Q3DSCameraNode *chooseLayerCamera(Q3DSLayerNode *layer3DS, Qt3DRender::QCamera **camera);
    Qt3DRender::QCamera *buildLayerCamera(Q3DSLayerNode *layer3DS, Q3DSCameraNode *camNode);
    void setCameraProperties(Q3DSCameraNode *camNode, int changeFlags);

    Qt3DCore::QTransform *initEntityForNode(Qt3DCore::QEntity *entity, Q3DSNode *node, Q3DSLayerNode *layer3DS);
    void setNodeProperties(Q3DSNode *node, Qt3DCore::QEntity *entity, Qt3DCore::QTransform *transform, SetNodePropFlags flags = SetNodePropFlags());
    void updateGlobals(Q3DSNode *node, UpdateGlobalFlags flags);

    Qt3DCore::QEntity *buildGroup(Q3DSGroupNode *group3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    Qt3DCore::QEntity *buildComponent(Q3DSComponentNode *comp3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);

    Qt3DCore::QEntity *buildText(Q3DSTextNode *text3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    void updateText(Q3DSTextNode *text3DS, bool needsNewImage);

    Qt3DCore::QEntity *buildLight(Q3DSLightNode *light3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    void setLightProperties(Q3DSLightNode *light3DS, bool forceUpdate = false);

    Qt3DCore::QEntity *buildModel(Q3DSModelNode *model3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    void buildModelMaterial(Q3DSModelNode *model3DS);
    void retagSubMeshes(Q3DSModelNode *model3DS);
    QVector<Qt3DRender::QParameter *> prepareDefaultMaterial(Q3DSDefaultMaterial *m, Q3DSModelNode *model3DS);
    void updateDefaultMaterial(Q3DSDefaultMaterial *m);
    void gatherLights(Q3DSGraphObject *root, QVector<Q3DSLightSource> *allLights, QVector<Q3DSLightSource> *nonAreaLights,
                      QVector<Q3DSLightSource> *areaLights, QVector<Q3DSLightNode *> *lightNodes);
    void updateLightsBuffer(const QVector<Q3DSLightSource> &lights, Qt3DRender::QBuffer *uniformBuffer);
    void updateModel(Q3DSModelNode *model3DS);

    Qt3DRender::QFrameGraphNode *buildCompositor(Qt3DRender::QFrameGraphNode *parent, Qt3DCore::QEntity *parentEntity);

    void buildFsQuad(Qt3DCore::QEntity *parentEntity,
                     const QStringList &passNames,
                     const QVector<Qt3DRender::QShaderProgram *> &passProgs,
                     Qt3DRender::QLayer *tag);

    void handlePropertyChange(Q3DSGraphObject *obj, const QSet<QString> &keys, int changeFlags);
    void updateNodeFromChangeFlags(Q3DSNode *node, Qt3DCore::QTransform *transform, int changeFlags);
    void updateSubTreeRecursive(Q3DSGraphObject *obj);
    void updateSubTree(Q3DSGraphObject *obj);

    void updateSlideObjectVisibilities(Q3DSSlide *slide);
    void setNodeVisibility(Q3DSNode *node, bool visible);
    bool scheduleNodeVisibilityUpdate(Q3DSGraphObject *obj, Q3DSComponentNode *component = nullptr);

    Q3DSGraphicsLimits m_gfxLimits;
    SceneBuilderFlags m_flags = SceneBuilderFlags();
    Q3DSPresentation *m_presentation;
    QSize m_presentationSize;
    Q3DSScene *m_scene;
    Q3DSSlide *m_masterSlide;
    Q3DSSlide *m_currentSlide;
    Qt3DCore::QEntity *m_rootEntity;
    Q3DSFrameUpdater *m_frameUpdater = nullptr;
    Q3DSDefaultMaterialGenerator *m_matGen;
    Q3DSTextMaterialGenerator *m_textMatGen;
    Q3DSAnimationManager *m_animationManager;
    Q3DSTextRenderer *m_textRenderer;
    QSet<Q3DSLayerNode *> m_layersWithDirtyLights;
    QSet<Q3DSNode *> m_pendingNodeShow;
    QSet<Q3DSNode *> m_pendingNodeHide;
    Qt3DRender::QLayer *m_fsQuadTag = nullptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSSceneManager::SceneBuilderFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSSceneManager::SetNodePropFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSSceneManager::UpdateGlobalFlags)

class Q3DSFrameUpdater : public QObject
{
public:
    Q3DSFrameUpdater(Q3DSSceneManager *manager) : m_sceneManager(manager) { }

    void frameAction(float dt);

private:
    Q3DSSceneManager *m_sceneManager;
};

QT_END_NAMESPACE

#endif // Q3DSSCENEBUILDER_H
