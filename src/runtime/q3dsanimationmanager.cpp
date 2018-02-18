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

#include "q3dsanimationmanager_p.h"
#include "q3dsdatamodelparser_p.h"
#include "q3dsscenemanager_p.h"

#include <QLoggingCategory>

#include <Qt3DCore/QEntity>

#include <Qt3DAnimation/QClipAnimator>
#include <Qt3DAnimation/QChannelMapper>
#include <Qt3DAnimation/QChannelMapping>
#include <Qt3DAnimation/QCallbackMapping>
#include <Qt3DAnimation/QAnimationClip>
#include <Qt3DAnimation/qclock.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcScene)

static struct AnimatableExtraMeta {
    QString type3DS;
    QString name3DS;
    Q3DSAnimationManager::SetterFunc setter;
    Q3DSAnimationManager::GetterFunc getter;
} extraMeta[] = {
    { QLatin1String("Node"), QLatin1String("position"), &Q3DSNode::setPosition, &Q3DSNode::getPosition },
    { QLatin1String("Node"), QLatin1String("rotation"), &Q3DSNode::setRotation, &Q3DSNode::getRotation },
    { QLatin1String("Node"), QLatin1String("scale"), &Q3DSNode::setScale, &Q3DSNode::getScale },
    { QLatin1String("Node"), QLatin1String("pivot"), &Q3DSNode::setPivot, &Q3DSNode::getPivot },
    { QLatin1String("Node"), QLatin1String("opacity"), &Q3DSNode::setLocalOpacity, &Q3DSNode::getLocalOpacity },

    { QLatin1String("Material"), QLatin1String("diffuse"), &Q3DSDefaultMaterial::setDiffuse, &Q3DSDefaultMaterial::getDiffuse },
    { QLatin1String("Material"), QLatin1String("speculartint"), &Q3DSDefaultMaterial::setSpecularTint, &Q3DSDefaultMaterial::getSpecularTint },
    { QLatin1String("Material"), QLatin1String("specularamount"), &Q3DSDefaultMaterial::setSpecularAmount, &Q3DSDefaultMaterial::getSpecularAmount },
    { QLatin1String("Material"), QLatin1String("specularroughness"), &Q3DSDefaultMaterial::setSpecularRoughness, &Q3DSDefaultMaterial::getSpecularRoughness },
    { QLatin1String("Material"), QLatin1String("fresnelPower"), &Q3DSDefaultMaterial::setFresnelPower, &Q3DSDefaultMaterial::getFresnelPower },
    { QLatin1String("Material"), QLatin1String("displaceamount"), &Q3DSDefaultMaterial::setDisplaceAmount, &Q3DSDefaultMaterial::getDisplaceAmount },
    { QLatin1String("Material"), QLatin1String("opacity"), &Q3DSDefaultMaterial::setOpacity, &Q3DSDefaultMaterial::getOpacity },
    { QLatin1String("Material"), QLatin1String("emissivecolor"), &Q3DSDefaultMaterial::setEmissiveColor, &Q3DSDefaultMaterial::getEmissiveColor },
    { QLatin1String("Material"), QLatin1String("emissivepower"), &Q3DSDefaultMaterial::setEmissivePower, &Q3DSDefaultMaterial::getEmissivePower },
    { QLatin1String("Material"), QLatin1String("bumpamount"), &Q3DSDefaultMaterial::setBumpAmount, &Q3DSDefaultMaterial::getBumpAmount },
    { QLatin1String("Material"), QLatin1String("translucentfalloff"), &Q3DSDefaultMaterial::setTranslucentFalloff, &Q3DSDefaultMaterial::getTranslucentFalloff },
    { QLatin1String("Material"), QLatin1String("diffuselightwrap"), &Q3DSDefaultMaterial::setDiffuseLightWrap, &Q3DSDefaultMaterial::getDiffuseLightWrap },

    { QLatin1String("Camera"), QLatin1String("fov"), &Q3DSCameraNode::setFov, &Q3DSCameraNode::getFov },
    { QLatin1String("Camera"), QLatin1String("clipnear"), &Q3DSCameraNode::setClipNear, &Q3DSCameraNode::getClipNear },
    { QLatin1String("Camera"), QLatin1String("clipfar"), &Q3DSCameraNode::setClipFar, &Q3DSCameraNode::getClipFar },

    { QLatin1String("Light"), QLatin1String("lightdiffuse"), &Q3DSLightNode::setDiffuse, &Q3DSLightNode::getDiffuse },
    { QLatin1String("Light"), QLatin1String("lightspecular"), &Q3DSLightNode::setSpecular, &Q3DSLightNode::getSpecular },
    { QLatin1String("Light"), QLatin1String("lightambient"), &Q3DSLightNode::setAmbient, &Q3DSLightNode::getAmbient },
    { QLatin1String("Light"), QLatin1String("brightness"), &Q3DSLightNode::setBrightness, &Q3DSLightNode::getBrightness },
    { QLatin1String("Light"), QLatin1String("linearfade"), &Q3DSLightNode::setLinearFade, &Q3DSLightNode::getLinearFade },
    { QLatin1String("Light"), QLatin1String("expfade"), &Q3DSLightNode::setExpFade, &Q3DSLightNode::getExpFade },
    { QLatin1String("Light"), QLatin1String("areawidth"), &Q3DSLightNode::setAreaWidth, &Q3DSLightNode::getAreaWidth },
    { QLatin1String("Light"), QLatin1String("areaheight"), &Q3DSLightNode::setAreaHeight, &Q3DSLightNode::getAreaHeight },
    { QLatin1String("Light"), QLatin1String("shdwfactor"), &Q3DSLightNode::setShadowFactor, &Q3DSLightNode::getShadowFactor },
    { QLatin1String("Light"), QLatin1String("shdwfilter"), &Q3DSLightNode::setShadowFilter, &Q3DSLightNode::getShadowFilter },
    { QLatin1String("Light"), QLatin1String("shdwbias"), &Q3DSLightNode::setShadowBias, &Q3DSLightNode::getShadowBias },
    { QLatin1String("Light"), QLatin1String("shdwmapfar"), &Q3DSLightNode::setShadowMapFar, &Q3DSLightNode::getShadowMapFar },
    { QLatin1String("Light"), QLatin1String("shdwmapfov"), &Q3DSLightNode::setShadowMapFov, &Q3DSLightNode::getShadowMapFov },

    { QLatin1String("Model"), QLatin1String("edgetess"), &Q3DSModelNode::setEdgeTess, &Q3DSModelNode::getEdgeTess },
    { QLatin1String("Model"), QLatin1String("innertess"), &Q3DSModelNode::setInnerTess, &Q3DSModelNode::getInnerTess },

    { QLatin1String("Text"), QLatin1String("textcolor"), &Q3DSTextNode::setColor, &Q3DSTextNode::getColor },
    { QLatin1String("Text"), QLatin1String("leading"), &Q3DSTextNode::setLeading, &Q3DSTextNode::getLeading },
    { QLatin1String("Text"), QLatin1String("tracking"), &Q3DSTextNode::setTracking, &Q3DSTextNode::getTracking },

    { QLatin1String("Image"), QLatin1String("scaleu"), &Q3DSImage::setScaleU, &Q3DSImage::getScaleU },
    { QLatin1String("Image"), QLatin1String("scalev"), &Q3DSImage::setScaleV, &Q3DSImage::getScaleV },
    { QLatin1String("Image"), QLatin1String("rotationuv"), &Q3DSImage::setRotationUV, &Q3DSImage::getRotationUV },
    { QLatin1String("Image"), QLatin1String("positionu"), &Q3DSImage::setPositionU, &Q3DSImage::getPositionU },
    { QLatin1String("Image"), QLatin1String("positionv"), &Q3DSImage::setPositionV, &Q3DSImage::getPositionV },
    { QLatin1String("Image"), QLatin1String("pivotu"), &Q3DSImage::setPivotU, &Q3DSImage::getPivotU },
    { QLatin1String("Image"), QLatin1String("pivotv"), &Q3DSImage::setPivotV, &Q3DSImage::getPivotV },

    { QLatin1String("Layer"), QLatin1String("left"), &Q3DSLayerNode::setLeft, &Q3DSLayerNode::getLeft },
    { QLatin1String("Layer"), QLatin1String("right"), &Q3DSLayerNode::setRight, &Q3DSLayerNode::getRight },
    { QLatin1String("Layer"), QLatin1String("width"), &Q3DSLayerNode::setWidth, &Q3DSLayerNode::getWidth },
    { QLatin1String("Layer"), QLatin1String("height"), &Q3DSLayerNode::setHeight, &Q3DSLayerNode::getHeight },
    { QLatin1String("Layer"), QLatin1String("top"), &Q3DSLayerNode::setTop, &Q3DSLayerNode::getTop },
    { QLatin1String("Layer"), QLatin1String("bottom"), &Q3DSLayerNode::setBottom, &Q3DSLayerNode::getBottom },
    { QLatin1String("Layer"), QLatin1String("aostrength"), &Q3DSLayerNode::setAoStrength, &Q3DSLayerNode::getAoStrength },
    { QLatin1String("Layer"), QLatin1String("aodistance"), &Q3DSLayerNode::setAoDistance, &Q3DSLayerNode::getAoDistance },
    { QLatin1String("Layer"), QLatin1String("aosoftness"), &Q3DSLayerNode::setAoSoftness, &Q3DSLayerNode::getAoSoftness },
    { QLatin1String("Layer"), QLatin1String("aobias"), &Q3DSLayerNode::setAoBias, &Q3DSLayerNode::getAoBias },
    { QLatin1String("Layer"), QLatin1String("aosamplerate"), &Q3DSLayerNode::setAoSampleRate, &Q3DSLayerNode::getAoSampleRate },
    { QLatin1String("Layer"), QLatin1String("shadowstrength"), &Q3DSLayerNode::setShadowStrength, &Q3DSLayerNode::getShadowStrength },
    { QLatin1String("Layer"), QLatin1String("shadowdist"), &Q3DSLayerNode::setShadowDist, &Q3DSLayerNode::getShadowDist },
    { QLatin1String("Layer"), QLatin1String("shadowsoftness"), &Q3DSLayerNode::setShadowSoftness, &Q3DSLayerNode::getShadowSoftness },
    { QLatin1String("Layer"), QLatin1String("shadowbias"), &Q3DSLayerNode::setShadowBias, &Q3DSLayerNode::getShadowBias },
    { QLatin1String("Layer"), QLatin1String("probebright"), &Q3DSLayerNode::setProbeBright, &Q3DSLayerNode::getProbeBright },
    { QLatin1String("Layer"), QLatin1String("probehorizon"), &Q3DSLayerNode::setProbeHorizon, &Q3DSLayerNode::getProbeHorizon },
    { QLatin1String("Layer"), QLatin1String("probefov"), &Q3DSLayerNode::setProbeFov, &Q3DSLayerNode::getProbeFov },
    { QLatin1String("Layer"), QLatin1String("probe2fade"), &Q3DSLayerNode::setProbe2Fade, &Q3DSLayerNode::getProbe2Fade },
    { QLatin1String("Layer"), QLatin1String("probe2window"), &Q3DSLayerNode::setProbe2Window, &Q3DSLayerNode::getProbe2Window },
    { QLatin1String("Layer"), QLatin1String("probe2pos"), &Q3DSLayerNode::setProbe2Pos, &Q3DSLayerNode::getProbe2Pos }

    // CustomMaterial and Effect are not listed here since they are handled
    // differently. There all dynamic properties are assumed to be animatable
    // but they need to be discovered on demand, when the target object is known.
};

