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

#ifndef Q3DSANIMATIONBUILDER_H
#define Q3DSANIMATIONBUILDER_H

#include <Qt3DStudioRuntime2/q3dspresentation.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

class Q3DSAnimationManager
{
public:
    void updateAnimations(Q3DSSlide *animSourceSlide, Q3DSSlide *prevAnimSourceSlide, Q3DSSlide *playModeSourceSlide);
    void applyChanges();
    void clearPendingChanges();

    typedef std::function<void (Q3DSGraphObject *, const QVariant &)> SetterFunc;
    typedef std::function<QVariant (Q3DSGraphObject *)> GetterFunc;

    struct Animatable {
        QString name;
        Q3DS::PropertyType type = Q3DS::Unknown;
        int componentCount = 0;
        SetterFunc setter;
        GetterFunc getter;
    };

    struct AnimationValueChange {
        QVariant value;
        QString name;
        Q3DSAnimationManager::SetterFunc setter;
    };

    void queueChange(Q3DSGraphObject *target, const AnimationValueChange &change);

private:
    typedef QHash<QString, Animatable> AnimatableTab;

    void gatherAnimatableMeta(const QString &type, AnimatableTab *dst);
    template<class AttT, class T> void updateAnimationHelper(const QHash<T *, QVector<const Q3DSAnimationTrack *> > &targets,
                                                             AnimatableTab *animatables,
                                                             Q3DSSlide *animSourceSlide,
                                                             Q3DSSlide *prevAnimSourceSlide,
                                                             Q3DSSlide *playModeSourceSlide);

    AnimatableTab m_defaultMaterialAnimatables;
    AnimatableTab m_cameraAnimatables;
    AnimatableTab m_lightAnimatables;
    AnimatableTab m_modelAnimatables;
    AnimatableTab m_groupAnimatables;
    AnimatableTab m_componentAnimatables;
    AnimatableTab m_textAnimatables;
    AnimatableTab m_imageAnimatables;
    AnimatableTab m_layerAnimatables;

    QMultiHash<Q3DSGraphObject *, AnimationValueChange> m_changes;

    friend class Q3DSAnimationCallback;
};

inline bool operator==(const Q3DSAnimationManager::Animatable &a, const Q3DSAnimationManager::Animatable &b) { return a.name == b.name; }
inline bool operator!=(const Q3DSAnimationManager::Animatable &a, const Q3DSAnimationManager::Animatable &b) { return !(a == b); }
inline uint qHash(const Q3DSAnimationManager::Animatable &a, uint seed = 0) { return qHash(a.name, seed); }
QDebug operator<<(QDebug dbg, const Q3DSAnimationManager::Animatable &a);

QT_END_NAMESPACE

#endif // Q3DSANIMATIONBUILDER_H
