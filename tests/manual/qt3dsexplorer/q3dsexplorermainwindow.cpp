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

#include <private/q3dswindow_p.h>
#include <private/q3dsengine_p.h>
#include <private/q3dsremotedeploymentmanager_p.h>
#include <private/q3dsviewportsettings_p.h>
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
#include "manualpresentationtest.h"

QT_BEGIN_NAMESPACE

QString Q3DSExplorerMainWindow::fileFilter()
{
    return tr("All Supported Formats (*.uia *.uip);;Studio UI Presentation (*.uip);;Application File (*.uia);;All Files (*)");
}

Q3DSExplorerMainWindow::Q3DSExplorerMainWindow(Q3DSWindow *view, Q3DSRemoteDeploymentManager *remote, QWidget *parent)
    : QMainWindow(parent)
    , m_view(view)
{
    QWidget *wrapper = QWidget::createWindowContainer(view);
    setCentralWidget(wrapper);

    connect(view->engine(), &Q3DSEngine::presentationLoaded, this, &Q3DSExplorerMainWindow::updatePresentation);

    // Add Dock Widgets
    auto slideDockWidget = new QDockWidget("Presentation", this);
    m_slideExplorer = new SlideExplorerWidget(slideDockWidget);
    slideDockWidget->setWidget(m_slideExplorer);
    this->addDockWidget(Qt::LeftDockWidgetArea, slideDockWidget);

    auto sceneExplorerDockWidget = new QDockWidget("Scene Explorer", this);
    m_sceneExplorer = new SceneExplorerWidget(this);
    sceneExplorerDockWidget->setWidget(m_sceneExplorer);
    this->addDockWidget(Qt::RightDockWidgetArea, sceneExplorerDockWidget);

    auto componentSlideDockWidget = new QDockWidget("Component", this);
    m_componentSlideExplorer = new SlideExplorerWidget(componentSlideDockWidget);
    componentSlideDockWidget->setWidget(m_componentSlideExplorer);
    addDockWidget(Qt::LeftDockWidgetArea, componentSlideDockWidget);
    m_componentSlideExplorer->setEnabled(false);

    tabifyDockWidget(componentSlideDockWidget, slideDockWidget);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Open..."), this, [=] {
        QString fn = QFileDialog::getOpenFileName(this, tr("Open"), QString(), fileFilter());
        if (!fn.isEmpty()) {
            view->engine()->setSource(fn);
            if (remote)
                remote->setState(Q3DSRemoteDeploymentManager::LocalProject);
        }
    }, QKeySequence::Open);
    if (remote)
        fileMenu->addAction(tr("Remote Setup"), this, [remote] {
            remote->showConnectionSetup();
        });
    fileMenu->addAction(tr("&Reload"), this, [=] {
        // Don't reload if on the ConnectionInfo screen
        if (remote &&
            remote->state() != Q3DSRemoteDeploymentManager::LocalProject &&
            remote->state() != Q3DSRemoteDeploymentManager::RemoteProject)
            return;
        view->engine()->setSource(view->engine()->source());
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
    QAction *showMatte = viewMenu->addAction(tr("Show Matte"));
    addAction(showMatte);
    showMatte->setCheckable(true);
    showMatte->setChecked(view->engine()->viewportSettings()->matteEnabled());
    showMatte->setShortcut(QKeySequence(tr("Ctrl+D")));
    connect(showMatte, &QAction::toggled, [=]() {
        view->engine()->viewportSettings()->setMatteEnabled(showMatte->isChecked());
    });
    QAction *scaleModeAction = new QAction(tr("Scale Mode"));
    addAction(scaleModeAction);
    scaleModeAction->setShortcut(QKeySequence(tr("Ctrl+Shift+S")));
    QMenu *scaleModeMenu = new QMenu();
    scaleModeAction->setMenu(scaleModeMenu);
    QAction *scaleModeCenter = new QAction(tr("Center"));
    scaleModeCenter->setCheckable(true);
    scaleModeCenter->setChecked(view->engine()->viewportSettings()->scaleMode() == Q3DSViewportSettings::ScaleModeCenter);
    scaleModeMenu->addAction(scaleModeCenter);

    QAction *scaleModeFit = new QAction(tr("Scale to Fit"));
    scaleModeFit->setCheckable(true);
    scaleModeFit->setChecked(view->engine()->viewportSettings()->scaleMode() == Q3DSViewportSettings::ScaleModeFit);
    scaleModeMenu->addAction(scaleModeFit);

    QAction *scaleModeFill = new QAction(tr("Scale to Fill"));
    scaleModeFill->setCheckable(true);
    scaleModeFill->setChecked(view->engine()->viewportSettings()->scaleMode() == Q3DSViewportSettings::ScaleModeFill);
    scaleModeMenu->addAction(scaleModeFill);

    connect(scaleModeFit, &QAction::triggered, [=]() {
        view->engine()->viewportSettings()->setScaleMode(Q3DSViewportSettings::ScaleModeFit);
        scaleModeCenter->setChecked(false);
        scaleModeFill->setChecked(false);
        scaleModeFit->setChecked(true);
    });
    connect(scaleModeCenter, &QAction::triggered, [=]() {
        view->engine()->viewportSettings()->setScaleMode(Q3DSViewportSettings::ScaleModeCenter);
        scaleModeCenter->setChecked(true);
        scaleModeFill->setChecked(false);
        scaleModeFit->setChecked(false);
    });
    connect(scaleModeFill, &QAction::triggered, [=]() {
        view->engine()->viewportSettings()->setScaleMode(Q3DSViewportSettings::ScaleModeFill);
        scaleModeCenter->setChecked(false);
        scaleModeFill->setChecked(true);
        scaleModeFit->setChecked(false);
    });
    connect(scaleModeAction, &QAction::triggered, [=]() {
        // toggle between the 3 scale modes
        if (scaleModeCenter->isChecked()) {
            scaleModeCenter->setChecked(false);
            scaleModeFit->setChecked(true);
            view->engine()->viewportSettings()->setScaleMode(Q3DSViewportSettings::ScaleModeFit);
        } else if (scaleModeFit->isChecked()) {
            scaleModeFit->setChecked(false);
            scaleModeFill->setChecked(true);
            view->engine()->viewportSettings()->setScaleMode(Q3DSViewportSettings::ScaleModeFill);
        } else {
            scaleModeFill->setChecked(false);
            scaleModeCenter->setChecked(true);
            view->engine()->viewportSettings()->setScaleMode(Q3DSViewportSettings::ScaleModeCenter);
        }
    });

    viewMenu->addMenu(scaleModeMenu);

    QAction *renderOnDemand = viewMenu->addAction(tr("&Render on demand only"));
    renderOnDemand->setCheckable(true);
    renderOnDemand->setChecked(false);
    connect(renderOnDemand, &QAction::toggled, [=]() {
        view->engine()->setOnDemandRendering(renderOnDemand->isChecked());
    });

    QMenu *devMenu = menuBar()->addMenu(tr("&Dev"));
    devMenu->addAction(tr("&Build me a scene"), this, [this, view] {
        if (!m_manualPresentationTest)
            m_manualPresentationTest = new ManualPresentationTest;
        auto p = m_manualPresentationTest->build();
        if (view->engine()->setPresentations(p))
            updatePresentation();
        else
            qDeleteAll(p);
    });
    QMenu *profileSubMenu = new QMenu(tr("&Profile and Debug"));
    QAction *showDebugView = profileSubMenu->addAction(tr("Toggle in-scene &debug view"), this, [view] {
        Q3DSEngine *engine = view->engine();
        engine->setProfileUiVisible(!engine->isProfileUiVisible());
    }, Qt::Key_F10);
    addAction(showDebugView);
    QAction *showConsole = profileSubMenu->addAction(tr("Toggle &console"), this, [view] {
        view->requestActivate(); // get key events flowing right away even without any click
        Q3DSEngine *engine = view->engine();
        engine->setProfileUiVisible(!engine->isProfileUiVisible(), true);
    }, Qt::Key_QuoteLeft);
    addAction(showConsole);
    QAction *scaleUpDebugView = profileSubMenu->addAction(tr("Scale in-scene debug view up"), this, [view] {
        Q3DSEngine *engine = view->engine();
        engine->configureProfileUi(engine->profileUiScaleFactor() + 0.2f);
    }, QKeySequence(QLatin1String("Ctrl+F10")));
    addAction(scaleUpDebugView);
    QAction *scaleDownDebugView = profileSubMenu->addAction(tr("Scale in-scene debug view down"), this, [view] {
        Q3DSEngine *engine = view->engine();
        engine->configureProfileUi(engine->profileUiScaleFactor() - 0.2f);
    }, QKeySequence(QLatin1String("Alt+F10")));
    addAction(scaleDownDebugView);
    devMenu->addMenu(profileSubMenu);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, [this]() {
        QMessageBox::about(this, tr("About q3dsexplorer"), tr("Qt 3D Studio Explorer 2.0"));
    });
    helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);

    // The view already has a desired size at this point, take it into account.
    // This won't be fully correct, use View->Force design size to get the real thing.
    resize(designSize + QSize(slideDockWidget->width() + sceneExplorerDockWidget->width(), menuBar()->height()));
    updatePresentation();

    connect(m_sceneExplorer, &SceneExplorerWidget::componentSelected, this, &Q3DSExplorerMainWindow::handleComponentSelected);
}

Q3DSExplorerMainWindow::~Q3DSExplorerMainWindow()
{
    delete m_manualPresentationTest;
}

void Q3DSExplorerMainWindow::updatePresentation()
{
    auto pres = m_view->engine()->presentation();
    m_slideExplorer->reset();
    m_sceneExplorer->reset();
    handleComponentSelected(nullptr);
    if (pres) {
        m_sceneExplorer->setPresentation(pres);
        m_slideExplorer->setPresentation(pres);
        m_slideExplorer->setSceneManager(m_view->engine()->sceneManager(0));
        m_componentSlideExplorer->setSceneManager(m_view->engine()->sceneManager(0));
    }
}

void Q3DSExplorerMainWindow::handleComponentSelected(Q3DSComponentNode *component)
{
    if (!component) {
        m_componentSlideExplorer->setEnabled(false);
        m_componentSlideExplorer->setComponent(nullptr);
        return;
    }

    m_componentSlideExplorer->setEnabled(true);
    m_componentSlideExplorer->setComponent(component);
    m_componentSlideExplorer->setSceneManager(m_view->engine()->sceneManager(0));
}

QT_END_NAMESPACE
