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
    : QObject(*new Q3DSViewerSettingsPrivate, parent)
{
}

Q3DSViewerSettings::Q3DSViewerSettings(Q3DSViewerSettingsPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
}

Q3DSViewerSettings::~Q3DSViewerSettings()
{
}

QColor Q3DSViewerSettings::matteColor() const
{
    Q_D(const Q3DSViewerSettings);
    return d->matteColor;
}

bool Q3DSViewerSettings::isShowingRenderStats() const
{
    Q_D(const Q3DSViewerSettings);
    return d->showRenderStats;
}

Q3DSViewerSettings::ShadeMode Q3DSViewerSettings::shadeMode() const
{
    Q_D(const Q3DSViewerSettings);
    return d->shadeMode;
}

Q3DSViewerSettings::ScaleMode Q3DSViewerSettings::scaleMode() const
{
    Q_D(const Q3DSViewerSettings);
    return d->scaleMode;
}

void Q3DSViewerSettings::setMatteColor(const QColor &color)
{
    Q_D(Q3DSViewerSettings);
    if (d->matteColor != color) {
        qWarning() << Q_FUNC_INFO << "not implemented";
        d->matteColor = color;
        emit matteColorChanged();
    }
}

void Q3DSViewerSettings::setShowRenderStats(bool show)
{
    Q_D(Q3DSViewerSettings);
    if (d->showRenderStats != show) {
        d->showRenderStats = show;
        emit showRenderStatsChanged();
    }
}

void Q3DSViewerSettings::setShadeMode(Q3DSViewerSettings::ShadeMode mode)
{
    Q_D(Q3DSViewerSettings);
    if (d->shadeMode != mode) {
        qWarning() << Q_FUNC_INFO << "not implemented";
        d->shadeMode = mode;
        emit shadeModeChanged();
    }
}

void Q3DSViewerSettings::setScaleMode(Q3DSViewerSettings::ScaleMode mode)
{
    Q_D(Q3DSViewerSettings);
    if (d->scaleMode != mode) {
        qWarning() << Q_FUNC_INFO << "not implemented";
        d->scaleMode = mode;
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
