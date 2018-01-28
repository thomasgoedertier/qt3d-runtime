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

#ifndef Q3DSEXPLORERMAINWINDOW_H
#define Q3DSEXPLORERMAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE

class Q3DSWindow;
class SlideExplorerWidget;
class SceneExplorerWidget;
class Q3DSPresentation;
class Q3DSComponentNode;

class Q3DSExplorerMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit Q3DSExplorerMainWindow(Q3DSWindow *view, QWidget *parent = 0);
    ~Q3DSExplorerMainWindow();

    void updatePresentation();

    static QString fileFilter();

private Q_SLOTS:
    void handleComponentSelected(Q3DSComponentNode *component);

private:
    Q3DSWindow *m_view;
    SlideExplorerWidget *m_slideExplorer;
    SlideExplorerWidget *m_componentSlideExplorer;
    SceneExplorerWidget *m_sceneExplorer;
};

QT_END_NAMESPACE

#endif // Q3DSEXPLORERMAINWINDOW_H
