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

#ifndef Q3DSTUDIOWINDOW_H
#define Q3DSTUDIOWINDOW_H

#include <QWindow>
#include <Qt3DCore/QAspectEngine>
#include <Qt3DStudioRuntime2/Q3DSUipDocument>
#include <Qt3DStudioRuntime2/q3dsscenemanager.h>

QT_BEGIN_NAMESPACE

class Q3DSSceneManager;

class Q3DSV_EXPORT Q3DStudioWindow : public QWindow
{
public:
    Q3DStudioWindow();

    bool setUipSource(const QString &filename);
    QString uipSource() const { return m_uipFileName; }

    enum InitFlag {
        MSAA4x = 0x01
    };
    Q_DECLARE_FLAGS(InitFlags, InitFlag)

    static void initStaticPreApp();
    static void initStaticPostApp(InitFlags flags);

    Q3DSUipDocument *uip() { return &m_uipDocument; }
    Q3DSSceneManager *sceneManager() { return &m_sceneManager; }

    void setOnDemandRendering(bool enabled);

protected:
    void exposeEvent(QExposeEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    void createAspectEngine();

    QString m_uipFileName;
    Q3DSUipDocument m_uipDocument;
    Q3DSSceneManager m_sceneManager;
    Q3DSSceneManager::Scene m_q3dscene;

    QScopedPointer<Qt3DCore::QAspectEngine> m_aspectEngine;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DStudioWindow::InitFlags)

QT_END_NAMESPACE

#endif // Q3DSTUDIOWINDOW_H
