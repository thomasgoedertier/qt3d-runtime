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

#include "q3dswindow.h"
#include "q3dsengine.h"

QT_BEGIN_NAMESPACE

Q3DSWindow::Q3DSWindow(QWindow *parent)
    : QWindow(parent)
{
    setSurfaceType(QSurface::OpenGLSurface);
}

Q3DSWindow::~Q3DSWindow()
{
}

void Q3DSWindow::setEngine(Q3DSEngine *engine)
{
    Q_ASSERT(!m_engine); // engine change is not supported atm
    Q_ASSERT(engine);

    m_engine = engine;
    m_engine->setSurface(this);

    connect(engine, &Q3DSEngine::presentationLoaded, this, [this]() {
        // Size the window to the presentation at first. The window takes
        // control for future resizes afterwards.
        if (!m_implicitSizeTaken) {
            m_implicitSizeTaken = true;
            resize(m_engine->implicitSize());
        }
    });
}

Q3DSEngine *Q3DSWindow::engine() const
{
    return m_engine;
}

void Q3DSWindow::forceResize(const QSize &size)
{
    m_implicitSizeTaken = true; // disable following the presentation size upon the first load
    resize(size);
}

void Q3DSWindow::exposeEvent(QExposeEvent *)
{
    if (isExposed() && m_engine)
        m_engine->start();
}

void Q3DSWindow::resizeEvent(QResizeEvent *)
{
    if (m_engine)
        m_engine->resize(size(), devicePixelRatio());
}

void Q3DSWindow::keyPressEvent(QKeyEvent *e)
{
    if (m_engine)
        m_engine->handleKeyPressEvent(e);
}

void Q3DSWindow::keyReleaseEvent(QKeyEvent *e)
{
    if (m_engine)
        m_engine->handleKeyReleaseEvent(e);
}

void Q3DSWindow::mousePressEvent(QMouseEvent *e)
{
    if (m_engine)
        m_engine->handleMousePressEvent(e);
}

void Q3DSWindow::mouseMoveEvent(QMouseEvent *e)
{
    if (m_engine)
        m_engine->handleMouseMoveEvent(e);
}

void Q3DSWindow::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_engine)
        m_engine->handleMouseReleaseEvent(e);
}

void Q3DSWindow::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (m_engine)
        m_engine->handleMouseDoubleClickEvent(e);
}

QT_END_NAMESPACE
