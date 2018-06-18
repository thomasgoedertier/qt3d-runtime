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
#include "q3dsslideplayer_p.h"
#include "q3dslogging_p.h"

#include <QLoggingCategory>

#include <Qt3DCore/QEntity>

#include <Qt3DAnimation/QClipAnimator>
#include <Qt3DAnimation/QChannelMapper>
#include <Qt3DAnimation/QChannelMapping>
#include <Qt3DAnimation/QCallbackMapping>
#include <Qt3DAnimation/QAnimationClip>
#include <Qt3DAnimation/qclock.h>

QT_BEGIN_NAMESPACE

class Q3DSAnimationCallback : public Qt3DAnimation::QAnimationCallback
{
public:
    Q3DSAnimationCallback(Q3DSGraphObject *target,
                          Q3DSAnimationManager *manager,
                          const QMetaProperty &property,
                          const QString &propertyName,
                          QVariant::Type type)
        : m_target(target),
          m_animationManager(manager),
          m_property(property),
          m_propertyName(propertyName),
          m_type(type) { }

    void valueChanged(const QVariant &value) override;

private:
    static QVariant stabilizeAnimatedValue(const QVariant &value, QVariant::Type type);

    Q3DSGraphObject *m_target;
    Q3DSAnimationManager *m_animationManager;
    QMetaProperty m_property;
    QString m_propertyName;
    QVariant::Type m_type;
};

void Q3DSAnimationCallback::valueChanged(const QVariant &value)
{
    // Do not directly change the value and trigger change notifications.
    // Instead, queue up (and compress), and defer to applyChanges() which is
    // invoked once per frame.

    Q3DSAnimationManager::AnimationValueChange change;
    // Don't use the property type/name directly as it might be a dynamic type, i.e., a QVariantMap
    change.value = stabilizeAnimatedValue(value, m_type);
    change.property = m_property;
    change.propertyName = m_propertyName;
    m_animationManager->queueChange(m_target, change);
}

