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

#ifndef Q3DSSTUDIO3DITEM_P_H
#define Q3DSSTUDIO3DITEM_P_H

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

#include <QQuickItem>
#include <private/q3dspresentation_p.h>

QT_BEGIN_NAMESPACE

class Q3DSStudio3DRenderer;
class Q3DSEngine;
class Q3DSPresentationItem;

class Q3DSStudio3DItem : public QQuickItem, public Q3DSPresentationController
{
    Q_OBJECT
    Q_PROPERTY(Q3DSPresentationItem *presentation READ presentation CONSTANT)

public:
    explicit Q3DSStudio3DItem(QQuickItem *parent = 0);
    ~Q3DSStudio3DItem();

    Q3DSPresentationItem *presentation() const;

private slots:
    void startEngine();
    void destroyEngine();

private:
    void handlePresentationSource(const QUrl &source) override;
    void handlePresentationReload() override;
    void handlePresentationKeyPressEvent(QKeyEvent *e) override;
    void handlePresentationKeyReleaseEvent(QKeyEvent *e) override;
    void handlePresentationMousePressEvent(QMouseEvent *e) override;
    void handlePresentationMouseMoveEvent(QMouseEvent *e) override;
    void handlePresentationMouseReleaseEvent(QMouseEvent *e) override;
    void handlePresentationMouseDoubleClickEvent(QMouseEvent *e) override;
#if QT_CONFIG(wheelevent)
    void handlePresentationWheelEvent(QWheelEvent *e) override;
#endif

    QSGNode *updatePaintNode(QSGNode *node, UpdatePaintNodeData *nodeData) override;
    void releaseResources() override;
    void itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &changeData) override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void componentComplete() override;

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
#if QT_CONFIG(wheelevent)
    void wheelEvent(QWheelEvent *event) override;
#endif
    void hoverMoveEvent(QHoverEvent *event) override;

    void createEngine();
    void sendResizeToQt3D(const QSize &size);
    void releaseEngineAndRenderer();

    Q3DSPresentationItem *m_presentation = nullptr;
    Q3DSStudio3DRenderer *m_renderer = nullptr;
    Q3DSEngine *m_engine = nullptr;
    QUrl m_source;
};

QT_END_NAMESPACE

#endif // Q3DSSTUDIO3DITEM_P_H
