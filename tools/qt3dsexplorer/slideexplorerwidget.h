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

#ifndef SLIDEEXPLORERWIDGET_H
#define SLIDEEXPLORERWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE

class Q3DSPresentation;
class Q3DSSceneBuilder;
class Q3DSComponentNode;
class Q3DSSlide;
class QListView;
class QSlider;
class SlideListModel;
class QPushButton;
class SlideExplorerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SlideExplorerWidget(Q3DSPresentation *presentation, Q3DSSceneBuilder *sceneBuilder, QWidget *parent = nullptr);
    explicit SlideExplorerWidget(Q3DSComponentNode *component, Q3DSSceneBuilder *sceneBuilder, QWidget *parent = nullptr);

private slots:
    void handleSelectionChanged(const QModelIndex &index);
    void handleCurrentSlideChanged(Q3DSSlide *slide);
    void switchToNextSlide();
    void switchToPrevSlide();
    void playCurrentSlide();
    void seekInCurrentSlide(int value);
private:
    void init();
    Q3DSComponentNode *m_component;
    Q3DSPresentation *m_presentation;
    Q3DSSceneBuilder *m_sceneBuilder;
    Q3DSSlide *m_masterSlide;
    Q3DSSlide *m_currentSlide;
    QListView *m_slideListView;

    QPushButton *m_nextSlideButton;
    QPushButton *m_prevSlideButton;
    QPushButton *m_playSlideButton;
    QSlider *m_slideSeekSlider;
    SlideListModel *m_slideModel;

    bool m_isSlidePlaying;
};

QT_END_NAMESPACE

#endif // SLIDEEXPLORERWIDGET_H
