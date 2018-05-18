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

#ifndef Q3DSSCENEMANAGER_P_H
#define Q3DSSCENEMANAGER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "q3dsuippresentation_p.h"
#include "q3dsgraphicslimits_p.h"
#include "q3dsinputmanager_p.h"

#include <QDebug>
#include <QWindow>
#include <QStack>
#include <QQueue>
#include <QElapsedTimer>
#include <QMutex>

QT_BEGIN_NAMESPACE

class Q3DSSceneManager;
class Q3DSFrameUpdater;
class Q3DSTextRenderer;
class Q3DSDefaultMaterialGenerator;
class Q3DSCustomMaterialGenerator;
class Q3DSTextMaterialGenerator;
class Q3DSProfiler;
class Q3DSProfileUi;
class Q3DSEngine;
class Q3DSMesh;
class Q3DSSlidePlayer;
class Q3DSConsoleCommands;

namespace Qt3DCore {
class QEntity;
class QTransform;
}

namespace Qt3DLogic {
class QFrameAction;
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
class QPaintedTextureImage;
class QLayerFilter;
class QRenderTargetSelector;
class QRenderTarget;
class QTechnique;
class QFilterKey;
class QRenderState;
class QRayCaster;
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
#define Q3DS_MAX_NUM_LIGHTS_ES2 8

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

class Q3DSV_PRIVATE_EXPORT Q3DSNodeAttached : public Q3DSGraphObjectAttached
{
public:
    struct LightsData {
        QVector<Q3DSLightSource> allLights;
        QVector<Q3DSLightSource> nonAreaLights;
        QVector<Q3DSLightSource> areaLights;
        QVector<Q3DSLightNode *> lightNodes;
        Qt3DRender::QParameter *allLightsParam = nullptr; // for default material
        Qt3DRender::QParameter *nonAreaLightsParam = nullptr; // split, for custom materials
        Qt3DRender::QParameter *areaLightsParam = nullptr; // split, for custom materials
        Qt3DRender::QBuffer *allLightsConstantBuffer = nullptr;
        Qt3DRender::QBuffer *nonAreaLightsConstantBuffer = nullptr;
        Qt3DRender::QBuffer *areaLightsConstantBuffer = nullptr;
        Qt3DRender::QParameter *lightAmbientTotalParamenter = nullptr;
    };
    Qt3DCore::QTransform *transform = nullptr;
    QMatrix4x4 globalTransform;
    float globalOpacity = 1;
    bool globalVisibility = true;
    Q3DSLayerNode *layer3DS = nullptr;
    QScopedPointer<LightsData> lightsData;
};

class Q3DSLayerAttached : public Q3DSNodeAttached
{
public:
    Q3DSLayerAttached() {
        // layers always have light data
        lightsData.reset(new Q3DSNodeAttached::LightsData);
    }
    Qt3DCore::QEntity *compositorEntity = nullptr;
    Qt3DRender::QFrameGraphNode *layerFgRoot = nullptr;
    Qt3DCore::QNode *layerFgRootParent = nullptr;
    Qt3DCore::QNode *layerFgDummyParent = nullptr;
    Q3DSCameraNode *cam3DS = nullptr;
    Qt3DRender::QCameraSelector *cameraSelector = nullptr;
    Qt3DRender::QClearBuffers *clearBuffers = nullptr;
    Qt3DRender::QRenderTargetSelector *rtSelector = nullptr;
    typedef std::function<void(Q3DSLayerNode *)> SizeChangeCallback;
    struct SizeManagedTexture {
        enum Flag {
            IgnoreSSAA = 0x01,
            CustomSizeCalculation = 0x02
        };
        Q_DECLARE_FLAGS(Flags, Flag)
        SizeManagedTexture() { }
        SizeManagedTexture(Qt3DRender::QAbstractTexture *t, SizeChangeCallback c = nullptr, Flags f = Flags())
            : sizeChangeCallback(c), texture(t), flags(f)
        { }
        SizeChangeCallback sizeChangeCallback = nullptr;
        Qt3DRender::QAbstractTexture *texture = nullptr;
        Flags flags;
    };

    QVector<SizeManagedTexture> sizeManagedTextures;
    QVector<SizeChangeCallback> layerSizeChangeCallbacks;
    Qt3DRender::QAbstractTexture *layerTexture = nullptr;
    Qt3DRender::QAbstractTexture *effLayerTexture = nullptr;
    Qt3DRender::QAbstractTexture *layerDS = nullptr;
    Qt3DRender::QParameter *compositorSourceParam = nullptr;
    Qt3DRender::QRenderPass *compositorRenderPass = nullptr;
    std::function<void()> updateCompositorCalculations = nullptr;
    std::function<void()> updateSubPresentationSize = nullptr;
    QSize layerSize;
    QSize parentSize;
    QPointF layerPos;
    int msaaSampleCount = 0;
    int ssaaScaleFactor = 1;
    int nonDirtyRenderCount = 0;
    bool usesDefaultCompositorProgram = true;
    bool effectActive = false;
    bool wasDirty = false;
    bool rayCasterBusy = false;
    Qt3DRender::QParameter *cameraPropertiesParam = nullptr;
    Qt3DRender::QLayer *opaqueTag = nullptr;
    Qt3DRender::QLayer *transparentTag = nullptr;
    Qt3DRender::QRayCaster *layerRayCaster = nullptr;

