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

#include <QApplication>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QStandardPaths>
#include <Q3DSEngine>
#include <Q3DSWindow>
#include <private/q3dsutils_p.h>
#include "q3dsmainwindow.h"

int main(int argc, char *argv[])
{
    Q3DSEngine::initStaticPreApp();
    QApplication app(argc, argv);
    Q3DSEngine::initStaticPostApp();

    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    cmdLineParser.addPositionalArgument(QLatin1String("filename"), QObject::tr("UIP or UIA file to open"));
    QCommandLineOption noMainWindowOption({ "w", "no-main-window" }, QObject::tr("Skips the QWidget-based main window (use this for eglfs)."));
    cmdLineParser.addOption(noMainWindowOption);
    QCommandLineOption fullScreenOption({ "f", "fullscreen" }, QObject::tr("Shows in fullscreen"));
    cmdLineParser.addOption(fullScreenOption);
    QCommandLineOption msaaOption({ "m", "multisample" }, QObject::tr("Forces 4x MSAA"));
    cmdLineParser.addOption(msaaOption);
    QCommandLineOption profOption({ "p", "profile" }, QObject::tr("Opens presentation with profiling enabled"));
    cmdLineParser.addOption(profOption);
    cmdLineParser.process(app);

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    const bool noWidgets = cmdLineParser.isSet(noMainWindowOption);
    const bool fullscreen = cmdLineParser.isSet(fullScreenOption);
#else
    const bool noWidgets = true;
    const bool fullscreen = true;
#endif

    QStringList fn = cmdLineParser.positionalArguments();
    if (noWidgets) {
        Q3DSUtils::setDialogsEnabled(false);
    } else if (fn.isEmpty()) {
        QString fileName = QFileDialog::getOpenFileName(nullptr, QObject::tr("Open"), QString(),
                                                        Q3DStudioMainWindow::fileFilter());
        if (!fileName.isEmpty())
            fn.append(fileName);
    }

    // Try a default file on mobile,
    // e.g. /storage/emulated/0/Documents/default.uip on Android.
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    if (fn.isEmpty()) {
        QStringList docPath = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
        if (!docPath.isEmpty())
            fn = QStringList { docPath.first() + QLatin1String("/default.uip") };
    }
#endif

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

    QScopedPointer<Q3DStudioMainWindow> mw;
    if (noWidgets) {
        if (fullscreen)
            view->showFullScreen();
        else
            view->show();
    } else {
        mw.reset(new Q3DStudioMainWindow(view.take()));
        if (fullscreen)
            mw->showFullScreen();
        else
            mw->show();
    }

    int r = app.exec();

    // make sure the engine is destroyed before the view (which is owned by mw by now)
    engine.reset();
    return r;
}
