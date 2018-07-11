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
#include "q3dsbehavior_p.h"
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

#define Q3DS_OBJECT \
    QT_WARNING_PUSH \
    Q_OBJECT_NO_OVERRIDE_WARNING \
    public: virtual const QMetaObject *metaObject() const { return &staticMetaObject; } \
    QT_WARNING_POP \
    Q_GADGET

class Q3DSUipParser;
class Q3DSUipPresentation;
struct Q3DSUipPresentationData;
class Q3DSLayerNode;
class Q3DSComponentNode;
class Q3DSSceneManager;
class QXmlStreamAttributes;

namespace Qt3DCore {
class QTransform;
class QEntity;
}

class Q3DSGraphObject;

namespace Qt3DAnimation {
class QClipAnimator;
class QAnimationCallback;
}

class Q3DSSlide;

class Q3DSV_PRIVATE_EXPORT Q3DSPropertyChange
{
public:
    Q3DSPropertyChange() = default;

    // When the new value is already set via a member or static setter.
    // Used by animations and any external call to a member setter.
    Q3DSPropertyChange(const QString &name_)
        : m_name(name_)
    { }

    // Value included.
    // Used by slides (on-enter property changes) and data input.
    // High frequency usage should be avoided.
    Q3DSPropertyChange(const QString &name_, const QString &value_)
        : m_name(name_), m_value(value_), m_hasValue(true)
    { }

    static Q3DSPropertyChange fromVariant(const QString &name, const QVariant &value);

    // name() and value() must be source compatible with QXmlStreamAttribute
    QStringRef name() const { return QStringRef(&m_name); }
    QStringRef value() const { Q_ASSERT(m_hasValue); return QStringRef(&m_value); }

    QString nameStr() const { return m_name; }
    QString valueStr() const { Q_ASSERT(m_hasValue); return m_value; }

    // A setter can return an invalid change when the new value is the same as
    // before. Such changes are ignored by the changelist.
    bool isValid() const { return !m_name.isEmpty(); }

    // A change without value can only be used with notifyPropertyChanges, not
    // with applyPropertyChanges.
    bool hasValue() const { return m_hasValue; }

private:
    QString m_name;
    QString m_value;
    bool m_hasValue = false;
};

Q_DECLARE_TYPEINFO(Q3DSPropertyChange, Q_MOVABLE_TYPE);

class Q3DSV_PRIVATE_EXPORT Q3DSPropertyChangeList
{
public:
    typedef const Q3DSPropertyChange *const_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    Q3DSPropertyChangeList() { }
#ifdef Q_COMPILER_INITIALIZER_LISTS
    Q3DSPropertyChangeList(std::initializer_list<Q3DSPropertyChange> args);
#endif

    const_iterator begin() const Q_DECL_NOTHROW { return m_changes.begin(); }
    const_iterator cbegin() const Q_DECL_NOTHROW { return begin(); }
    const_iterator end() const Q_DECL_NOTHROW { return m_changes.end(); }
    const_iterator cend() const Q_DECL_NOTHROW { return end(); }
    const_reverse_iterator rbegin() const Q_DECL_NOTHROW { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const Q_DECL_NOTHROW { return rbegin(); }
    const_reverse_iterator rend() const Q_DECL_NOTHROW { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const Q_DECL_NOTHROW { return rend(); }

    bool isEmpty() const { return m_changes.isEmpty(); }
    int count() const { return m_changes.count(); }
    void clear() { m_changes.clear(); m_keys.clear(); }
    void append(const Q3DSPropertyChange &change);
    QSet<QString> keys() const { return m_keys; }

    typedef Q3DSPropertyChange value_type;

private:
    QVector<Q3DSPropertyChange> m_changes;
    QSet<QString> m_keys;
};

class Q3DSV_PRIVATE_EXPORT Q3DSGraphObjectEvents
{
public:
    static QString pressureDownEvent();
    static QString pressureUpEvent();
    static QString tapEvent();

    static QString slideEnterEvent();
    static QString slideExitEvent();
};

class Q3DSV_PRIVATE_EXPORT Q3DSGraphObjectAttached
{
public:
    virtual ~Q3DSGraphObjectAttached();
    struct AnimationData {
        QVector<Qt3DAnimation::QAnimationCallback *> animationCallbacks;
    };
    QHash<Q3DSSlide *, AnimationData *> animationDataMap;
    Qt3DCore::QEntity *entity = nullptr;
    Q3DSComponentNode *component = nullptr;

    enum VisibilityTag
    {
        Visible,
        Hidden
    };

    enum FrameDirtyFlag {
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
        EffectDirty = 0x2000,
        AliasDirty = 0x4000,
        BehaviorDirty = 0x8000
    };
    Q_DECLARE_FLAGS(FrameDirtyFlags, FrameDirtyFlag)

    VisibilityTag visibilityTag = Hidden;
    FrameDirtyFlags frameDirty;
    int frameChangeFlags = 0;
    int propertyChangeObserverIndex = -1;
    int eventObserverIndex = -1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSGraphObjectAttached::FrameDirtyFlags)

// GraphObjects have 4 types of setters:
//
// 1. setProperties -> initialize from XML (all properties), called once per
// object from the parser. This is followed up by a call to resolveReferences
// from the parser after the graph is complete.
//
// 2. applyPropertyChanges -> update the value of the given properties (e.g. on
// slide change or when invoked manually)
//
// 3. normal setters, with the twist of returning a Q3DSPropertyChange
//
// Note that there is no implicit resolveReferences() in cases 2 and 3.
// Changing an Image or ObjectRef property thus needs an explicit call to it.
// Also note that 1 & 2 work with XML-style "#id" strings for image and object
// references.
//
// None of these invoke the property change callbacks -> that needs an explicit
// notifyPropertyChanges().
//
// Instances of custom materials, effects and behaviors have custom properties
// on top (dynamicProperties(), applyDynamicProperties()). These are settable via
// applyPropertyChanges as well.

class Q3DSV_PRIVATE_EXPORT Q3DSGraphObject
{
    Q3DS_OBJECT
    Q_PROPERTY(QByteArray id READ id)
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(qint32 starttime READ startTime WRITE setStartTime)
    Q_PROPERTY(qint32 endtime READ endTime WRITE setEndTime)
    Q_PROPERTY(QVariantMap dynamicProperties READ dynamicProperties WRITE applyDynamicProperties)

public:
    enum Type {
        Asset = 0,
        // direct subtypes
        Scene,
        Slide,
        Image,
        DefaultMaterial,
        ReferencedMaterial,
        CustomMaterial,
        Effect,
        Behavior,
        // node subtypes
        Layer = 100,
        Camera,
        Light,
        Model,
        Group,
        Text,
        Component,
        Alias
    };
    Q_ENUM(Type)

    static constexpr Type AnyObject = Type::Asset;
    static constexpr Type FirstNodeType = Type::Layer;

    enum PropSetFlag {
        PropSetDefaults = 0x01,
        PropSetOnMaster = 0x02
    };
    Q_DECLARE_FLAGS(PropSetFlags, PropSetFlag)

    enum DirtyFlag {
        DirtyNodeAdded = 0x01,
        DirtyNodeRemoved = 0x02
    };

    enum State {
        Enabled,
        Disabled
    };

    Q_DECLARE_FLAGS(DirtyFlags, DirtyFlag)

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
    void removeAllChildNodes();
    void prependChildNode(Q3DSGraphObject *node);
    void appendChildNode(Q3DSGraphObject *node);
    void insertChildNodeBefore(Q3DSGraphObject *node, Q3DSGraphObject *before);
    void insertChildNodeAfter(Q3DSGraphObject *node, Q3DSGraphObject *after);
    void reparentChildNodesTo(Q3DSGraphObject *newParent);
    void markDirty(DirtyFlags bits);

    virtual void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags);
    virtual void applyPropertyChanges(const Q3DSPropertyChangeList &changeList);
    virtual void resolveReferences(Q3DSUipPresentation &) { }

    virtual int mapChangeFlags(const Q3DSPropertyChangeList &changeList);
    void notifyPropertyChanges(const Q3DSPropertyChangeList &changeList);

    bool isNode() const { return m_type >= FirstNodeType; }

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
    void addDataInputControlledProperties(const DataInputControlledProperties &props)
    { m_dataInputControlledProperties += props; }

    struct Event {
        Event() = default;
        Event(Q3DSGraphObject *target_, const QString &event_, const QVariantList &args_ = QVariantList())
            : target(target_), event(event_), args(args_)
        { }
        Q3DSGraphObject *target = nullptr;
        QString event;
        QVariantList args;
    };

    typedef std::function<void(const Event &)> EventCallback;
    int addEventHandler(const QString &event, EventCallback callback);
    void removeEventHandler(const QString &event, int callbackId);
    void processEvent(const Event &e);

    // Convenience
    QByteArray typeAsString() const;
    QVariantMap dynamicProperties() const;

    // Returns complete list of properties (i.e., both static and dynamic properties).
    QVector<QByteArray> propertyNames() const;
    QVector<QVariant> propertyValues() const;

    // QObject style
    QVariant property(const char *name);
    bool setProperty(const char *name, const QVariant &v);
    QVector<QByteArray> dynamicPropertyNames() const;
    QVector<QVariant> dynamicPropertyValues() const;

    // NOTE: Unlike setProperty(), applyDynamicProperties() won't modify
    // (or add) a property that doesn't already exists.
    Q3DSPropertyChangeList applyDynamicProperties(const QVariantMap &v);

    // Properties
    QByteArray id() const { return m_id; } // always unique
    QString name() const { return m_name; } // as set in the editor, may not be unique
    qint32 startTime() const { return m_startTime; }
    qint32 endTime() const { return m_endTime; }

    // There is no setId(). Use Q3DSUipPresentation::registerObject() instead.
    Q3DSPropertyChange setName(const QString &v);
    Q3DSPropertyChange setStartTime(qint32 v);
    Q3DSPropertyChange setEndTime(qint32 v);

    State state() const { return m_state; }

protected:
    void destroyGraph();

    QByteArray m_id;
    QString m_name;
    qint32 m_startTime = 0;
    qint32 m_endTime = 10000;
    QHash<QString, QVector<EventCallback> > m_eventHandlers;

private:
    Q_DISABLE_COPY(Q3DSGraphObject)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    struct Q3DSObjectExtraMetaData
    {
        struct Data {
            QVector<QByteArray> propertyNames;
            QVector<QVariant> propertyValues;
        };
        QScopedPointer<Data> data;
    } metaData;

    Q3DSObjectExtraMetaData *extraMetaData() { return &metaData; }
    const Q3DSObjectExtraMetaData *extraMetaData() const { return &metaData; }

    Q3DSGraphObject *m_parent = nullptr;
    Q3DSGraphObject *m_firstChild = nullptr;
    Q3DSGraphObject *m_lastChild = nullptr;
    Q3DSGraphObject *m_nextSibling = nullptr;
    Q3DSGraphObject *m_previousSibling = nullptr;
    QVector<PropertyChangeCallback> m_callbacks;
    Q3DSGraphObjectAttached *m_attached = nullptr;
    DataInputControlledProperties m_dataInputControlledProperties;
    Type m_type = AnyObject;
    State m_state = Enabled;

    friend class Q3DSUipPresentation;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSGraphObject::PropSetFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSGraphObject::DirtyFlags)
Q_DECLARE_TYPEINFO(Q3DSGraphObject::Event, Q_MOVABLE_TYPE);

class Q3DSV_PRIVATE_EXPORT Q3DSScene : public Q3DSGraphObject
{
    Q3DS_OBJECT
    Q_PROPERTY(bool bgcolorenable READ useClearColor WRITE setUseClearColor)
    Q_PROPERTY(QColor backgroundcolor READ clearColor WRITE setClearColor)
public:
    Q3DSScene();
    ~Q3DSScene();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;

