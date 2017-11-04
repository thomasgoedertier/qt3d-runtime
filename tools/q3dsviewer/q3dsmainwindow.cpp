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
#include <Qt3DStudioRuntime2/q3dswindow.h>
#include <Qt3DStudioRuntime2/q3dsutils.h>
#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>

QT_BEGIN_NAMESPACE

Q3DStudioMainWindow::Q3DStudioMainWindow(Q3DStudioWindow *view, QWidget *parent)
    : QMainWindow(parent)
{
    QWidget *wrapper = QWidget::createWindowContainer(view);
    setCentralWidget(wrapper);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Open..."), this, [=] {
        QString fn = QFileDialog::getOpenFileName(this, tr("Open"), QString(),
                                                  tr("UIP Files (*.uip);;All Files (*)"));
        if (!fn.isEmpty())
            view->setUipSource(fn);
    }, QKeySequence::Open);
    fileMenu->addAction(tr("&Add subpresentation..."), this, [=] {
        QString fn = QFileDialog::getOpenFileName(this, tr("Open"), QString(),
                                                  tr("UIP Files (*.uip);;All Files (*)"));
        if (!fn.isEmpty())
            view->addSubPresentation(fn);
    });
    fileMenu->addAction(tr("&Reload"), this, [=] {
        view->setUipSource(view->uipSource());
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

    QAction *pauseAnims = viewMenu->addAction(tr("&Stop animations"));
    pauseAnims->setCheckable(true);
    pauseAnims->setChecked(false);
    connect(pauseAnims, &QAction::toggled, [=]() {
        Q3DSSceneManager *sb = view->sceneManager();
        Q3DSSlide *slide = sb->currentSlide();
        if (slide) {
            sb->setAnimationsRunning(sb->masterSlide(), !pauseAnims->isChecked());
            sb->setAnimationsRunning(slide, !pauseAnims->isChecked());
        }
    });

    QMenu *debugMenu = menuBar()->addMenu(tr("&Debug"));
    debugMenu->addAction(tr("&Object graph..."), [=]() {
        Q3DSUtils::showObjectGraph(view->uip()->presentation()->scene());
    });
    debugMenu->addAction(tr("&Scene slide graph..."), [=]() {
        Q3DSUtils::showObjectGraph(view->uip()->presentation()->masterSlide());
    });
    QAction *depthTexAction = debugMenu->addAction(tr("&Force depth texture"));
    depthTexAction->setCheckable(true);
    depthTexAction->setChecked(false);
    connect(depthTexAction, &QAction::toggled, [=]() {
        Q3DSPresentation::forAllLayers(view->uip()->presentation()->scene(), [=](Q3DSLayerNode *layer3DS) {
            view->sceneManager()->setDepthTextureEnabled(layer3DS, depthTexAction->isChecked());
        });
    });
    QAction *ssaoAction = debugMenu->addAction(tr("Force SS&AO"));
    ssaoAction->setCheckable(true);
    ssaoAction->setChecked(false);
    connect(ssaoAction, &QAction::toggled, [=]() {
        Q3DSPresentation::forAllLayers(view->uip()->presentation()->scene(), [=](Q3DSLayerNode *layer3DS) {
            Q3DSPropertyChangeList changeList;
            const QString value = ssaoAction->isChecked() ? QLatin1String("50") : QLatin1String("0");
            changeList.append(Q3DSPropertyChange(QLatin1String("aostrength"), value));
            layer3DS->applyPropertyChanges(&changeList);
            layer3DS->notifyPropertyChanges(&changeList);
        });
    });
    QAction *rebuildMatAction = debugMenu->addAction(tr("&Rebuild model materials"));
    connect(rebuildMatAction, &QAction::triggered, [=]() {
        Q3DSPresentation::forAllModels(view->uip()->presentation()->scene(), [=](Q3DSModelNode *model3DS) {
            view->sceneManager()->rebuildModelMaterial(model3DS);
        });
    });
    QAction *toggleShadowAction = debugMenu->addAction(tr("&Toggle shadow casting for point lights"));
    connect(toggleShadowAction, &QAction::triggered, [=]() {
        Q3DSPresentation::forAllObjectsOfType(view->uip()->presentation()->scene(), Q3DSGraphObject::Light, [=](Q3DSGraphObject *obj) {
            Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(obj);
            if (light3DS->flags().testFlag(Q3DSNode::Active) && light3DS->lightType() == Q3DSLightNode::Point) {
                Q3DSPropertyChangeList changeList;
                const QString value = light3DS->castShadow() ? QLatin1String("false") : QLatin1String("true");
                changeList.append(Q3DSPropertyChange(QLatin1String("castshadow"), value));
                light3DS->applyPropertyChanges(&changeList);
                light3DS->notifyPropertyChanges(&changeList);
            }
        });
    });

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, [this]() {
        QMessageBox::about(this, tr("About q3dsviewer"), tr("Qt 3D Studio Viewer 2.0"));
    });
    helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);

    // The view already has a desired size at this point, take it into account.
    // This won't be fully correct, use View->Force design size to get the real thing.
    resize(designSize + QSize(0, menuBar()->height()));
}

QT_END_NAMESPACE