void Q3DSAnimationManager::gatherAnimatableMeta(const QString &type, AnimatableTab *dst)
{
    Q3DSDataModelParser *dataModelParser = Q3DSDataModelParser::instance();
    const QVector<Q3DSDataModelParser::Property> *propMeta = dataModelParser->propertiesForType(type);
    if (propMeta) {
        for (const Q3DSDataModelParser::Property &prop : *propMeta) {
            // Filter out the ones explicitly marked with animatable="False",
            // although there's still going to be many that will never get animated (enum, Image, etc. types).
            // There's a second filter below based on extraMeta anyways.
            if (prop.animatable) {
                Animatable a;
                a.name = prop.name;
                a.type = prop.type;
                a.componentCount = prop.componentCount;

                bool known = false;
                const size_t extraCount = sizeof(extraMeta) / sizeof(AnimatableExtraMeta);
                for (size_t i = 0; i < extraCount; ++i) {
                    if (extraMeta[i].type3DS == type && extraMeta[i].name3DS == a.name) {
                        a.setter = extraMeta[i].setter;
                        a.getter = extraMeta[i].getter;
                        known = true;
                        break;
                    }
                }

                if (known)
                    dst->insert(a.name, a);
            }
        }
    }
}

static void dynamicPropertySetter(Q3DSGraphObject *obj, const QVariant &value, const QString &name)
{
    QVariantMap *p = nullptr;
    if (obj->type() == Q3DSGraphObject::CustomMaterial) {
        Q3DSCustomMaterialInstance *mat3DS = static_cast<Q3DSCustomMaterialInstance *>(obj);
        p = mat3DS->materialPropertyValues();
    } else if (obj->type() == Q3DSGraphObject::Effect) {
        Q3DSEffectInstance *eff3DS = static_cast<Q3DSEffectInstance *>(obj);
        p = eff3DS->effectPropertyValues();
    }
    if (p) {
        Q_ASSERT(p->contains(name));
        (*p)[name] = value;
    }
}