    typedef std::function<void(Q3DSScene *, Q3DSGraphObject::DirtyFlag change, Q3DSGraphObject *)> SceneChangeCallback;
    int addSceneChangeObserver(SceneChangeCallback callback);
    void removeSceneChangeObserver(int callbackId);

    // Properties
    bool useClearColor() const { return m_useClearColor; }
    QColor clearColor() const { return m_clearColor; }

    Q3DSPropertyChange setUseClearColor(bool v);
    Q3DSPropertyChange setClearColor(const QColor &v);

private:
    Q_DISABLE_COPY(Q3DSScene)
    void notifyNodeChange(Q3DSGraphObject *obj, Q3DSGraphObject::DirtyFlags bits);

    bool m_useClearColor = true;
    QColor m_clearColor = Qt::black;
    QVector<SceneChangeCallback> m_sceneChangeCallbacks;

    friend class Q3DSGraphObject;
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
        KeyFrame() = default;
        KeyFrame(float time_, float value_)
            : time(time_), value(value_)
        { }
        float time = 0; // seconds
        float value = 0;
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
    using KeyFrameList = QVector<KeyFrame>;

    Q3DSAnimationTrack() = default;
    Q3DSAnimationTrack(AnimationType type_, Q3DSGraphObject *target_, const QString &property_)
        : m_type(type_), m_target(target_), m_property(property_)
    { }

    AnimationType type() const { return m_type; }
    Q3DSGraphObject *target() const { return m_target; }
    QString property() const { return m_property; } // e.g. rotation.x

    bool isDynamic() const { return m_dynamic; }
    void setDynamic(bool dynamic) { m_dynamic = dynamic; }

    const KeyFrameList &keyFrames() const { return m_keyFrames; }
    void setKeyFrames(const KeyFrameList &keyFrames) { m_keyFrames = keyFrames; }

private:
    AnimationType m_type = NoAnimation;
    Q3DSGraphObject *m_target = nullptr;
    QString m_property;
    bool m_dynamic = false;
    KeyFrameList m_keyFrames;

    friend class Q3DSUipParser;
};

Q_DECLARE_TYPEINFO(Q3DSAnimationTrack::KeyFrame, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSAnimationTrack, Q_MOVABLE_TYPE);

inline bool operator==(const Q3DSAnimationTrack::KeyFrame &a, const Q3DSAnimationTrack::KeyFrame &b)
{
    return a.time == b.time;
}

inline bool operator!=(const Q3DSAnimationTrack::KeyFrame &a, const Q3DSAnimationTrack::KeyFrame &b)
{
    return !(a == b);
}

inline bool operator==(const Q3DSAnimationTrack &a, const Q3DSAnimationTrack &b)
{
    return a.target() == b.target() && a.property() == b.property();
}

inline bool operator!=(const Q3DSAnimationTrack &a, const Q3DSAnimationTrack &b)
{
    return !(a == b);
}

class Q3DSV_PRIVATE_EXPORT Q3DSAction
{
public:
    enum HandlerType {
        SetProperty,
        FireEvent,
        EmitSignal,
        GoToSlide,
        NextSlide,
        PreviousSlide,
        PrecedingSlide,
        Play,
        Pause,
        GoToTime,
        BehaviorHandler
    };

    struct HandlerArgument {
        enum Type {
            Unknown = 0,
            Property,
            Dependent,
            Slide,
            Event,
            Object,
            Signal
        };

        QString name;
        Q3DS::PropertyType type = Q3DS::Unknown;
        Type argType = Unknown;
        QString value;

        bool isValid() const { return type != Q3DS::Unknown; }
    };

    Q3DSGraphObject *owner = nullptr;
    QByteArray id;

    bool eyeball = true;

    QString triggerObject_unresolved;
    Q3DSGraphObject *triggerObject = nullptr;

    QString event;

    QString targetObject_unresolved;
    Q3DSGraphObject *targetObject = nullptr;

    HandlerType handler;
    QVector<HandlerArgument> handlerArgs; // when handler != BehaviorHandler
    QString behaviorHandler; // when handler == BehaviorHandler

    HandlerArgument handlerWithName(const QString &name) const {
        auto it = std::find_if(handlerArgs.cbegin(), handlerArgs.cend(),
                               [name](const HandlerArgument &a) { return a.name == name; });
        return it != handlerArgs.cend() ? *it : HandlerArgument();
    }
    HandlerArgument handlerWithArgType(HandlerArgument::Type t) const {
        auto it = std::find_if(handlerArgs.cbegin(), handlerArgs.cend(),
                               [t](const HandlerArgument &a) { return a.argType == t; });
        return it != handlerArgs.cend() ? *it : HandlerArgument();
    }
};

Q_DECLARE_TYPEINFO(Q3DSAction::HandlerArgument, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSAction, Q_MOVABLE_TYPE);

inline bool operator==(const Q3DSAction &a, const Q3DSAction &b)
{
    return a.id == b.id;
}

inline bool operator!=(const Q3DSAction &a, const Q3DSAction &b)
{
    return !(a == b);
}

class Q3DSV_PRIVATE_EXPORT Q3DSSlide : public Q3DSGraphObject
{
    Q3DS_OBJECT
    Q_PROPERTY(PlayMode playmode READ playMode WRITE setPlayMode)
    Q_PROPERTY(InitialPlayState initialplaystate READ initialPlayState WRITE setInitialPlayState)
    Q_PROPERTY(PlayThrough playthroughto READ playThrough WRITE setPlayThrough)
    Q_PROPERTY(QVariant playthroughValue READ playThroughValue WRITE setPlayThroughValue)
public:
    enum PlayMode {
        StopAtEnd = 0,
        Looping,
        PingPong,
        Ping,
        PlayThroughTo
    };
    Q_ENUM(PlayMode)

