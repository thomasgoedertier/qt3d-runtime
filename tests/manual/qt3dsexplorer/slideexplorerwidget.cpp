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
#include <private/q3dsuippresentation_p.h>
#include <private/q3dsscenemanager_p.h>
#include <private/q3dsslideplayer_p.h>
#include <QVBoxLayout>
#include <QListView>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QAbstractListModel>
#include <QMainWindow>
#include <QApplication>
#include <QStatusBar>
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

void SlideExplorerWidget::setPresentation(Q3DSUipPresentation *pres)
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
    m_sceneManager = nullptr;
    m_slidePlayer = nullptr;
    m_slideModel->setMasterSlide(nullptr);
}

void SlideExplorerWidget::handleSelectionChanged(const QModelIndex &index)
{
    Q3DSSlide *slide = static_cast<Q3DSSlide *>(index.internalPointer());
    qDebug("New slide selected %s", qPrintable(slide->name()));
    Q_ASSERT(slide->parent());
    // Get the master slide, it has the slide player for this slide
    Q3DSSlide *masterSlide = static_cast<Q3DSSlide *>(slide->parent());
    qDebug("Getting player from master slide %s", qPrintable(masterSlide->name()));
    Q3DSSlidePlayer *player = masterSlide->attached<Q3DSSlideAttached>()->slidePlayer;
    Q_ASSERT(player);
    player->slideDeck()->setCurrentSlide(index.row());
}

void SlideExplorerWidget::handleCurrentSlideChanged(Q3DSSlide *slide)
{
    // Set the current slide selection
    m_slideListView->setCurrentIndex(m_slideModel->getSlideIndex(slide));

    if (slide) {
        const qint32 startTime = 0;
        qint32 endTime = 0;
        Q3DSSlideUtils::getStartAndEndTime(slide, nullptr, &endTime);
        m_slideSeekSlider->setValue(startTime);
        m_slideSeekSlider->setMaximum(endTime);
        m_slideSeekSlider->setTickPosition(QSlider::TicksBelow);
        m_slideSeekSlider->setTickInterval(100);
   }
}

void SlideExplorerWidget::switchToNextSlide()
{
    if (m_slidePlayer)
        m_slidePlayer->nextSlide();
}

void SlideExplorerWidget::switchToPrevSlide()
{
    if (m_slidePlayer)
        m_slidePlayer->previousSlide();
}

void SlideExplorerWidget::playCurrentSlide()
{
    if (!m_slidePlayer)
        return;

    Q3DSSlidePlayer::PlayerState state = m_sceneManager->slidePlayer()->state();
    if (state == Q3DSSlidePlayer::PlayerState::Ready
            || state == Q3DSSlidePlayer::PlayerState::Paused
            || state == Q3DSSlidePlayer::PlayerState::Stopped) {
        m_slidePlayer->play();
    } else {
        m_slidePlayer->pause();
    }
}

void SlideExplorerWidget::stopCurrentSlide()
{
    if (m_slidePlayer)
        m_slidePlayer->stop();
}

void SlideExplorerWidget::setRate(int rate)
{
    if (m_slidePlayer)
        m_slidePlayer->setPlaybackRate(float(rate));
}

static QMainWindow* getMainWindow()
{
    for (const auto widget : QApplication::topLevelWidgets())
        if (QMainWindow *mainWindow = qobject_cast<QMainWindow*>(widget))
            return mainWindow;
    return nullptr;
}

void SlideExplorerWidget::seekInCurrentSlide(int value)
{
    if (!m_slidePlayer)
        return;

    m_slidePlayer->seek(value);
    const float seconds = value / 1000.0f;
    getMainWindow()->statusBar()->showMessage(
        QString::number(seconds) + QStringLiteral(" seconds"));
}

void SlideExplorerWidget::setPlayerMode(int value)
{
    if (!m_slidePlayer)
        return;

    const auto mode = (value == Qt::Checked) ? Q3DSSlidePlayer::PlayerMode::Viewer
                                             : Q3DSSlidePlayer::PlayerMode::Editor;
    m_slidePlayer->setMode(mode);
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
    m_stopSlideButton = new QPushButton("stop", this);
    connect(m_stopSlideButton, &QPushButton::clicked, this, &SlideExplorerWidget::stopCurrentSlide);
    mainLayout->addWidget(m_stopSlideButton);
    m_rateWidget = new QSpinBox(this);
    m_rateWidget->setRange(-5, 5);
    m_rateWidget->setValue(1);
    connect(m_rateWidget, QOverload<int>::of(&QSpinBox::valueChanged), this, &SlideExplorerWidget::setRate);
    mainLayout->addWidget(m_rateWidget);
    m_slideSeekSlider = new QSlider(Qt::Horizontal, this);
    connect(m_slideSeekSlider, &QSlider::valueChanged, this, &SlideExplorerWidget::seekInCurrentSlide);
    mainLayout->addWidget(m_slideSeekSlider);
    m_playerModeCheckBox = new QCheckBox("Viewer mode", this);
    connect(m_playerModeCheckBox, &QCheckBox::stateChanged, this, &SlideExplorerWidget::setPlayerMode);
    mainLayout->addWidget(m_playerModeCheckBox);
}

void SlideExplorerWidget::updateModel()
{
    Q3DSSlide *masterSlide = nullptr;
    if (m_presentation)
        masterSlide = m_presentation->masterSlide();
    else if (m_component)
        masterSlide = m_component->masterSlide();

    m_slideModel->setMasterSlide(masterSlide);

    // Slide player

    Q3DSSlideAttached *data = masterSlide ? masterSlide->attached<Q3DSSlideAttached>()
                                          : nullptr;
    if (!data)
        return;

    Q3DSSlidePlayer *player = data->slidePlayer;
    if (m_slidePlayer && m_slidePlayer != player)
        disconnect(m_slidePlayer);

    if (player) {
        m_slidePlayer = player;
        setPlayerMode(m_playerModeCheckBox->checkState());
        connect(m_slidePlayer, &Q3DSSlidePlayer::positionChanged, [this](float v) {
            m_slideSeekSlider->blockSignals(true);
            m_slideSeekSlider->setValue(int(v));
            const float seconds = v / 1000.0f;
            getMainWindow()->statusBar()->showMessage(
                QString::number(seconds) + QStringLiteral(" seconds"));
            m_slideSeekSlider->blockSignals(false);
        });
        handleCurrentSlideChanged(m_slidePlayer->slideDeck()->currentSlide());
        connect(m_slidePlayer, &Q3DSSlidePlayer::slideChanged,
                this, &SlideExplorerWidget::handleCurrentSlideChanged);
        connect(m_slidePlayer, &Q3DSSlidePlayer::stateChanged,
                [this](Q3DSSlidePlayer::PlayerState state) {
            if (state == Q3DSSlidePlayer::PlayerState::Playing)
                m_playSlideButton->setText(QLatin1String("Pause"));
            else
                m_playSlideButton->setText(QLatin1String("Play"));
        });
    }
}

QT_END_NAMESPACE

#include "moc_slideexplorerwidget.cpp"