static QVariant dynamicPropertyGetter(Q3DSGraphObject *obj, const QString &name)
{
    const QVariantMap *p = nullptr;
    if (obj->type() == Q3DSGraphObject::CustomMaterial) {
        Q3DSCustomMaterialInstance *mat3DS = static_cast<Q3DSCustomMaterialInstance *>(obj);
        p = mat3DS->materialPropertyValues();
    } else if (obj->type() == Q3DSGraphObject::Effect) {
        Q3DSEffectInstance *eff3DS = static_cast<Q3DSEffectInstance *>(obj);
        p = eff3DS->effectPropertyValues();
    }
    return p ? p->value(name) : QVariant();
}

void Q3DSAnimationManager::gatherDynamicProperties(const QVariantMap *dynProps,
                                                   const QMap<QString, Q3DSMaterial::PropertyElement> &propMeta,
                                                   AnimatableTab *dst)
{
    // dynProps is the name - value map with the actual (or the default) values for the custom material or effect instance
    // propMeta is the metadata for all the dynamic properties of the custom material or effect
    for (auto propIt = dynProps->cbegin(), propItEnd = dynProps->cend(); propIt != propItEnd; ++propIt) {
        Animatable a;
        a.name = propIt.key();
        Q_ASSERT(propMeta.contains(a.name));
        Q3DSMaterial::PropertyElement prop = propMeta.value(a.name);
        a.type = prop.type;
        a.componentCount = prop.componentCount;
        // Have generic setter/getter implementations (this is possible
        // due to the 3rd/2nd QString parameter in the signatures).
        a.setter = dynamicPropertySetter;
        a.getter = dynamicPropertyGetter;
        dst->insert(a.name, a);
    }
}

