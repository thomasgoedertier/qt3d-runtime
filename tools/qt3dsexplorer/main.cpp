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
#include <private/q3dsengine_p.h>
#include <private/q3dswindow_p.h>
#include <private/q3dsutils_p.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QSurfaceFormat::setDefaultFormat(Q3DSEngine::surfaceFormat());

    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    cmdLineParser.addPositionalArgument(QLatin1String("filename"), QObject::tr("UIP or UIA file to open"));
    QCommandLineOption msaaOption({ "m", "multisample" }, QObject::tr("Force 4x MSAA"));
    cmdLineParser.addOption(msaaOption);
    QCommandLineOption profOption({ "p", "profile" }, QObject::tr("Open scene with profiling enabled"));
    cmdLineParser.addOption(profOption);
    cmdLineParser.process(app);

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

    Q3DSEngine::Flags flags = 0;
    if (cmdLineParser.isSet(msaaOption))
        flags |= Q3DSEngine::Force4xMSAA;
    if (cmdLineParser.isSet(profOption))
        flags |= Q3DSEngine::EnableProfiling;

    QScopedPointer<Q3DSEngine> engine(new Q3DSEngine);
    QScopedPointer<Q3DSWindow> view(new Q3DSWindow);
    view->setEngine(engine.data());
    engine->setFlags(flags);
    if (!engine->setSource(fn.first()))
        return 0;

    QScopedPointer<Q3DSExplorerMainWindow> mw;
    mw.reset(new Q3DSExplorerMainWindow(view.take()));
    mw->show();

    int r = app.exec();

    // make sure the engine is destroyed before the view (which is owned by mw by now)
    engine.reset();
    return r;
}