    enum InitialPlayState {
        Play = 0,
        Pause
    };
    Q_ENUM(InitialPlayState)

    enum PlayThrough {
        Next,
        Previous,
        Value
    };
    Q_ENUM(PlayThrough)

    using PropertyChanges = QHash<Q3DSGraphObject *, Q3DSPropertyChangeList *>;

    Q3DSSlide();
    ~Q3DSSlide();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres) override;

    const QSet<Q3DSGraphObject *> &objects() const { return m_objects; } // NB does not include objects from master
    void addObject(Q3DSGraphObject *obj);
    void removeObject(Q3DSGraphObject *obj);

    const PropertyChanges &propertyChanges() const { return m_propChanges; }
    void addPropertyChanges(Q3DSGraphObject *target, Q3DSPropertyChangeList *changeList); // changeList ownership transferred
    void removePropertyChanges(Q3DSGraphObject *target);
    Q3DSPropertyChangeList *takePropertyChanges(Q3DSGraphObject *target);

    const QVector<Q3DSAnimationTrack> &animations() const { return m_anims; }
    void addAnimation(const Q3DSAnimationTrack &track);
    void removeAnimation(const Q3DSAnimationTrack &track);

    const QVector<Q3DSAction> &actions() const { return m_actions; }
    void addAction(const Q3DSAction &action);
    void removeAction(const Q3DSAction &action);

    // The child added/removed notifications are managed by the master slide,
    // similarly to how scene does it for other types of objects.
    typedef std::function<void(Q3DSSlide *, Q3DSGraphObject::DirtyFlag change, Q3DSSlide *)> SlideGraphChangeCallback;
    int addSlideGraphChangeObserver(SlideGraphChangeCallback callback);
    void removeSlideGraphChangeObserver(int callbackId);

    // The slide-specific concepts (list of objects belonging to slide,
    // property changes to trigger when entering the slide, animation tracks)
    // are observable as well but that's managed on a per-slide basis, not
    // centrally by the master.
    enum SlideObjectChangeType {
        InvalidSlideObjectChange,
        SlideObjectAdded,
        SlideObjectRemoved,
        SlidePropertyChangesAdded,
        SlidePropertyChangesRemoved,
        SlideAnimationAdded,
        SlideAnimationRemoved,
        SlideActionAdded,
        SlideActionRemoved
    };
    struct SlideObjectChange {
        // the validity of the members depends on type
        SlideObjectChangeType type = InvalidSlideObjectChange;
        Q3DSGraphObject *obj = nullptr;
        Q3DSPropertyChangeList *changeList = nullptr;
        Q3DSAnimationTrack animation;
        Q3DSAction action;
    };
    typedef std::function<void(Q3DSSlide *, const SlideObjectChange &)> SlideObjectChangeCallback;
    int addSlideObjectChangeObserver(SlideObjectChangeCallback callback);
    void removeSlideObjectChangeObserver(int callbackId);

    // Properties
    PlayMode playMode() const { return m_playMode; }
    InitialPlayState initialPlayState() const { return m_initialPlayState; }
    PlayThrough playThrough() const { return m_playThrough; }
    QVariant playThroughValue() const { return m_playThroughValue; }

    Q3DSPropertyChange setPlayMode(PlayMode v);
    Q3DSPropertyChange setInitialPlayState(InitialPlayState v);
    Q3DSPropertyChange setPlayThrough(PlayThrough v);
    Q3DSPropertyChange setPlayThroughValue(const QVariant &v);

private:
    Q_DISABLE_COPY(Q3DSSlide)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    void notifySlideGraphChange(Q3DSSlide *slide, Q3DSGraphObject::DirtyFlags bits);
    void notifySlideObjectChange(const SlideObjectChange &change);

    PlayMode m_playMode = StopAtEnd;
    InitialPlayState m_initialPlayState = Play;
    PlayThrough m_playThrough = Next;
    QVariant m_playThroughValue;
    QSet<Q3DSGraphObject *> m_objects;
    PropertyChanges m_propChanges;
    QVector<Q3DSAnimationTrack> m_anims;
    QVector<Q3DSAction> m_actions;
    QVector<SlideGraphChangeCallback> m_slideGraphChangeCallbacks; // master only
    QVector<SlideObjectChangeCallback> m_slideObjectChangeCallbacks;

    friend class Q3DSUipParser;
    friend class Q3DSGraphObject;
};

Q_DECLARE_TYPEINFO(Q3DSSlide::SlideObjectChange, Q_MOVABLE_TYPE);

class Q3DSV_PRIVATE_EXPORT Q3DSImage : public Q3DSGraphObject
{
    Q3DS_OBJECT
    Q_PROPERTY(QString sourcepath READ sourcePath WRITE setSourcePath)
    Q_PROPERTY(float scaleu READ scaleU WRITE setScaleU)
    Q_PROPERTY(float scalev READ scaleV WRITE setScaleV)
    Q_PROPERTY(MappingMode mappingmode READ mappingMode WRITE setMappingMode)
    Q_PROPERTY(TilingMode tilingmodehorz READ horizontalTiling WRITE setHorizontalTiling)
    Q_PROPERTY(TilingMode tilingmodevert READ verticalTiling WRITE setVerticalTiling)
    Q_PROPERTY(float rotationuv READ rotationUV WRITE setRotationUV)
    Q_PROPERTY(float positionu READ positionU WRITE setPositionU)
    Q_PROPERTY(float positionv READ positionV WRITE setPositionV)
    Q_PROPERTY(float pivotu READ pivotU WRITE setPivotU)
    Q_PROPERTY(float pivotv READ pivotV WRITE setPivotV)
    Q_PROPERTY(QString subpresentation READ subPresentation WRITE setSubPresentation)
public:
    enum MappingMode {
        UVMapping = 0,
        EnvironmentalMapping,
        LightProbe,
        IBLOverride
    };
    Q_ENUM(MappingMode)

    enum TilingMode {
        Tiled = 0,
        Mirrored,
        NoTiling
    };
    Q_ENUM(TilingMode)

    Q3DSImage();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres) override;

    void calculateTextureTransform();
    const QMatrix4x4 &textureTransform() const { return m_textureTransform; }

    bool hasTransparency(Q3DSUipPresentation *presentation);
    bool hasPremultipliedAlpha() const;

    // Properties
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

    Q3DSPropertyChange setSourcePath(const QString &v);
    Q3DSPropertyChange setScaleU(float v);
    Q3DSPropertyChange setScaleV(float v);
    Q3DSPropertyChange setMappingMode(MappingMode v);
    Q3DSPropertyChange setHorizontalTiling(TilingMode v);
    Q3DSPropertyChange setVerticalTiling(TilingMode v);
    Q3DSPropertyChange setRotationUV(float v);
    Q3DSPropertyChange setPositionU(float v);
    Q3DSPropertyChange setPositionV(float v);
    Q3DSPropertyChange setPivotU(float v);
    Q3DSPropertyChange setPivotV(float v);
    Q3DSPropertyChange setSubPresentation(const QString &v);

private:
    Q_DISABLE_COPY(Q3DSImage)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

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

class Q3DSV_PRIVATE_EXPORT Q3DSNode : public Q3DSGraphObject
{
    Q_GADGET
    Q_PROPERTY(bool eyeball READ eyeballEnabled WRITE setEyeballEnabled)
    Q_PROPERTY(bool ignoresparent READ ignoresParent WRITE setIgnoresParent)
    Q_PROPERTY(QVector3D rotation READ rotation WRITE setRotation)
    Q_PROPERTY(QVector3D position READ position WRITE setPosition)
    Q_PROPERTY(QVector3D scale READ scale WRITE setScale)
    Q_PROPERTY(QVector3D pivot READ pivot WRITE setPivot)
    Q_PROPERTY(float opacity READ localOpacity WRITE setLocalOpacity)
    Q_PROPERTY(qint32 boneid READ skeletonId WRITE setSkeletonId)
    Q_PROPERTY(RotationOrder rotationorder READ rotationOrder WRITE setRotationOrder)
    Q_PROPERTY(Orientation orientation READ orientation WRITE setOrientation)
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
    Q_ENUM(RotationOrder)

    enum Orientation {
        LeftHanded = 0,
        RightHanded
    };
    Q_ENUM(Orientation)

