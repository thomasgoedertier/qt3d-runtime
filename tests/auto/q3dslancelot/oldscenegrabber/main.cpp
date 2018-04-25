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

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtCore/QHashFunctions>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QWindow>
#include <QtGui/QOpenGLContext>

#include <QtStudio3D/Q3DSSurfaceViewer>
#include <QtStudio3D/Q3DSViewerSettings>
#include <QtStudio3D/Q3DSPresentation>

// Timeout values:

// A valid screen grab requires the scene to not change
// for SCENE_STABLE_TIME ms
#define SCENE_STABLE_TIME 200

// Give up after SCENE_TIMEOUT ms
#define SCENE_TIMEOUT     15000

class Grabber : public QObject
{
    Q_OBJECT

public:
    Grabber(Q3DSSurfaceViewer *viewer, const QString &outputFile)
        : ofile(outputFile)
        , grabNo(0)
        , isGrabbing(false)
        , surfaceViewer(viewer)
    {
        grabTimer = new QTimer(this);
        grabTimer->setSingleShot(true);
        grabTimer->setInterval(SCENE_STABLE_TIME);
        connect(grabTimer, SIGNAL(timeout()), this, SLOT(grab()));

        grabTimer->start();
        QTimer::singleShot(SCENE_TIMEOUT, this, SLOT(timedOut()));
    }

private slots:
    void grab()
    {
        if (isGrabbing)
            return;
        isGrabbing = true;
        grabNo++;

        auto image = surfaceViewer->grab();

        if (!image.isNull() && image == lastGrab) {
            sceneStabilized();
        } else {
            lastGrab = image;
            grabTimer->start();
        }

        isGrabbing = false;
    }

    void sceneStabilized()
    {
        if (ofile == "-") {   // Write to stdout
            QFile of;
            if (!of.open(1, QIODevice::WriteOnly) || !lastGrab.save(&of, "ppm")) {
                qWarning() << "Error: failed to write grabbed image to stdout.";
                QGuiApplication::exit(2);
                return;
            }
        } else {
            if (!lastGrab.save(ofile)) {
                qWarning() << "Error: failed to store grabbed image to" << ofile;
                QGuiApplication::exit(2);
                return;
            }
        }
        QGuiApplication::exit(0);
    }

    void timedOut()
    {
        qWarning() << "Error: timed out waiting for scene to stabilize." << grabNo << "grab(s) done. Last grab was" << (lastGrab.isNull() ? "invalid." : "valid.");
        QGuiApplication::exit(3);
    }

private:
    QImage lastGrab;
    QTimer *grabTimer;
    QString ofile;
    int grabNo;
    bool isGrabbing;
    Q3DSSurfaceViewer *surfaceViewer;
};

int main(int argc, char *argv[])
{
    qSetGlobalQHashSeed(0);

    QGuiApplication a(argc, argv);

    // Parse command line
    QString ifile, ofile;
    QStringList args = a.arguments();
    int i = 0;
    bool argError = false;
    while (++i < args.size()) {
        QString arg = args.at(i);
        if ((arg == "-o") && (i < args.size()-1)) {
            ofile = args.at(++i);
        } else if (ifile.isEmpty()) {
            ifile = arg;
        } else {
            argError = true;
            break;
        }
    }
    if (argError || ifile.isEmpty() || ofile.isEmpty()) {
        qWarning() << "Usage:" << args.at(0).toLatin1().constData() << "<uip-infile> -o <outfile or - for ppm on stdout>";
        return 1;
    }

    QFileInfo ifi(ifile);
    if (!ifi.exists() || !ifi.isReadable() || !ifi.isFile()) {
        qWarning() << args.at(0).toLatin1().constData() << " error: unreadable input file" << ifile;
        return 1;
    }
    // End parsing


    QWindow window;

    QSize size(1280, 720);
    window.resize(size);
    window.setSurfaceType(QSurface::OpenGLSurface);
    window.setTitle(QStringLiteral("Qt 3D Studio 1.x Scene Grabber"));
    window.create();

    QOpenGLContext context;
    context.setFormat(window.format());
    context.create();

    Q3DSSurfaceViewer viewer;
    viewer.presentation()->setSource(QUrl::fromLocalFile(ifile));
    viewer.setUpdateInterval(0);
    viewer.settings()->setScaleMode(Q3DSViewerSettings::ScaleModeFill);
    viewer.settings()->setShowRenderStats(false);

    viewer.initialize(&window, &context);

    window.show();

    Grabber grabber(&viewer, ofile);

    int retVal = a.exec();
    return retVal;
}

#include "main.moc"

