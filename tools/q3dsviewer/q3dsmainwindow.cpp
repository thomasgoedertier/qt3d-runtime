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

#include "q3dsmainwindow.h"
#include "q3dsaboutdialog.h"
#include <private/q3dswindow_p.h>
#include <private/q3dsengine_p.h>
#include <private/q3dsutils_p.h>
#include <private/q3dsslideplayer_p.h>
#include <private/q3dsviewportsettings_p.h>
#include <private/q3dsremotedeploymentmanager_p.h>
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QApplication>

QT_BEGIN_NAMESPACE

QString Q3DStudioMainWindow::fileFilter()
{
    return tr("All Supported Formats (*.uia *.uip);;Studio UI Presentation (*.uip);;Application File (*.uia);;All Files (*)");
}

Q3DStudioMainWindow::Q3DStudioMainWindow(Q3DSWindow *view, Q3DSRemoteDeploymentManager *remote, QWidget *parent)
    : QMainWindow(parent)
{
    // Load and apply stylesheet for the application
    QFile styleFile(":/resources/style.qss");
    styleFile.open(QFile::ReadOnly);
    qApp->setStyleSheet(styleFile.readAll());

    // This timer makes sure that reloads are not called more often than once per second
    m_refreshTimer.setInterval(1000);
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, [this] {
        m_okToReload = true;
    });

    static const bool enableDebugMenu = qEnvironmentVariableIntValue("Q3DS_DEBUG") >= 1;

    QWidget *wrapper = QWidget::createWindowContainer(view);
    setCentralWidget(wrapper);

    const QSize designSize = view->size(); // because Q3DSWindow::setSource() set this already

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    auto open = [=]() {
        QString dir;
        QString prevFilename = view->engine()->source();
        if (!prevFilename.isEmpty())
            dir = QFileInfo(prevFilename).canonicalPath();
        QString fn = QFileDialog::getOpenFileName(this, tr("Open"), dir, fileFilter());
        if (!fn.isEmpty())
            view->engine()->setSource(fn);
        if (remote)
            remote->setState(Q3DSRemoteDeploymentManager::LocalProject);
    };
    QAction *openAction = fileMenu->addAction(tr("&Open..."), this, [=] {
        view->engine()->setFlag(Q3DSEngine::EnableProfiling, true);
        open();
    } , QKeySequence::Open);
    fileMenu->addAction(tr("Open Without &Profiling..."), this, [=] {
        view->engine()->setFlag(Q3DSEngine::EnableProfiling, false);
        open();
    });
    addAction(openAction);
    if (remote)
        fileMenu->addAction(tr("Remote Setup"), this, [remote] {
            remote->showConnectionSetup();
        });
    QAction *reloadAction = new QAction(tr("&Reload"), this);
    reloadAction->setShortcut(QKeySequence::Refresh);
    connect(reloadAction, &QAction::triggered, [=] (){
        // Don't reload if on the ConnectionInfo screen
        if (remote &&
            remote->state() != Q3DSRemoteDeploymentManager::LocalProject &&
            remote->state() != Q3DSRemoteDeploymentManager::RemoteProject)
            return;
        if (m_okToReload) {
            view->engine()->setSource(view->engine()->source());
            m_refreshTimer.start();
            m_okToReload = false;
        }
    });
    fileMenu->addAction(reloadAction);
    addAction(reloadAction);
    QAction *exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);
    addAction(exitAction);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));
    if (enableDebugMenu) {
        QAction *forcePresSize = viewMenu->addAction(tr("&Force design size"));
        forcePresSize->setCheckable(true);
        forcePresSize->setChecked(false);
        connect(forcePresSize, &QAction::toggled, [=]() {
            if (forcePresSize->isChecked()) {
                wrapper->setMinimumSize(designSize);
                wrapper->setMaximumSize(designSize);
            } else {
                wrapper->setMinimumSize(1, 1);
                wrapper->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            }
        });
    }
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
    QAction *fullscreenAction = new QAction(tr("Full Scree&n"), this);
    connect(fullscreenAction, &QAction::triggered, [this]() {
        if (!windowState().testFlag(Qt::WindowFullScreen)) {
            showFullScreen();
            menuBar()->hide();
        } else {
            showNormal();
            menuBar()->show();
        }
    });
    fullscreenAction->setShortcut(QKeySequence::FullScreen);
    addAction(fullscreenAction);
    viewMenu->addAction(fullscreenAction);

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
    viewMenu->addMenu(profileSubMenu);

    if (enableDebugMenu) {
        QMenu *debugMenu = menuBar()->addMenu(tr("&Debug"));
        debugMenu->addAction(tr("&Object graph..."), [=]() {
            Q3DSUtils::showObjectGraph(view->engine()->presentation()->scene());
        });
        debugMenu->addAction(tr("&Scene slide graph..."), [=]() {
            Q3DSUtils::showObjectGraph(view->engine()->presentation()->masterSlide());
        });
        QAction *depthTexAction = debugMenu->addAction(tr("&Force depth texture"));
        depthTexAction->setCheckable(true);
        depthTexAction->setChecked(false);
        connect(depthTexAction, &QAction::toggled, [=]() {
            Q3DSUipPresentation::forAllLayers(view->engine()->presentation()->scene(),
                                              [=](Q3DSLayerNode *layer3DS) {
                view->engine()->sceneManager()->setDepthTextureEnabled(
                            layer3DS, depthTexAction->isChecked());
            });
        });
        QAction *ssaoAction = debugMenu->addAction(tr("Force SS&AO"));
        ssaoAction->setCheckable(true);
        ssaoAction->setChecked(false);
        connect(ssaoAction, &QAction::toggled, [=]() {
            Q3DSUipPresentation::forAllLayers(view->engine()->presentation()->scene(),
                                              [=](Q3DSLayerNode *layer3DS) {
                Q3DSPropertyChangeList changeList;
                const QString value = ssaoAction->isChecked() ? QLatin1String("50") : QLatin1String("0");
                changeList.append(Q3DSPropertyChange(QLatin1String("aostrength"), value));
                layer3DS->applyPropertyChanges(changeList);
                layer3DS->notifyPropertyChanges(changeList);
            });
        });
        QAction *rebuildMatAction = debugMenu->addAction(tr("&Rebuild model materials"));
        connect(rebuildMatAction, &QAction::triggered, [=]() {
            Q3DSUipPresentation::forAllModels(view->engine()->presentation()->scene(),
                                              [=](Q3DSModelNode *model3DS) {
                view->engine()->sceneManager()->rebuildModelMaterial(model3DS);
            });
        });
        QAction *toggleShadowAction = debugMenu->addAction(tr("&Toggle shadow casting for point lights"));
        connect(toggleShadowAction, &QAction::triggered, [=]() {
            Q3DSUipPresentation::forAllObjectsOfType(view->engine()->presentation()->scene(),
                                                     Q3DSGraphObject::Light, [=](Q3DSGraphObject *obj) {
                Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(obj);
                if (light3DS->flags().testFlag(Q3DSNode::Active) &&
                        light3DS->lightType() == Q3DSLightNode::Point) {
                    Q3DSPropertyChangeList changeList;
                    const QString value = light3DS->castShadow() ? QLatin1String("false") : QLatin1String("true");
                    changeList.append(Q3DSPropertyChange(QLatin1String("castshadow"), value));
                    light3DS->applyPropertyChanges(changeList);
                    light3DS->notifyPropertyChanges(changeList);
                }
            });
        });
        QAction *shadowResChangeAction = debugMenu->addAction(tr("&Maximize shadow map resolution for lights"));
        connect(shadowResChangeAction, &QAction::triggered, [=]() {
            Q3DSUipPresentation::forAllObjectsOfType(view->engine()->presentation()->scene(),
                                                     Q3DSGraphObject::Light, [=](Q3DSGraphObject *obj) {
                Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(obj);
                if (light3DS->flags().testFlag(Q3DSNode::Active)) {
                    Q3DSPropertyChangeList changeList;
                    const QString value = QLatin1String("11"); // 8..11
                    changeList.append(Q3DSPropertyChange(QLatin1String("shdwmapres"), value));
                    light3DS->applyPropertyChanges(changeList);
                    light3DS->notifyPropertyChanges(changeList);
                }
            });
        });
        QAction *pauseAnims = debugMenu->addAction(tr("&Pause animations"));
        pauseAnims->setCheckable(true);
        pauseAnims->setChecked(false);
        connect(pauseAnims, &QAction::toggled, [=]() {
            Q3DSSceneManager *sb = view->engine()->sceneManager();
            Q3DSSlidePlayer *player = sb->slidePlayer();
            if (player) {
                if (pauseAnims->isChecked())
                    player->pause();
                else
                    player->play();
            }
        });
        QAction *renderOnDemand = debugMenu->addAction(tr("Render on &demand only"));
        renderOnDemand->setCheckable(true);
        renderOnDemand->setChecked(false);
        connect(renderOnDemand, &QAction::toggled, [=]() {
            view->engine()->setOnDemandRendering(renderOnDemand->isChecked());
        });
    }

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, []() {
        Q3DSAboutDialog dialog;
        dialog.exec();
    });
    helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);

    // Initial size for the main window. We are sizing the main window, not the
    // embedded one (view), so add some extra height to be closer to the design
    // size. ### This should be made more accurate at some point. Note that by
    // default we are not required to strictly follow the design size, that's
    // what Force Design Size is for.
    resize(designSize + QSize(0, 40));
}

QT_END_NAMESPACE
