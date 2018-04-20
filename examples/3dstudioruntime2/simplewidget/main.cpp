/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QUrl>

#include <q3dsruntimeglobal.h>
#include <Q3DSWidget>
#include <Q3DSPresentation>
#include <Q3DSDataInput>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QSurfaceFormat::setDefaultFormat(Q3DS::surfaceFormat());

    QWidget w;
    w.setWindowTitle(QLatin1String("Qt 3D Studio Widget Example"));
    QVBoxLayout *layout = new QVBoxLayout;
    w.setLayout(layout);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    layout->addLayout(buttonLayout);

    Q3DSWidget *w3DS = new Q3DSWidget;
    QObject::connect(w3DS, &Q3DSWidget::errorChanged, w3DS, [&w, w3DS] {
        const QString msg = w3DS->error();
        if (!msg.isEmpty())
            QMessageBox::critical(&w, QLatin1String("Failed to load presentation"), msg, QLatin1String("Ok"));
    });

    // The presentation has a data input entry "di_text" for the textstring
    // property of one of the Text nodes. Provide a custom value. Do this in a
    // manner so that the value is set even when doing a Reload or changing the
    // presentation object's source.
    Q3DSDataInput dataInput(w3DS->presentation(), QLatin1String("di_text"));
    QObject::connect(w3DS, &Q3DSWidget::presentationLoaded, w3DS, [&dataInput] {
        dataInput.setValue(QLatin1String("Hello world"));
    });

    w3DS->presentation()->setProfilingEnabled(true);
    w3DS->presentation()->setSource(QUrl(QLatin1String("qrc:/barrel.uip")));
    layout->addWidget(w3DS);

    QPushButton *openBtn = new QPushButton(QLatin1String("Open"));
    QObject::connect(openBtn, &QPushButton::clicked, w3DS, [&w, w3DS] {
        const char *filter = "All Supported Formats (*.uia *.uip);;"
                             "Studio UI Presentation (*.uip);;"
                             "Application File (*.uia);;"
                             "All Files (*)";
        const QString fn = QFileDialog::getOpenFileName(&w, QLatin1String("Open"), QString(),
                                                        QString::fromLatin1(filter));
        if (!fn.isEmpty())
            w3DS->presentation()->setSource(QUrl::fromLocalFile(fn));
    });
    buttonLayout->addWidget(openBtn);
    QPushButton *reloadBtn = new QPushButton(QLatin1String("Reload"));
    QObject::connect(reloadBtn, &QPushButton::clicked, w3DS, [w3DS] { w3DS->presentation()->reload(); });
    buttonLayout->addWidget(reloadBtn);
    QPushButton *profBtn = new QPushButton(QLatin1String("Toggle profile UI"));
    QObject::connect(profBtn, &QPushButton::clicked, w3DS, [w3DS] {
        w3DS->presentation()->setProfileUiVisible(!w3DS->presentation()->isProfileUiVisible());
    });
    buttonLayout->addWidget(profBtn);

    w.resize(1024, 768);
    w.show();

    return app.exec();
}
