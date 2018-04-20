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

#ifndef Q3DSWIDGET_H
#define Q3DSWIDGET_H

#include <Qt3DStudioRuntime2/q3dsruntimeglobal.h>
#include <QtWidgets/qopenglwidget.h>

QT_BEGIN_NAMESPACE

class Q3DSWidgetPrivate;
class Q3DSPresentation;

class Q3DSV_EXPORT Q3DSWidget : public QOpenGLWidget
{
    Q_OBJECT
    Q_PROPERTY(Q3DSPresentation *presentation READ presentation CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(int updateInterval READ updateInterval WRITE setUpdateInterval NOTIFY updateIntervalChanged)

public:
    explicit Q3DSWidget(QWidget *parent = nullptr);
    ~Q3DSWidget();

    Q3DSPresentation *presentation() const;

    QString error() const;
    bool isRunning() const;

    int updateInterval() const;
    void setUpdateInterval(int interval);

Q_SIGNALS:
    void presentationLoaded();
    void frameUpdate();
    void errorChanged();
    void runningChanged();
    void updateIntervalChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
#if QT_CONFIG(wheelevent)
    void wheelEvent(QWheelEvent *event) override;
#endif

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    Q_DISABLE_COPY(Q3DSWidget)
    Q_DECLARE_PRIVATE(Q3DSWidget)
    // QOpenGLWidgetPrivate is not exposed/exported unfortunately so have our own d pointer
    Q3DSWidgetPrivate *d_ptr;
};

QT_END_NAMESPACE

#endif // Q3DSWIDGET_H