    enum NodePropertyChanges {
        TransformChanges = 1 << 0,
        OpacityChanges = 1 << 1,
        EyeballChanges = 1 << 2
    };
    static const int FIRST_FREE_PROPERTY_CHANGE_BIT = 3;

    Q3DSNode(Type type);

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    int mapChangeFlags(const Q3DSPropertyChangeList &changeList) override;

    const Q3DSPropertyChangeList &masterRollbackList() const { return m_masterRollbackList; }

    Flags flags() const { return m_flags; }
    // Properties
    bool eyeballEnabled() const { return m_flags.testFlag(Q3DSNode::Active); }
    bool ignoresParent() const { return m_flags.testFlag(Q3DSNode::IgnoresParentTransform); }
    QVector3D rotation() const { return m_rotation; } // degrees
    QVector3D position() const { return m_position; }
    QVector3D scale() const { return m_scale; }
    QVector3D pivot() const { return m_pivot; }
    float localOpacity() const { return m_localOpacity; }
    qint32 skeletonId() const { return m_skeletonId; }
    RotationOrder rotationOrder() const { return m_rotationOrder; }
    Orientation orientation() const { return m_orientation; }

    Q3DSPropertyChange setFlag(NodeFlag flag, bool v);
    Q3DSPropertyChange setEyeballEnabled(bool v);
    Q3DSPropertyChange setIgnoresParent(bool v);
    Q3DSPropertyChange setRotation(const QVector3D &v);
    Q3DSPropertyChange setPosition(const QVector3D &v);
    Q3DSPropertyChange setScale(const QVector3D &v);
    Q3DSPropertyChange setPivot(const QVector3D &v);
    Q3DSPropertyChange setLocalOpacity(float v);
    Q3DSPropertyChange setSkeletonId(int v);
    Q3DSPropertyChange setRotationOrder(RotationOrder v);
    Q3DSPropertyChange setOrientation(Orientation v);

protected:
    Flags m_flags = Active;
    QVector3D m_rotation;
    QVector3D m_position;
    QVector3D m_scale = QVector3D(1, 1, 1);
    QVector3D m_pivot;
    float m_localOpacity = 100.0f;
    qint32 m_skeletonId = -1;
    RotationOrder m_rotationOrder = YXZ;
    Orientation m_orientation = LeftHanded;
    Q3DSPropertyChangeList m_masterRollbackList;

private:
    Q_DISABLE_COPY(Q3DSNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSNode::Flags)

class Q3DSV_PRIVATE_EXPORT Q3DSLayerNode : public Q3DSNode
{
    Q3DS_OBJECT
    Q_PROPERTY(bool disabledepthtest READ depthTestDisabled WRITE setDepthTestDisabled)
    Q_PROPERTY(bool disabledepthprepass READ depthPrePassDisabled WRITE setDepthPrePassDisabled)
    Q_PROPERTY(bool temporalaa READ temporalAAEnabled WRITE setTemporalAAEnabled)
    Q_PROPERTY(bool fastibl READ fastIBLEnabled WRITE setFastIBLEnabled)
    Q_PROPERTY(ProgressiveAA progressiveaa READ progressiveAA WRITE setProgressiveAA)
    Q_PROPERTY(MultisampleAA multisampleaa READ multisampleAA WRITE setMultisampleAA)
    Q_PROPERTY(LayerBackground background READ layerBackground WRITE setLayerBackground)
    Q_PROPERTY(QColor backgroundcolor READ backgroundColor WRITE setBackgroundColor)
    Q_PROPERTY(BlendType blendtype READ blendType WRITE setBlendType)
    Q_PROPERTY(HorizontalFields horzfields READ horizontalFields WRITE setHorizontalFields)
    Q_PROPERTY(float left READ left WRITE setLeft)
    Q_PROPERTY(Units leftunits READ leftUnits WRITE setLeftUnits)
    Q_PROPERTY(float width READ width WRITE setWidth)
    Q_PROPERTY(Units widthunits READ widthUnits WRITE setWidthUnits)
    Q_PROPERTY(float right READ right WRITE setRight)
    Q_PROPERTY(Units rightunits READ rightUnits WRITE setRightUnits)
    Q_PROPERTY(VerticalFields vertfields READ verticalFields WRITE setVerticalFields)
    Q_PROPERTY(float top READ top WRITE setTop)
    Q_PROPERTY(Units topunits READ topUnits WRITE setTopUnits)
    Q_PROPERTY(float height READ height WRITE setHeight)
    Q_PROPERTY(Units heightunits READ heightUnits WRITE setHeightUnits)
    Q_PROPERTY(float bottom READ bottom WRITE setBottom)
    Q_PROPERTY(Units bottouUnits READ bottomUnits WRITE setBottomUnits)
    Q_PROPERTY(QString sourcepath READ sourcePath WRITE setSourcePath)
    Q_PROPERTY(float aostrength READ aoStrength WRITE setAoStrength)
    Q_PROPERTY(float aodistance READ aoDistance WRITE setAoDistance)
    Q_PROPERTY(float aosoftness READ aoSoftness WRITE setAoSoftness)
    Q_PROPERTY(float aobias READ aoBias WRITE setAoBias)
    Q_PROPERTY(qint32 aosamplerate READ aoSampleRate WRITE setAoSampleRate)
    Q_PROPERTY(bool aodither READ aoDither WRITE setAoDither)
    Q_PROPERTY(float shadowstrength READ shadowStrength WRITE setShadowStrength)
    Q_PROPERTY(float shadowdist READ shadowDist WRITE setShadowDist)
    Q_PROPERTY(float shadowsoftness READ shadowSoftness WRITE setShadowSoftness)
    Q_PROPERTY(float shadowbias READ shadowBias WRITE setShadowBias)
    Q_PROPERTY(Q3DSImage * lightprobe READ lightProbe WRITE setLightProbe)
    Q_PROPERTY(float probebright READ probeBright WRITE setProbeBright)
    Q_PROPERTY(float probehorizon READ probeHorizon WRITE setProbeHorizon)
    Q_PROPERTY(float provefov READ probeFov WRITE setProbeFov)
    Q_PROPERTY(Q3DSImage * lightprobe2 READ lightProbe2 WRITE setLightProbe2)
    Q_PROPERTY(float probe2fade READ probe2Fade WRITE setProbe2Fade)
    Q_PROPERTY(float probe2window READ probe2Window WRITE setProbe2Window)
    Q_PROPERTY(float probe2pos READ probe2Pos WRITE setProbe2Pos)
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
    Q_ENUM(ProgressiveAA)

    enum MultisampleAA {
        NoMSAA = 0,
        MSAA2x,
        MSAA4x,
        SSAA
    };
    Q_ENUM(MultisampleAA)

    enum LayerBackground {
        Transparent = 0,
        SolidColor,
        Unspecified
    };
    Q_ENUM(LayerBackground)

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
    Q_ENUM(BlendType)

    enum HorizontalFields {
        LeftWidth = 0,
        LeftRight,
        WidthRight
    };
    Q_ENUM(HorizontalFields)

    enum VerticalFields {
        TopHeight = 0,
        TopBottom,
        HeightBottom
    };
    Q_ENUM(VerticalFields)

    enum Units {
        Percent = 0,
        Pixels
    };
    Q_ENUM(Units)

    enum LayerPropertyChanges {
        AoOrShadowChanges = 1 << Q3DSNode::FIRST_FREE_PROPERTY_CHANGE_BIT,
        LayerContentSubTreeChanges = 1 << (Q3DSNode::FIRST_FREE_PROPERTY_CHANGE_BIT + 1),
        LayerContentSubTreeLightsChange = 1 << (Q3DSNode::FIRST_FREE_PROPERTY_CHANGE_BIT + 2)
    };

    Q3DSLayerNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres) override;
    int mapChangeFlags(const Q3DSPropertyChangeList &changeList) override;

    // Properties
    Flags layerFlags() const { return m_layerFlags; }
    bool depthTestDisabled() const { return m_layerFlags.testFlag(DisableDepthTest); }
    bool depthPrePassDisabled() const { return m_layerFlags.testFlag(DisableDepthPrePass); }
    bool temporalAAEnabled() const { return m_layerFlags.testFlag(TemporalAA); }
    bool fastIBLEnabled() const { return m_layerFlags.testFlag(FastIBL); }
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
    float probeBright() const { return m_probeBright; }
    float probeHorizon() const { return m_probeHorizon; }
    float probeFov() const { return m_probeFov; }
    Q3DSImage *lightProbe2() const { return m_lightProbe2; }
    float probe2Fade() const { return m_probe2Fade; }
    float probe2Window() const { return m_probe2Window; }
    float probe2Pos() const { return m_probe2Pos; }

