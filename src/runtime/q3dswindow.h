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

#ifndef Q3DSWINDOW_H
#define Q3DSWINDOW_H

#include <Qt3DStudioRuntime2/q3dsruntimeglobal.h>
#include <QWindow>

QT_BEGIN_NAMESPACE

class Q3DSEngine;

class Q3DSV_EXPORT Q3DSWindow : public QWindow
{
    Q_OBJECT
public:
    Q3DSWindow(QWindow *parent = nullptr);
    ~Q3DSWindow();

    void setEngine(Q3DSEngine *engine);
    Q3DSEngine *engine() const;

    void forceResize(const QSize &size);
    void forceResize(int w, int h) { forceResize(QSize(w, h)); }

protected:
    void exposeEvent(QExposeEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    Q3DSEngine *m_engine = nullptr;
    bool m_implicitSizeTaken = false;
};

QT_END_NAMESPACE

#endif // Q3DSWINDOW_H
