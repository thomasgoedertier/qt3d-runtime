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

#include "q3dsviewportsettings_p.h"

QT_BEGIN_NAMESPACE

Q3DSViewportSettings::Q3DSViewportSettings(QObject *parent)
    : QObject(parent)
{
}

bool Q3DSViewportSettings::matteEnabled() const
{
    return m_matteEnabled;
}

QColor Q3DSViewportSettings::matteColor() const
{
    return m_matteColor;
}

bool Q3DSViewportSettings::isShowingRenderStats() const
{
    return m_showRenderStats;
}

Q3DSViewportSettings::ScaleMode Q3DSViewportSettings::scaleMode() const
{
    return m_scaleMode;
}

void Q3DSViewportSettings::setMatteEnabled(bool isEnabled)
{
    if (m_matteEnabled != isEnabled) {
        m_matteEnabled = isEnabled;
        emit matteEnabledChanged();
    }
}

void Q3DSViewportSettings::setMatteColor(const QColor &color)
{
    if (m_matteColor != color) {
        m_matteColor = color;
        emit matteColorChanged();
    }
}

void Q3DSViewportSettings::setShowRenderStats(bool show)
{
    if (m_showRenderStats != show) {
        m_showRenderStats = show;
        emit showRenderStatsChanged();
    }
}

void Q3DSViewportSettings::setScaleMode(Q3DSViewportSettings::ScaleMode mode)
{
    if (m_scaleMode != mode) {
        m_scaleMode = mode;
        emit scaleModeChanged();
    }
}

QT_END_NAMESPACE