    Q3DSPropertyChange setLayerFlag(Flag flag, bool v);
    Q3DSPropertyChange setDepthTestDisabled(bool v);
    Q3DSPropertyChange setDepthPrePassDisabled(bool v);
    Q3DSPropertyChange setTemporalAAEnabled(bool v);
    Q3DSPropertyChange setFastIBLEnabled(bool v);
    Q3DSPropertyChange setProgressiveAA(ProgressiveAA v);
    Q3DSPropertyChange setMultisampleAA(MultisampleAA v);
    Q3DSPropertyChange setLayerBackground(LayerBackground v);
    Q3DSPropertyChange setBackgroundColor(const QColor &v);
    Q3DSPropertyChange setBlendType(BlendType v);
    Q3DSPropertyChange setHorizontalFields(HorizontalFields v);
    Q3DSPropertyChange setLeft(float v);
    Q3DSPropertyChange setLeftUnits(Units v);
    Q3DSPropertyChange setWidth(float v);
    Q3DSPropertyChange setWidthUnits(Units v);
    Q3DSPropertyChange setRight(float v);
    Q3DSPropertyChange setRightUnits(Units v);
    Q3DSPropertyChange setVerticalFields(VerticalFields v);
    Q3DSPropertyChange setTop(float v);
    Q3DSPropertyChange setTopUnits(Units v);
    Q3DSPropertyChange setHeight(float v);
    Q3DSPropertyChange setHeightUnits(Units v);
    Q3DSPropertyChange setBottom(float v);
    Q3DSPropertyChange setBottomUnits(Units v);
    Q3DSPropertyChange setSourcePath(const QString &v);
    Q3DSPropertyChange setAoStrength(float v);
    Q3DSPropertyChange setAoDistance(float v);
    Q3DSPropertyChange setAoSoftness(float v);
    Q3DSPropertyChange setAoBias(float v);
    Q3DSPropertyChange setAoSampleRate(int v);
    Q3DSPropertyChange setAoDither(bool v);
    Q3DSPropertyChange setShadowStrength(float v);
    Q3DSPropertyChange setShadowDist(float v);
    Q3DSPropertyChange setShadowSoftness(float v);
    Q3DSPropertyChange setShadowBias(float v);
    Q3DSPropertyChange setLightProbe(Q3DSImage *v);
    Q3DSPropertyChange setProbeBright(float v);
    Q3DSPropertyChange setProbeHorizon(float v);
    Q3DSPropertyChange setProbeFov(float v);
    Q3DSPropertyChange setLightProbe2(Q3DSImage *v);
    Q3DSPropertyChange setProbe2Fade(float v);
    Q3DSPropertyChange setProbe2Window(float v);
    Q3DSPropertyChange setProbe2Pos(float v);

private:
    Q_DISABLE_COPY(Q3DSLayerNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    Flags m_layerFlags = FastIBL;
    ProgressiveAA m_progressiveAA = NoPAA;
    MultisampleAA m_multisampleAA = NoMSAA;
    LayerBackground m_layerBackground = Transparent;
    QColor m_backgroundColor;
    BlendType m_blendType = Normal;

    HorizontalFields m_horizontalFields = LeftWidth;
    float m_left = 0;
    Units m_leftUnits = Percent;
    float m_width = 100;
    Units m_widthUnits = Percent;
    float m_right = 0;
    Units m_rightUnits = Percent;
    VerticalFields m_verticalFields = TopHeight;
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
    Q3DS_OBJECT
    Q_PROPERTY(bool orthographic READ orthographic WRITE setOrthographic)
    Q_PROPERTY(float fov READ fov WRITE setFov)
    Q_PROPERTY(float clipnear READ clipNear WRITE setClipNear)
    Q_PROPERTY(float clipfar READ clipFar WRITE setClipFar)
    Q_PROPERTY(ScaleMode scalemode READ scaleMode WRITE setScaleMode)
    Q_PROPERTY(ScaleAnchor scaleanchor READ scaleAnchor WRITE setScaleAnchor)
public:
    enum ScaleMode {
        SameSize = 0,
        Fit,
        FitHorizontal,
        FitVertical
    };
    Q_ENUM(ScaleMode)

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
    Q_ENUM(ScaleAnchor)

    Q3DSCameraNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;

    // Properties
    bool orthographic() const { return m_orthographic; }
    float fov() const { return m_fov; }
    float clipNear() const { return m_clipNear; }
    float clipFar() const { return m_clipFar; }
    ScaleMode scaleMode() const { return m_scaleMode; }
    ScaleAnchor scaleAnchor() const { return m_scaleAnchor; }

    Q3DSPropertyChange setOrthographic(bool v);
    Q3DSPropertyChange setFov(float v);
    Q3DSPropertyChange setClipNear(float v);
    Q3DSPropertyChange setClipFar(float v);
    Q3DSPropertyChange setScaleMode(ScaleMode v);
    Q3DSPropertyChange setScaleAnchor(ScaleAnchor v);

private:
    Q_DISABLE_COPY(Q3DSCameraNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    bool m_orthographic = false;
    float m_fov = 60;
    float m_clipNear = 10;
    float m_clipFar = 5000;
    ScaleMode m_scaleMode = Fit;
    ScaleAnchor m_scaleAnchor = Center;
};

class Q3DSV_PRIVATE_EXPORT Q3DSLightNode : public Q3DSNode
{
    Q3DS_OBJECT
    Q_PROPERTY(Q3DSGraphObject * scope READ scope WRITE setScope)
    Q_PROPERTY(LightType lighttype READ lightType WRITE setLightType)
    Q_PROPERTY(QColor lightdiffuse READ diffuse WRITE setDiffuse)
    Q_PROPERTY(QColor lightspecular READ specular WRITE setSpecular)
    Q_PROPERTY(QColor lightambient READ ambient WRITE setAmbient)
    Q_PROPERTY(float brightness READ brightness WRITE setBrightness)
    Q_PROPERTY(float linearfade READ linearFade WRITE setLinearFade)
    Q_PROPERTY(float expfade READ expFade WRITE setExpFade)
    Q_PROPERTY(float areawidth READ areaWidth WRITE setAreaWidth)
    Q_PROPERTY(float areaheight READ areaHeight WRITE setAreaHeight)
    Q_PROPERTY(bool castshadow READ castShadow WRITE setCastShadow)
    Q_PROPERTY(float shdwfactor READ shadowFactor WRITE setShadowFactor)
    Q_PROPERTY(float shdwfilter READ shadowFilter WRITE setShadowFilter)
    Q_PROPERTY(qint32 shdwmapres READ shadowMapRes WRITE setShadowMapRes)
    Q_PROPERTY(float shdwbias READ shadowBias WRITE setShadowBias)
    Q_PROPERTY(float shdwmapfar READ shadowMapFar WRITE setShadowMapFar)
    Q_PROPERTY(float shdwmapfov READ shadowMapFov WRITE setShadowMapFov)
public:
    enum LightType {
        Directional = 0,
        Point,
        Area
    };
    Q_ENUM(LightType)

    Q3DSLightNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres) override;

    // Properties
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

    Q3DSPropertyChange setLightType(LightType v);
    Q3DSPropertyChange setScope(Q3DSGraphObject *v);
    Q3DSPropertyChange setDiffuse(const QColor &v);
    Q3DSPropertyChange setSpecular(const QColor &v);
    Q3DSPropertyChange setAmbient(const QColor &v);
    Q3DSPropertyChange setBrightness(float v);
    Q3DSPropertyChange setLinearFade(float v);
    Q3DSPropertyChange setExpFade(float v);
    Q3DSPropertyChange setAreaWidth(float v);
    Q3DSPropertyChange setAreaHeight(float v);
    Q3DSPropertyChange setCastShadow(bool v);
    Q3DSPropertyChange setShadowFactor(float v);
    Q3DSPropertyChange setShadowFilter(float v);
    Q3DSPropertyChange setShadowMapRes(int v);
    Q3DSPropertyChange setShadowBias(float v);
    Q3DSPropertyChange setShadowMapFar(float v);
    Q3DSPropertyChange setShadowMapFov(float v);

private:
    Q_DISABLE_COPY(Q3DSLightNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_scope_unresolved;
    Q3DSGraphObject *m_scope = nullptr;
    LightType m_lightType = Directional;
    QColor m_lightDiffuse = Qt::white;
    QColor m_lightSpecular = Qt::white;
    QColor m_lightAmbient = Qt::black;
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
    Q3DS_OBJECT
    Q_PROPERTY(QString sourcepath READ sourcePath /* setMesh */)
    Q_PROPERTY(qint32 poseroot READ skeletonRoot WRITE setSkeletonRoot)
    Q_PROPERTY(Tessellation tessellation READ tessellation WRITE setTessellation)
    Q_PROPERTY(float edgetess READ edgeTess WRITE setEdgeTess)
    Q_PROPERTY(float innertess READ innerTess WRITE setInnerTess)
public:
    enum Tessellation {
        None = 0,
        Linear,
        Phong,
        NPatch
    };
    Q_ENUM(Tessellation)

    enum ModelPropertyChanges {
        MeshChanges = 1 << Q3DSNode::FIRST_FREE_PROPERTY_CHANGE_BIT
    };

    Q3DSModelNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    int mapChangeFlags(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres) override;

    // Properties
    MeshList mesh() const { return m_mesh; }
    QString sourcePath() const { return m_mesh_unresolved; }
    qint32 skeletonRoot() const { return m_skeletonRoot; }
    Tessellation tessellation() const { return m_tessellation; }
    float edgeTess() const { return m_edgeTess; }
    float innerTess() const { return m_innerTess; }

    Q3DSPropertyChange setMesh(const QString &v);
    Q3DSPropertyChange setSkeletonRoot(int v);
    Q3DSPropertyChange setTessellation(Tessellation v);
    Q3DSPropertyChange setEdgeTess(float v);
    Q3DSPropertyChange setInnerTess(float v);

private:
    Q_DISABLE_COPY(Q3DSModelNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_mesh_unresolved;
    MeshList m_mesh;
    qint32 m_skeletonRoot = -1;
    Tessellation m_tessellation = None;
    float m_edgeTess = 4;
    float m_innerTess = 4;
};

class Q3DSV_PRIVATE_EXPORT Q3DSGroupNode : public Q3DSNode
{
    Q3DS_OBJECT
public:
    Q3DSGroupNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;

private:
    Q_DISABLE_COPY(Q3DSGroupNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);
};

class Q3DSV_PRIVATE_EXPORT Q3DSComponentNode : public Q3DSNode
{
    Q3DS_OBJECT
public:
    Q3DSComponentNode();
    ~Q3DSComponentNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &) override;

    Q3DSSlide *masterSlide() const { return m_masterSlide; }
    Q3DSSlide *currentSlide() const { return m_currentSlide; }
    void setCurrentSlide(Q3DSSlide *slide);

private:
    Q_DISABLE_COPY(Q3DSComponentNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    Q3DSSlide *m_masterSlide = nullptr;
    Q3DSSlide *m_currentSlide = nullptr;

    friend class Q3DSUipParser;
    friend class Q3DSSceneManager;
};

class Q3DSV_PRIVATE_EXPORT Q3DSTextNode : public Q3DSNode
{
    Q3DS_OBJECT
    Q_PROPERTY(QString textstring READ text WRITE setText)
    Q_PROPERTY(QColor textcolor READ color WRITE setColor)
    Q_PROPERTY(QString font READ font WRITE setFont)
    Q_PROPERTY(float size READ size WRITE setSize)
    Q_PROPERTY(HorizontalAlignment horzalign READ horizontalAlignment WRITE setHorizontalAlignment)
    Q_PROPERTY(VerticalAlignment vertalign READ verticalAlignment WRITE setVerticalAlignment)
    Q_PROPERTY(float leading READ leading WRITE setLeading)
    Q_PROPERTY(float tracking READ tracking WRITE setTracking)
public:
    enum HorizontalAlignment {
        Left = 0,
        Center,
        Right
    };
    Q_ENUM(HorizontalAlignment)

    enum VerticalAlignment {
        Top = 0,
        Middle,
        Bottom
    };
    Q_ENUM(VerticalAlignment)

    enum TextPropertyChanges {
        TextureImageDepChanges = 1 << Q3DSNode::FIRST_FREE_PROPERTY_CHANGE_BIT
    };

    Q3DSTextNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    int mapChangeFlags(const Q3DSPropertyChangeList &changeList) override;

    // Properties
    QString text() const { return m_text; }
    QColor color() const { return m_color; }
    QString font() const { return m_font; }
    float size() const { return m_size; }
    HorizontalAlignment horizontalAlignment() const { return m_horizAlign; }
    VerticalAlignment verticalAlignment() const { return m_vertAlign; }
    float leading() const { return m_leading; }
    float tracking() const { return m_tracking; }

    Q3DSPropertyChange setText(const QString &v);
    Q3DSPropertyChange setColor(const QColor &v);
    Q3DSPropertyChange setFont(const QString &v);
    Q3DSPropertyChange setSize(float v);
    Q3DSPropertyChange setHorizontalAlignment(HorizontalAlignment v);
    Q3DSPropertyChange setVerticalAlignment(VerticalAlignment v);
    Q3DSPropertyChange setLeading(float v);
    Q3DSPropertyChange setTracking(float v);

private:
    Q_DISABLE_COPY(Q3DSTextNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_text = QStringLiteral("Text");
    QColor m_color = Qt::white;
    QString m_font = QStringLiteral("TitilliumWeb-Regular");
    float m_size = 36;
    HorizontalAlignment m_horizAlign = Center;
    VerticalAlignment m_vertAlign = Middle;
    float m_leading = 0;
    float m_tracking = 0;
};

class Q3DSV_PRIVATE_EXPORT Q3DSDefaultMaterial : public Q3DSGraphObject
{
    Q3DS_OBJECT
    Q_PROPERTY(ShaderLighting shaderlighting READ shaderLighting WRITE setShaderLighting)
    Q_PROPERTY(BlendMode blendmode READ blendMode WRITE setBlendMode)
    Q_PROPERTY(bool vertexcolors READ vertexColors)
    Q_PROPERTY(QColor diffuse READ diffuse WRITE setDiffuse)
    Q_PROPERTY(Q3DSImage * diffusemap READ diffuseMap WRITE setDiffuseMap)
    Q_PROPERTY(Q3DSImage * diffusemap2 READ diffuseMap2 WRITE setDiffuseMap2)
    Q_PROPERTY(Q3DSImage * diffusemap3 READ diffuseMap3 WRITE setDiffuseMap3)
    Q_PROPERTY(Q3DSImage * specularreflection READ specularReflection WRITE setSpecularReflection)
    Q_PROPERTY(QColor speculartint READ specularTint WRITE setSpecularTint)
    Q_PROPERTY(float specularamount READ specularAmount WRITE setSpecularAmount)
    Q_PROPERTY(Q3DSImage * specularmap READ specularMap WRITE setSpecularMap)
    Q_PROPERTY(SpecularModel specularmodel READ specularModel WRITE setSpecularModel)
    Q_PROPERTY(float specularroughness READ specularRoughness WRITE setSpecularRoughness)
    Q_PROPERTY(Q3DSImage * roughnessmap READ roughnessMap WRITE setRoughnessMap)
    Q_PROPERTY(float fresnelpower READ fresnelPower WRITE setFresnelPower)
    Q_PROPERTY(float ior READ ior WRITE setIor)
    Q_PROPERTY(Q3DSImage * bumpmap READ bumpMap WRITE setBumpMap)
    Q_PROPERTY(Q3DSImage * normalmap READ normalMap WRITE setNormalMap)
    Q_PROPERTY(float bumpamount READ bumpAmount WRITE setBumpAmount)
    Q_PROPERTY(Q3DSImage * displacementmap READ displacementMap WRITE setDisplacementMap)
    Q_PROPERTY(float displaceamount READ displaceAmount WRITE setDisplaceAmount)
    Q_PROPERTY(float opacity READ opacity WRITE setOpacity)
    Q_PROPERTY(Q3DSImage * opacitymap READ opacityMap WRITE setOpacityMap)
    Q_PROPERTY(QColor emissivecolor READ emissiveColor WRITE setEmissiveColor)
    Q_PROPERTY(float emissivepower READ emissivePower WRITE setEmissivePower)
    Q_PROPERTY(Q3DSImage * emissivemap READ emissiveMap WRITE setEmissiveMap)
    Q_PROPERTY(Q3DSImage * emissivemap2 READ emissiveMap2 WRITE setEmissiveMap2)
    Q_PROPERTY(Q3DSImage * translucencymap READ translucencyMap WRITE setTranslucencyMap)
    Q_PROPERTY(float translucentfalloff READ translucentFalloff)
    Q_PROPERTY(float diffuselightwrap READ diffuseLightWrap WRITE setDiffuseLightWrap)
    Q_PROPERTY(Q3DSImage * lightmapindirect READ lightmapIndirectMap WRITE setLightmapIndirectMap)
    Q_PROPERTY(Q3DSImage * lightmapradiosity READ lightmapRadiosityMap WRITE setLightmapRadiosityMap)
    Q_PROPERTY(Q3DSImage * lightmapshadow READ lightmapShadowMap WRITE setLightmapShadowMap)
    Q_PROPERTY(Q3DSImage * iblprobe READ lightProbe WRITE setLightProbe)
public:
    enum ShaderLighting {
        PixelShaderLighting = 0,
        NoShaderLighting
    };
    Q_ENUM(ShaderLighting)

    enum BlendMode {
        Normal = 0,
        Screen,
        Multiply,
        Overlay,
        ColorBurn,
        ColorDodge
    };
    Q_ENUM(BlendMode)

    enum SpecularModel {
        DefaultSpecularModel = 0,
        KGGX,
        KWard
    };
    Q_ENUM(SpecularModel)

    enum DefaultMaterialPropertyChanges {
        BlendModeChanges = 1 << 0
    };

    Q3DSDefaultMaterial();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres) override;
    int mapChangeFlags(const Q3DSPropertyChangeList &changeList) override;

    // Properties
    ShaderLighting shaderLighting() const { return m_shaderLighting; }
    BlendMode blendMode() const { return m_blendMode; }
    bool vertexColors() const { return m_vertexColors; }
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
    Q3DSImage *roughnessMap() const { return m_roughnessMap; }
    float fresnelPower() const { return m_fresnelPower; }
    float ior() const { return m_ior; }
    Q3DSImage *bumpMap() const { return m_bumpMap; }
    Q3DSImage *normalMap() const { return m_normalMap; }
    float bumpAmount() const { return m_bumpAmount; }
    Q3DSImage *displacementMap() const { return m_displacementMap; }
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

    Q3DSPropertyChange setShaderLighting(ShaderLighting v);
    Q3DSPropertyChange setBlendMode(BlendMode v);
    Q3DSPropertyChange setDiffuse(const QColor &v);
    Q3DSPropertyChange setDiffuseMap(Q3DSImage *v);
    Q3DSPropertyChange setDiffuseMap2(Q3DSImage *v);
    Q3DSPropertyChange setDiffuseMap3(Q3DSImage *v);
    Q3DSPropertyChange setSpecularReflection(Q3DSImage *v);
    Q3DSPropertyChange setSpecularTint(const QColor &v);
    Q3DSPropertyChange setSpecularAmount(float v);
    Q3DSPropertyChange setSpecularMap(Q3DSImage *v);
    Q3DSPropertyChange setSpecularModel(SpecularModel v);
    Q3DSPropertyChange setSpecularRoughness(float v);
    Q3DSPropertyChange setRoughnessMap(Q3DSImage *v);
    Q3DSPropertyChange setFresnelPower(float v);
    Q3DSPropertyChange setIor(float v);
    Q3DSPropertyChange setBumpMap(Q3DSImage *v);
    Q3DSPropertyChange setNormalMap(Q3DSImage *v);
    Q3DSPropertyChange setBumpAmount(float v);
    Q3DSPropertyChange setDisplacementMap(Q3DSImage *v);
    Q3DSPropertyChange setDisplaceAmount(float v);
    Q3DSPropertyChange setOpacity(float v);
    Q3DSPropertyChange setOpacityMap(Q3DSImage *v);
    Q3DSPropertyChange setEmissiveColor(const QColor &v);
    Q3DSPropertyChange setEmissivePower(float v);
    Q3DSPropertyChange setEmissiveMap(Q3DSImage *v);
    Q3DSPropertyChange setEmissiveMap2(Q3DSImage *v);
    Q3DSPropertyChange setTranslucencyMap(Q3DSImage *v);
    Q3DSPropertyChange setTranslucentFalloff(float v);
    Q3DSPropertyChange setDiffuseLightWrap(float v);
    Q3DSPropertyChange setLightmapIndirectMap(Q3DSImage *v);
    Q3DSPropertyChange setLightmapRadiosityMap(Q3DSImage *v);
    Q3DSPropertyChange setLightmapShadowMap(Q3DSImage *v);
    Q3DSPropertyChange setLightProbe(Q3DSImage *v);

private:
    Q_DISABLE_COPY(Q3DSDefaultMaterial)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    ShaderLighting m_shaderLighting = PixelShaderLighting;
    BlendMode m_blendMode = Normal;
    bool m_vertexColors = false;
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
    QString m_roughnessMap_unresolved;
    Q3DSImage *m_roughnessMap = nullptr;
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
    float m_opacity = 100;
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
    Q3DS_OBJECT
    Q_PROPERTY(Q3DSGraphObject * referencedmaterial READ referencedMaterial WRITE setReferencedMaterial)
    Q_PROPERTY(Q3DSImage * lightmapindirect READ lightmapIndirectMap WRITE setLightmapIndirectMap)
    Q_PROPERTY(Q3DSImage * lightmapradiosity READ lightmapRadiosityMap WRITE setLightmapRadiosityMap)
    Q_PROPERTY(Q3DSImage * lightmapshadow READ lightmapShadowMap WRITE setLightmapShadowMap)
    Q_PROPERTY(Q3DSImage * iblprobe READ lightProbe WRITE setLightProbe)
public:
    Q3DSReferencedMaterial();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres) override;

    // Properties
    Q3DSGraphObject *referencedMaterial() const { return m_referencedMaterial; }
    // lightmap overrides
    Q3DSImage *lightmapIndirectMap() const { return m_lightmapIndirectMap; }
    Q3DSImage *lightmapRadiosityMap() const { return m_lightmapRadiosityMap; }
    Q3DSImage *lightmapShadowMap() const { return m_lightmapShadowMap; }
    // IBL override
    Q3DSImage *lightProbe() const { return m_lightProbe; }

    Q3DSPropertyChange setReferencedMaterial(Q3DSGraphObject *v);
    Q3DSPropertyChange setLightmapIndirectMap(Q3DSImage *v);
    Q3DSPropertyChange setLightmapRadiosityMap(Q3DSImage *v);
    Q3DSPropertyChange setLightmapShadowMap(Q3DSImage *v);
    Q3DSPropertyChange setLightProbe(Q3DSImage *v);

private:
    Q_DISABLE_COPY(Q3DSReferencedMaterial)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

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
    Q3DS_OBJECT
    Q_PROPERTY(QString class READ clazz)
    Q_PROPERTY(const Q3DSCustomMaterial * material READ material /* resolveReferences */)
    Q_PROPERTY(Q3DSImage * lightmapindirect READ lightmapIndirectMap WRITE setLightmapIndirectMap)
    Q_PROPERTY(Q3DSImage * lightmapradiosity READ lightmapRadiosityMap WRITE setLightmapRadiosityMap)
    Q_PROPERTY(Q3DSImage * lightmapshadow READ lightmapShadowMap WRITE setLightmapShadowMap)
    Q_PROPERTY(Q3DSImage * iblprobe READ lightProbe WRITE setLightProbe)
public:
    Q3DSCustomMaterialInstance();
    Q3DSCustomMaterialInstance(const Q3DSCustomMaterial &material);

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres) override;

    // Properties
    QString clazz() const { return m_material_unresolved; }
    const Q3DSCustomMaterial *material() const { return &m_material; }
    // lightmaps
    Q3DSImage *lightmapIndirectMap() const { return m_lightmapIndirectMap; }
    Q3DSImage *lightmapRadiosityMap() const { return m_lightmapRadiosityMap; }
    Q3DSImage *lightmapShadowMap() const { return m_lightmapShadowMap; }
    // IBL override
    Q3DSImage *lightProbe() const { return m_lightProbe; }

    Q3DSPropertyChange setLightmapIndirectMap(Q3DSImage *v);
    Q3DSPropertyChange setLightmapRadiosityMap(Q3DSImage *v);
    Q3DSPropertyChange setLightmapShadowMap(Q3DSImage *v);
    Q3DSPropertyChange setLightProbe(Q3DSImage *v);

private:
    Q_DISABLE_COPY(Q3DSCustomMaterialInstance)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_material_unresolved;
    Q3DSCustomMaterial m_material;
    bool m_materialIsResolved = false;
    QVariantMap m_materialPropertyVals;
    Q3DSPropertyChangeList m_pendingCustomProperties;
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
    Q3DS_OBJECT
    Q_PROPERTY(QString class READ clazz)
    Q_PROPERTY(bool eyeball READ eyeballEnabled)
public:
    enum EffectInstancePropertyChanges {
        EyeBallChanges = 1 << 0
    };

    Q3DSEffectInstance();
    Q3DSEffectInstance(const Q3DSEffect &effect);

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres) override;
    int mapChangeFlags(const Q3DSPropertyChangeList &changeList) override;

