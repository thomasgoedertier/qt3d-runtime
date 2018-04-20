/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt 3D Studio.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QDebug>

#include <q3dsruntimeglobal.h>
#include <Q3DSSurfaceViewer>
#include <Q3DSPresentation>
#include <Q3DSDataInput>

#include "window_manualupdate.h"
#include "window_autoupdate.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QSurfaceFormat::setDefaultFormat(Q3DS::surfaceFormat());

    QCommandLineParser parser;
    QCommandLineOption multiWindowOption("multi", "Multiple windows");
    parser.addOption(multiWindowOption);
    parser.process(app);

    ManualUpdateWindow w;
    w.setTitle(QLatin1String("Qt 3D Studio plain QWindow Example"));
    Q3DSSurfaceViewer viewer;
    QObject::connect(&viewer, &Q3DSSurfaceViewer::errorChanged, &viewer, [&viewer] {
        const QString msg = viewer.error();
        if (!msg.isEmpty())
            qWarning() << msg;
    });

    // The presentation has a data input entry "di_text" for the textstring
    // property of one of the Text nodes. Provide a custom value.
    Q3DSDataInput dataInput(viewer.presentation(), QLatin1String("di_text"));
    // Assuming the source is never changed or reloaded, a plain setValue()
    // call is good enough. Otherwise, we would need to connect to the
    // presentationLoaded() signal and set the value whenever a new
    // presentation is loaded.
    dataInput.setValue(QLatin1String("Hello world"));

    viewer.presentation()->setSource(QUrl(QLatin1String("qrc:/barrel.uip")));
    viewer.create(&w, w.context());
    w.setViewer(&viewer);
    w.resize(1024, 768);
    w.show();

    QScopedPointer<AutoUpdateWindow> w2;
    QScopedPointer<Q3DSSurfaceViewer> viewer2;
    if (parser.isSet(multiWindowOption)) {
        w2.reset(new AutoUpdateWindow);
        viewer2.reset(new Q3DSSurfaceViewer);
        viewer2->presentation()->setSource(QUrl(QLatin1String("qrc:/barrel.uip")));
        // change from the default -1 (auto-update disabled) to an interval of
        // 0 since AutoUpdateWindow does not do anything on its own that would
        // lead to calling update()
        viewer2->setUpdateInterval(0);
        viewer2->create(w2.data(), w2->context());
        w2->resize(800, 600);
        w2->show();
    }

    return app.exec();
}
