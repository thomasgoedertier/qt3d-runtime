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

#ifndef Q3DSANIMATIONBUILDER_P_H
#define Q3DSANIMATIONBUILDER_P_H

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

#include "q3dsslideplayer_p.h"
#include "q3dsuippresentation_p.h"
#include <QtCore/qmetaobject.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

class Q3DSAnimationManager
{
public:
    using AnimationTrackList = QVector<const Q3DSAnimationTrack *>;
    using AnimationTrackListMap = QHash<Q3DSGraphObject *, AnimationTrackList>;

    void updateAnimations(Q3DSSlide *slide, bool editorMode = false);
    void clearAnimations(Q3DSSlide *slide);
    void applyChanges();
    void clearPendingChanges();
    void objectAboutToBeRemovedFromScene(Q3DSGraphObject *obj);

    struct AnimationValueChange {
        QVariant value;
        QString propertyName;
        QMetaProperty property;
    };

    void queueChange(Q3DSGraphObject *target, const AnimationValueChange &change);

private:
    void updateAnimationHelper(const AnimationTrackListMap &targets,
                               Q3DSSlide *slide,
                               bool editorMode);

    void buildClipAnimator(Q3DSSlide *slide);

    QMultiHash<Q3DSGraphObject *, AnimationValueChange> m_changes;

    QSet<Q3DSGraphObject *> m_activeTargets;

    friend class Q3DSAnimationCallback;
};

QT_END_NAMESPACE

#endif // Q3DSANIMATIONBUILDER_P_H