    struct RayCastQueueEntry {
        QVector3D direction;
        QVector3D origin;
        float length;
        Q3DSInputManager::InputState inputState;
        int eventId;
    };
    QQueue<RayCastQueueEntry> rayCastQueue;

    struct DepthTextureData {
        bool enabled = false;
        Qt3DRender::QRenderTargetSelector *rtSelector = nullptr;
        Qt3DRender::QAbstractTexture *depthTexture = nullptr;
        Qt3DRender::QClearBuffers *clearBuffers = nullptr;
        Qt3DRender::QLayerFilter *layerFilterOpaque = nullptr;
        Qt3DRender::QLayerFilter *layerFilterTransparent = nullptr;
    } depthTextureData;

    struct SsaoTextureData {
        bool enabled = false;
        Qt3DRender::QRenderTargetSelector *rtSelector = nullptr;
        Qt3DRender::QAbstractTexture *ssaoTexture = nullptr;
        Qt3DRender::QClearBuffers *clearBuffers = nullptr;
        Qt3DRender::QLayerFilter *layerFilter = nullptr;
        Qt3DRender::QParameter *depthSampler = nullptr;
        Qt3DRender::QBuffer *aoDataBuf = nullptr;
        Qt3DRender::QParameter *aoDataBufParam = nullptr;
        Qt3DRender::QParameter *ssaoTextureSampler = nullptr;
    } ssaoTextureData;

    struct PerLightShadowMapData {
        bool active = false;
        Q3DSLightNode *lightNode = nullptr;
        Qt3DRender::QFrameGraphNode *subTreeRoot = nullptr;
        Qt3DRender::QAbstractTexture *shadowMapTextureTemp = nullptr;
        Qt3DRender::QAbstractTexture *shadowMapTexture = nullptr;
        Qt3DRender::QAbstractTexture *shadowDS = nullptr;
        Qt3DRender::QParameter *cameraPositionParam = nullptr;
        struct {
            Qt3DRender::QParameter *shadowSampler = nullptr;
            Qt3DRender::QParameter *shadowMatrixParam = nullptr;
            Qt3DRender::QParameter *shadowControlParam = nullptr;
        } materialParams;
        Qt3DRender::QParameter *shadowCamPropsParam = nullptr;
        Qt3DRender::QCamera *shadowCamOrtho = nullptr;
        Qt3DRender::QCamera *shadowCamProj[6];
    };
    struct ShadowMapData {
        Qt3DRender::QFrameGraphNode *shadowRoot = nullptr;
        QVector<PerLightShadowMapData> shadowCasters;
        Qt3DRender::QAbstractTexture *defaultShadowDS = nullptr;
    } shadowMapData;

    struct ProgAAData {
        Qt3DRender::QFrameGraphNode *fg = nullptr;
        Qt3DRender::QRenderTargetSelector *rtSel = nullptr;
        Qt3DRender::QRenderTarget *rts[2];
        Qt3DRender::QAbstractTexture *currentAccumulatorTexture = nullptr;
        Qt3DRender::QAbstractTexture *currentOutputTexture = nullptr;
        Qt3DRender::QAbstractTexture *stolenColorBuf = nullptr;
        Qt3DRender::QAbstractTexture *extraColorBuf = nullptr;
        Qt3DRender::QParameter *accumTexParam = nullptr;
        Qt3DRender::QParameter *lastTexParam = nullptr;
        Qt3DRender::QParameter *blendFactorsParam = nullptr;
        Qt3DRender::QLayerFilter *layerFilter = nullptr;
        int pass = 0;
        int curTarget = 0;
        bool cameraAltered = false;
        bool enabled = false;
    } progAA;

    struct TempAAData {
        int nonDirtyPass = 0;
        int passIndex = 0; // wraps around
        Qt3DRender::QFrameGraphNode *fg = nullptr;
        Qt3DRender::QRenderTargetSelector *rtSel = nullptr;
        Qt3DRender::QRenderTarget *rts[2];
        Qt3DRender::QAbstractTexture *currentAccumulatorTexture = nullptr;
        Qt3DRender::QAbstractTexture *currentOutputTexture = nullptr;
        Qt3DRender::QAbstractTexture *stolenColorBuf = nullptr;
        Qt3DRender::QAbstractTexture *extraColorBuf = nullptr;
        Qt3DRender::QParameter *accumTexParam = nullptr;
        Qt3DRender::QParameter *lastTexParam = nullptr;
        Qt3DRender::QParameter *blendFactorsParam = nullptr;
        Qt3DRender::QLayerFilter *layerFilter = nullptr;
    } tempAA;

