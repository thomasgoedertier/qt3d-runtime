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
#include <private/q3dspresentation_p.h>
#include <private/q3dsscenemanager_p.h>
#include <QVBoxLayout>
#include <QListView>
#include <QPushButton>
#include <QSlider>
#include <QAbstractListModel>
#include <Qt3DAnimation/qclipanimator.h>
#include <Qt3DAnimation/qabstractanimationclip.h>
#include <Qt3DCore/qnode.h>

QT_BEGIN_NAMESPACE

class SlideListModel : public QAbstractListModel
{
public:
    void setMasterSlide(Q3DSSlide *slide)
    {
        beginResetModel();
        m_masterSlide = slide;
        endResetModel();
    }

    int rowCount(const QModelIndex &parent) const override
    {
        Q_UNUSED(parent)
        if (!m_masterSlide)
            return 0;
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
        if (!m_masterSlide)
            return QModelIndex();

        if (!hasIndex(row, column, parent))
            return QModelIndex();

        if (row >= m_masterSlide->childCount())
            return QModelIndex();

        return createIndex(row, column, m_masterSlide->childAtIndex(row));
    }

    QModelIndex getSlideIndex(Q3DSSlide *slide)
    {
        if (!m_masterSlide)
            return QModelIndex();

        for (int i = 0; i < m_masterSlide->childCount(); ++i) {
            auto checkSlide = m_masterSlide->childAtIndex(i);
            if ( checkSlide == slide)
                return createIndex(i, 0, checkSlide);
        }

        return QModelIndex();
    }

