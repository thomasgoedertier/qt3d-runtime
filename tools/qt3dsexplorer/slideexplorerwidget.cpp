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

#include "slideexplorerwidget.h"
#include <Qt3DStudioRuntime2/q3dspresentation.h>
#include <Qt3DStudioRuntime2/q3dsscenemanager.h>
#include <QVBoxLayout>
#include <QListView>
#include <QPushButton>
#include <QSlider>
#include <QAbstractListModel>

QT_BEGIN_NAMESPACE

class SlideListModel : public QAbstractListModel
{
public:
    SlideListModel(Q3DSSlide *masterSlide)
        : m_masterSlide(masterSlide)
    {

    }
    int rowCount(const QModelIndex &parent) const override
    {
        Q_UNUSED(parent)
        return m_masterSlide->childCount();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid())
            return QVariant();

        if (role != Qt::DisplayRole)
            return QVariant();


       auto slide = static_cast<Q3DSSlide *>(index.internalPointer());
       return slide->name();

    }

    QModelIndex index(int row, int column, const QModelIndex &parent) const override
    {
        if (!hasIndex(row, column, parent))
            return QModelIndex();

        if (row >= m_masterSlide->childCount())
            return QModelIndex();

        return createIndex(row, column, m_masterSlide->childAtIndex(row));
    }

    QModelIndex getSlideIndex(Q3DSSlide *slide)
    {
        for (int i = 0; i < m_masterSlide->childCount(); ++i) {
            auto checkSlide = m_masterSlide->childAtIndex(i);
            if ( checkSlide == slide)
                return createIndex(i, 0, checkSlide);
        }

        return QModelIndex();
    }

    Q3DSSlide* getNextSlide(Q3DSSlide *slide) {
        for (int i = 0; i < m_masterSlide->childCount(); ++i) {
            auto checkSlide = m_masterSlide->childAtIndex(i);
            if ( checkSlide == slide) {
                int index = i + 1;
                if (index >= m_masterSlide->childCount())
                    index = 0;
                return static_cast<Q3DSSlide*>(m_masterSlide->childAtIndex(index));
            }
        }

        return nullptr;
    }

    Q3DSSlide* getPrevSlide(Q3DSSlide *slide) {
        for (int i = 0; i < m_masterSlide->childCount(); ++i) {
            auto checkSlide = m_masterSlide->childAtIndex(i);
            if ( checkSlide == slide) {
                int index = i - 1;
                if (index < 0)
                    index = m_masterSlide->childCount() - 1;
                return static_cast<Q3DSSlide*>(m_masterSlide->childAtIndex(index));
            }
        }

        return nullptr;
    }

private:
    Q3DSSlide *m_masterSlide;
};

SlideExplorerWidget::SlideExplorerWidget(Q3DSPresentation *presentation, Q3DSSceneManager *sceneManager, QWidget *parent)
    : QWidget(parent)
    , m_component(nullptr)
    , m_presentation(presentation)
    , m_sceneManager(sceneManager)
{
    m_masterSlide = m_presentation->masterSlide();
    m_currentSlide = m_sceneManager->currentSlide();
    init();
    handleCurrentSlideChanged(m_currentSlide);
}

SlideExplorerWidget::SlideExplorerWidget(Q3DSComponentNode *component, Q3DSSceneManager *sceneManager, QWidget *parent)
    : QWidget(parent)
    , m_component(component)
    , m_presentation(nullptr)
    , m_sceneManager(sceneManager)
{
    m_masterSlide = m_component->masterSlide();
    m_currentSlide = m_component->currentSlide();
    init();
    handleCurrentSlideChanged(m_currentSlide);
}

void SlideExplorerWidget::handleSelectionChanged(const QModelIndex &index)
{
    auto slide = static_cast<Q3DSSlide*>(index.internalPointer());
    if (slide && m_currentSlide != slide) {
        m_currentSlide = slide;
        handleCurrentSlideChanged(m_currentSlide);
    }
}

void SlideExplorerWidget::handleCurrentSlideChanged(Q3DSSlide *slide)
{
    // Set the current slide selection
    m_slideListView->setCurrentIndex(m_slideModel->getSlideIndex(slide));
    m_slideSeekSlider->setMinimum(0);
    m_slideSeekSlider->setValue(slide->startTime());
    m_slideSeekSlider->setMaximum(slide->endTime());
    if (m_presentation) {
        m_sceneManager->setCurrentSlide(slide);
    } else if (m_component) {
        m_component->setCurrentSlide(slide);
    }
    m_isSlidePlaying = true;
    m_playSlideButton->setText("stop");
}

void SlideExplorerWidget::switchToNextSlide()
{
    auto nextSlide = m_slideModel->getNextSlide(m_currentSlide);
    if (nextSlide) {
        m_currentSlide = nextSlide;
        handleCurrentSlideChanged(m_currentSlide);
    }
}

void SlideExplorerWidget::switchToPrevSlide()
{
    auto prevSlide = m_slideModel->getPrevSlide(m_currentSlide);
    if (prevSlide) {
        m_currentSlide = prevSlide;
        handleCurrentSlideChanged(m_currentSlide);
    }
}

void SlideExplorerWidget::playCurrentSlide()
{
    if (m_isSlidePlaying) {
        m_playSlideButton->setText("play");
    } else {
        m_playSlideButton->setText("stop");
    }
    m_isSlidePlaying = !m_isSlidePlaying;
    m_sceneManager->setAnimationsRunning(m_currentSlide, m_isSlidePlaying);
}

void SlideExplorerWidget::seekInCurrentSlide(int value)
{
    Q_UNUSED(value)
}

void SlideExplorerWidget::init()
{
    m_slideModel = new SlideListModel(m_masterSlide);

    auto mainLayout = new QVBoxLayout(this);
    setLayout(mainLayout);
    m_slideListView = new QListView(this);
    m_slideListView->setModel(m_slideModel);
    m_slideListView->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_slideListView, &QAbstractItemView::activated, this, &SlideExplorerWidget::handleSelectionChanged);
    mainLayout->addWidget(m_slideListView);
    m_nextSlideButton = new QPushButton("next slide", this);
    connect(m_nextSlideButton, &QPushButton::clicked, this, &SlideExplorerWidget::switchToNextSlide);
    mainLayout->addWidget(m_nextSlideButton);
    m_prevSlideButton = new QPushButton("prev slide", this);
    connect(m_prevSlideButton, &QPushButton::clicked, this, &SlideExplorerWidget::switchToPrevSlide);
    mainLayout->addWidget(m_prevSlideButton);
    m_playSlideButton = new QPushButton("play", this);
    connect(m_playSlideButton, &QPushButton::clicked, this, &SlideExplorerWidget::playCurrentSlide);
    mainLayout->addWidget(m_playSlideButton);
    m_slideSeekSlider = new QSlider(Qt::Horizontal, this);
    connect(m_slideSeekSlider, &QSlider::valueChanged, this, &SlideExplorerWidget::seekInCurrentSlide);
    mainLayout->addWidget(m_slideSeekSlider);
}

QT_END_NAMESPACE

#include "moc_slideexplorerwidget.cpp"
