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

class Q3DSUipPresentation;
class Q3DSSceneManager;
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
    explicit SlideExplorerWidget(QWidget *parent = nullptr);

    void setPresentation(Q3DSUipPresentation *pres);
    void setComponent(Q3DSComponentNode *component);
    void setSceneManager(Q3DSSceneManager *sceneManager);

    void reset();

private slots:
    void handleSelectionChanged(const QModelIndex &index);
    void handleCurrentSlideChanged(Q3DSSlide *slide, Q3DSSlide *oldSlide);
    void switchToNextSlide();
    void switchToPrevSlide();
    void playCurrentSlide();
    void seekInCurrentSlide(int value);
private:
    void init();
    void updateModel();
    Q3DSComponentNode *m_component = nullptr;
    Q3DSUipPresentation *m_presentation = nullptr;
    Q3DSSceneManager *m_sceneManager = nullptr;
    Q3DSSlide *m_masterSlide = nullptr;
    Q3DSSlide *m_currentSlide = nullptr;
    QListView *m_slideListView;

    QPushButton *m_nextSlideButton;
    QPushButton *m_prevSlideButton;
    QPushButton *m_playSlideButton;
    QSlider *m_slideSeekSlider;
    SlideListModel *m_slideModel;

    bool m_isSlidePlaying = false;
    bool m_slideAtEnd = false;
};

QT_END_NAMESPACE

#endif // SLIDEEXPLORERWIDGET_H