    // Properties
    QString clazz() const { return m_effect_unresolved; }
    const Q3DSEffect *effect() const { return &m_effect; }
    bool eyeballEnabled() const { return m_eyeballEnabled; }

    const Q3DSPropertyChangeList &masterRollbackList() const { return m_masterRollbackList; }

private:
    Q_DISABLE_COPY(Q3DSEffectInstance)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_effect_unresolved;
    Q3DSEffect m_effect;
    bool m_effectIsResolved = false;
    bool m_eyeballEnabled = true;
    Q3DSPropertyChangeList m_pendingCustomProperties;
    Q3DSPropertyChangeList m_masterRollbackList;
};

class Q3DSV_PRIVATE_EXPORT Q3DSBehaviorInstance : public Q3DSGraphObject
{
    Q3DS_OBJECT
    Q_PROPERTY(QString class READ clazz)
    Q_PROPERTY(bool eyeball READ eyeballEnabled)
    Q_PROPERTY(const Q3DSBehavior * behavior READ behavior)
public:
    enum BehaviorInstancePropertyChanges {
        EyeBallChanges = 1 << 0
    };

    Q3DSBehaviorInstance();
    Q3DSBehaviorInstance(const Q3DSBehavior &behavior);

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres) override;
    int mapChangeFlags(const Q3DSPropertyChangeList &changeList) override;

    QString qmlErrorString() const { return m_qmlErrorString; }
    void setQmlErrorString(const QString &error) { m_qmlErrorString = error; }

    // Properties
    const Q3DSBehavior *behavior() const { return &m_behavior; }
    QString clazz() const { return m_behavior_unresolved; }
    bool eyeballEnabled() const { return m_eyeballEnabled; }

