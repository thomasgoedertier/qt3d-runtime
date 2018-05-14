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

#ifdef Q3DSVIEWER_WIDGETS
#include <QApplication>
#include <QFileDialog>
#include "q3dsmainwindow.h"
#else
#include <QGuiApplication>
#endif

#include "q3dsremotedeploymentmanager.h"

#include <QCommandLineParser>
#include <QStandardPaths>
#include <private/q3dsengine_p.h>
#include <private/q3dswindow_p.h>
#include <private/q3dsutils_p.h>

QT_BEGIN_NAMESPACE
class Q3DStudioMainWindow;
QT_END_NAMESPACE

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setOrganizationName(QStringLiteral("The Qt Company"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qt.io"));
    QCoreApplication::setApplicationName(QStringLiteral("Qt 3D Viewer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.0"));

#ifdef Q3DSVIEWER_WIDGETS
    QApplication app(argc, argv);
#else
    QGuiApplication app(argc, argv);
#endif
    QSurfaceFormat::setDefaultFormat(Q3DS::surfaceFormat());

    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    cmdLineParser.addPositionalArgument(QLatin1String("filename"), QObject::tr("UIP or UIA file to open"));
    QCommandLineOption noMainWindowOption({ "w", "no-main-window" }, QObject::tr("Skips the QWidget-based main window (use this for eglfs)."));
    cmdLineParser.addOption(noMainWindowOption);
    QCommandLineOption fullScreenOption({ "f", "fullscreen" }, QObject::tr("Shows in fullscreen"));
    cmdLineParser.addOption(fullScreenOption);
    QCommandLineOption msaaOption({ "m", "multisample" }, QObject::tr("Forces 4x MSAA"));
    cmdLineParser.addOption(msaaOption);
    QCommandLineOption noProfOption({ "p", "no-profile" }, QObject::tr("Opens presentation without profiling enabled"));
    cmdLineParser.addOption(noProfOption);
    cmdLineParser.addOption({"connect", QObject::tr("main",
                             "If this parameter is specified, the viewer\n"
                             "is started in connection mode.\n"
                             "The default value is 36000."),
                             QObject::tr("main", "port"), QString::number(36000)});

    cmdLineParser.process(app);

#if !defined(Q_OS_ANDROID) && !defined(Q_OS_IOS)
    bool noWidgets = cmdLineParser.isSet(noMainWindowOption);
    const bool fullscreen = cmdLineParser.isSet(fullScreenOption);
#else
    bool noWidgets = true;
    const bool fullscreen = true;
#endif

#if !defined(Q3DSVIEWER_WIDGETS)
    noWidgets = true;
#endif

    QStringList fn = cmdLineParser.positionalArguments();
    if (noWidgets) {
        Q3DSUtils::setDialogsEnabled(false);
    }

    Q3DSEngine::Flags flags = 0;
    if (cmdLineParser.isSet(msaaOption))
        flags |= Q3DSEngine::Force4xMSAA;
    if (!cmdLineParser.isSet(noProfOption))
        flags |= Q3DSEngine::EnableProfiling;

    QScopedPointer<Q3DSEngine> engine(new Q3DSEngine);
    QScopedPointer<Q3DSWindow> view(new Q3DSWindow);
    view->setEngine(engine.data());
    engine->setFlags(flags);

    // Setup Remote Viewer
    QScopedPointer<Q3DSRemoteDeploymentManager> remote(new Q3DSRemoteDeploymentManager(engine.data()));
    int port = 36000;
    if (cmdLineParser.isSet(QStringLiteral("connect")))
        port = cmdLineParser.value(QStringLiteral("connect")).toInt();
    remote->setConnectionPort(port);
    if (fn.isEmpty()) {
        remote->startServer();
        remote->showConnectionSetup();
    } else {
        // Try and set the source for the engine
        if (!engine->setSource(fn.first()))
            return 0;
    }

#ifdef Q3DSVIEWER_WIDGETS
    Q3DStudioMainWindow *mw = nullptr;
#endif
    if (noWidgets) {
        if (fullscreen)
            view->showFullScreen();
        else
            view->show();
        engine->setOnDemandRendering(true);
    } else {
#ifdef Q3DSVIEWER_WIDGETS
        mw = new Q3DStudioMainWindow(view.take(), remote.data());
        if (fullscreen)
            mw->showFullScreen();
        else
            mw->show();
#endif
    }

    int r = app.exec();

    // make sure the engine is destroyed before the view (which is owned by mw by now)
    engine.reset();
#ifdef Q3DSVIEWER_WIDGETS
    delete mw;
#endif
    return r;
}