class Q3DSAnimationCallback : public Qt3DAnimation::QAnimationCallback
{
public:
    Q3DSAnimationCallback(Q3DSGraphObject *target, const Q3DSAnimationManager::Animatable &animMeta, Q3DSAnimationManager *manager)
        : m_target(target), m_animMeta(animMeta), m_animationManager(manager) { }

    void valueChanged(const QVariant &value) override;

private:
    Q3DSGraphObject *m_target;
    Q3DSAnimationManager::Animatable m_animMeta;
    Q3DSAnimationManager *m_animationManager;
};

void Q3DSAnimationCallback::valueChanged(const QVariant &value)
{
    // Do not directly change the value and trigger change notifications.
    // Instead, queue up (and compress), and defer to applyChanges() which is
    // invoked once per frame.

    Q3DSAnimationManager::AnimationValueChange change;
    change.value = value;
    change.name = m_animMeta.name;
    change.setter = m_animMeta.setter;
    m_animationManager->queueChange(m_target, change);
}

static int componentSuffixToIndex(const QString &s)
{
    if (s == QStringLiteral("x"))
        return 0;
    else if (s == QStringLiteral("y"))
        return 1;
    else if (s == QStringLiteral("z"))
        return 2;

    return -1;
}

template<class T>
void Q3DSAnimationManager::updateAnimationHelper(const AnimationTrackListMap<T *> &targets,
                                                 AnimatableTab *animatables,
                                                 Q3DSSlide *slide)
{
    for (auto it = targets.cbegin(), ite = targets.cend(); it != ite; ++it) {
        T *target = it.key();
        Q3DSGraphObjectAttached *data = target->attached();
        Q_ASSERT(data);
        const auto initAnimator = [this](Q3DSGraphObjectAttached *data, Q3DSSlide *slide) {
            static const bool animDebug = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 2;
            if (animDebug)
                qCDebug(lcScene) << "initAnimator@:" << data << "animationData:" << data->animationDataMap[slide]
                                    << "slide:" << slide->name();

            Q_ASSERT(data->entity);

            Qt3DAnimation::QClipAnimator *animator = new Qt3DAnimation::QClipAnimator;
            animator->setClock(new Qt3DAnimation::QClock);

            Q3DSGraphObjectAttached::AnimationData *animationData = data->animationDataMap.value(slide);
            Q_ASSERT(!animationData);
            animationData = new Q3DSGraphObjectAttached::AnimationData;
            data->animationDataMap.insert(slide, animationData);

            Q3DSSlideAttached *slideAttached = static_cast<Q3DSSlideAttached *>(slide->attached());
            slideAttached->animators.append(animator);
            if (animDebug)
                qCDebug(lcScene) << "\tnew Clip Animator: " << animator << "slideAttached: "
                                 << slideAttached << "new list: " << slideAttached->animators;

            return animator;
        };

        Qt3DAnimation::QClipAnimator *animator = initAnimator(data, slide);

        QScopedPointer<Qt3DAnimation::QAnimationClip> clip(new Qt3DAnimation::QAnimationClip);
        QScopedPointer<Qt3DAnimation::QChannelMapper> mapper(new Qt3DAnimation::QChannelMapper);
        Qt3DAnimation::QAnimationClipData clipData;

        // Break down the following input (which is not guaranteed to be sorted
        // based on 'property') into a table with a key of the property name
        // and a value of ordered (x,y,z -> 0,1,2) QChannelComponents with the
        // keyframes added as-is.
        //
        //        <AnimationTrack property="position.x" type="EaseInOut" >0 0 100 100 4.64 300 100 100 10 305.361 100 100</AnimationTrack>
        //        <AnimationTrack property="position.y" type="EaseInOut" >0 0 100 100 4.64 0 100 100 10 -96.8999 100 100</AnimationTrack>
        //        <AnimationTrack property="position.z" type="EaseInOut" >0 0 100 100 4.64 0 100 100 10 -56.6207 100 100</AnimationTrack>
        //        <AnimationTrack property="rotation.x" type="EaseInOut" >0 -52.4901 100 100 6.544 -52.4901 100 100 10 -52.4901 100 100</AnimationTrack>
        //        <AnimationTrack property="rotation.y" type="EaseInOut" >0 -200 100 100 6.544 -200 100 100 10 -10.7851 100 100</AnimationTrack>
        //        <AnimationTrack property="rotation.z" type="EaseInOut" >0 -20.7213 100 100 6.544 -20.7213 100 100 10 -20.7213 100 100</AnimationTrack>

        struct ChannelComponents {
            Animatable meta;
            Qt3DAnimation::QChannelComponent comps[4];
            Qt3DAnimation::QChannel channel;
        };
        QHash<QString, ChannelComponents> channelData;

        for (const Q3DSAnimationTrack *animTrack : it.value()) {
            const QStringList prop = animTrack->property().split('.');
            if (prop.count() < 1)
                continue;
            const QString &propertyName = prop[0];
            if (!animatables->contains(propertyName))
                continue;

            Animatable *animMeta = &(*animatables)[propertyName];
            ChannelComponents &c(channelData[animMeta->name]);
            c.meta = *animMeta;

            for (const Q3DSAnimationTrack::KeyFrame &kf : *animTrack->keyFrames()) {
                Qt3DAnimation::QKeyFrame qkf;
#if 0 // ### not supported yet, fix Qt3D first
                switch (animTrack->type()) {
                case Q3DSAnimationTrack::EaseInOut:
                    qkf = Qt3DAnimation::QKeyFrame(QVector2D(kf.time, kf.value), kf.easeIn / 100.0f, kf.easeOut / 100.0f);
                    break;
                case Q3DSAnimationTrack::Bezier:
                    qkf = Qt3DAnimation::QKeyFrame(QVector2D(kf.time, kf.value),
                                                   QVector2D(kf.c1time, kf.c1value / 100.0f),
                                                   QVector2D(kf.c2time, kf.c2value / 100.0f));
                    break;

                default: // linear interpolation
                    qkf = Qt3DAnimation::QKeyFrame(QVector2D(kf.time, kf.value));
                    break;
                }
#else
                qkf = Qt3DAnimation::QKeyFrame(QVector2D(kf.time, kf.value));
#endif
                if (prop.count() == 1) {
                    c.comps[0].appendKeyFrame(qkf);
                } else {
                    int idx = componentSuffixToIndex(prop[1]);
                    if (idx < 0) {
                        qWarning("Unknown component suffix %s for animated property %s", qPrintable(prop[1]), qPrintable(propertyName));
                        continue;
                    }
                    c.comps[idx].appendKeyFrame(qkf);
                }
            }
        }

        static const bool animSetupDebug = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 2;

        for (auto chIt = channelData.begin(), chItEnd = channelData.end(); chIt != chItEnd; ++chIt) {
            const QString channelName = chIt.key(); // == chIt->meta.name
            if (animSetupDebug)
                qDebug() << target << target->id() << "channel" << channelName;

            // Now a QChannel can be created.
            chIt->channel.setName(channelName);

            // Channels must be fully specified. Fortunately the uip
            // documents seem to fulfill this criteria and all 3
            // components are present always.
            for (int i = 0; i < chIt->meta.componentCount; ++i) {
                // Leave the component name unset. This way Qt3D will not waste
                // time on string comparisons for figuring out the right index
                // (e.g. 1) for e.g. QChannelComponent("BlahBlah Y"), but uses
                // the component's index (which is already correct) as-is.
                chIt->channel.appendChannelComponent(chIt->comps[i]);

                if (animSetupDebug) {
                    qDebug() << "  channel component" << i;
                    for (auto kit = chIt->comps[i].cbegin(); kit != chIt->comps[i].cend(); ++kit)
                        qDebug() << "    " << kit->coordinates();
                }
            }

            clipData.appendChannel(chIt->channel);

            // Figure out the QVariant/QMetaType type enum value.
            const int type = Q3DS::animatablePropertyTypeToMetaType(chIt->meta.type);
            if (type == QVariant::Invalid) {
                qWarning("Cannot map channel type for animated property %s", qPrintable(channelName));
                continue;
            }

            // Create a mapping with a custom callback.
            QScopedPointer<Qt3DAnimation::QCallbackMapping> mapping(new Qt3DAnimation::QCallbackMapping);
            mapping->setChannelName(channelName);
            Q3DSAnimationCallback *cb = new Q3DSAnimationCallback(target, chIt->meta, this);
            data->animationDataMap[slide]->animationCallbacks.append(cb);
            mapping->setCallback(type, cb, 0);
            mapper->addMapping(mapping.take());

            // Save the current value of the animated property.
            if (chIt->meta.getter) {
                Q3DSGraphObjectAttached::AnimatedValueRollbackData rd;
                rd.obj = target;
                rd.name = chIt->meta.name;
                rd.value = chIt->meta.getter(target, chIt->meta.name);
                rd.setter = chIt->meta.setter;
                data->animationRollbacks.append(rd);
            }
        }

        Q_ASSERT(animator);
        animator->setChannelMapper(mapper.take());
        clip->setClipData(clipData);
        animator->setClip(clip.take());
        slide->attached()->entity->addComponent(animator);
    }
}