    struct AdvBlendData {
        Qt3DRender::QAbstractTexture *tempTexture = nullptr;
        Qt3DRender::QRenderTarget *tempRt = nullptr;
    } advBlend;

    struct EffectData {
        Qt3DRender::QFrameGraphNode *effectRoot = nullptr;
        QVector<Q3DSEffectInstance *> effects;
        Qt3DRender::QAbstractTexture *sourceTexture = nullptr;
        bool ownsSourceTexture = false;
        Qt3DRender::QFrameGraphNode *resolve = nullptr;
    } effectData;

    struct IBLProbeData {
        Qt3DRender::QAbstractTexture *lightProbeTexture = nullptr;
        Qt3DRender::QAbstractTexture *lightProbe2Texture = nullptr;

        Qt3DRender::QParameter *lightProbeSampler = nullptr;
        Qt3DRender::QParameter *lightProbeRotation = nullptr;
        Qt3DRender::QParameter *lightProbeOffset = nullptr;

        Qt3DRender::QParameter *lightProbeProperties = nullptr;
        Qt3DRender::QParameter *lightProbeOptions = nullptr;
        Qt3DRender::QParameter *lightProbe2Sampler = nullptr;
        Qt3DRender::QParameter *lightProbe2Properties = nullptr;
    } iblProbeData;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSLayerAttached::SizeManagedTexture::Flags)
// NB! Q3DSLayerAttached::SizeManagedTexture cannot be Q_MOVABLE_TYPE due to std::function in it
Q_DECLARE_TYPEINFO(Q3DSLayerAttached::PerLightShadowMapData, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSLayerAttached::ProgAAData, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSLayerAttached::TempAAData, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSLayerAttached::RayCastQueueEntry, Q_MOVABLE_TYPE);

// ensure a lookup based on a texture hits the entry regardless of the callback or flags
inline bool operator==(const Q3DSLayerAttached::SizeManagedTexture &a, const Q3DSLayerAttached::SizeManagedTexture &b)
{
    return a.texture == b.texture;
}

inline bool operator!=(const Q3DSLayerAttached::SizeManagedTexture &a, const Q3DSLayerAttached::SizeManagedTexture &b)
{
    return a.texture != b.texture;
}

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
    Qt3DExtras::QPlaneMesh *mesh = nullptr;
    Qt3DRender::QParameter *opacityParam = nullptr;
    Qt3DRender::QParameter *colorParam = nullptr;
    Qt3DRender::QParameter *textureParam = nullptr;
    Qt3DRender::QAbstractTexture *texture = nullptr;
    Qt3DRender::QPaintedTextureImage *textureImage = nullptr;
};

class Q3DSLightAttached : public Q3DSNodeAttached
{
public:
    Q3DSLightSource lightSource;
};

class Q3DSAliasAttached : public Q3DSNodeAttached
{
public:
};


class Q3DSModelAttached : public Q3DSNodeAttached
{
public:
    struct SubMesh {
        Q3DSMesh *mesh = nullptr;
        Qt3DRender::QMaterial *materialComponent = nullptr;
        Q3DSGraphObject *material = nullptr; // Default, Custom, Referenced
        Q3DSGraphObject *resolvedMaterial = nullptr; // Default, Custom normally, but can still be Referenced for invalid refs
        Q3DSReferencedMaterial *referencingMaterial = nullptr; // When valid, there are some overrides possible by referenced material
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
    Qt3DRender::QParameter *size = nullptr;

    QVector<Qt3DRender::QParameter *> parameters() const { return { sampler, offsets, rotations, size }; }

