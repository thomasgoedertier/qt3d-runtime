/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
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

#ifndef Q3DSVIEWERSETTINGS_P_H
#define Q3DSVIEWERSETTINGS_P_H

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

#include <private/q3dsruntimeglobal_p.h>
#include "q3dsviewersettings.h"
#include <private/qobject_p.h>
#include <private/q3dsviewportsettings_p.h>

QT_BEGIN_NAMESPACE

class Q3DSV_PRIVATE_EXPORT Q3DSViewerSettingsPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(Q3DSViewerSettings)

public:
    static Q3DSViewerSettingsPrivate *get(Q3DSViewerSettings *p) { return p->d_func(); }

    Q3DSViewportSettings *createViewportSettingsProxy();

    bool matteEnabled = false;
    QColor matteColor = QColor(51, 51, 51);
    bool showRenderStats = false;
    Q3DSViewerSettings::ShadeMode shadeMode = Q3DSViewerSettings::ShadeModeShaded;
    Q3DSViewerSettings::ScaleMode scaleMode = Q3DSViewerSettings::ScaleModeFill;
};

QT_END_NAMESPACE

#endif // Q3DSVIEWERSETTINGS_P_H