private:
    Q_DISABLE_COPY(Q3DSBehaviorInstance)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_behavior_unresolved;
    Q3DSBehavior m_behavior;
    bool m_behaviorIsResolved = false;
    bool m_eyeballEnabled = true;
    Q3DSPropertyChangeList m_pendingCustomProperties;
    QVariantMap m_behaviorPropertyVals;
    QString m_qmlErrorString;
};

class Q3DSV_PRIVATE_EXPORT Q3DSAliasNode : public Q3DSNode
{
    Q3DS_OBJECT
    Q_PROPERTY(Q3DSGraphObject * referencednode READ referencedNode WRITE setReferencedNode)
public:
    Q3DSAliasNode();

    void setProperties(const QXmlStreamAttributes &attrs, PropSetFlags flags) override;
    void applyPropertyChanges(const Q3DSPropertyChangeList &changeList) override;
    void resolveReferences(Q3DSUipPresentation &pres) override;

    // Properties
    Q3DSGraphObject *referencedNode() const { return m_referencedNode; }

    Q3DSPropertyChange setReferencedNode(Q3DSGraphObject *v);

private:
    Q_DISABLE_COPY(Q3DSAliasNode)
    template<typename V> void setProps(const V &attrs, PropSetFlags flags);

    QString m_referencedNode_unresolved;
    Q3DSGraphObject *m_referencedNode = nullptr;
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
    void setSourceFile(const QString &s);
    QString assetFileName(const QString &xmlFileNameRef, int *part) const;

