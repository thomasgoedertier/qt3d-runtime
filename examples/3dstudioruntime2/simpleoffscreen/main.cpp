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

#include <q3dsruntimeglobal.h>
#include <Q3DSSurfaceViewer>
#include <Q3DSPresentation>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QThread>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QSurfaceFormat::setDefaultFormat(Q3DS::surfaceFormat());

    QOpenGLContext context;
    if (!context.create())
        qFatal("Failed to create OpenGL context");

    // Must have a "surface". Depending on the platform this may be a small
    // window, pbuffer surface, or even nothing (in case surfaceless contexts are
    // supported) under the hood.
    QOffscreenSurface surface;
    surface.create();

    if (!context.makeCurrent(&surface))
        qFatal("makeCurrent failed");

    const QSize size(1280, 800);
    QOpenGLFramebufferObject fbo(size, QOpenGLFramebufferObject::CombinedDepthStencil);

    Q3DSSurfaceViewer viewer;
    viewer.presentation()->setSource(QUrl(QStringLiteral("qrc:/barrel.uip")));

    // Will call update() manually when a new frame is wanted.
    viewer.setUpdateInterval(-1);

    // Automatic sizing must be turned off.
    viewer.setAutoSize(false);

    // Provide the render target size instead.
    viewer.setSize(size);

    if (!viewer.create(&surface, &context, fbo.handle()))
        qFatal("Initialization failed");

    // Render a few frames, read them back, and save into png files.
    for (int frame = 1; frame <= 20; ++frame) {
        // Render the next frame.
        viewer.update();

        // Write it to a file.
        context.makeCurrent(&surface);
        const QString fn = QString(QLatin1String("output_%1.png")).arg(frame);
        fbo.toImage().save(fn);

        qDebug("Rendered and saved frame %d to %s", frame, qPrintable(fn));

        // ### hack until QT3DS-1041 is in place
        QThread::msleep(10);
    }

    // Now one could do the following to generate a 60 fps video from these frames:
    //   ffmpeg -r 60 -f image2 -s 1280x800 -i output_%d.png -vcodec libx264 -crf 25 -pix_fmt yuv420p output.mp4

    // ### this should not be needed. Qt3D should be able to shut down without it.
    QGuiApplication::processEvents();

    return 0;
}