    Qt3DRender::QAbstractTexture *texture = nullptr;
    QString subPresId;
};

// common base class for default and custom material data
class Q3DSMaterialAttached : public Q3DSGraphObjectAttached
{
public:
    // One default or custom material may be associated with multiple models
    // when a ReferencedMaterial is used.
    struct PerModelData {
        float combinedOpacity = 1.0f;
        union {
            Qt3DRender::QParameter *materialDiffuseParam;
            Qt3DRender::QParameter *objectOpacityParam;
        };
    };
    QHash<Q3DSModelNode *, PerModelData> perModelData;
};

Q_DECLARE_TYPEINFO(Q3DSMaterialAttached::PerModelData, Q_MOVABLE_TYPE);

class Q3DSDefaultMaterialAttached : public Q3DSMaterialAttached
{
public:
    Qt3DRender::QParameter *diffuseParam = nullptr;
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
    Q3DSTextureParameters roughnessMapParams;
    Q3DSTextureParameters bumpMapParams;
    Q3DSTextureParameters normalMapParams;
    Q3DSTextureParameters displacementMapParams;
    Q3DSTextureParameters opacityMapParams;
    Q3DSTextureParameters emissiveMapParams;
    Q3DSTextureParameters emissiveMap2Params;
    Q3DSTextureParameters translucencyMapParams;
    // Lightmaps
    Q3DSTextureParameters lightmapIndirectParams;
    Q3DSTextureParameters lightmapRadiosityParams;
    Q3DSTextureParameters lightmapShadowParams;
    // IBL
    QMetaObject::Connection updateOffsetConnection;
    Qt3DRender::QAbstractTexture *lightProbeOverrideTexture = nullptr;
    Qt3DRender::QParameter *lightProbeSampler = nullptr;
    Qt3DRender::QParameter *lightProbeRotation = nullptr;
    Qt3DRender::QParameter *lightProbeOffset = nullptr;
};

struct Q3DSCustomPropertyParameter {
    Q3DSCustomPropertyParameter(Qt3DRender::QParameter *param_, const QVariant &value, const Q3DSMaterial::PropertyElement &meta_)
        : param(param_),
          inputValue(value),
          meta(meta_)
    { }
    Q3DSCustomPropertyParameter() { }
    Qt3DRender::QParameter *param = nullptr;
    QVariant inputValue; // e.g. Texture: inputValue is a string whereas param->value is a QAbstractTexture*
    Q3DSMaterial::PropertyElement meta;
    Qt3DRender::QParameter *texInfoParam = nullptr;
    Qt3DRender::QParameter *texFlagParam = nullptr;
};

Q_DECLARE_TYPEINFO(Q3DSCustomPropertyParameter, Q_MOVABLE_TYPE);

class Q3DSCustomMaterialAttached : public Q3DSMaterialAttached
{
public:
    QHash<QString, Q3DSCustomPropertyParameter> params;
    // Lightmaps
    Q3DSTextureParameters lightmapIndirectParams;
    Q3DSTextureParameters lightmapRadiosityParams;
    Q3DSTextureParameters lightmapShadowParams;

    // IBL
    QMetaObject::Connection updateOffsetConnection;
    Qt3DRender::QAbstractTexture *lightProbeOverrideTexture = nullptr;
    Qt3DRender::QParameter *lightProbeSampler = nullptr;
    Qt3DRender::QParameter *lightProbeRotation = nullptr;
    Qt3DRender::QParameter *lightProbeOffset = nullptr;
};

class Q3DSEffectAttached : public Q3DSGraphObjectAttached
{
public:
    bool active = false;
    Q3DSLayerNode *layer3DS = nullptr;
    Qt3DCore::QEntity *quadEntity = nullptr;
    Qt3DRender::QLayer *quadEntityTag = nullptr;
    QHash<QString, Q3DSCustomPropertyParameter> params;
    Qt3DRender::QParameter *appFrameParam = nullptr;
    Qt3DRender::QParameter *fpsParam = nullptr;
    Qt3DRender::QParameter *cameraClipRangeParam = nullptr;
    struct TextureBuffer {
        Qt3DRender::QAbstractTexture *texture = nullptr;
        QVector<Qt3DRender::QParameter *> textureInfoParams;
        bool hasSceneLifetime = false;
    };
    QHash<QString, TextureBuffer> textureBuffers;
    struct PassData {
        Qt3DRender::QAbstractTexture *passInput = nullptr;
        Qt3DRender::QParameter *texture0InfoParam = nullptr;
        Qt3DRender::QAbstractTexture *passOutput = nullptr;
        Qt3DRender::QParameter *destSizeParam = nullptr;
    };
    QVector<PassData> passData;
    QVector<QPair<Qt3DRender::QParameter *, Qt3DRender::QAbstractTexture *> > sourceDepTextureInfoParams;
    QVector<Qt3DRender::QFrameGraphNode *> passFgRoots;
    Qt3DRender::QAbstractTexture *sourceTexture = nullptr; // never owned
    Qt3DRender::QAbstractTexture *outputTexture = nullptr;
    bool ownsOutputTexture = false;
};

class Q3DSSlideAttached : public Q3DSGraphObjectAttached
{
public:
    Q3DSSlidePlayer *slidePlayer = nullptr;
    Qt3DAnimation::QClipAnimator *animator = nullptr;
    QVector<Qt3DAnimation::QClipAnimator *> animators;
    QSet<Q3DSNode *> needsMasterRollback;
};

class Q3DSImageAttached : public Q3DSGraphObjectAttached
{
public:
    QSet<Q3DSDefaultMaterial *> referencingDefaultMaterials;
};

class Q3DSBehaviorAttached : public Q3DSGraphObjectAttached
{
public:
};

struct Q3DSSubPresentation
{
    QString id;
    Q3DSSceneManager *sceneManager = nullptr;
    Qt3DRender::QAbstractTexture *colorTex = nullptr;
    Qt3DRender::QAbstractTexture *depthOrDepthStencilTex = nullptr;
    Qt3DRender::QAbstractTexture *stencilTex = nullptr;
};

struct Q3DSGuiData
{
    Qt3DRender::QLayer *tag = nullptr;
    Qt3DRender::QLayer *activeTag = nullptr;
    Qt3DRender::QFilterKey *techniqueFilterKey = nullptr;
    Qt3DRender::QCamera *camera = nullptr;
    Qt3DCore::QEntity *rootEntity = nullptr;
    QSize outputSize;
    qreal outputDpr = 1;
};

class Q3DSV_PRIVATE_EXPORT Q3DSSceneManager
{
public:
    enum SceneBuilderFlag {
        Force4xMSAA = 0x01,
        SubPresentation = 0x02,
        EnableProfiling = 0x04
    };
    Q_DECLARE_FLAGS(SceneBuilderFlags, SceneBuilderFlag)