class DummyCallback : public Qt3DAnimation::QAnimationCallback
{
public:
    void valueChanged(const QVariant &value) override { Q_UNUSED(value); }
};

void Q3DSAnimationManager::clearAnimations(Q3DSSlide *slide)
{
    qCDebug(lcScene, "Clearing animations for slide (%s)", qPrintable(slide->name()));

    // Clear the old slide animator
    static const auto clearSlideAnimator = [](Q3DSSlide *slide) {
        Q_ASSERT(slide);
        Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
        auto animator = data->animator;
        if (animator) {
            Q_ASSERT(!animator->isRunning());
            slide->attached()->entity->removeComponent(animator);
            delete animator;
            data->animator = nullptr;
        }
    };

    if (slide)
        clearSlideAnimator(slide);

    Q3DSSlide *masterSlide = static_cast<Q3DSSlide *>(slide->parent());

    const bool hasAnimationData = !slide->animations()->isEmpty()
            || !(masterSlide && masterSlide->animations()->isEmpty());

    if (!hasAnimationData)
        return;

    const auto clearAndRollback = [this](const QVector<Q3DSAnimationTrack> *anims, Q3DSSlide *slide) {
        // Rollback properties
        for (const Q3DSAnimationTrack &track : *anims) {
            Q3DSGraphObjectAttached *data = track.target()->attached();
            Q3DSGraphObjectAttached::AnimationData *animationData = data->animationDataMap.value(slide);
            if (animationData) {
                // Properties that were animated before have to be reset to their
                // original value, otherwise things will flicker when switching between
                // slides since the animations we build may not update the first value
                // in time for the next frame.
                if (!data->animationRollbacks.isEmpty()) {
                    for (const auto &rd : qAsConst(data->animationRollbacks)) {
                        Q3DSAnimationManager::AnimationValueChange change;
                        change.value = rd.value;
                        change.name = rd.name;
                        change.setter = rd.setter;
                        queueChange(rd.obj, change);
                    }
                    // Set the values right away, do not wait until the next frame.
                    // This is important since updateAnimations() may query some of the
                    // now-restored values from the object.
                    applyChanges();
                }
                // Cleanup previous animation callbacks
                qDeleteAll(animationData->animationCallbacks);
                animationData->animationCallbacks.clear();
                data->animationDataMap.remove(slide);
                delete animationData;
            }
        }

        // Remove all other animatiors that was associated with this slide
        Q3DSSlideAttached *slideAttached = static_cast<Q3DSSlideAttached *>(slide->attached());
        for (auto animator : slideAttached->animators) {
            Q_ASSERT(!animator->isRunning());
            slideAttached->entity->removeComponent(animator);
            delete animator;
        }

        slideAttached->animators.clear();
        Q_ASSERT(slideAttached->animators.isEmpty());
    };

    // NOTE: The second argument is the slide the animations are associated with, which
    // we set to be the "current" slide in updateAnimations().
    clearAndRollback(masterSlide->animations(), slide);
    clearAndRollback(slide->animations(), slide);
}