    Q3DSSlide* getNextSlide(Q3DSSlide *slide) {
        if (!m_masterSlide)
            return nullptr;

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
        if (!m_masterSlide)
            return nullptr;

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
    Q3DSSlide *m_masterSlide = nullptr;
};

SlideExplorerWidget::SlideExplorerWidget(QWidget *parent)
    : QWidget(parent)
{
    init();
}

void SlideExplorerWidget::setPresentation(Q3DSPresentation *pres)
{
    m_presentation = pres;
    m_component = nullptr;

    if (!pres) {
    reset();
        return;
    }

    updateModel();
}

void SlideExplorerWidget::setComponent(Q3DSComponentNode *component)
{
    m_presentation = nullptr;
    m_component = component;
    if (!component) {
        reset();
        return;
    }

    updateModel();
}

void SlideExplorerWidget::setSceneManager(Q3DSSceneManager *sceneManager)
{
    m_sceneManager = sceneManager;
    if (!sceneManager) {
        reset();
        return;
    }

    updateModel();
}

void SlideExplorerWidget::reset()
{
    m_masterSlide = nullptr;
    m_currentSlide = nullptr;
    m_sceneManager = nullptr;
    m_slideModel->setMasterSlide(nullptr);
}

void SlideExplorerWidget::handleSelectionChanged(const QModelIndex &index)
{
    auto slide = static_cast<Q3DSSlide*>(index.internalPointer());
    if (slide && m_currentSlide != slide)
        handleCurrentSlideChanged(slide, m_currentSlide);
}

void SlideExplorerWidget::handleCurrentSlideChanged(Q3DSSlide *slide, Q3DSSlide *oldSlide)
{
    // Set the current slide selection
    m_slideListView->setCurrentIndex(m_slideModel->getSlideIndex(slide));

    if (slide) {
        const qint32 startTime = slide->startTime();
        const qint32 endTime = slide->endTime();
        m_slideSeekSlider->setValue(startTime);
        m_slideSeekSlider->setMaximum(endTime);
        m_slideSeekSlider->setTickPosition(QSlider::TicksBelow);
        m_slideSeekSlider->setTickInterval(1000);
   }

    if (m_presentation) {
        m_sceneManager->setCurrentSlide(slide);
    } else if (m_component) {
        m_sceneManager->setComponentCurrentSlide(m_component, slide);
    }

    if (oldSlide) {
        Q3DSSlideAttached *data = static_cast<Q3DSSlideAttached *>(oldSlide->attached());
        if (data && !data->animators.isEmpty()) {
            Qt3DAnimation::QClipAnimator *animator = data->animators.at(0);
            animator->clearPropertyTrackings();
            animator->disconnect();
        }

        m_currentSlide = slide;
    }

    if (slide) {
        Q3DSSlideAttached *data = static_cast<Q3DSSlideAttached *>(slide->attached());
        if (!data->animators.isEmpty()) {
            Qt3DAnimation::QClipAnimator *animator = data->animators.at(0);
            QObject::connect(animator, &Qt3DAnimation::QClipAnimator::normalizedTimeChanged, [this](float t) {
                const qint32 duration = m_currentSlide->endTime() - m_currentSlide->startTime();
                m_slideSeekSlider->setValue(int(duration * t));
                m_slideAtEnd = (t == 1.0f);
            });

            const auto onRunningChanged = [this](bool r) {
                m_playSlideButton->setText(r ? QStringLiteral("Pause") : QStringLiteral("Play"));
                m_isSlidePlaying = r;
            };
            onRunningChanged(animator->isRunning());
            QObject::connect(animator, &Qt3DAnimation::QClipAnimator::runningChanged, onRunningChanged);


            animator->setPropertyTracking(QStringLiteral("normalizedTime"), Qt3DCore::QNode::TrackAllValues);
            animator->setPropertyTracking(QStringLiteral("running"), Qt3DCore::QNode::TrackAllValues);
        }
    }
}

void SlideExplorerWidget::switchToNextSlide()
{
    auto nextSlide = m_slideModel->getNextSlide(m_currentSlide);
    if (nextSlide)
        handleCurrentSlideChanged(nextSlide, m_currentSlide);
}

void SlideExplorerWidget::switchToPrevSlide()
{
    auto prevSlide = m_slideModel->getPrevSlide(m_currentSlide);
    if (prevSlide)
        handleCurrentSlideChanged(prevSlide, m_currentSlide);
}

void SlideExplorerWidget::playCurrentSlide()
{
    if (m_sceneManager) {
        m_isSlidePlaying = !m_isSlidePlaying;
        const bool restart = m_isSlidePlaying && m_slideAtEnd;
        m_sceneManager->setAnimationsRunning(m_currentSlide, m_isSlidePlaying, restart);
        m_sceneManager->setAnimationsRunning(m_masterSlide, m_isSlidePlaying, restart);
    }
}

void SlideExplorerWidget::seekInCurrentSlide(int value)
{
    static const auto seekInSlide = [](Q3DSSlide *slide, float normalizedTime) {
        Q3DSSlideAttached *data = static_cast<Q3DSSlideAttached *>(slide->attached());
        if (!data)
            return;
        for (Qt3DAnimation::QClipAnimator *animator : data->animators)
            animator->setNormalizedTime(normalizedTime);
    };

    Q3DSSlideAttached *data = static_cast<Q3DSSlideAttached *>(m_currentSlide->attached());
    if (!data)
        return;

    if (data->animators.isEmpty())
        return;

    const float durationInMS = m_currentSlide->endTime() - m_currentSlide->startTime();
    const float normalized = value / durationInMS;

    seekInSlide(m_currentSlide, normalized);
}

void SlideExplorerWidget::init()
{
    m_slideModel = new SlideListModel();

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

void SlideExplorerWidget::updateModel()
{
    if (m_sceneManager) {
        if (m_presentation) {
            m_masterSlide = m_presentation->masterSlide();
            m_currentSlide = m_sceneManager->currentSlide();
        } else if (m_component) {
            m_masterSlide = m_component->masterSlide();
            m_currentSlide = m_component->currentSlide();
        }
        m_slideModel->setMasterSlide(m_masterSlide);
        handleCurrentSlideChanged(m_currentSlide, nullptr);
    }
}

QT_END_NAMESPACE

#include "moc_slideexplorerwidget.cpp"
