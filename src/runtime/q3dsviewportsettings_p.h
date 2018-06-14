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

#ifndef Q3DSVIEWPORTSETTINGS_P_H
#define Q3DSVIEWPORTSETTINGS_P_H

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
#include <QObject>
#include <QColor>

QT_BEGIN_NAMESPACE

class Q3DSV_PRIVATE_EXPORT Q3DSViewportSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool matteEnabled READ matteEnabled WRITE setMatteEnabled NOTIFY matteEnabledChanged)
    Q_PROPERTY(QColor matteColor READ matteColor WRITE setMatteColor NOTIFY matteColorChanged)
    Q_PROPERTY(bool showRenderStats READ isShowingRenderStats WRITE setShowRenderStats NOTIFY showRenderStatsChanged)
    Q_PROPERTY(ScaleMode scaleMode READ scaleMode WRITE setScaleMode NOTIFY scaleModeChanged)

public:
    enum ScaleMode {
        ScaleModeFit,
        ScaleModeFill,
        ScaleModeCenter
    };
    Q_ENUM(ScaleMode)

    explicit Q3DSViewportSettings(QObject *parent = nullptr);

    bool matteEnabled() const;
    QColor matteColor() const;
    bool isShowingRenderStats() const;
    ScaleMode scaleMode() const;

public Q_SLOTS:
    void setMatteEnabled(bool isEnabled);
    void setMatteColor(const QColor &color);
    void setShowRenderStats(bool show);
    void setScaleMode(ScaleMode mode);

Q_SIGNALS:
    void matteEnabledChanged();
    void matteColorChanged();
    void showRenderStatsChanged();
    void scaleModeChanged();

private:
    Q_DISABLE_COPY(Q3DSViewportSettings)

    bool m_matteEnabled = false;
    QColor m_matteColor = QColor(51, 51, 51);
    bool m_showRenderStats = false;
    Q3DSViewportSettings::ScaleMode m_scaleMode = ScaleModeFill;
};

QT_END_NAMESPACE

#endif
