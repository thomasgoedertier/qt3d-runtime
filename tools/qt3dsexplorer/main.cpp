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
#include <QApplication>
#include <QCommandLineParser>
#include <QFileDialog>
#include <Qt3DStudioRuntime2/q3dsutils.h>
#include <Qt3DStudioRuntime2/q3dswindow.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Q3DStudioWindow::initStaticPreApp();
    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    cmdLineParser.addPositionalArgument(QLatin1String("filename"), QObject::tr("UIP or UIA file to open"));
    QCommandLineOption msaaOption({ "m", "multisample" }, QObject::tr("Enable 4x MSAA"));
    cmdLineParser.addOption(msaaOption);
    cmdLineParser.process(app);

    Q3DStudioWindow::InitFlags initFlags = 0;
    if (cmdLineParser.isSet(msaaOption))
        initFlags |= Q3DStudioWindow::MSAA4x;
    Q3DStudioWindow::initStaticPostApp(initFlags);

    QStringList fn = cmdLineParser.positionalArguments();
    if (fn.isEmpty()) {
        QString fileName = QFileDialog::getOpenFileName(nullptr, QObject::tr("Open"),
                                                        QString(), Q3DSExplorerMainWindow::fileFilter());
        if (!fileName.isEmpty())
            fn.append(fileName);
    }

    if (fn.isEmpty()) {
        Q3DSUtils::showMessage(QObject::tr("No file specified."));
        return 0;
    }

    QScopedPointer<Q3DStudioWindow> view;
    view.reset(new Q3DStudioWindow);
    if (!view->setSource(fn.first()))
        return 0;

    QScopedPointer<Q3DSExplorerMainWindow> mw;
    mw.reset(new Q3DSExplorerMainWindow(view.take()));
    mw->show();

    return app.exec();
}
