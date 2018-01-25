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

#include <Qt3DStudioRuntime2/Q3DSWindow>
#include <Qt3DStudioRuntime2/Q3DSEngine>
#include <Qt3DStudioRuntime2/private/q3dsutils_p.h>

#include <Qt3DRender/QRenderCapture>
#include <Qt3DRender/QRenderCaptureReply>

// Timeout values:

// A valid screen grab requires the scene to not change
// for SCENE_STABLE_TIME ms
#define SCENE_STABLE_TIME 200

// Give up after SCENE_TIMEOUT ms
#define SCENE_TIMEOUT     6000

class GrabbingView : public Q3DSWindow
{
    Q_OBJECT

public:
    GrabbingView(const QString &outputFile)
        : q3dsEngine(new Q3DSEngine)
        , ofile(outputFile)
        , grabNo(0)
        , isGrabbing(false)
        , initDone(false)
    {
        setEngine(q3dsEngine);
        grabTimer = new QTimer(this);
        grabTimer->setSingleShot(true);
        grabTimer->setInterval(SCENE_STABLE_TIME);
        connect(grabTimer, SIGNAL(timeout()), q3dsEngine, SLOT(requestGrab()));
        connect(q3dsEngine, SIGNAL(nextFrameStarting()), SLOT(startGrabbing()));
        connect(q3dsEngine, SIGNAL(grabReady(QImage)), this, SLOT(grab(QImage)));

        QTimer::singleShot(SCENE_TIMEOUT, this, SLOT(timedOut()));
    }
    ~GrabbingView()
    {
        delete q3dsEngine;
    }

private slots:
    void startGrabbing()
    {
        if (!initDone) {
            initDone = true;
            grabTimer->start();
        }
    }

    void grab(QImage image)
    {
        if (isGrabbing)
            return;
        isGrabbing = true;
        grabNo++;

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
    Q3DSEngine *q3dsEngine;
    QImage lastGrab;
    QTimer *grabTimer;
    QString ofile;
    int grabNo;
    bool isGrabbing;
    bool initDone;
};

int main(int argc, char *argv[])
{
    qSetGlobalQHashSeed(0);

    Q3DSEngine::initStaticPreApp();
    QGuiApplication a(argc, argv);
    Q3DSEngine::initStaticPostApp();

    Q3DSUtils::setDialogsEnabled(false);

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

    GrabbingView v(ofile);
    v.engine()->setSource(ifile);
    v.show();

    int retVal = a.exec();
    return retVal;
}

#include "main.moc"