// Dummy animator for keeping track of the time line for the current slide
static void buildClipAnimator(Q3DSSlide *slide)
{
    using namespace Qt3DAnimation;

    // Get the first start time and the last endtime of all layers in the slide
    // TODO: Make sure we get the correct time
    float startTime = 0.0f;
    float endTime = 0.0f;
    bool found = false;
    for (Q3DSGraphObject *obj : slide->propertyChanges()->keys()) {
        if (obj->type() == Q3DSGraphObject::Layer) {
            if (startTime < obj->startTime()) {
                found = true;
                startTime = obj->startTime();
            }
            if (endTime < obj->endTime()) {
                found = true;
                endTime = obj->endTime();
            }
        }
    }

    if (!found) { // Fallback to using the time set on the slide
        startTime = slide->startTime();
        endTime = slide->endTime();
    }

    QClipAnimator *animator = new QClipAnimator;
    animator->setClock(new QClock);

    QAnimationClip *clip = new QAnimationClip;
    QAnimationClipData clipData;

    const QString channelName = slide->name() + QLatin1String("_timeDummy");
    QChannel channel(channelName);
    QChannelComponent component;
    QChannelMapper *mapper = new QChannelMapper;
    QCallbackMapping *mapping = new QCallbackMapping;
    mapping->setChannelName(channelName);
    mapping->setCallback(QMetaType::Float, new DummyCallback, 0);
    mapper->addMapping(mapping);
    animator->setChannelMapper(mapper);
    // TODO: We could just use this to get the time values directly...
    QKeyFrame keyFrameStart(QVector2D(startTime / 1000.0f, 0.0f));
    QKeyFrame keyFrameEnd(QVector2D(endTime / 1000.0f, endTime / 1000.0f));
    component.appendKeyFrame(keyFrameStart);
    component.appendKeyFrame(keyFrameEnd);
    channel.appendChannelComponent(component);
    clipData.appendChannel(channel);

    clip->setClipData(clipData);
    animator->setClip(clip);

    Q3DSSlideAttached *data = slide->attached<Q3DSSlideAttached>();
    Q_ASSERT(data->animator == nullptr);
    data->animator = animator;
    data->entity->addComponent(animator);
}

template <typename T>
static void insertTrack(T &trackList, const Q3DSAnimationTrack &animTrack, bool overWrite)
{
    if (overWrite) {
        auto it = std::find_if(trackList.begin(), trackList.end(), [&animTrack](const Q3DSAnimationTrack *track) {
            return (animTrack.property() == track->property());
        });

        if (it != trackList.end())
            *it = &animTrack;
        else
            trackList.append(&animTrack);
    } else {
        trackList.append(&animTrack);
    }
}

