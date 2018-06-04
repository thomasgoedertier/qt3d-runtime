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

#include "q3dsaboutdialog.h"
#include "ui_q3dsaboutdialog.h"

#include <QtGui/QPainter>

QT_BEGIN_NAMESPACE

Q3DSAboutDialog::Q3DSAboutDialog(QWidget *parent) :
    QDialog(parent),
    m_ui(new Ui::Q3DSAboutDialog)
{
    m_ui->setupUi(this);

    // File selectors are not automatic with QPixmap
    if (devicePixelRatio() < 2.0)
        m_backgroundPixmap = QPixmap(":/resources/images/open_dialog.png");
    else
        m_backgroundPixmap = QPixmap(":/resources/images/open_dialog@2x.png");
    initDialog();
    window()->setFixedSize(size());
}

Q3DSAboutDialog::~Q3DSAboutDialog()
{
    delete m_ui;
}

void Q3DSAboutDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.drawPixmap(0, 0, m_backgroundPixmap);
}

namespace {
QString compilerString()
{
#if defined(Q_CC_CLANG) // must be before GNU, because clang claims to be GNU too
    QString isAppleString;
#if defined(__apple_build_version__) // Apple clang has other version numbers
    isAppleString = QStringLiteral(" (Apple)");
#endif
    return QStringLiteral("Clang " ) + QString::number(__clang_major__) + QStringLiteral(".")
            + QString::number(__clang_minor__) + isAppleString;
#elif defined(Q_CC_GNU)
    return QStringLiteral("GCC " ) + QStringLiteral(__VERSION__);
#elif defined(Q_CC_MSVC)
    if (_MSC_VER > 1999)
        return QStringLiteral("MSVC <unknown>");
    if (_MSC_VER >= 1910)
        return QStringLiteral("MSVC 2017");
    if (_MSC_VER >= 1900)
        return QStringLiteral("MSVC 2015");
#endif
    return QStringLiteral("<unknown compiler>");
}

}

void Q3DSAboutDialog::initDialog()
{
    const QString masterColor(QStringLiteral("5caa15"));

    // Set the Studio version
    m_ProductVersionStr = QCoreApplication::applicationName()
            + QStringLiteral(" ") + QCoreApplication::applicationVersion();

    // Set the copyright string
    m_CopyrightStr = QObject::tr("Copyright (C) 2018 The Qt Company. All rights reserved.");

    // Set the credit strings
    m_Credit1Str = QObject::tr("");

    // Add link to Web site
    QString theURL(QStringLiteral("https://www.qt.io/3d-studio"));

    m_ui->m_WebSite->setText(QString("<a href=\"%1\"><font color=\"#%2\">%3</font></a>").arg(
                                 theURL,
                                 masterColor,
                                 theURL));
    m_ui->m_WebSite->setToolTip(tr("Click to visit Qt web site"));
    m_ui->m_WebSite->setOpenExternalLinks(true);

    // Add link to support address
    const QString theSupport = QStringLiteral("https://account.qt.io/support");

    m_ui->m_Email->setText(QString("<a href=\"%1\"><font color=\"#%2\">%3</font></a>").arg(
                               theSupport,
                               masterColor,
                               theSupport));
    m_ui->m_Email->setToolTip(tr("Send a Studio support request to the Qt Company"));
    m_ui->m_Email->setOpenExternalLinks(true);

    // Make the font bold for version number
    m_ui->m_ProductVersion->setStyleSheet("font-weight: bold;");

    m_ui->m_ProductVersion->setText(m_ProductVersionStr);
    m_ui->m_Copyright->setText(m_CopyrightStr);
    m_ui->m_Credit1->setText(m_Credit1Str);

// "might prevent reproducible builds [-Werror=date-time]"
// So only enable this when doing release packages.
#ifdef QT3DS_ADD_BUILD_TIMESTAMP
#if defined(Q_CC_GNU)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdate-time"
#endif
#define QT3DS_BUILD_DATE __DATE__
#define QT3DS_BUILD_TIME __TIME__
#else
#define QT3DS_BUILD_DATE "DATE"
#define QT3DS_BUILD_TIME "TIME"
#endif

    // Information about build
    m_ui->m_buildTimestamp->setText(
                tr("Built on %1 %2").arg(QStringLiteral(QT3DS_BUILD_DATE), QStringLiteral(QT3DS_BUILD_TIME)));
    m_ui->m_qtVersion->setText(
                tr("Based on Qt %1 (%2, %3 bit)").arg(
                    QString::fromLatin1(qVersion()),
                    compilerString(),
                    QString::number(QSysInfo::WordSize)));
#ifdef QT3DSTUDIO_REVISION
    m_ui->m_revisionSHA->setText(
                tr("From revision %1").arg(
                    QString::fromLatin1(QT3DSTUDIO_REVISION_STR).left(10)));
#else
    m_ui->m_revisionSHA->setText(QString());
    m_ui->m_revisionSHA->setMaximumHeight(0);
#endif

#ifdef QT3DS_ADD_BUILD_TIMESTAMP
#if defined(Q_CC_GNU)
#pragma GCC diagnostic pop
#endif
#endif
}

QT_END_NAMESPACE
