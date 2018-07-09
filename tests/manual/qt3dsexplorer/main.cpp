/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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
#include <private/q3dsviewportsettings_p.h>
#include <private/q3dsremotedeploymentmanager_p.h>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setOrganizationName(QStringLiteral("The Qt Company"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qt.io"));
    QCoreApplication::setApplicationName(QStringLiteral("Qt 3D Scene Explorer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("2.0"));

    QApplication app(argc, argv);
    QSurfaceFormat::setDefaultFormat(Q3DS::surfaceFormat());

    QCommandLineParser cmdLineParser;
    cmdLineParser.addHelpOption();
    cmdLineParser.addPositionalArgument(QLatin1String("filename"), QObject::tr("UIP or UIA file to open"));
    QCommandLineOption msaaOption({ "m", "multisample" }, QObject::tr("Force 4x MSAA"));
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

    QStringList fn = cmdLineParser.positionalArguments();

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
    QScopedPointer<Q3DSRemoteDeploymentManager> remote(new Q3DSRemoteDeploymentManager(engine.data(), port, true));
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

    QScopedPointer<Q3DSExplorerMainWindow> mw;
    mw.reset(new Q3DSExplorerMainWindow(view.take(), remote.data()));
    mw->show();

    int r = app.exec();

    // make sure the engine is destroyed before the view (which is owned by mw by now)
    engine.reset();
#ifdef Q3DSVIEWER_WIDGETS
    delete mw;
#endif
    return r;
}
