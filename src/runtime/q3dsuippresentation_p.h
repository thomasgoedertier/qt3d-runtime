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

#ifndef Q3DSUIPPRESENTATION_P_H
#define Q3DSUIPPRESENTATION_P_H

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

#include "q3dsruntimeglobal_p.h"
#include "q3dscustommaterial_p.h"
#include "q3dseffect_p.h"
#include "q3dsmeshloader_p.h"
#include "q3dsdatainputentry_p.h"
#include <QString>
#include <QVector>
#include <QSet>
#include <QHash>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>
#include <QColor>
#include <QVariant>

#include <functional>

QT_BEGIN_NAMESPACE

class Q3DSUipParser;
class Q3DSUipPresentation;
struct Q3DSUipPresentationData;
class Q3DSLayerNode;
class Q3DSSceneManager;
class QXmlStreamAttributes;

namespace Qt3DCore {
class QTransform;
class QEntity;
}

namespace Qt3DAnimation {
class QClipAnimator;
class QAnimationCallback;
}

class Q3DSGraphObject;
class Q3DSSlide;

class Q3DSV_PRIVATE_EXPORT Q3DSPropertyChange
{
public:
    Q3DSPropertyChange() = default;
    Q3DSPropertyChange(const QString &name_, const QString &value_)
        : m_name(name_), m_value(value_)
    { }

    // name() and value() must be source compatible with QXmlStreamAttribute
    QStringRef name() const { return QStringRef(&m_name); }
    QStringRef value() const { return QStringRef(&m_value); }

    QString nameStr() const { return m_name; }
    QString valueStr() const { return m_value; }

private:
    QString m_name;
    QString m_value;
};

