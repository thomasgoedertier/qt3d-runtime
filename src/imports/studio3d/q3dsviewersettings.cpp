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

#include "q3dsviewersettings_p.h"
#include <QDebug>

QT_BEGIN_NAMESPACE

Q3DSViewerSettings::Q3DSViewerSettings(QObject *parent)
    : QObject(parent)
{
}

Q3DSViewerSettings::~Q3DSViewerSettings()
{
}

QColor Q3DSViewerSettings::matteColor() const
{
    return m_matteColor;
}

bool Q3DSViewerSettings::isShowRenderStats() const
{
    return m_showRenderStats;
}

Q3DSViewerSettings::ShadeMode Q3DSViewerSettings::shadeMode() const
{
    return m_shadeMode;
}

Q3DSViewerSettings::ScaleMode Q3DSViewerSettings::scaleMode() const
{
    return m_scaleMode;
}

void Q3DSViewerSettings::setMatteColor(const QColor &color)
{
    if (m_matteColor != color) {
        qWarning() << Q_FUNC_INFO << "not implemented";
        m_matteColor = color;
        emit matteColorChanged();
    }
}

void Q3DSViewerSettings::setShowRenderStats(bool show)
{
    if (m_showRenderStats != show) {
        m_showRenderStats = show;
        emit showRenderStatsChanged();
    }
}

void Q3DSViewerSettings::setShadeMode(Q3DSViewerSettings::ShadeMode mode)
{
    if (m_shadeMode != mode) {
        qWarning() << Q_FUNC_INFO << "not implemented";
        m_shadeMode = mode;
        emit shadeModeChanged();
    }
}

void Q3DSViewerSettings::setScaleMode(Q3DSViewerSettings::ScaleMode mode)
{
    if (m_scaleMode != mode) {
        qWarning() << Q_FUNC_INFO << "not implemented";
        m_scaleMode = mode;
        emit scaleModeChanged();
    }
}

void Q3DSViewerSettings::save(const QString &group,
                              const QString &organization,
                              const QString &application)
{
    Q_UNUSED(group);
    Q_UNUSED(organization);
    Q_UNUSED(application);
    qWarning() << Q_FUNC_INFO << "not implemented";
}

void Q3DSViewerSettings::load(const QString &group,
                              const QString &organization,
                              const QString &application)
{
    Q_UNUSED(group);
    Q_UNUSED(organization);
    Q_UNUSED(application);
    qWarning() << Q_FUNC_INFO << "not implemented";
}

QT_END_NAMESPACE