    struct SceneBuilderParams {
        SceneBuilderFlags flags;
        QSize outputSize;
        qreal outputDpr = 1;
        QObject *surface = nullptr; // null for subpresentations that go into a texture
        Qt3DRender::QFrameGraphNode *frameGraphRoot = nullptr; // when !window
        Q3DSEngine *engine = nullptr;
    };

    struct Scene {
        Qt3DCore::QEntity *rootEntity = nullptr;
        Qt3DRender::QFrameGraphNode *frameGraphRoot = nullptr;
        Qt3DRender::QFrameGraphNode *subPresFrameGraphRoot = nullptr; // when params.window
        Qt3DRender::QRenderSettings *renderSettings = nullptr; // when params.window
        Qt3DLogic::QFrameAction *frameAction = nullptr;
    };

    Q3DSSceneManager();
    ~Q3DSSceneManager();

    Scene buildScene(Q3DSUipPresentation *presentation, const SceneBuilderParams &params);
    void finalizeMainScene(const QVector<Q3DSSubPresentation> &subPresentations);
    void updateSizes(const QSize &size, qreal dpr, bool forceSynchronous = false);

    void prepareEngineReset();
    static void prepareEngineResetGlobal();

    Q3DSSlide *currentSlide() const { return m_currentSlide; }
    Q3DSSlide *masterSlide() const { return m_masterSlide; }
    void setCurrentSlide(Q3DSSlide *slide, bool fromSlidePlayer = false);
    void setComponentCurrentSlide(Q3DSComponentNode *component, Q3DSSlide *newSlide);

    void setLayerCaching(bool enabled);

    void prepareAnimators();

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

    enum BuildLayerQuadFlag {
        LayerQuadBlend = 0x01,
        LayerQuadCustomShader = 0x02
    };
    Q_DECLARE_FLAGS(BuildLayerQuadFlags, BuildLayerQuadFlag)

    enum FsQuadFlag {
        FsQuadCustomDepthSettings = 0x01
    };
    Q_DECLARE_FLAGS(FsQuadFlags, FsQuadFlag)

    enum EffectActivationFlag {
        EffIsFirst = 0x01,
        EffIsLast = 0x02
    };
    Q_DECLARE_FLAGS(EffectActivationFlags, EffectActivationFlag)

    static QVector<Qt3DRender::QRenderPass *> standardRenderPasses(Qt3DRender::QShaderProgram *program,
                                                                   Q3DSLayerNode *layer3DS,
                                                                   Q3DSDefaultMaterial::BlendMode blendMode = Q3DSDefaultMaterial::Normal,
                                                                   bool hasDisplacement = false);
    static QVector<Qt3DRender::QTechnique *> computeTechniques(Q3DSLayerNode *layer3DS);
    static void markAsMainTechnique(Qt3DRender::QTechnique *technique);

    Q3DSProfiler *profiler() { return m_profiler; }

    void setProfileUiVisible(bool visible, bool openLogAndConsole = false);
    bool isProfileUiVisible() const;
    void setProfileUiInputEventSource(QObject *obj);
    void configureProfileUi(float scale);

    Q3DSInputManager *inputManager() { return m_inputManager; }

    // for testing from the viewer - to be moved private later
    void setDepthTextureEnabled(Q3DSLayerNode *layer3DS, bool enabled);
    void rebuildModelMaterial(Q3DSModelNode *model3DS);

    void addLog(const QString &msg);
    void addLog(const char *fmt, ...);
    Q3DSSlidePlayer *slidePlayer() const { return m_slidePlayer; }
    Qt3DCore::QEntity *getRootEntity() const { return m_rootEntity; }

