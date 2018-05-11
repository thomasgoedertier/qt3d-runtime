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

#ifndef Q3DSSURFACEVIEWER_H
#define Q3DSSURFACEVIEWER_H

#include <Qt3DStudioRuntime2/q3dsruntimeglobal.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtGui/qimage.h>

QT_BEGIN_NAMESPACE

class Q3DSSurfaceViewerPrivate;
class QSurface;
class QOpenGLContext;
class Q3DSPresentation;
class Q3DSViewerSettings;

// hack. no clue why Cpp.ignoretokens does not work.
#ifdef Q_CLANG_QDOC
#define Q3DSV_EXPORT
#endif

class Q3DSV_EXPORT Q3DSSurfaceViewer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QSize size READ size WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(bool autoSize READ autoSize WRITE setAutoSize NOTIFY autoSizeChanged)
    Q_PROPERTY(int updateInterval READ updateInterval WRITE setUpdateInterval NOTIFY updateIntervalChanged)

public:
    explicit Q3DSSurfaceViewer(QObject *parent = nullptr);
    ~Q3DSSurfaceViewer();

    bool create(QSurface *surface, QOpenGLContext *context);
    bool create(QSurface *surface, QOpenGLContext *context, uint fboId);
    void destroy();

    Q3DSPresentation *presentation() const;
    Q3DSViewerSettings *settings() const;

    QString error() const;
    bool isRunning() const;

    QSize size() const;
    void setSize(const QSize &size);

    bool autoSize() const;
    void setAutoSize(bool autoSize);

    int updateInterval() const;
    void setUpdateInterval(int interval);

    uint fboId() const;
    QSurface *surface() const;
    QOpenGLContext *context() const;

    QImage grab(const QRect &rect = QRect());

public Q_SLOTS:
    void update();

Q_SIGNALS:
    void presentationLoaded();
    void frameUpdate();
    void errorChanged();
    void runningChanged();
    void sizeChanged();
    void autoSizeChanged();
    void updateIntervalChanged();

protected:
    Q3DSSurfaceViewer(Q3DSSurfaceViewerPrivate &dd, QObject *parent);

private:
    Q_DISABLE_COPY(Q3DSSurfaceViewer)
    Q_DECLARE_PRIVATE(Q3DSSurfaceViewer)
};

QT_END_NAMESPACE

#endif // Q3DSSURFACEVIEWER_H
