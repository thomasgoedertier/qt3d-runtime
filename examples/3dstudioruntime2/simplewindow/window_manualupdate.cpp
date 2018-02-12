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

#include "window_manualupdate.h"
#include <QOpenGLContext>
#include <Q3DSSurfaceViewer>
#include <Q3DSPresentation>

// Here the window implementation drives rendering by manually calling
// Q3DSSurfaceViewer::update() as seen fit.

// It also relies on the default of autoSize==true (meaning the rendering
// is automatically following the QWindow size).

ManualUpdateWindow::ManualUpdateWindow()
{
    setSurfaceType(QSurface::OpenGLSurface);
    m_context = new QOpenGLContext(this);
    m_context->create();
}

void ManualUpdateWindow::update()
{
    if (isExposed() && m_viewer) {
        m_viewer->update();
        // render continuously
        requestUpdate();
    }
}

bool ManualUpdateWindow::event(QEvent *e)
{
    if (e->type() == QEvent::Expose || e->type() == QEvent::UpdateRequest)
        update();

    return QWindow::event(e);
}

void ManualUpdateWindow::keyPressEvent(QKeyEvent *e)
{
    m_viewer->presentation()->keyPressEvent(e);
}

void ManualUpdateWindow::keyReleaseEvent(QKeyEvent *e)
{
    m_viewer->presentation()->keyReleaseEvent(e);
}

void ManualUpdateWindow::mousePressEvent(QMouseEvent *e)
{
    m_viewer->presentation()->mousePressEvent(e);
}

void ManualUpdateWindow::mouseMoveEvent(QMouseEvent *e)
{
    // Moves must be forwarded even when no button is down; the
    // default QWindow behavior is just what we need in this respect.
    m_viewer->presentation()->mouseMoveEvent(e);
}

void ManualUpdateWindow::mouseReleaseEvent(QMouseEvent *e)
{
    m_viewer->presentation()->mouseReleaseEvent(e);
}

void ManualUpdateWindow::mouseDoubleClickEvent(QMouseEvent *e)
{
    m_viewer->presentation()->mouseDoubleClickEvent(e);
}

#if QT_CONFIG(wheelevent)
void ManualUpdateWindow::wheelEvent(QWheelEvent *e)
{
    m_viewer->presentation()->wheelEvent(e);
}
#endif
