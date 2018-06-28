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
#include <QTimer>

#include "q3dsremotedeploymentmanager.h"

#include <QCommandLineParser>
#include <QStandardPaths>
#include <private/q3dsengine_p.h>
#include <private/q3dswindow_p.h>
#include <private/q3dsutils_p.h>
#include <private/q3dsviewportsettings_p.h>

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
    QCommandLineOption remoteOption("port",
                                    QObject::tr("Sets the <port> to listen on in remote connection mode. The default <port> is 36000."),
                                    QObject::tr("port"), QLatin1String("36000"));
    cmdLineParser.addOption(remoteOption);
    QCommandLineOption autoExitOption({ "x", "exitafter" }, QObject::tr("Exit after <n> seconds."), QObject::tr("n"), QLatin1String("5"));
    cmdLineParser.addOption(autoExitOption);
    QCommandLineOption scaleModeOption("scalemode",
                                       QObject::tr("Specifies scaling mode.\n"
                                       "The default value is 'center'."),
                                       QObject::tr("center|fit|fill"),
                                       QStringLiteral("center"));
    cmdLineParser.addOption(scaleModeOption);
    QCommandLineOption matteColorOption("mattecolor",
                                        QObject::tr("Specifies custom matte color\n"
                                        "using #000000 syntax.\n"
                                        "For example, white matte: #ffffff"),
                                        QObject::tr("color"), QStringLiteral("#333333"));
    cmdLineParser.addOption(matteColorOption);
    cmdLineParser.process(app);

    QSurfaceFormat::setDefaultFormat(Q3DS::surfaceFormat());

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
    int port = 36000;
    if (cmdLineParser.isSet(remoteOption))
        port = cmdLineParser.value(remoteOption).toInt();
    QScopedPointer<Q3DSRemoteDeploymentManager> remote(new Q3DSRemoteDeploymentManager(engine.data(), port, !noWidgets));
    if (fn.isEmpty()) {
        remote->startServer();
        remote->showConnectionSetup();
    } else {
        // Try and set the source for the engine
        if (!engine->setSource(fn.first()))
            return 0;
    }

    // Setup Viewport Settings
    Q3DSViewportSettings *viewportSettings = new Q3DSViewportSettings;
    engine->setViewportSettings(viewportSettings);
    viewportSettings->setMatteEnabled(true);
    viewportSettings->setScaleMode(Q3DSViewportSettings::ScaleModeCenter);
    if (cmdLineParser.isSet(scaleModeOption)) {
        const QString scaleMode = cmdLineParser.value(scaleModeOption);
        if (scaleMode == QStringLiteral("center"))
            viewportSettings->setScaleMode(Q3DSViewportSettings::ScaleModeCenter);
        else if (scaleMode == QStringLiteral("fit"))
            viewportSettings->setScaleMode(Q3DSViewportSettings::ScaleModeFit);
        else
            viewportSettings->setScaleMode(Q3DSViewportSettings::ScaleModeFill);
    }
    if (cmdLineParser.isSet(matteColorOption))
        viewportSettings->setMatteColor(cmdLineParser.value(matteColorOption));

#ifdef Q3DSVIEWER_WIDGETS
    Q3DStudioMainWindow *mw = nullptr;
#endif
    if (noWidgets) {
        if (fullscreen)
            view->showFullScreen();
        else
            view->show();
        engine->setOnDemandRendering(true);
        // in QWindow mode let F10 and double double clicks activate the
        // profileui via the engine
        engine->setAutoToggleProfileUi(true);
    } else {
#ifdef Q3DSVIEWER_WIDGETS
        mw = new Q3DStudioMainWindow(view.take(), remote.data());
        if (fullscreen)
            mw->showFullScreen();
        else
            mw->show();
        // with widgets the engine's built-in profileui activation is not
        // desirable since we have menu items with the same shortcuts (F10)
        engine->setAutoToggleProfileUi(false);
#endif
    }

    if (cmdLineParser.isSet(autoExitOption))
        QTimer::singleShot(cmdLineParser.value(autoExitOption).toInt() * 1000, &app, SLOT(quit()));

    int r = app.exec();

    // make sure the engine is destroyed before the view (which is owned by mw by now)
    engine.reset();
#ifdef Q3DSVIEWER_WIDGETS
    delete mw;
#endif
    return r;
}
