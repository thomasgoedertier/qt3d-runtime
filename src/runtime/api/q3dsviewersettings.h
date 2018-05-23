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

#ifndef Q3DSVIEWERSETTINGS_H
#define Q3DSVIEWERSETTINGS_H

#include <Qt3DStudioRuntime2/q3dsruntimeglobal.h>
#include <QObject>
#include <QColor>

QT_BEGIN_NAMESPACE

class Q3DSViewerSettingsPrivate;

// hack. no clue why Cpp.ignoretokens does not work.
#ifdef Q_CLANG_QDOC
#define Q3DSV_EXPORT
#endif

class Q3DSV_EXPORT Q3DSViewerSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool matteEnabled READ matteEnabled WRITE setMatteEnabled NOTIFY matteEnabledChanged)
    Q_PROPERTY(QColor matteColor READ matteColor WRITE setMatteColor NOTIFY matteColorChanged)
    Q_PROPERTY(bool showRenderStats READ isShowingRenderStats WRITE setShowRenderStats NOTIFY showRenderStatsChanged)
    Q_PROPERTY(ShadeMode shadeMode READ shadeMode WRITE setShadeMode NOTIFY shadeModeChanged)
    Q_PROPERTY(ScaleMode scaleMode READ scaleMode WRITE setScaleMode NOTIFY scaleModeChanged)

public:
    enum ShadeMode {
        ShadeModeShaded,
        ShadeModeShadedWireframe
    };
    Q_ENUM(ShadeMode)

    enum ScaleMode {
        ScaleModeFit,
        ScaleModeFill,
        ScaleModeCenter
    };
    Q_ENUM(ScaleMode)

    explicit Q3DSViewerSettings(QObject *parent = nullptr);
    ~Q3DSViewerSettings();

    bool matteEnabled() const;
    QColor matteColor() const;
    bool isShowingRenderStats() const;
    ShadeMode shadeMode() const;
    ScaleMode scaleMode() const;

    Q_INVOKABLE void save(const QString &group,
                          const QString &organization = QString(),
                          const QString &application = QString());
    Q_INVOKABLE void load(const QString &group,
                          const QString &organization = QString(),
                          const QString &application = QString());

public Q_SLOTS:
    void setMatteEnabled(bool isEnabled);
    void setMatteColor(const QColor &color);
    void setShowRenderStats(bool show);
    void setShadeMode(ShadeMode mode);
    void setScaleMode(ScaleMode mode);

Q_SIGNALS:
    void matteEnabledChanged();
    void matteColorChanged();
    void showRenderStatsChanged();
    void shadeModeChanged();
    void scaleModeChanged();

protected:
    Q3DSViewerSettings(Q3DSViewerSettingsPrivate &dd, QObject *parent);

private:
    Q_DISABLE_COPY(Q3DSViewerSettings)
    Q_DECLARE_PRIVATE(Q3DSViewerSettings)
};

QT_END_NAMESPACE

#endif // Q3DSVIEWERSETTINGS_H