class Q3DSV_PRIVATE_EXPORT Q3DSPropertyChangeList
{
public:
    typedef const Q3DSPropertyChange *const_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    const_iterator begin() const Q_DECL_NOTHROW { return m_changes.begin(); }
    const_iterator cbegin() const Q_DECL_NOTHROW { return begin(); }
    const_iterator end() const Q_DECL_NOTHROW { return m_changes.end(); }
    const_iterator cend() const Q_DECL_NOTHROW { return end(); }
    const_reverse_iterator rbegin() const Q_DECL_NOTHROW { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const Q_DECL_NOTHROW { return rbegin(); }
    const_reverse_iterator rend() const Q_DECL_NOTHROW { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const Q_DECL_NOTHROW { return rend(); }

    enum Flag {
        NodeTransformChanges = 0x01,
        NodeOpacityChanges = 0x02,
        EyeballChanges = 0x04,
        TextTextureImageDepChanges = 0x08,
        AoOrShadowChanges = 0x10,
        BlendModeChanges = 0x20
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    bool isEmpty() const { return m_changes.isEmpty(); }
    int count() const { return m_changes.count(); }
    void append(const Q3DSPropertyChange &change);
    QSet<QString> keys() const { return m_keys; }
    Flags flags() const { return m_flags; }

    static const int ALL_CHANGE_FLAGS = 0xFFFF;

    typedef Q3DSPropertyChange value_type;

private:
    QVector<Q3DSPropertyChange> m_changes;
    QSet<QString> m_keys;
    Flags m_flags = Flags();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSPropertyChangeList::Flags)

class Q3DSV_PRIVATE_EXPORT Q3DSGraphObjectAttached
{
public:
    virtual ~Q3DSGraphObjectAttached();
    struct AnimationData {
        QVector<Qt3DAnimation::QAnimationCallback *> animationCallbacks;
    };
    QHash<Q3DSSlide *, AnimationData *> animationDataMap;
    Qt3DCore::QEntity *entity = nullptr;

    struct AnimatedValueRollbackData {
        Q3DSGraphObject *obj;
        QString name;
        QVariant value;
        std::function<void (Q3DSGraphObject *, const QVariant &, const QString &)> setter;
    };
    QVector<AnimatedValueRollbackData> animationRollbacks;

    enum DirtyFlag {
        GroupDirty = 0x01,
        LightDirty = 0x02,
        ModelDirty = 0x04,
        GlobalTransformDirty = 0x08,
        GlobalOpacityDirty = 0x10,
        GlobalVisibilityDirty = 0x20,
        TextDirty = 0x40,
        ComponentDirty = 0x80,
        CameraDirty = 0x100,
        DefaultMaterialDirty = 0x200,
        ImageDirty = 0x400,
        LayerDirty = 0x800,
        CustomMaterialDirty = 0x1000,
        EffectDirty = 0x2000
    };
    Q_DECLARE_FLAGS(DirtyFlags, DirtyFlag)

    DirtyFlags dirty = DirtyFlags();
    Q3DSPropertyChangeList::Flags changeFlags = Q3DSPropertyChangeList::Flags();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSGraphObjectAttached::DirtyFlags)

class Q3DSV_PRIVATE_EXPORT Q3DSGraphObject
{
public:
    enum Type {
        AnyObject = 0,
        // direct subtypes
        Scene,
        Slide,
        Image,
        DefaultMaterial,
        ReferencedMaterial,
        CustomMaterial,
        Effect,
        // node subtypes
        _FirstNodeType = 100,
        Layer = _FirstNodeType,
        Camera,
        Light,
        Model,
        Group,
        Text,
        Component
    };

    enum PropSetFlag {
        PropSetDefaults = 0x01,
        PropSetOnMaster = 0x02
    };
    Q_DECLARE_FLAGS(PropSetFlags, PropSetFlag)

    Q3DSGraphObject(Type type);
    virtual ~Q3DSGraphObject();

    // hmm where have I seen this before...
    Type type() const { return m_type; }
    Q3DSGraphObject *parent() const { return m_parent; }
    Q3DSGraphObject *firstChild() const { return m_firstChild; }
    Q3DSGraphObject *lastChild() const { return m_lastChild; }
    Q3DSGraphObject *nextSibling() const { return m_nextSibling; }
    Q3DSGraphObject *previousSibling() const { return m_previousSibling; }
    int childCount() const;
    Q3DSGraphObject *childAtIndex(int idx) const;
    void removeChildNode(Q3DSGraphObject *node);
    void prependChildNode(Q3DSGraphObject *node);
    void appendChildNode(Q3DSGraphObject *node);

    virtual void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags);
    virtual void applyPropertyChanges(const Q3DSPropertyChangeList *);
    virtual void resolveReferences(Q3DSUipPresentation &, Q3DSUipParser &) { }

    void notifyPropertyChanges(const Q3DSPropertyChangeList *changeList);

    QByteArray id() const { return m_id; }
    bool isNode() const { return m_type >= Q3DSGraphObject::_FirstNodeType; }

    qint32 startTime() const { return m_startTime; }
    qint32 endTime() const { return m_endTime; }

    typedef std::function<void(Q3DSGraphObject *, const QSet<QString> &, int)> PropertyChangeCallback;
    int addPropertyChangeObserver(PropertyChangeCallback callback);
    void removePropertyChangeObserver(int callbackId);

    template <typename T = Q3DSGraphObjectAttached>
    const T *attached() const { return static_cast<const T *>(m_attached); }
    template <typename T = Q3DSGraphObjectAttached>
    T *attached() { return static_cast<T *>(m_attached); }
    void setAttached(Q3DSGraphObjectAttached *attached_) { m_attached = attached_; }

    typedef QMultiHash<QString, QString> DataInputControlledProperties; // data input entry name - property name
    const DataInputControlledProperties *dataInputControlledProperties() const
    { return &m_dataInputControlledProperties; }
    void setDataInputControlledProperties(const DataInputControlledProperties &props)
    { m_dataInputControlledProperties = props; }

    // for the built-in graph explorer
    QString gex_typeAsString() const;
    virtual QStringList gex_propertyNames() const;
    virtual QVariantList gex_propertyValues() const;

protected:
    qint32 m_startTime = 0;
    qint32 m_endTime = 10000;

private:
    Q_DISABLE_COPY(Q3DSGraphObject)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    Type m_type = AnyObject;
    Q3DSGraphObject *m_parent = nullptr;
    Q3DSGraphObject *m_firstChild = nullptr;
    Q3DSGraphObject *m_lastChild = nullptr;
    Q3DSGraphObject *m_nextSibling = nullptr;
    Q3DSGraphObject *m_previousSibling = nullptr;
    QByteArray m_id;
    QVector<PropertyChangeCallback> m_callbacks;
    Q3DSGraphObjectAttached *m_attached = nullptr;
    DataInputControlledProperties m_dataInputControlledProperties;

    friend class Q3DSUipPresentation;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSGraphObject::PropSetFlags)

class Q3DSV_PRIVATE_EXPORT Q3DSScene : public Q3DSGraphObject
{
public:
    Q3DSScene();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;

    QString name() const { return m_name; }
    bool useClearColor() const { return m_useClearColor; }
    QColor clearColor() const { return m_clearColor; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

private:
    Q_DISABLE_COPY(Q3DSScene)

    QString m_name;
    bool m_useClearColor = true;
    QColor m_clearColor = Qt::black;
};

class Q3DSV_PRIVATE_EXPORT Q3DSAnimationTrack
{
public:
    enum AnimationType {
        NoAnimation = 0,
        Linear,
        EaseInOut,
        Bezier
    };

    struct KeyFrame {
        float time;
        float value;
        union {
            float easeIn;
            float c2time;
        };
        union {
            float easeOut;
            float c2value;
        };
        float c1time;
        float c1value;
    };

    Q3DSGraphObject *target() const { return m_target; }
    QString property() const { return m_property; }
    bool isDynamic() const { return m_dynamic; }
    AnimationType type() const { return m_type; }
    const QVector<KeyFrame> *keyFrames() const { return &m_keyFrames; }

private:
    Q3DSGraphObject *m_target = nullptr;
    QString m_property;
    bool m_dynamic = false;
    AnimationType m_type = NoAnimation;
    QVector<KeyFrame> m_keyFrames;

    friend class Q3DSUipParser;
};

class Q3DSV_PRIVATE_EXPORT Q3DSSlide : public Q3DSGraphObject
{
public:
    enum PlayMode {
        StopAtEnd = 0,
        Looping,
        PingPong,
        Ping,
        PlayThroughTo
    };

    enum InitialPlayState {
        Play = 0,
        Pause
    };

    enum PlayThrough {
        Next = 0,
        Previous
    };

    Q3DSSlide();
    ~Q3DSSlide();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;

    QString name() const { return m_name; }
    PlayMode playMode() const { return m_playMode; }
    InitialPlayState initialPlayState() const { return m_initialPlayState; }
    bool playThroughHasExplicitValue() const { return m_playThroughHasExplicitValue; }
    PlayThrough playThrough() const { return m_playThrough; }
    int playThroughValue() const { return m_playThroughValue; }

    const QSet<Q3DSGraphObject *> *objects() const { return &m_objects; } // NB does not include objects from master
    const QHash<Q3DSGraphObject *, Q3DSPropertyChangeList *> *propertyChanges() const { return &m_propChanges; }
    const QVector<Q3DSAnimationTrack> *animations() const { return &m_anims; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

protected:
    void addObject(Q3DSGraphObject *obj);

private:
    Q_DISABLE_COPY(Q3DSSlide)

    QString m_name;
    PlayMode m_playMode = StopAtEnd;
    InitialPlayState m_initialPlayState = Play;
    bool m_playThroughHasExplicitValue = false;
    PlayThrough m_playThrough = Next;
    int m_playThroughValue = 0;
    QSet<Q3DSGraphObject *> m_objects;
    QHash<Q3DSGraphObject *, Q3DSPropertyChangeList *> m_propChanges;
    QVector<Q3DSAnimationTrack> m_anims;

    friend class Q3DSUipParser;
};

// Node/material/resource-like GraphObjects have 3 types of setters:
//
// 1. virtual setProperties -> initialize from XML (all properties)
// 2. virtual applyPropertyChanges -> just update the value of the given properties, mainly when changing slide
// 3. static setters -> used by the animation system
//
// None of these invoke the property change callbacks -> needs an explicit notifyPropertyChanges()
//
// Similarly, getters have two variants: animatable properties have a static getter as well.

class Q3DSV_PRIVATE_EXPORT Q3DSImage : public Q3DSGraphObject
{
public:
    enum MappingMode {
        UVMapping = 0,
        EnvironmentalMapping,
        LightProbe,
        IBLOverride
    };

    enum TilingMode {
        Tiled = 0,
        Mirrored,
        NoTiling
    };

    Q3DSImage();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres, Q3DSUipParser &parser) override;

    QString name() const { return m_name; }
    QString sourcePath() const { return m_sourcePath; } // already adjusted, can be opened as-is
    float scaleU() const { return m_scaleU; }
    float scaleV() const { return m_scaleV; }
    MappingMode mappingMode() const { return m_mappingMode; }
    TilingMode horizontalTiling() const { return m_tilingHoriz; }
    TilingMode verticalTiling() const { return m_tilingVert; }
    float rotationUV() const { return m_rotationUV; }
    float positionU() const { return m_positionU; }
    float positionV() const { return m_positionV; }
    float pivotU() const { return m_pivotU; }
    float pivotV() const { return m_pivotV; }
    QString subPresentation() const { return m_subPresentation; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

    void calculateTextureTransform();
    const QMatrix4x4 &textureTransform() const { return m_textureTransform; }

    bool hasTransparency();

    static void setScaleU(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSImage *>(obj)->m_scaleU = v.toFloat(); }
    static void setScaleV(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSImage *>(obj)->m_scaleV = v.toFloat(); }
    static void setRotationUV(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSImage *>(obj)->m_rotationUV = v.toFloat(); }
    static void setPositionU(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSImage *>(obj)->m_positionU = v.toFloat(); }
    static void setPositionV(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSImage *>(obj)->m_positionV = v.toFloat(); }
    static void setPivotU(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSImage *>(obj)->m_pivotU = v.toFloat(); }
    static void setPivotV(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSImage *>(obj)->m_pivotV = v.toFloat(); }

    static QVariant getScaleU(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSImage *>(obj)->m_scaleU; }
    static QVariant getScaleV(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSImage *>(obj)->m_scaleV; }
    static QVariant getRotationUV(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSImage *>(obj)->m_rotationUV; }
    static QVariant getPositionU(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSImage *>(obj)->m_positionU; }
    static QVariant getPositionV(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSImage *>(obj)->m_positionV; }
    static QVariant getPivotU(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSImage *>(obj)->m_pivotU; }
    static QVariant getPivotV(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSImage *>(obj)->m_pivotV; }

private:
    Q_DISABLE_COPY(Q3DSImage)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
    QString m_sourcePath;
    float m_scaleU = 1;
    float m_scaleV = 1;
    MappingMode m_mappingMode = UVMapping;
    TilingMode m_tilingHoriz = NoTiling;
    TilingMode m_tilingVert = NoTiling;
    float m_rotationUV = 0;
    float m_positionU = 0;
    float m_positionV = 0;
    float m_pivotU = 0;
    float m_pivotV = 0;
    QString m_subPresentation;
    QMatrix4x4 m_textureTransform;
    bool m_hasTransparency = false;
    bool m_scannedForTransparency = false;
};

class Q3DSComponentNode;
class Q3DSV_PRIVATE_EXPORT Q3DSNodeAttached : public Q3DSGraphObjectAttached
{
public:
    Qt3DCore::QTransform *transform = nullptr;
    QMatrix4x4 globalTransform;
    float globalOpacity = 1;
    bool globalVisibility = true;
    Q3DSLayerNode *layer3DS = nullptr;
    Q3DSComponentNode *component = nullptr;
};

class Q3DSV_PRIVATE_EXPORT Q3DSNode : public Q3DSGraphObject
{
public:
    enum NodeFlag {
        Active = 0x01, // eyeball
        IgnoresParentTransform = 0x02
    };
    Q_DECLARE_FLAGS(Flags, NodeFlag)

    enum RotationOrder {
        XYZ = 0,
        YZX,
        ZXY,
        XZY,
        YXZ,
        ZYX,
        XYZr,
        YZXr,
        ZXYr,
        XZYr,
        YXZr,
        ZYXr
    };

    enum Orientation {
        LeftHanded = 0,
        RightHanded
    };

    Q3DSNode(Type type);

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;

    Flags flags() const { return m_flags; }
    QVector3D rotation() const { return m_rotation; } // degrees
    QVector3D position() const { return m_position; }
    QVector3D scale() const { return m_scale; }
    QVector3D pivot() const { return m_pivot; }
    float localOpacity() const { return m_localOpacity; }
    qint32 skeletonId() const { return m_skeletonId; }
    RotationOrder rotationOrder() const { return m_rotationOrder; }
    Orientation orientation() const { return m_orientation; }
    const Q3DSPropertyChangeList *masterRollbackList() const { return m_masterRollbackList.data(); }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

    static void setRotation(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSNode *>(obj)->m_rotation = v.value<QVector3D>(); }
    static void setPosition(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSNode *>(obj)->m_position = v.value<QVector3D>(); }
    static void setScale(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSNode *>(obj)->m_scale = v.value<QVector3D>(); }
    static void setPivot(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSNode *>(obj)->m_pivot = v.value<QVector3D>(); }
    static void setLocalOpacity(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSNode *>(obj)->m_localOpacity = v.toFloat() / 100.0f; }

    static QVariant getRotation(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSNode *>(obj)->m_rotation; }
    static QVariant getPosition(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSNode *>(obj)->m_position; }
    static QVariant getScale(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSNode *>(obj)->m_scale; }
    static QVariant getPivot(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSNode *>(obj)->m_pivot; }
    static QVariant getLocalOpacity(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSNode *>(obj)->m_localOpacity * 100.0f; }

protected:
    Flags m_flags = Active;
    QVector3D m_rotation;
    QVector3D m_position;
    QVector3D m_scale = QVector3D(1, 1, 1);
    QVector3D m_pivot;
    float m_localOpacity = 1;
    qint32 m_skeletonId = -1;
    RotationOrder m_rotationOrder = YXZ;
    Orientation m_orientation = LeftHanded;
    QScopedPointer<Q3DSPropertyChangeList> m_masterRollbackList;

private:
    Q_DISABLE_COPY(Q3DSNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSNode::Flags)

class Q3DSV_PRIVATE_EXPORT Q3DSLayerNode : public Q3DSNode
{
public:
    enum Flag {
        DisableDepthTest = 0x01,
        DisableDepthPrePass = 0x02,
        TemporalAA = 0x04,
        FastIBL = 0x08
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum ProgressiveAA {
        NoPAA = 0,
        PAA2x,
        PAA4x,
        PAA8x
    };

    enum MultisampleAA {
        NoMSAA = 0,
        MSAA2x,
        MSAA4x,
        SSAA
    };

    enum LayerBackground {
        Transparent = 0,
        SolidColor,
        Unspecified
    };

    enum BlendType {
        Normal = 0,
        Screen,
        Multiply,
        Add,
        Subtract,
        Overlay,
        ColorBurn,
        ColorDodge
    };

    enum HorizontalFields {
        LeftWidth = 0,
        LeftRight,
        WidthRight
    };

    enum VerticalFields {
        TopHeight = 0,
        TopBottom,
        HeightBottom
    };

    enum Units {
        Percent = 0,
        Pixels
    };

    Q3DSLayerNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres, Q3DSUipParser &parser) override;

    QString name() const { return m_name; }
    Flags layerFlags() const { return m_layerFlags; }
    ProgressiveAA progressiveAA() const { return m_progressiveAA; }
    MultisampleAA multisampleAA() const { return m_multisampleAA; }
    LayerBackground layerBackground() const { return m_layerBackground; }
    QColor backgroundColor() const { return m_backgroundColor; }
    BlendType blendType() const { return m_blendType; }

    HorizontalFields horizontalFields() const { return m_horizontalFields; }
    float left() const { return m_left; }
    Units leftUnits() const { return m_leftUnits; }
    float width() const { return m_width; }
    Units widthUnits() const { return m_widthUnits; }
    float right() const { return m_right; }
    Units rightUnits() const { return m_rightUnits; }
    VerticalFields verticalFields() const { return m_verticalFields; }
    float top() const { return m_top; }
    Units topUnits() const { return m_topUnits; }
    float height() const { return m_height; }
    Units heightUnits() const { return m_heightUnits; }
    float bottom() const { return m_bottom; }
    Units bottomUnits() const { return m_bottomUnits; }

    QString sourcePath() const { return m_sourcePath; }

    // SSAO
    float aoStrength() const { return m_aoStrength; }
    float aoDistance() const { return m_aoDistance; }
    float aoSoftness() const { return m_aoSoftness; }
    float aoBias() const { return m_aoBias; }
    qint32 aoSampleRate() const { return m_aoSampleRate; }
    bool aoDither() const { return m_aoDither; }

    // SSDO
    float shadowStrength() const { return m_shadowStrength; }
    float shadowDist() const { return m_shadowDist; }
    float shadowSoftness() const { return m_shadowSoftness; }
    float shadowBias() const { return m_shadowBias; }

    // IBL
    Q3DSImage *lightProbe() const { return m_lightProbe; }
    float probebright() const { return m_probeBright; }
    float probehorizon() const { return m_probeHorizon; }
    float probefov() const { return m_probeFov; }
    Q3DSImage *lightProbe2() const { return m_lightProbe2; }
    float probe2fade() const { return m_probe2Fade; }
    float probe2window() const { return m_probe2Window; }
    float probe2pos() const { return m_probe2Pos; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

    static void setLeft(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_left = v.toFloat(); }
    static void setRight(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_right = v.toFloat(); }
    static void setWidth(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_width = v.toFloat(); }
    static void setHeight(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_height = v.toFloat(); }
    static void setTop(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_top = v.toFloat(); }
    static void setBottom(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_bottom = v.toFloat(); }
    static void setAoStrength(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_aoStrength = v.toFloat(); }
    static void setAoDistance(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_aoDistance = v.toFloat(); }
    static void setAoSoftness(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_aoSoftness = v.toFloat(); }
    static void setAoBias(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_aoBias = v.toFloat(); }
    static void setAoSampleRate(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_aoSampleRate = v.toInt(); }
    static void setShadowStrength(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_shadowStrength = v.toFloat(); }
    static void setShadowDist(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_shadowDist = v.toFloat(); }
    static void setShadowSoftness(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_shadowSoftness = v.toFloat(); }
    static void setShadowBias(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_shadowBias = v.toFloat(); }
    static void setProbeBright(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_probeBright = v.toFloat(); }
    static void setProbeHorizon(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_probeHorizon = v.toFloat(); }
    static void setProbeFov(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_probeFov = v.toFloat(); }
    static void setProbe2Fade(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_probe2Fade = v.toFloat(); }
    static void setProbe2Window(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_probe2Window = v.toFloat(); }
    static void setProbe2Pos(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLayerNode *>(obj)->m_probe2Pos = v.toFloat(); }

    static QVariant getLeft(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_left; }
    static QVariant getRight(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_right; }
    static QVariant getWidth(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_width; }
    static QVariant getHeight(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_height; }
    static QVariant getTop(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_top; }
    static QVariant getBottom(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_bottom; }
    static QVariant getAoStrength(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_aoStrength; }
    static QVariant getAoDistance(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_aoDistance; }
    static QVariant getAoSoftness(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_aoSoftness; }
    static QVariant getAoBias(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_aoBias; }
    static QVariant getAoSampleRate(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_aoSampleRate; }
    static QVariant getShadowStrength(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_shadowStrength; }
    static QVariant getShadowDist(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_shadowDist; }
    static QVariant getShadowSoftness(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_shadowSoftness; }
    static QVariant getShadowBias(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_shadowBias; }
    static QVariant getProbeBright(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_probeBright; }
    static QVariant getProbeHorizon(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_probeHorizon; }
    static QVariant getProbeFov(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_probeFov; }
    static QVariant getProbe2Fade(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_probe2Fade; }
    static QVariant getProbe2Window(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_probe2Window; }
    static QVariant getProbe2Pos(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLayerNode *>(obj)->m_probe2Pos; }

private:
    Q_DISABLE_COPY(Q3DSLayerNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
    Flags m_layerFlags = FastIBL;
    ProgressiveAA m_progressiveAA = NoPAA;
    MultisampleAA m_multisampleAA = NoMSAA;
    LayerBackground m_layerBackground = Transparent;
    QColor m_backgroundColor;
    BlendType m_blendType = Normal;

    HorizontalFields m_horizontalFields;
    float m_left = 0;
    Units m_leftUnits = Percent;
    float m_width = 100;
    Units m_widthUnits = Percent;
    float m_right = 0;
    Units m_rightUnits = Percent;
    VerticalFields m_verticalFields;
    float m_top = 0;
    Units m_topUnits = Percent;
    float m_height = 100;
    Units m_heightUnits = Percent;
    float m_bottom = 0;
    Units m_bottomUnits = Percent;

    QString m_sourcePath;

    float m_aoStrength = 0;
    float m_aoDistance = 5;
    float m_aoSoftness = 50;
    float m_aoBias = 0;
    qint32 m_aoSampleRate = 2;
    bool m_aoDither = true;

    float m_shadowStrength = 0;
    float m_shadowDist = 10;
    float m_shadowSoftness = 100;
    float m_shadowBias = 0;

    QString m_lightProbe_unresolved;
    Q3DSImage *m_lightProbe = nullptr;
    float m_probeBright = 100;
    float m_probeHorizon = -1;
    float m_probeFov = 180;
    QString m_lightProbe2_unresolved;
    Q3DSImage *m_lightProbe2 = nullptr;
    float m_probe2Fade = 1;
    float m_probe2Window = 1;
    float m_probe2Pos = 0.5f;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSLayerNode::Flags)

class Q3DSV_PRIVATE_EXPORT Q3DSCameraNode : public Q3DSNode
{
public:
    enum ScaleMode {
        SameSize = 0,
        Fit,
        FitHorizontal,
        FitVertical
    };

    enum ScaleAnchor {
        Center = 0,
        N,
        NE,
        E,
        SE,
        S,
        SW,
        W,
        NW
    };

    Q3DSCameraNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;

    QString name() const { return m_name; }
    bool orthographic() const { return m_orthographic; }
    float fov() const { return m_fov; }
    float clipNear() const { return m_clipNear; }
    float clipFar() const { return m_clipFar; }
    ScaleMode scaleMode() const { return m_scaleMode; }
    ScaleAnchor scaleAnchor() const { return m_scaleAnchor; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

    static void setFov(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSCameraNode *>(obj)->m_fov = v.toFloat(); }
    static void setClipNear(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSCameraNode *>(obj)->m_clipNear = v.toFloat(); }
    static void setClipFar(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSCameraNode *>(obj)->m_clipFar = v.toFloat(); }

    static QVariant getFov(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSCameraNode *>(obj)->m_fov; }
    static QVariant getClipNear(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSCameraNode *>(obj)->m_clipNear; }
    static QVariant getClipFar(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSCameraNode *>(obj)->m_clipFar; }

private:
    Q_DISABLE_COPY(Q3DSCameraNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
    bool m_orthographic = false;
    float m_fov = 60;
    float m_clipNear = 10;
    float m_clipFar = 5000;
    ScaleMode m_scaleMode = Fit;
    ScaleAnchor m_scaleAnchor = Center;
};

class Q3DSV_PRIVATE_EXPORT Q3DSLightNode : public Q3DSNode
{
public:
    enum LightType {
        Directional = 0,
        Point,
        Area
    };

    Q3DSLightNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres, Q3DSUipParser &parser) override;

    QString name() const { return m_name; }
    LightType lightType() const { return m_lightType; }
    Q3DSGraphObject *scope() const { return m_scope; }
    QColor diffuse() const { return m_lightDiffuse; }
    QColor specular() const { return m_lightSpecular; }
    QColor ambient() const { return m_lightAmbient; }
    float brightness() const { return m_brightness; }
    float linearFade() const { return m_linearFade; }
    float expFade() const { return m_expFade; }
    float areaWidth() const { return m_areaWidth; }
    float areaHeight() const { return m_areaHeight; }
    bool castShadow() const { return m_castShadow; }
    float shadowFactor() const { return m_shadowFactor; }
    float shadowFilter() const { return m_shadowFilter; }
    qint32 shadowMapRes() const { return m_shadowMapRes; }
    float shadowBias() const { return m_shadowBias; }
    float shadowMapFar() const { return m_shadowMapFar; }
    float shadowMapFov() const { return m_shadowMapFov; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

    static void setDiffuse(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_lightDiffuse = v.value<QColor>(); }
    static void setSpecular(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_lightSpecular = v.value<QColor>(); }
    static void setAmbient(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_lightAmbient = v.value<QColor>(); }
    static void setBrightness(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_brightness = v.toFloat(); }
    static void setLinearFade(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_linearFade = v.toFloat(); }
    static void setExpFade(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_expFade = v.toFloat(); }
    static void setAreaWidth(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_areaWidth = v.toFloat(); }
    static void setAreaHeight(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_areaHeight = v.toFloat(); }
    static void setShadowFactor(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_shadowFactor = v.toFloat(); }
    static void setShadowFilter(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_shadowFilter = v.toFloat(); }
    static void setShadowBias(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_shadowBias = v.toFloat(); }
    static void setShadowMapFar(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_shadowMapFar = v.toFloat(); }
    static void setShadowMapFov(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSLightNode *>(obj)->m_shadowMapFov = v.toFloat(); }

    static QVariant getDiffuse(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_lightDiffuse; }
    static QVariant getSpecular(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_lightSpecular; }
    static QVariant getAmbient(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_lightAmbient; }
    static QVariant getBrightness(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_brightness; }
    static QVariant getLinearFade(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_linearFade; }
    static QVariant getExpFade(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_expFade; }
    static QVariant getAreaWidth(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_areaWidth; }
    static QVariant getAreaHeight(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_areaHeight; }
    static QVariant getShadowFactor(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_shadowFactor; }
    static QVariant getShadowFilter(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_shadowFilter; }
    static QVariant getShadowBias(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_shadowBias; }
    static QVariant getShadowMapFar(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_shadowMapFar; }
    static QVariant getShadowMapFov(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSLightNode *>(obj)->m_shadowMapFov; }

private:
    Q_DISABLE_COPY(Q3DSLightNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
    QString m_scope_unresolved;
    Q3DSGraphObject *m_scope = nullptr;
    LightType m_lightType = Directional;
    QColor m_lightDiffuse;
    QColor m_lightSpecular;
    QColor m_lightAmbient;
    float m_brightness = 100;
    float m_linearFade = 0;
    float m_expFade = 0;
    float m_areaWidth = 100;
    float m_areaHeight = 100;
    bool m_castShadow = false;
    float m_shadowFactor = 10;
    float m_shadowFilter = 35;
    qint32 m_shadowMapRes = 9;
    float m_shadowBias = 0;
    float m_shadowMapFar = 5000;
    float m_shadowMapFov = 90;
};

class Q3DSV_PRIVATE_EXPORT Q3DSModelNode : public Q3DSNode
{
public:
    enum Tessellation {
        None = 0,
        Linear,
        Phong,
        NPatch
    };

    Q3DSModelNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres, Q3DSUipParser &parser) override;

    QString name() const { return m_name; }
    MeshList mesh() const { return m_mesh; }
    qint32 skeletonRoot() const { return m_skeletonRoot; }
    Tessellation tessellation() const { return m_tessellation; }
    float edgeTess() const { return m_edgeTess; }
    float innertTess() const { return m_innerTess; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

    static void setEdgeTess(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSModelNode *>(obj)->m_edgeTess = v.toFloat(); }
    static void setInnerTess(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSModelNode *>(obj)->m_innerTess = v.toFloat(); }

    static QVariant getEdgeTess(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSModelNode *>(obj)->m_edgeTess; }
    static QVariant getInnerTess(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSModelNode *>(obj)->m_innerTess; }

private:
    Q_DISABLE_COPY(Q3DSModelNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
    QString m_mesh_unresolved;
    MeshList m_mesh;
    qint32 m_skeletonRoot = -1;
    Tessellation m_tessellation = None;
    float m_edgeTess = 4;
    float m_innerTess = 4;
};

class Q3DSV_PRIVATE_EXPORT Q3DSGroupNode : public Q3DSNode
{
public:
    Q3DSGroupNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;

    QString name() const { return m_name; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

private:
    Q_DISABLE_COPY(Q3DSGroupNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
};

class Q3DSV_PRIVATE_EXPORT Q3DSComponentNode : public Q3DSNode
{
public:
    Q3DSComponentNode();
    ~Q3DSComponentNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;

    QString name() const { return m_name; }

    Q3DSSlide *masterSlide() const { return m_masterSlide; }
    Q3DSSlide *currentSlide() const { return m_currentSlide; }
    void setCurrentSlide(Q3DSSlide *slide);

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

private:
    Q_DISABLE_COPY(Q3DSComponentNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
    Q3DSSlide *m_masterSlide = nullptr;
    Q3DSSlide *m_currentSlide = nullptr;

    friend class Q3DSUipParser;
    friend class Q3DSSceneManager;
};

class Q3DSV_PRIVATE_EXPORT Q3DSTextNode : public Q3DSNode
{
public:
    enum HorizontalAlignment {
        Left = 0,
        Center,
        Right
    };

    enum VerticalAlignment {
        Top = 0,
        Middle,
        Bottom
    };

    Q3DSTextNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;

    QString name() const { return m_name; }
    QString text() const { return m_text; }
    QColor color() const { return m_color; }
    QString font() const { return m_font; }
    float size() const { return m_size; }
    HorizontalAlignment horizontalAlignment() const { return m_horizAlign; }
    VerticalAlignment verticalAlignment() const { return m_vertAlign; }
    float leading() const { return m_leading; }
    float tracking() const { return m_tracking; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

    static void setColor(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSTextNode *>(obj)->m_color = v.value<QColor>(); }
    static void setLeading(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSTextNode *>(obj)->m_leading = v.toFloat(); }
    static void setTracking(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSTextNode *>(obj)->m_tracking = v.toFloat(); }

    static QVariant getColor(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSTextNode *>(obj)->m_color; }
    static QVariant getLeading(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSTextNode *>(obj)->m_leading; }
    static QVariant getTracking(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSTextNode *>(obj)->m_tracking; }

private:
    Q_DISABLE_COPY(Q3DSTextNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
    QString m_text;
    QColor m_color;
    QString m_font;
    float m_size;
    HorizontalAlignment m_horizAlign;
    VerticalAlignment m_vertAlign;
    float m_leading;
    float m_tracking;
};

class Q3DSV_PRIVATE_EXPORT Q3DSDefaultMaterial : public Q3DSGraphObject
{
public:
    enum ShaderLighting {
        PixelShaderLighting = 0,
        NoShaderLighting
    };

    enum BlendMode {
        Normal = 0,
        Screen,
        Multiply,
        Overlay,
        ColorBurn,
        ColorDodge
    };

    enum SpecularModel {
        DefaultSpecularModel = 0,
        KGGX,
        KWard
    };

    Q3DSDefaultMaterial();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres, Q3DSUipParser &parser) override;

    QString name() const { return m_name; }
    ShaderLighting shaderLighting() const { return m_shaderLighting; }
    BlendMode blendMode() const { return m_blendMode; }
    QColor diffuse() const { return m_diffuse; }
    Q3DSImage *diffuseMap() const { return m_diffuseMap; }
    Q3DSImage *diffuseMap2() const { return m_diffuseMap2; }
    Q3DSImage *diffuseMap3() const { return m_diffuseMap3; }
    Q3DSImage *specularReflection() const { return m_specularReflection; }
    QColor specularTint() const { return m_specularTint; }
    float specularAmount() const { return m_specularAmount; }
    Q3DSImage *specularMap() const { return m_specularMap; }
    SpecularModel specularModel() const { return m_specularModel; }
    float specularRoughness() const { return m_specularRoughness; }
    float fresnelPower() const { return m_fresnelPower; }
    float ior() const { return m_ior; }
    Q3DSImage *bumpMap() const { return m_bumpMap; }
    Q3DSImage *normalMap() const { return m_normalMap; }
    float bumpAmount() const { return m_bumpAmount; }
    Q3DSImage *displacementmap() const { return m_displacementMap; }
    float displaceAmount() const { return m_displaceAmount; }
    float opacity() const { return m_opacity; }
    Q3DSImage *opacityMap() const { return m_opacityMap; }
    QColor emissiveColor() const { return m_emissiveColor; }
    float emissivePower() const { return m_emissivePower; }
    Q3DSImage *emissiveMap() const { return m_emissiveMap; }
    Q3DSImage *emissiveMap2() const { return m_emissiveMap2; }
    Q3DSImage *translucencyMap() const { return m_translucencyMap; }
    float translucentFalloff() const { return m_translucentFalloff; }
    float diffuseLightWrap() const { return m_diffuseLightWrap; }
    // lightmaps
    Q3DSImage *lightmapIndirectMap() const { return m_lightmapIndirectMap; }
    Q3DSImage *lightmapRadiosityMap() const { return m_lightmapRadiosityMap; }
    Q3DSImage *lightmapShadowMap() const { return m_lightmapShadowMap; }
    // IBL override
    Q3DSImage *lightProbe() const { return m_lightProbe; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

    static void setDiffuse(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_diffuse = v.value<QColor>(); }
    static void setSpecularTint(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_specularTint = v.value<QColor>(); }
    static void setSpecularAmount(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_specularAmount = v.toFloat(); }
    static void setSpecularRoughness(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_specularRoughness = v.toFloat(); }
    static void setFresnelPower(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_fresnelPower = v.toFloat(); }
    static void setDisplaceAmount(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_displaceAmount = v.toFloat(); }
    static void setOpacity(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_opacity = v.toFloat() / 100.0f; }
    static void setEmissiveColor(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_emissiveColor = v.value<QColor>(); }
    static void setEmissivePower(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_emissivePower = v.toFloat(); }
    static void setBumpAmount(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_bumpAmount = v.toFloat(); }
    static void setTranslucentFalloff(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_translucentFalloff = v.toFloat(); }
    static void setDiffuseLightWrap(Q3DSGraphObject *obj, const QVariant &v, const QString &) { static_cast<Q3DSDefaultMaterial *>(obj)->m_diffuseLightWrap = v.toFloat(); }

    static QVariant getDiffuse(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_diffuse; }
    static QVariant getSpecularTint(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_specularTint; }
    static QVariant getSpecularAmount(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_specularAmount; }
    static QVariant getSpecularRoughness(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_specularRoughness; }
    static QVariant getFresnelPower(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_fresnelPower; }
    static QVariant getDisplaceAmount(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_displaceAmount; }
    static QVariant getOpacity(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_opacity * 100.0f; }
    static QVariant getEmissiveColor(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_emissiveColor; }
    static QVariant getEmissivePower(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_emissivePower; }
    static QVariant getBumpAmount(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_bumpAmount; }
    static QVariant getTranslucentFalloff(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_translucentFalloff; }
    static QVariant getDiffuseLightWrap(Q3DSGraphObject *obj, const QString &) { return static_cast<Q3DSDefaultMaterial *>(obj)->m_diffuseLightWrap; }

private:
    Q_DISABLE_COPY(Q3DSDefaultMaterial)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
    ShaderLighting m_shaderLighting = PixelShaderLighting;
    BlendMode m_blendMode = Normal;
    QColor m_diffuse = Qt::white;
    QString m_diffuseMap_unresolved;
    Q3DSImage *m_diffuseMap = nullptr;
    QString m_diffuseMap2_unresolved;
    Q3DSImage *m_diffuseMap2 = nullptr;
    QString m_diffuseMap3_unresolved;
    Q3DSImage *m_diffuseMap3 = nullptr;
    QString m_specularReflection_unresolved;
    Q3DSImage *m_specularReflection = nullptr;
    QColor m_specularTint = Qt::white;
    float m_specularAmount = 0;
    QString m_specularMap_unresolved;
    Q3DSImage *m_specularMap = nullptr;
    SpecularModel m_specularModel = DefaultSpecularModel;
    float m_specularRoughness = 0;
    float m_fresnelPower = 0;
    float m_ior = 0.2f;
    QString m_bumpMap_unresolved;
    Q3DSImage *m_bumpMap = nullptr;
    QString m_normalMap_unresolved;
    Q3DSImage *m_normalMap = nullptr;
    float m_bumpAmount = 0.5f;
    QString m_displacementMap_unresolved;
    Q3DSImage *m_displacementMap = nullptr;
    float m_displaceAmount = 20;
    float m_opacity = 1;
    QString m_opacityMap_unresolved;
    Q3DSImage *m_opacityMap = nullptr;
    QColor m_emissiveColor = Qt::white;
    float m_emissivePower = 0;
    QString m_emissiveMap_unresolved;
    Q3DSImage *m_emissiveMap = nullptr;
    QString m_emissiveMap2_unresolved;
    Q3DSImage *m_emissiveMap2 = nullptr;
    QString m_translucencyMap_unresolved;
    Q3DSImage *m_translucencyMap = nullptr;
    float m_translucentFalloff = 1;
    float m_diffuseLightWrap = 0;
    // lightmaps
    QString m_lightmapIndirectMap_unresolved;
    Q3DSImage *m_lightmapIndirectMap = nullptr;
    QString m_lightmapRadiosityMap_unresolved;
    Q3DSImage *m_lightmapRadiosityMap = nullptr;
    QString m_lightmapShadowMap_unresolved;
    Q3DSImage *m_lightmapShadowMap = nullptr;
    // IBL override
    QString m_lightProbe_unresolved;
    Q3DSImage *m_lightProbe = nullptr;

};

class Q3DSV_PRIVATE_EXPORT Q3DSReferencedMaterial : public Q3DSGraphObject
{
public:
    Q3DSReferencedMaterial();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres, Q3DSUipParser &parser) override;

    QString name() const { return m_name; }
    Q3DSGraphObject *referencedMaterial() const { return m_referencedMaterial; }

    // lightmap overrides
    Q3DSImage *lightmapIndirectMap() const { return m_lightmapIndirectMap; }
    Q3DSImage *lightmapRadiosityMap() const { return m_lightmapRadiosityMap; }
    Q3DSImage *lightmapShadowMap() const { return m_lightmapShadowMap; }
    // IBL override
    Q3DSImage *lightProbe() const { return m_lightProbe; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

private:
    Q_DISABLE_COPY(Q3DSReferencedMaterial)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
    QString m_referencedMaterial_unresolved;
    Q3DSGraphObject *m_referencedMaterial = nullptr;
    // lightmap overrides
    QString m_lightmapIndirectMap_unresolved;
    Q3DSImage *m_lightmapIndirectMap = nullptr;
    QString m_lightmapRadiosityMap_unresolved;
    Q3DSImage *m_lightmapRadiosityMap = nullptr;
    QString m_lightmapShadowMap_unresolved;
    Q3DSImage *m_lightmapShadowMap = nullptr;
    // IBL override
    QString m_lightProbe_unresolved;
    Q3DSImage *m_lightProbe = nullptr;
};

class Q3DSV_PRIVATE_EXPORT Q3DSCustomMaterialInstance : public Q3DSGraphObject
{
public:
    Q3DSCustomMaterialInstance();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres, Q3DSUipParser &parser) override;

    QString name() const { return m_name; }
    const Q3DSCustomMaterial *material() const { return &m_material; }

    // lightmaps
    Q3DSImage *lightmapIndirectMap() const { return m_lightmapIndirectMap; }
    Q3DSImage *lightmapRadiosityMap() const { return m_lightmapRadiosityMap; }
    Q3DSImage *lightmapShadowMap() const { return m_lightmapShadowMap; }
    // IBL override
    Q3DSImage *lightProbe() const { return m_lightProbe; }

    // All custom properties, either with the default or the instance-specific value.
    // Filenames are already sanitized.
    const QVariantMap *materialPropertyValues() const { return &m_materialPropertyVals; }
    QVariantMap *materialPropertyValues() { return &m_materialPropertyVals; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

private:
    Q_DISABLE_COPY(Q3DSCustomMaterialInstance)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
    QString m_material_unresolved;
    Q3DSCustomMaterial m_material;
    QVariantMap m_materialPropertyVals;
    Q3DSPropertyChangeList m_attrs;
    // lightmaps
    QString m_lightmapIndirectMap_unresolved;
    Q3DSImage *m_lightmapIndirectMap = nullptr;
    QString m_lightmapRadiosityMap_unresolved;
    Q3DSImage *m_lightmapRadiosityMap = nullptr;
    QString m_lightmapShadowMap_unresolved;
    Q3DSImage *m_lightmapShadowMap = nullptr;
    // IBL override
    QString m_lightProbe_unresolved;
    Q3DSImage *m_lightProbe = nullptr;
};

class Q3DSV_PRIVATE_EXPORT Q3DSEffectInstance : public Q3DSGraphObject
{
public:
    Q3DSEffectInstance();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList *changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres, Q3DSUipParser &parser) override;

    QString name() const { return m_name; }
    const Q3DSEffect *effect() const { return &m_effect; }

    // All custom properties, either with the default or the instance-specific value.
    // Filenames are already sanitized.
    const QVariantMap *effectPropertyValues() const { return &m_effectPropertyVals; }
    QVariantMap *effectPropertyValues() { return &m_effectPropertyVals; }

    QStringList gex_propertyNames() const override;
    QVariantList gex_propertyValues() const override;

private:
    Q_DISABLE_COPY(Q3DSEffectInstance)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_name;
    QString m_effect_unresolved;
    Q3DSEffect m_effect;
    QVariantMap m_effectPropertyVals;
    Q3DSPropertyChangeList m_attrs;
};

class Q3DSV_PRIVATE_EXPORT Q3DSUipPresentation
{
public:
    Q3DSUipPresentation();
    ~Q3DSUipPresentation();
    void reset();

    enum Rotation {
        NoRotation = 0,
        Clockwise90,
        Clockwise180,
        Clockwise270
    };

    QString sourceFile() const;
    QString author() const;
    QString company() const;
    int presentationWidth() const;
    int presentationHeight() const;
    Rotation presentationRotation() const;
    bool maintainAspectRatio() const;

    Q3DSScene *scene() const;
    Q3DSSlide *masterSlide() const;
    Q3DSGraphObject *object(const QByteArray &id) const;
    Q3DSCustomMaterial customMaterial(const QByteArray &id) const;
    Q3DSEffect effect(const QByteArray &id) const;
    MeshList mesh(const QString &assetFilename, int part);

    typedef QHash<QString, bool> ImageBufferMap;
    const ImageBufferMap &imageBuffer() const;

    static void forAllObjectsOfType(Q3DSGraphObject *root,
                                    Q3DSGraphObject::Type type,
                                    std::function<void(Q3DSGraphObject *)> f);
    static void forAllNodes(Q3DSGraphObject *root,
                            std::function<void(Q3DSNode *)> f);
    static void forAllLayers(Q3DSScene *scene, std::function<void(Q3DSLayerNode*)> f, bool reverse = false);
    static void forAllModels(Q3DSGraphObject *obj, std::function<void(Q3DSModelNode *)> f, bool includeHidden = false);
    void forAllImages(std::function<void(Q3DSImage *)> f);

    void applySlidePropertyChanges(Q3DSSlide *slide) const;

    qint64 loadTimeMsecs() const;
    qint64 meshesLoadTimeMsecs() const;

    void setDataInputEntries(const Q3DSDataInputEntry::Map *entries);
    const Q3DSDataInputEntry::Map *dataInputEntries() const;

    typedef QMultiHash<QString, Q3DSGraphObject *> DataInputMap; // data input entry name - target object
    const DataInputMap *dataInputMap() const;
    void registerDataInputTarget(Q3DSGraphObject *obj);
    void removeDataInputTarget(Q3DSGraphObject *obj);

private:
    Q_DISABLE_COPY(Q3DSUipPresentation)

    void setSourceFile(const QString &s);
    void setAuthor(const QString &s);
    void setCompany(const QString &s);
    void setPresentationWidth(int w);
    void setPresentationHeight(int h);
    void setPresentationRotation(Q3DSUipPresentation::Rotation r);
    void setMaintainAspectRatio(bool b);
    void setScene(Q3DSScene *p);
    void setMasterSlide(Q3DSSlide *p);
    void setLoadTime(qint64 ms);
    void registerImageBuffer(const QString &sourcePath, bool hasTransparency);
    void registerObject(const QByteArray &id, Q3DSGraphObject *p);
    bool loadCustomMaterial(const QStringRef &id, const QStringRef &name, const QString &assetFilename);
    bool loadEffect(const QStringRef &id, const QStringRef &name, const QString &assetFilename);

    QScopedPointer<Q3DSUipPresentationData> d;
    friend class Q3DSUipParser;
};

struct Q3DSUipPresentationData
{
    QString sourceFile;
    QString author;
    QString company;
    int presentationWidth = 0;
    int presentationHeight = 0;
    Q3DSUipPresentation::Rotation presentationRotation = Q3DSUipPresentation::NoRotation;
    bool maintainAspectRatio = false;
    qint64 loadTime = 0;
    qint64 meshesLoadTime = 0;

    Q3DSScene *scene = nullptr;
    Q3DSSlide *masterSlide = nullptr;
    QHash<QByteArray, Q3DSGraphObject *> objects; // node ptrs managed by scene, not owned
    QHash<QByteArray, Q3DSCustomMaterial> customMaterials;
    QHash<QByteArray, Q3DSEffect> effects;
    // Note: the key here is the sourcePath before it's resolved!
    QHash<QString, bool /* hasTransparency */> imageBuffers;

    struct MeshId {
        MeshId(const QString &fn_, int part_) : fn(fn_), part(part_) { }
        QString fn;
        int part;
    };
    QHash<MeshId, MeshList> meshes;

    const Q3DSDataInputEntry::Map *dataInputEntries = nullptr;
    Q3DSUipPresentation::DataInputMap dataInputMap;
};

Q_DECLARE_TYPEINFO(Q3DSUipPresentationData::MeshId, Q_MOVABLE_TYPE);

inline bool operator==(const Q3DSUipPresentationData::MeshId &lhs, const Q3DSUipPresentationData::MeshId &rhs) Q_DECL_NOTHROW
{
    return lhs.fn == rhs.fn && lhs.part == rhs.part;
}

inline bool operator!=(const Q3DSUipPresentationData::MeshId &lhs, const Q3DSUipPresentationData::MeshId &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

inline uint qHash(const Q3DSUipPresentationData::MeshId &key, uint seed = 0) Q_DECL_NOTHROW
{
    QtPrivate::QHashCombine hash;
    seed = hash(seed, key.fn);
    seed = hash(seed, key.part);
    return seed;
}

QT_END_NAMESPACE

#endif // Q3DSUIPPRESENTATION_P_H