void Q3DSAnimationManager::updateAnimations(Q3DSSlide *slide)
{
    Q_ASSERT(slide);

    qCDebug(lcScene, "Updating animations for slide (%s)", qPrintable(slide->name()));

    Q3DSSlide *masterSlide = static_cast<Q3DSSlide *>(slide->parent());

    const bool hasAnimationData = !slide->animations()->isEmpty()
            || !(masterSlide && masterSlide->animations()->isEmpty());

    if (!hasAnimationData)
        return;

    buildClipAnimator(slide);

    AnimationTrackListMap<Q3DSDefaultMaterial *> defMatAnims;
    AnimationTrackListMap<Q3DSCustomMaterialInstance *> customMatAnims;
    AnimationTrackListMap<Q3DSEffectInstance *> effectAnims;
    AnimationTrackListMap<Q3DSCameraNode *> camAnims;
    AnimationTrackListMap<Q3DSLightNode *> lightAnims;
    AnimationTrackListMap<Q3DSModelNode *> modelAnims;
    AnimationTrackListMap<Q3DSGroupNode *> groupAnims;
    AnimationTrackListMap<Q3DSComponentNode *> compAnims;
    AnimationTrackListMap<Q3DSTextNode *> textAnims;
    AnimationTrackListMap<Q3DSImage *> imageAnims;
    AnimationTrackListMap<Q3DSLayerNode *> layerAnims;

    const auto buildTrackListMap = [&](Q3DSSlide *slide, bool overwrite) {
        if (!slide)
            return;

        const QVector<Q3DSAnimationTrack> &anims = *slide->animations();
        for (const Q3DSAnimationTrack &animTrack : anims) {
            Q3DSGraphObject *target = animTrack.target();
            switch (target->type()) {
            case Q3DSGraphObject::DefaultMaterial:
            {
                Q3DSDefaultMaterial *mat = static_cast<Q3DSDefaultMaterial *>(target);
                insertTrack(defMatAnims[mat], animTrack, overwrite);
              }
                break;
            case Q3DSGraphObject::CustomMaterial:
            {
                Q3DSCustomMaterialInstance *mat = static_cast<Q3DSCustomMaterialInstance *>(target);
                insertTrack(customMatAnims[mat], animTrack, overwrite);
            }
                break;
            case Q3DSGraphObject::Effect:
            {
                Q3DSEffectInstance *mat = static_cast<Q3DSEffectInstance *>(target);
                insertTrack(effectAnims[mat], animTrack, overwrite);
            }
                break;
            case Q3DSGraphObject::Camera:
            {
                Q3DSCameraNode *cam3DS = static_cast<Q3DSCameraNode *>(animTrack.target());
                insertTrack(camAnims[cam3DS], animTrack, overwrite);
            }
                break;
            case Q3DSGraphObject::Light:
            {
                Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(animTrack.target());
                insertTrack(lightAnims[light3DS], animTrack, overwrite);
            }
                break;
            case Q3DSGraphObject::Model:
            {
                Q3DSModelNode *model3DS = static_cast<Q3DSModelNode *>(animTrack.target());
                insertTrack(modelAnims[model3DS], animTrack, overwrite);
            }
                break;
            case Q3DSGraphObject::Group:
            {
                Q3DSGroupNode *group3DS = static_cast<Q3DSGroupNode *>(animTrack.target());
                insertTrack(groupAnims[group3DS], animTrack, overwrite);
            }
                break;
            case Q3DSGraphObject::Component:
            {
                Q3DSComponentNode *comp3DS = static_cast<Q3DSComponentNode *>(animTrack.target());
                insertTrack(compAnims[comp3DS], animTrack, overwrite);
            }
                break;
            case Q3DSGraphObject::Text:
            {
                Q3DSTextNode *text3DS = static_cast<Q3DSTextNode *>(animTrack.target());
                insertTrack(textAnims[text3DS], animTrack, overwrite);
            }
                break;
            case Q3DSGraphObject::Image:
            {
                Q3DSImage *image3DS = static_cast<Q3DSImage *>(animTrack.target());
                insertTrack(imageAnims[image3DS], animTrack, overwrite);
            }
                break;
            case Q3DSGraphObject::Layer:
            {
                Q3DSLayerNode *layer3DS = static_cast<Q3DSLayerNode *>(animTrack.target());
                insertTrack(layerAnims[layer3DS], animTrack, overwrite);
            }
                break;
            default:
                break;
            }
        }
    };

    // Build the animation track list from the master slide first,
    // then overwrite any tracks that are also on the current slide.
    buildTrackListMap(masterSlide, false);
    buildTrackListMap(slide, true);

    qCDebug(lcScene, "Slide %s has %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d animated objects", slide->id().constData(),
            defMatAnims.count(), customMatAnims.count(), effectAnims.count(), camAnims.count(), lightAnims.count(), modelAnims.count(), groupAnims.count(),
            compAnims.count(), textAnims.count(), imageAnims.count(), layerAnims.count());

    if (m_defaultMaterialAnimatables.isEmpty())
        gatherAnimatableMeta(QLatin1String("Material"), &m_defaultMaterialAnimatables);

    updateAnimationHelper(defMatAnims, &m_defaultMaterialAnimatables, slide);

    if (m_cameraAnimatables.isEmpty()) {
        gatherAnimatableMeta(QLatin1String("Node"), &m_cameraAnimatables);
        gatherAnimatableMeta(QLatin1String("Camera"), &m_cameraAnimatables);
    }

    updateAnimationHelper(camAnims, &m_cameraAnimatables, slide);

    if (m_lightAnimatables.isEmpty()) {
        gatherAnimatableMeta(QLatin1String("Node"), &m_lightAnimatables);
        gatherAnimatableMeta(QLatin1String("Light"), &m_lightAnimatables);
    }

    updateAnimationHelper(lightAnims, &m_lightAnimatables, slide);

    if (m_modelAnimatables.isEmpty()) {
        gatherAnimatableMeta(QLatin1String("Node"), &m_modelAnimatables);
        gatherAnimatableMeta(QLatin1String("Model"), &m_modelAnimatables);
    }

    updateAnimationHelper(modelAnims, &m_modelAnimatables, slide);

    if (m_groupAnimatables.isEmpty()) {
        gatherAnimatableMeta(QLatin1String("Node"), &m_groupAnimatables);
        gatherAnimatableMeta(QLatin1String("Group"), &m_groupAnimatables);
    }

    updateAnimationHelper(groupAnims, &m_groupAnimatables, slide);

    if (m_componentAnimatables.isEmpty())
        gatherAnimatableMeta(QLatin1String("Node"), &m_componentAnimatables);

    updateAnimationHelper(compAnims, &m_componentAnimatables, slide);

    if (m_textAnimatables.isEmpty()) {
        gatherAnimatableMeta(QLatin1String("Node"), &m_textAnimatables);
        gatherAnimatableMeta(QLatin1String("Text"), &m_textAnimatables);
    }

    updateAnimationHelper(textAnims, &m_textAnimatables, slide);

    if (m_imageAnimatables.isEmpty())
        gatherAnimatableMeta(QLatin1String("Image"), &m_imageAnimatables);

    updateAnimationHelper(imageAnims, &m_imageAnimatables, slide);

    if (m_layerAnimatables.isEmpty())
        gatherAnimatableMeta(QLatin1String("Layer"), &m_layerAnimatables);

    updateAnimationHelper(layerAnims, &m_layerAnimatables, slide);

    // custom materials and effects need special handling due to their dynamic properties
    if (!customMatAnims.isEmpty()) {
        AnimatableTab customMaterialAnimatables;
        for (auto it = customMatAnims.cbegin(), itEnd = customMatAnims.cend(); it != itEnd; ++it) {
            Q3DSCustomMaterialInstance *mat3DS = it.key();
            gatherDynamicProperties(mat3DS->materialPropertyValues(), mat3DS->material()->properties(), &customMaterialAnimatables);
        }
        updateAnimationHelper(customMatAnims, &customMaterialAnimatables, slide);
    }
    if (!effectAnims.isEmpty()) {
        AnimatableTab effectAnimatables;
        for (auto it = effectAnims.cbegin(), itEnd = effectAnims.cend(); it != itEnd; ++it) {
            Q3DSEffectInstance *eff3DS = it.key();
            gatherDynamicProperties(eff3DS->effectPropertyValues(), eff3DS->effect()->properties(), &effectAnimatables);
        }
        updateAnimationHelper(effectAnims, &effectAnimatables, slide);
    }
}

