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

#ifndef Q3DSWIDGET_P_H
#define Q3DSWIDGET_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the QtStudio3D API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/q3dsruntimeglobal_p.h>
#include "q3dswidget.h"
#include "q3dspresentation_p.h"
#include "q3dsviewersettings_p.h"
#include <QTimer>

QT_BEGIN_NAMESPACE

class Q3DSEngine;

namespace Qt3DRender {
class QRenderAspect;
}

class Q3DSV_PRIVATE_EXPORT Q3DSWidgetPrivate : public Q3DSPresentationController
{
    Q_DECLARE_PUBLIC(Q3DSWidget)

public:
    Q3DSWidgetPrivate(Q3DSWidget *q);
    ~Q3DSWidgetPrivate();

    static Q3DSWidgetPrivate *get(Q3DSWidget *w) { return w->d_func(); }

    void createEngine();
    void destroyEngine();
    void sendResizeToQt3D(const QSize &size);

    void handlePresentationSource(const QUrl &source,
                                  SourceFlags flags,
                                  const QVector<Q3DSInlineQmlSubPresentation *> &inlineSubPres) override;
    void handlePresentationReload() override;

    Q3DSWidget *q_ptr;
    Q3DSPresentation *presentation;
    Q3DSViewerSettings *viewerSettings;
    Q3DSEngine *engine = nullptr;
    QUrl source;
    SourceFlags sourceFlags;
    QVector<Q3DSInlineQmlSubPresentation *> inlineQmlSubPresentations;
    bool sourceLoaded = false;
    QString error;
    Qt3DRender::QRenderAspect *renderAspect = nullptr;
    bool needsInit = false;
    int updateInterval = 0; // enable automatic updates by default
    QTimer updateTimer;
};

QT_END_NAMESPACE

#endif // Q3DSWIDGET_P_H
