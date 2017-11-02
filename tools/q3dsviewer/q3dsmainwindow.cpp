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
#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

QT_BEGIN_NAMESPACE

class Q3DStudioSlideWindow : public QWidget
{
public:
    Q3DStudioSlideWindow(Q3DStudioWindow *view, bool component);

private:
};

Q3DStudioSlideWindow::Q3DStudioSlideWindow(Q3DStudioWindow *view, bool component)
{
    auto pres = view->uip()->presentation();
    QGridLayout *layout = new QGridLayout;
    bool canChange = true;
    QComboBox *slideSource = nullptr, *compSource = nullptr;

    if (!component) {
        QHBoxLayout *slideSelLayout = new QHBoxLayout;
        slideSelLayout->addWidget(new QLabel(tr("Scene slide")), 2);
        QComboBox *slideSel = new QComboBox;
        slideSource = slideSel;
        slideSelLayout->addWidget(slideSel, 8);
        Q3DSGraphObject *slide = pres->masterSlide()->firstChild();
        while (slide) {
            slideSel->addItem(QString::fromUtf8(slide->id()));
            if (view->sceneManager()->currentSlide() == static_cast<Q3DSSlide *>(slide))
                slideSel->setCurrentIndex(slideSel->count() - 1);
            slide = slide->nextSibling();
        }
        layout->addLayout(slideSelLayout, 0, 0, 5, 2);
    } else {
        QHBoxLayout *compSelLayout = new QHBoxLayout;
        compSelLayout->addWidget(new QLabel(tr("Component")), 2);
        QComboBox *compSel = new QComboBox;
        compSource = compSel;
        compSelLayout->addWidget(compSel, 8);
        Q3DSPresentation::forAllObjectsOfType(pres->scene(), Q3DSGraphObject::Component, [=](Q3DSGraphObject *obj) {
            compSel->addItem(QString::fromUtf8(obj->id()));
        });
        QHBoxLayout *compSlideSelLayout = new QHBoxLayout;
        compSlideSelLayout->addWidget(new QLabel(tr("Slide")), 2);
        QComboBox *compSlideSel = new QComboBox;
        slideSource = compSlideSel;
        compSlideSelLayout->addWidget(compSlideSel, 8);
        auto compSlideUpdate = [=]() {
            if (compSel->count()) {
                compSlideSel->clear();
                Q3DSComponentNode *comp = static_cast<Q3DSComponentNode *>(pres->object(compSel->currentText().toUtf8()));
                Q_ASSERT(comp);
                Q3DSGraphObject *slide = comp->masterSlide()->firstChild();
                while (slide) {
                    compSlideSel->addItem(QString::fromUtf8(slide->id()));
                    if (comp->currentSlide() == static_cast<Q3DSSlide *>(slide))
                        compSlideSel->setCurrentIndex(compSlideSel->count() - 1);
                    slide = slide->nextSibling();
                }
            }
        };
        connect(compSel, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [=]() { compSlideUpdate(); });
        compSlideUpdate();
        layout->addLayout(compSelLayout, 0, 0, 2, 2);
        layout->addLayout(compSlideSelLayout, 3, 0, 2, 2);
        canChange = compSel->count() > 0;
    }

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"));
    cancelBtn->setShortcut(QKeySequence::Cancel);
    connect(cancelBtn, &QPushButton::clicked, [this] { close(); });
    if (canChange) {
        QPushButton *okBtn = new QPushButton(tr("Ok"));
        buttons->addWidget(okBtn);
        connect(okBtn, &QPushButton::clicked, [=]() {
#if 0
            Q3DSSlide *s = static_cast<Q3DSSlide *>(pres->object(slideSource->currentText().toUtf8()));
            Q_ASSERT(s);
            if (!component) {
                view->sceneBuilder()->setCurrentSlide(s);
            } else {
                Q3DSComponentNode *c = static_cast<Q3DSComponentNode *>(pres->object(compSource->currentText().toUtf8()));
                Q_ASSERT(c);
                c->setCurrentSlide(s, view->sceneBuilder(), pres);
            }
#else
            Q_UNUSED(slideSource);
            Q_UNUSED(compSource);
#endif
            close();
        });
    } else {
        Q3DSUtils::showMessage(tr("No components"));
    }
    buttons->addWidget(cancelBtn);
    layout->addLayout(buttons, 8, 0, 1, 2);

    setLayout(layout);

    resize(500, 300);
}

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

#if 0
    viewMenu->addAction(tr("&Next slide"), [=]() {
        Q3DSPresentation *pres = view->uip()->presentation();
        if (pres->currentSlide() && pres->currentSlide()->nextSibling())
            pres->setCurrentSlide(static_cast<Q3DSSlide *>(pres->currentSlide()->nextSibling()), view->sceneBuilder());
    });
    viewMenu->addAction(tr("&Previous slide"), [=]() {
        Q3DSPresentation *pres = view->uip()->presentation();
        if (pres->currentSlide() && pres->currentSlide()->previousSibling())
            pres->setCurrentSlide(static_cast<Q3DSSlide *>(pres->currentSlide()->previousSibling()), view->sceneBuilder());
    });
    QMenu *enterSlideSubMenu = viewMenu->addMenu(tr("&Enter slide"));
    enterSlideSubMenu->addAction(tr("Scene slides"), [=]() {
        Q3DStudioSlideWindow *slideSelect = new Q3DStudioSlideWindow(view, false);
        slideSelect->show();
    });
    enterSlideSubMenu->addAction(tr("Component slides"), [=]() {
        Q3DStudioSlideWindow *slideSelect = new Q3DStudioSlideWindow(view, true);
        slideSelect->show();
    });
#endif

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
    QAction *ssaoTexAction = debugMenu->addAction(tr("Force SS&AO texture"));
    ssaoTexAction->setCheckable(true);
    ssaoTexAction->setChecked(false);
    connect(ssaoTexAction, &QAction::toggled, [=]() {
        Q3DSPresentation::forAllLayers(view->uip()->presentation()->scene(), [=](Q3DSLayerNode *layer3DS) {
            view->sceneManager()->setSsaoTextureEnabled(layer3DS, ssaoTexAction->isChecked());
        });
    });
    QAction *rebuildMatAction = debugMenu->addAction(tr("&Rebuild model materials"));
    connect(rebuildMatAction, &QAction::triggered, [=]() {
        Q3DSPresentation::forAllModels(view->uip()->presentation()->scene(), [=](Q3DSModelNode *model3DS) {
            view->sceneManager()->rebuildModelMaterial(model3DS);
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