    QString name() const;
    void setName(const QString &s);

    QString author() const;
    QString company() const;
    int presentationWidth() const;
    int presentationHeight() const;
    Rotation presentationRotation() const;
    bool maintainAspectRatio() const;

    void setAuthor(const QString &author);
    void setCompany(const QString &company);
    void setPresentationWidth(int w);
    void setPresentationHeight(int h);
    void setPresentationRotation(Rotation r);
    void setMaintainAspectRatio(bool maintain);

    Q3DSScene *scene() const;
    Q3DSSlide *masterSlide() const;

    void setScene(Q3DSScene *p);
    void setMasterSlide(Q3DSSlide *p);

    bool registerObject(const QByteArray &id, Q3DSGraphObject *p); // covers both the scene and slide graphs
    void unregisterObject(const QByteArray &id);

    template <typename T = Q3DSGraphObject>
    const T *object(const QByteArray &id) const { return static_cast<const T *>(getObject(id)); }
    template <typename T = Q3DSGraphObject>
    T *object(const QByteArray &id) { return static_cast<T *>(getObject(id)); }

    template <typename T = Q3DSGraphObject>
    const T *objectByName(const QString &name) const { return static_cast<const T *>(getObjectByName(name)); }
    template <typename T = Q3DSGraphObject>
    T *objectByName(const QString &name) { return static_cast<T *>(getObjectByName(name)); }

    void registerImageBuffer(const QString &sourcePath, bool hasTransparency);

    Q3DSCustomMaterial customMaterial(const QByteArray &id) const;
    Q3DSEffect effect(const QByteArray &id) const;
    Q3DSBehavior behavior(const QByteArray &id) const;
    MeshList mesh(const QString &assetFilename, int part = 1);

    typedef QHash<QString, bool> ImageBufferMap;
    const ImageBufferMap &imageBuffer() const;

    static void forAllObjects(Q3DSGraphObject *root,
                              std::function<void(Q3DSGraphObject *)> f);
    static void forAllObjectsInSubTree(Q3DSGraphObject *root,
                                       std::function<void(Q3DSGraphObject *)> f);
    static void forAllObjectsOfType(Q3DSGraphObject *root,
                                    Q3DSGraphObject::Type type,
                                    std::function<void(Q3DSGraphObject *)> f);
    static void forAllNodes(Q3DSGraphObject *root,
                            std::function<void(Q3DSNode *)> f);
    static void forAllLayers(Q3DSScene *scene, std::function<void(Q3DSLayerNode*)> f, bool reverse = false);
    static void forAllModels(Q3DSGraphObject *obj, std::function<void(Q3DSModelNode *)> f, bool includeHidden = false);

    void notifyPropertyChanges(const Q3DSSlide::PropertyChanges &changeList) const;
    void applyPropertyChanges(const Q3DSSlide::PropertyChanges &changeList) const;
    void applySlidePropertyChanges(Q3DSSlide *slide) const;

    qint64 loadTimeMsecs() const;
    qint64 meshesLoadTimeMsecs() const;

    void setDataInputEntries(const Q3DSDataInputEntry::Map *entries);
    const Q3DSDataInputEntry::Map *dataInputEntries() const;

    typedef QMultiHash<QString, Q3DSGraphObject *> DataInputMap; // data input entry name - target object
    const DataInputMap *dataInputMap() const;
    void registerDataInputTarget(Q3DSGraphObject *obj);
    void removeDataInputTarget(Q3DSGraphObject *obj);

    template<typename T> T *newObject(const QByteArray &id)
    {
        T *obj = new T;
        return registerObject(id, obj) ? obj : nullptr; // also sets obj->id
    }

    void unlinkObject(Q3DSGraphObject *obj)
    {
        Q3DSUipPresentation::forAllObjectsInSubTree(obj, [this](Q3DSGraphObject *objOrChild) {
            unregisterObject(objOrChild->id());
        });
        if (obj->parent())
            obj->parent()->removeChildNode(obj);
    }

    Q3DSGraphObject *newObject(const char *type, const QByteArray &id);
    void resolveAliases();
    void updateObjectStateForSubTrees();
    void addImplicitPropertyChanges();
    QHash<QString, bool> &imageTransparencyHash();

private:
    Q_DISABLE_COPY(Q3DSUipPresentation)

    void setLoadTime(qint64 ms);
    bool loadCustomMaterial(const QStringRef &id, const QStringRef &name, const QString &assetFilename);
    bool loadEffect(const QStringRef &id, const QStringRef &name, const QString &assetFilename);
    bool loadBehavior(const QStringRef &id, const QStringRef &name, const QString &assetFilename);
    Q3DSGraphObject *getObject(const QByteArray &id) const;
    Q3DSGraphObject *getObjectByName(const QString &name) const;

    QScopedPointer<Q3DSUipPresentationData> d;
    QHash<QString, bool> m_imageTransparencyHash;
    friend class Q3DSUipParser;
};

struct Q3DSUipPresentationData
{
    QString sourceFile;
    QString name;
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
    QHash<QByteArray, Q3DSBehavior> behaviors;
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

Q_DECLARE_METATYPE(Q3DSGraphObject *)
Q_DECLARE_METATYPE(Q3DSImage *)
Q_DECLARE_METATYPE(Q3DSSlide *)

#endif // Q3DSUIPPRESENTATION_P_H