QDebug operator<<(QDebug dbg, const Q3DSAnimationManager::Animatable &a)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "Animatable(" << a.name << ' ' << a.type << ' ' << a.componentCount << " )";
    return dbg;
}

void Q3DSAnimationManager::applyChanges()
{
    // Expected to be called once per frame (or in special cases, like when
    // initializing animations for an object with previously animated values).
    // One target can have more than one property change. These get compressed
    // so that there is still only one notifyPropertyChanges() call per object.

    static const bool animDebug = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 3;
    const QList<Q3DSGraphObject *> keys = m_changes.uniqueKeys();
    for (Q3DSGraphObject *target : keys) {
        auto it = m_changes.find(target);
        Q3DSPropertyChangeList changeList;
        while (it != m_changes.cend() && it.key() == target) {
            if (Q_UNLIKELY(animDebug))
                qDebug() << "animate:" << target->id() << it->name << it->value;
            it->setter(target, it->value, it->name);
            changeList.append(Q3DSPropertyChange(it->name));
            ++it;
        }
        if (!changeList.isEmpty())
            target->notifyPropertyChanges(&changeList);
    }
    m_changes.clear();
}

void Q3DSAnimationManager::clearPendingChanges()
{
    m_changes.clear();
}

void Q3DSAnimationManager::queueChange(Q3DSGraphObject *target, const AnimationValueChange &change)
{
    m_changes.insert(target, change);
}

QT_END_NAMESPACE