    void setDataInputValue(const QString &dataInputName, const QVariant &value);
    void changeSlideByName(Q3DSGraphObject *sceneOrComponent, const QString &name);
    void changeSlideByIndex(Q3DSGraphObject *sceneOrComponent, int index);
    void changeSlideByDirection(Q3DSGraphObject *sceneOrComponent, bool next, bool wrap);
    void goToTime(Q3DSGraphObject *sceneOrComponent, float milliseconds, bool pause = false);

    void queueEvent(const Q3DSGraphObject::Event &e);

private:
    Q_DISABLE_COPY(Q3DSSceneManager)

    void buildLayer(Q3DSLayerNode *layer3DS, Qt3DRender::QFrameGraphNode *parent, const QSize &parentSize);
    void buildSubPresentationLayer(Q3DSLayerNode *layer3DS, const QSize &parentSize);
    Qt3DRender::QRenderTarget *newLayerRenderTarget(const QSize &layerPixelSize, int msaaSampleCount,
                                                    Qt3DRender::QAbstractTexture **colorTex, Qt3DRender::QAbstractTexture **dsTexOrRb,
                                                    Qt3DCore::QNode *textureParentNode, Q3DSLayerNode *layer3DS,
                                                    Qt3DRender::QAbstractTexture *existingDS = nullptr);
    QSize calculateLayerSize(Q3DSLayerNode *layer3DS, const QSize &parentSize);
    QPointF calculateLayerPos(Q3DSLayerNode *layer3DS, const QSize &parentSize);
    void updateSizesForLayer(Q3DSLayerNode *layer3DS, const QSize &newParentSize);
    void setLayerCameraSizeProperties(Q3DSLayerNode *layer3DS, const QVector2D &offset = QVector2D());
    void setLayerSizeProperties(Q3DSLayerNode *layer3DS);
    void setLayerProperties(Q3DSLayerNode *layer3DS);
    void initNonNode(Q3DSGraphObject *obj);
    void buildLayerScene(Q3DSGraphObject *obj, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    void setSsaoTextureEnabled(Q3DSLayerNode *layer3DS, bool enabled);
    void updateAoParameters(Q3DSLayerNode *layer3DS);
    void updateSsaoStatus(Q3DSLayerNode *layer3DS, bool *aoDidChange = nullptr);
    void updateShadowMapStatus(Q3DSLayerNode *layer3DS, bool *smDidChange = nullptr);
    void updateCubeShadowMapParams(Q3DSLayerAttached::PerLightShadowMapData *d, Q3DSLightNode *light3DS, const QString &lightIndexStr);
    void updateCubeShadowCam(Q3DSLayerAttached::PerLightShadowMapData *d, int faceIdx, Q3DSLightNode *light3DS);
    void genCubeBlurPassFg(Q3DSLayerAttached::PerLightShadowMapData *d, Qt3DRender::QAbstractTexture *inTex,
                           Qt3DRender::QAbstractTexture *outTex, const QString &passName, Q3DSLightNode *light3DS);
    void updateOrthoShadowMapParams(Q3DSLayerAttached::PerLightShadowMapData *d, Q3DSLightNode *light3DS, const QString &lightIndexStr);
    void updateOrthoShadowCam(Q3DSLayerAttached::PerLightShadowMapData *d, Q3DSLightNode *light3DS, Q3DSLayerAttached *layerData);
    void genOrthoBlurPassFg(Q3DSLayerAttached::PerLightShadowMapData *d, Qt3DRender::QAbstractTexture *inTex,
                            Qt3DRender::QAbstractTexture *outTex, const QString &passName, Q3DSLightNode *light3DS);
    void stealLayerRenderTarget(Qt3DRender::QAbstractTexture **stolenColorBuf, Q3DSLayerNode *layer3DS);
    Qt3DRender::QAbstractTexture *createProgressiveTemporalAAExtraBuffer(Q3DSLayerNode *layer3DS);
    bool updateProgressiveAA(Q3DSLayerNode *layer3DS);
    void updateTemporalAA(Q3DSLayerNode *layer3DS);

    Qt3DRender::QCamera *buildCamera(Q3DSCameraNode *cam3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    void setCameraProperties(Q3DSCameraNode *camNode, int changeFlags);
    bool setActiveLayerCamera(Q3DSCameraNode *cam3DS, Q3DSLayerNode *layer3DS);
    void updateLayerCamera(Q3DSLayerNode *layer3DS);
    Q3DSCameraNode *findFirstCamera(Q3DSLayerNode *layer3DS);

    Qt3DCore::QTransform *initEntityForNode(Qt3DCore::QEntity *entity, Q3DSNode *node, Q3DSLayerNode *layer3DS);
    void setNodeProperties(Q3DSNode *node, Qt3DCore::QEntity *entity, Qt3DCore::QTransform *transform, SetNodePropFlags flags = SetNodePropFlags());
    void updateGlobals(Q3DSNode *node, UpdateGlobalFlags flags);

    Qt3DCore::QEntity *buildGroup(Q3DSGroupNode *group3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    Qt3DCore::QEntity *buildComponent(Q3DSComponentNode *comp3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    Qt3DCore::QEntity *buildAlias(Q3DSAliasNode *alias3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);

    Qt3DCore::QEntity *buildText(Q3DSTextNode *text3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    void updateText(Q3DSTextNode *text3DS, bool needsNewImage);

    Qt3DCore::QEntity *buildLight(Q3DSLightNode *light3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    void setLightProperties(Q3DSLightNode *light3DS, bool forceUpdate = false);

    Qt3DCore::QEntity *buildModel(Q3DSModelNode *model3DS, Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parent);
    void buildModelMaterial(Q3DSModelNode *model3DS);
    void retagSubMeshes(Q3DSModelNode *model3DS);
    void prepareTextureParameters(Q3DSTextureParameters &textureParameters, const QString &name, Q3DSImage *image3DS);
    QVector<Qt3DRender::QParameter *> prepareDefaultMaterial(Q3DSDefaultMaterial *m, Q3DSReferencedMaterial *rm, Q3DSModelNode *model3DS);
    Qt3DRender::QAbstractTexture *createCustomPropertyTexture(const Q3DSCustomPropertyParameter &p);
    QVector<Qt3DRender::QParameter *> prepareCustomMaterial(Q3DSCustomMaterialInstance *m, Q3DSReferencedMaterial *rm, Q3DSModelNode *model3DS);
    void setImageTextureFromSubPresentation(Qt3DRender::QParameter *sampler, Q3DSImage *image);
    void updateTextureParameters(Q3DSTextureParameters &textureParameters, Q3DSImage *image);
    void updateDefaultMaterial(Q3DSDefaultMaterial *m, Q3DSReferencedMaterial *rm, Q3DSModelNode *model3DS);
    void updateCustomMaterial(Q3DSCustomMaterialInstance *m, Q3DSReferencedMaterial *rm, Q3DSModelNode *model3DS);
    void buildEffect(Q3DSEffectInstance *eff3DS, Q3DSLayerNode *layer3DS);
    void updateEffectStatus(Q3DSLayerNode *layer3DS);
    void ensureEffectSource(Q3DSLayerNode *layer3DS);
    void cleanupEffectSource(Q3DSLayerNode *layer3DS);
    void activateEffect(Q3DSEffectInstance *eff3DS, Q3DSLayerNode *layer3DS, EffectActivationFlags flags, Qt3DRender::QAbstractTexture *prevOutput);
    void deactivateEffect(Q3DSEffectInstance *eff3DS, Q3DSLayerNode *layer3DS);
    void setupEffectTextureBuffer(Q3DSEffectAttached::TextureBuffer *tb, const Q3DSMaterial::PassBuffer &bufDesc, Q3DSLayerNode *layer3DS);
    void createEffectBuffers(Q3DSEffectInstance *eff3DS);
    void updateEffect(Q3DSEffectInstance *eff3DS);
    void updateEffectForNextFrame(Q3DSEffectInstance *eff3DS, qint64 nextFrameNo);
    void gatherLights(Q3DSLayerNode *layer);
    void updateLightsBuffer(const QVector<Q3DSLightSource> &lights, Qt3DRender::QBuffer *uniformBuffer);
    void updateModel(Q3DSModelNode *model3DS);
    QVector<Q3DSNodeAttached::LightsData *> getLightsDataForNode(Q3DSGraphObject *object);
    QVector<Qt3DRender::QParameter *> prepareSeparateLightUniforms(const QVector<Q3DSLightSource> &allLights, const QString &lightsUniformName);

    void buildLayerQuadEntity(Q3DSLayerNode *layer3DS, Qt3DCore::QEntity *parentEntity, Qt3DRender::QLayer *tag,
                              BuildLayerQuadFlags flags, int layerDepth = 0);
    void updateLayerCompositorProgram(Q3DSLayerNode *layer3DS);
    void buildCompositor(Qt3DRender::QFrameGraphNode *parent, Qt3DCore::QEntity *parentEntity);
    void buildGuiPass(Qt3DRender::QFrameGraphNode *parent, Qt3DCore::QEntity *parentEntity);

    struct FsQuadParams {
        FsQuadFlags flags;
        Qt3DCore::QEntity *parentEntity = nullptr;
        QStringList passNames;
        QVector<Qt3DRender::QShaderProgram *> passProgs;
        Qt3DRender::QLayer *tag = nullptr;
        QVector<Qt3DRender::QParameter *> params;
        QVector<Qt3DRender::QRenderState *> renderStates;
    };
    Qt3DCore::QEntity *buildFsQuad(const FsQuadParams &info);

    void handlePropertyChange(Q3DSGraphObject *obj, const QSet<QString> &keys, int changeFlags);
    void updateNodeFromChangeFlags(Q3DSNode *node, Qt3DCore::QTransform *transform, int changeFlags);
    void updateSubTreeRecursive(Q3DSGraphObject *obj);
    void updateSubTree(Q3DSGraphObject *obj);
    void prepareNextFrame();

    bool isComponentVisible(Q3DSComponentNode *component);
    void setNodeVisibility(Q3DSNode *node, bool visible);

    void handleSceneChange(Q3DSScene *scene, Q3DSGraphObject::DirtyFlag change, Q3DSGraphObject *obj);

    void handleEvent(const Q3DSGraphObject::Event &e);
    void flushEventQueue();
    void runAction(const Q3DSAction &action);

    Qt3DRender::QAbstractTexture *dummyTexture();

    Q3DSGraphicsLimits m_gfxLimits;
    SceneBuilderFlags m_flags = SceneBuilderFlags();
    Q3DSEngine *m_engine;
    Q3DSUipPresentation *m_presentation;
    QSize m_presentationSize;
    Q3DSScene *m_scene;
    Q3DSSlide *m_masterSlide;
    Q3DSSlide *m_currentSlide;
    Qt3DCore::QEntity *m_rootEntity;
    Q3DSFrameUpdater *m_frameUpdater = nullptr;
    Q3DSDefaultMaterialGenerator *m_matGen;
    Q3DSCustomMaterialGenerator *m_customMaterialGen;
    Q3DSTextMaterialGenerator *m_textMatGen;
    Q3DSTextRenderer *m_textRenderer;
    QSet<Q3DSGraphObject *> m_subTreeWithDirtyLights;
    QSet<Q3DSDefaultMaterial *> m_pendingDefMatRebuild;
    QSet<Q3DSNode *> m_pendingNodeShow;
    QSet<Q3DSNode *> m_pendingNodeHide;
    Qt3DRender::QLayer *m_fsQuadTag = nullptr;
    QStack<Q3DSComponentNode *> m_componentNodeStack;
    QSet<Q3DSLayerNode *> m_pendingSubPresLayers;
    QVector<QPair<Qt3DRender::QParameter *, Q3DSImage *> > m_pendingSubPresImages;
    QVector<Q3DSSubPresentation> m_subPresentations;
    Qt3DRender::QAbstractTexture *m_dummyTex = nullptr;
    bool m_wasDirty = false;
    Q3DSProfiler *m_profiler = nullptr;
    Q3DSGuiData m_guiData;
    Q3DSProfileUi *m_profileUi = nullptr;
    Q3DSConsoleCommands *m_consoleCommands = nullptr;
    QSize m_outputPixelSize;
    QVector<std::function<void()> > m_compositorOutputSizeChangeCallbacks;
    qint64 m_firstFrameActionTime = 0;
    QMutex m_logMutex;
    Q3DSSlidePlayer *m_slidePlayer = nullptr;
    Q3DSInputManager *m_inputManager = nullptr;
    QVector<Q3DSGraphObject::Event> m_eventQueue;
    bool m_inDestructor = false;
    bool m_layerCaching = true;
    bool m_layerUncachePending = false;
    QSet<Q3DSSceneManager *> m_layerCacheDeps;

    friend class Q3DSFrameUpdater;
    friend class Q3DSProfiler;
    friend class Q3DSSlidePlayer;
    friend class Q3DSInputManager;
    friend class Q3DSConsoleCommands;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSSceneManager::SceneBuilderFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSSceneManager::SetNodePropFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSSceneManager::UpdateGlobalFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSSceneManager::BuildLayerQuadFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSSceneManager::FsQuadFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSSceneManager::EffectActivationFlags)

class Q3DSFrameUpdater : public QObject
{
public:
    Q3DSFrameUpdater(Q3DSSceneManager *manager) : m_sceneManager(manager) { }
    void frameAction(float dt);
    qint64 frameCounter() const { return m_frameCounter; }
    void startTimeFirstFrame() { m_firstFrameActionTimer.start(); }

private:
    Q3DSSceneManager *m_sceneManager;
    qint64 m_frameCounter = 0;
    QElapsedTimer m_firstFrameActionTimer;
    bool m_firstFrameAction = true;
};

Q3DSV_PRIVATE_EXPORT QDebug operator<<(QDebug dbg, const Q3DSSceneManager::SceneBuilderParams &p);

QT_END_NAMESPACE

#endif // Q3DSSCENEMANAGER_P_H