QVariant Q3DSAnimationCallback::stabilizeAnimatedValue(const QVariant &value, QVariant::Type type)
{
    if (type == QVariant::Color && value.type() == QVariant::Vector3D) {
        const QVector3D v = value.value<QVector3D>();
        // rgb is already in range [0, 1] but let's make sure the 0 and 1 are really 0 and 1.
        // This avoids confusing QColor when qFuzzyCompare(r, 1.0f) && r > 1.0f
        const float r = qBound(0.0f, v.x(), 1.0f);
        const float g = qBound(0.0f, v.y(), 1.0f);
        const float b = qBound(0.0f, v.z(), 1.0f);
        return QVariant::fromValue(QColor::fromRgbF(qreal(r), qreal(g), qreal(b)));
    }

    return value;
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

void Q3DSAnimationManager::updateAnimationHelper(const AnimationTrackListMap &targets,
                                                 Q3DSSlide *slide,
                                                 bool editorMode)
{
    for (auto it = targets.cbegin(), ite = targets.cend(); it != ite; ++it) {
        Q3DSGraphObject *target = it.key();
        Q3DSGraphObjectAttached *data = target->attached();
        Q_ASSERT(data);
        m_activeTargets.insert(target);

        static const auto initAnimator = [](Q3DSGraphObjectAttached *data, Q3DSSlide *slide) {
            static const bool animDebug = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 2;
            if (animDebug)
                qCDebug(lcAnim) << "initAnimator@:" << data << "animationData:" << data->animationDataMap[slide]
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
                qCDebug(lcAnim) << "\tnew Clip Animator: " << animator << "slideAttached: "
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
            QMetaProperty property;
            Qt3DAnimation::QChannelComponent comps[4];
            Qt3DAnimation::QChannel channel;
            // Type and name will be the real type if it's a dynamic property
            QVariant::Type propertyType = QVariant::Invalid;
            bool dynamicTrack = false;
            bool dynamicProperty = false;
        };
        QHash<QString, ChannelComponents> channelData;

        static const auto buildKeyFrames = [](const Q3DSAnimationTrack::KeyFrameList &keyFrames,
                                              Q3DSAnimationTrack::AnimationType type,
                                              ChannelComponents &channelComponent,
                                              const QStringList &prop) {

            const auto end = keyFrames.constEnd();
            const auto begin = keyFrames.constBegin();
            auto it = begin;
            Qt3DAnimation::QKeyFrame qkf;
            while (it != end) {
                switch (type) {
                case Q3DSAnimationTrack::EaseInOut:
                {
                    // c1 (t, v) -> first/right control point (ease in).
                    // Easing value (for t) is between 0 and 1, where 0 is the current keyframe's start time
                    // and 1 is the next keyframe's start time.
                    // c1's value is always the same as the current keyframe's value, as that's the
                    // only option we support at the moment.

                    // c2 (t, v) -> second/left control point (ease out).
                    // Easing value (for t) is between 0 and 1, where 0 is the next keyframe's start time
                    // and 1 is the current keyframe's start time.
                    // c2's value is always the same as the next keyframe's value, as that's the only
                    // option we support at the moment.

                    // Get normalized value [0..1]
                    const float easeIn = qBound(0.0f, (it->easeIn / 100.0f), 1.0f);
                    const float easeOut = qBound(0.0f, (it->easeOut / 100.0f), 1.0f);

                    // Next and previous keyframes, if any.
                    const auto next = ((it + 1) != end) ? (it + 1) : it;
                    const auto previous = (it != begin) ? (it - 1) : it;

                    // Adjustment to the easing values, to limit the range of the control points,
                    // so we get the same "smooth" easing curves as in Studio 1.0
                    static const float adjustment = 1.0f / 3.0f;

                    // p0
                    const QVector2D coordinates(it->time, it->value);

                    // c1
                    float dt = (next->time - it->time);
                    const float p1t = qBound(it->time, it->time + (dt * easeIn * adjustment), next->time);
                    const float p1v = it->value;
                    const QVector2D rightControlPoint(p1t, p1v);

                    // c2
                    dt = (it->time - previous->time);
                    const float p2t = qBound(previous->time, it->time - (dt * easeOut * adjustment), it->time);
                    const float p2v = it->value;
                    const QVector2D leftControlPoint(p2t, p2v);

                    qkf = Qt3DAnimation::QKeyFrame(coordinates, leftControlPoint, rightControlPoint);
                }
                    break;
                case Q3DSAnimationTrack::Bezier:
                    qkf = Qt3DAnimation::QKeyFrame(QVector2D(it->time, it->value),
                                                   QVector2D(it->c1time, it->c1value / 100.0f),
                                                   QVector2D(it->c2time, it->c2value / 100.0f));
                    break;
                default:
                    qkf = Qt3DAnimation::QKeyFrame(QVector2D(it->time, it->value));
                    break;
                }

                if (prop.count() == 1) {
                    channelComponent.comps[0].appendKeyFrame(qkf);
                } else {
                    int idx = componentSuffixToIndex(prop[1]);
                    if (idx < 0) {
                        qWarning("Unknown component suffix %s for animated property %s", qPrintable(prop[1]), qPrintable(prop[0]));
                        continue;
                    }
                    channelComponent.comps[idx].appendKeyFrame(qkf);
                }
                ++it;
            }
        };

        static const auto updateDynamicKeyFrame = [](Q3DSAnimationTrack::KeyFrame &keyFrame,
                                                     Q3DSGraphObject *target,
                                                     const QStringList &prop) {
            qCDebug(lcAnim, "Building dynamic key-frame for %s's property %s", target->id().constData(), qPrintable(prop[0]));
            const QVariant value = target->property(prop[0].toLatin1());
            if (!value.isValid())
                return;

            if (prop.count() == 1) {
                keyFrame.value = value.toFloat();
            } else {
                qreal x = 0.0, y = 0.0, z = 0.0;
                if (value.type() == QVariant::Color) {
                    qvariant_cast<QColor>(value).getRgbF(&x, &y, &z);
                } else {
                    x = qreal(qvariant_cast<QVector3D>(value)[0]);
                    y = qreal(qvariant_cast<QVector3D>(value)[1]);
                    z = qreal(qvariant_cast<QVector3D>(value)[2]);
                }
                if (prop[1] == QString::fromLatin1("x"))
                    keyFrame.value = float(x);
                else if (prop[1] == QString::fromLatin1("y"))
                    keyFrame.value = float(y);
                else if (prop[1] == QString::fromLatin1("z"))
                    keyFrame.value = float(z);
            }
        };

        const auto &animatonTracks = it.value();
        for (const Q3DSAnimationTrack *animationTrack : animatonTracks) {
            const QStringList prop = animationTrack->property().split('.');
            if (prop.count() < 1)
                continue;

            const QString &propertyName = prop[0];
            ChannelComponents &channelComponent(channelData[propertyName]);
            int idx = target->metaObject()->indexOfProperty(propertyName.toLatin1().constData());

            // No property found on object, so check if it's a dynamic property...
            if (idx == -1) {
                const auto properties = target->dynamicPropertyNames();
                idx = properties.indexOf(propertyName.toLatin1());
                if (idx != -1) {
                    channelComponent.propertyType = target->dynamicPropertyValues().at(idx).type();
                    channelComponent.dynamicProperty = true;
                    // Adjust the property idx, as we'll use it to get the meta-method
                    // we're using for this property further down
                    idx = target->metaObject()->indexOfProperty("dynamicProperties");
                    Q_ASSERT(idx != -1);
                }
            }

            if (idx == -1)
                continue;

            auto keyFrames = animationTrack->keyFrames();
            if (keyFrames.isEmpty())
                continue;

            const QMetaProperty property = target->metaObject()->property(idx);
            Q_ASSERT(property.isWritable());

            channelComponent.property = property;
            // Keep the property type in sync
            if (!channelComponent.dynamicProperty)
                channelComponent.propertyType = property.type();
            channelComponent.dynamicTrack = animationTrack->isDynamic();

            const auto type = animationTrack->type();
            const bool isDynamic = animationTrack->isDynamic() && !editorMode;
            // If track is marked as dynamic, update the first keyframe so it interpolates from
            // the current position to the next.
            if (isDynamic)
                updateDynamicKeyFrame(keyFrames[0], target, prop);
            buildKeyFrames(keyFrames, type, channelComponent, prop);
        }

        static const bool animSetupDebug = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 2;

        for (auto chIt = channelData.begin(), chItEnd = channelData.end(); chIt != chItEnd; ++chIt) {
            const QString channelName = chIt.key(); // == chIt->meta.name
            if (animSetupDebug)
                qDebug() << target << target->id() << "channel" << channelName;

            // Now a QChannel can be created.
            chIt->channel.setName(channelName);

            QVariant::Type type = chIt->propertyType;
            const int componentCount = [type]() {
                switch (type) {
                case QVariant::Vector2D:
                    return 2;
                case QVariant::Vector3D:
                    return 3;
                case QVariant::Color:
                    return 3;
                default:
                    return 1;
                }
            }();
            for (int i = 0; i < componentCount; ++i) {
                // Leave the component name unset. This way Qt3D will not waste
                // time on string comparisons for figuring out the right index
                // (e.g. 1) for e.g. QChannelComponent("BlahBlah Y"), but uses
                // the component's index (which is already correct) as-is.

                // Channels must be fully specified. The uip documents fulfill
                // this criteria and all (1 or 3) components are present
                // always, but programmatically created animations may lack
                // this. Add a keyframe at 0 with value 0 since this is still
                // better than asserting in Qt3D.
                if (chIt->comps[i].keyFrameCount() == 0) {
                    if (animSetupDebug)
                        qDebug() << "  channel component" << i << "has no keyframes; adding dummy one";
                    chIt->comps[i].appendKeyFrame(Qt3DAnimation::QKeyFrame(QVector2D(0, 0)));
                }

                chIt->channel.appendChannelComponent(chIt->comps[i]);

                if (animSetupDebug) {
                    qDebug() << "  channel component" << i;
                    for (auto kit = chIt->comps[i].cbegin(); kit != chIt->comps[i].cend(); ++kit)
                        qDebug() << "    " << kit->coordinates();
                }
            }

            clipData.appendChannel(chIt->channel);

            // Figure out the QVariant/QMetaType type enum value.
            if (type == QVariant::Invalid) {
                qWarning("Cannot map channel type for animated property %s", qPrintable(channelName));
                continue;
            }

            // Workaround for QColor::fromRgbF() warning and generating invalid
            // colors when some component is very slightly over 1.0. Due to the
            // Qt 3D animation fw or QVariant or something else, we get
            // sometimes such results. We can handle this in our side but we
            // cannot let Qt 3D do the QColor creation. So pretend we have a
            // QVector3D instead. This will be 'reversed' in stabilizeAnimatedValue().
            if (type == QVariant::Color)
                type = QVariant::Vector3D;

            // Create a mapping with a custom callback.
            QScopedPointer<Qt3DAnimation::QCallbackMapping> mapping(new Qt3DAnimation::QCallbackMapping);
            mapping->setChannelName(channelName);
            Q3DSAnimationCallback *cb = new Q3DSAnimationCallback(target, this, chIt->property, chIt.key(), chIt->propertyType);
            data->animationDataMap[slide]->animationCallbacks.append(cb);
            mapping->setCallback(type, cb, 0);
            mapper->addMapping(mapping.take());
        }

        Q_ASSERT(animator);
        animator->setChannelMapper(mapper.take());
        clip->setClipData(clipData);
        animator->setClip(clip.take());
        slide->attached()->entity->addComponent(animator);
    }
}

void Q3DSAnimationManager::clearAnimations(Q3DSSlide *slide)
{
    qCDebug(lcAnim, "Clearing animations for slide (%s)", qPrintable(slide->name()));

    Q3DSSlide *masterSlide = static_cast<Q3DSSlide *>(slide->parent());

    const bool hasAnimationData = !slide->animations().isEmpty()
            || !(masterSlide && masterSlide->animations().isEmpty());

    if (!hasAnimationData)
        return;

    const auto cleanUpAnimationData = [this](const QVector<Q3DSAnimationTrack> &anims, Q3DSSlide *slide) {
        for (const Q3DSAnimationTrack &track : anims) {
            if (!m_activeTargets.contains(track.target()) || track.target()->state() != Q3DSGraphObject::Enabled)
                continue;

            Q3DSGraphObjectAttached *data = track.target()->attached();
            Q3DSGraphObjectAttached::AnimationData *animationData = data->animationDataMap.value(slide);
            if (animationData) {
                // Cleanup previous animation callbacks
                qDeleteAll(animationData->animationCallbacks);
                animationData->animationCallbacks.clear();
                data->animationDataMap.remove(slide);
                delete animationData;
            }

            m_activeTargets.remove(track.target());
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

    cleanUpAnimationData(masterSlide->animations(), slide);
    cleanUpAnimationData(slide->animations(), slide);
}

static void insertTrack(Q3DSAnimationManager::AnimationTrackList &trackList,
                        const Q3DSAnimationTrack &animTrack,
                        bool overWrite)
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

void Q3DSAnimationManager::updateAnimations(Q3DSSlide *slide, bool editorMode)
{
    Q_ASSERT(slide);

    qCDebug(lcAnim, "Updating animations for slide (%s)", qPrintable(slide->name()));

    Q3DSSlide *masterSlide = static_cast<Q3DSSlide *>(slide->parent());

    const bool hasAnimationData = !slide->animations().isEmpty()
            || !(masterSlide && masterSlide->animations().isEmpty());

    if (!hasAnimationData)
        return;

    AnimationTrackListMap trackListMap;
    const auto buildTrackListMap = [&](Q3DSSlide *slide, bool overwrite) {
        if (!slide)
            return;

        const QVector<Q3DSAnimationTrack> &anims = slide->animations();
        for (const Q3DSAnimationTrack &animTrack : anims) {
            Q3DSGraphObject *target = animTrack.target();

            if (target->state() != Q3DSGraphObject::Enabled)
                continue;

            insertTrack(trackListMap[target], animTrack, overwrite);
        }
    };

    // Build the animation track list from the master slide first,
    // then overwrite any tracks that are also on the current slide.
    buildTrackListMap(masterSlide, false);
    buildTrackListMap(slide, true);

    updateAnimationHelper(trackListMap, slide, editorMode);
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
        if (!m_activeTargets.contains(target))
            continue;

        auto it = m_changes.find(target);
        Q3DSPropertyChangeList changeList;
        while (it != m_changes.cend() && it.key() == target) {
            if (Q_UNLIKELY(animDebug))
                qDebug() << "animate:" << target->id() << it->propertyName << it->value;
            if (it->property.type() == QVariant::Map) {
                it->property.writeOnGadget(target, QVariantMap{{it->propertyName, it->value}});
            } else {
                it->property.writeOnGadget(target, it->value);
            }
            changeList.append(Q3DSPropertyChange(it->propertyName));
            ++it;
        }
        if (!changeList.isEmpty())
            target->notifyPropertyChanges(changeList);
    }
    m_changes.clear();
}

void Q3DSAnimationManager::clearPendingChanges()
{
    m_changes.clear();
}

void Q3DSAnimationManager::objectAboutToBeRemovedFromScene(Q3DSGraphObject *obj)
{
    m_activeTargets.remove(obj);
}

void Q3DSAnimationManager::queueChange(Q3DSGraphObject *target, const AnimationValueChange &change)
{
    if (m_activeTargets.contains(target))
        m_changes.insert(target, change);
}

QT_END_NAMESPACE
