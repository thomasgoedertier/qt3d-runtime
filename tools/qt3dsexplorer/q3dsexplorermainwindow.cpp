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

#include "q3dsexplorermainwindow.h"

#include <Qt3DStudioRuntime2/q3dswindow.h>
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QDockWidget>
#include "slideexplorerwidget.h"
#include "sceneexplorerwidget.h"

Q3DSExplorerMainWindow::Q3DSExplorerMainWindow(Q3DStudioWindow *view, QWidget *parent)
    : QMainWindow(parent)
    , m_view(view)
{
    QWidget *wrapper = QWidget::createWindowContainer(view);
    setCentralWidget(wrapper);

    // Add Dock Widgets
    auto slideDockWidget = new QDockWidget("Presentation Slide Explorer", this);
    m_slideExplorer = new SlideExplorerWidget(m_view->sceneManager(), slideDockWidget);
    slideDockWidget->setWidget(m_slideExplorer);
    this->addDockWidget(Qt::LeftDockWidgetArea, slideDockWidget);

    auto sceneExplorerDockWidget = new QDockWidget("Scene Explorer", this);
    m_sceneExplorer = new SceneExplorerWidget(this);
    sceneExplorerDockWidget->setWidget(m_sceneExplorer);
    this->addDockWidget(Qt::RightDockWidgetArea, sceneExplorerDockWidget);


    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Open..."), this, [=] {
        QString fn = QFileDialog::getOpenFileName(this, tr("Open"), QString(),
                                                  tr("UIP Files (*.uip);;All Files (*)"));
        if (!fn.isEmpty()) {
            view->setUipSource(fn);
            updatePresentation();
        }
    }, QKeySequence::Open);
    fileMenu->addAction(tr("&Reload"), this, [=] {
        view->setUipSource(view->uipSource());
        updatePresentation();
    }, QKeySequence::Refresh);
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    QAction *forcePresSize = viewMenu->addAction(tr("&Force design size"));
    forcePresSize->setCheckable(true);
    forcePresSize->setChecked(false);
    const QSize designSize = view->size();
    connect(forcePresSize, &QAction::toggled, [=]() {
        if (forcePresSize->isChecked()) {
            wrapper->setMinimumSize(designSize);
            wrapper->setMaximumSize(designSize);
        } else {
            wrapper->setMinimumSize(1, 1);
            wrapper->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        }
    });

    QAction *renderOnDemand = viewMenu->addAction(tr("&Render on demand only"));
    renderOnDemand->setCheckable(true);
    renderOnDemand->setChecked(false);
    connect(renderOnDemand, &QAction::toggled, [=]() {
        view->setOnDemandRendering(renderOnDemand->isChecked());
    });

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, [this]() {
        QMessageBox::about(this, tr("About q3dsviewer"), tr("Qt 3D Studio Viewer 2.0"));
    });
    helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);

    // The view already has a desired size at this point, take it into account.
    // This won't be fully correct, use View->Force design size to get the real thing.
    resize(designSize + QSize(slideDockWidget->width() + sceneExplorerDockWidget->width(), menuBar()->height()));
    updatePresentation();
}

Q3DSExplorerMainWindow::~Q3DSExplorerMainWindow()
{
}

void Q3DSExplorerMainWindow::updatePresentation()
{
    auto pres = m_view->uip()->presentation();
    if (pres) {
        m_sceneExplorer->setPresentation(pres);
        m_slideExplorer->setPresentation(pres);
    }
}