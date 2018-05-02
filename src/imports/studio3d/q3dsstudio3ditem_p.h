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
class Q3DSViewerSettings;

class Q3DSStudio3DItem : public QQuickItem, public Q3DSPresentationController
{
    Q_OBJECT
    Q_PROPERTY(Q3DSPresentationItem *presentation READ presentation CONSTANT)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(EventIgnoreFlags ignoredEvents READ ignoredEvents WRITE setIgnoredEvents NOTIFY ignoredEventsChanged)

public:
    enum EventIgnoreFlag {
        EnableAllEvents = 0,
        IgnoreMouseEvents = 0x01,
        IgnoreWheelEvents = 0x02,
        IgnoreKeyboardEvents = 0x04,
        IgnoreAllInputEvents = IgnoreMouseEvents | IgnoreWheelEvents | IgnoreKeyboardEvents
    };
    Q_DECLARE_FLAGS(EventIgnoreFlags, EventIgnoreFlag)
    Q_FLAG(EventIgnoreFlags)

    explicit Q3DSStudio3DItem(QQuickItem *parent = 0);
    ~Q3DSStudio3DItem();

    Q3DSPresentationItem *presentation() const;
    bool isRunning() const;
    QString error() const;

    EventIgnoreFlags ignoredEvents() const;
    void setIgnoredEvents(EventIgnoreFlags flags);

signals:
    void runningChanged();
    void errorChanged();
    void ignoredEventsChanged();
    void presentationReady();
    void frameUpdate();

private slots:
    void startEngine();
    void destroyEngine();

private:
    void handlePresentationSource(const QUrl &source,
                                  SourceFlags flags,
                                  const QVector<Q3DSInlineQmlSubPresentation *> &inlineSubPres) override;
    void handlePresentationReload() override;

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

    void updateEventMasks();
    void createEngine();
    void sendResizeToQt3D(const QSize &size);
    void releaseEngineAndRenderer();

    Q3DSPresentationItem *m_presentation = nullptr;
    Q3DSViewerSettings *m_viewerSettings = nullptr;
    Q3DSStudio3DRenderer *m_renderer = nullptr;
    Q3DSEngine *m_engine = nullptr;
    QUrl m_source;
    SourceFlags m_sourceFlags;
    QVector<Q3DSInlineQmlSubPresentation *> m_inlineQmlSubPresentations;
    bool m_sourceLoaded = false;
    bool m_running = false;
    QString m_error;
    EventIgnoreFlags m_eventIgnoreFlags;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DSStudio3DItem::EventIgnoreFlags)

QT_END_NAMESPACE

#endif // Q3DSSTUDIO3DITEM_P_H
