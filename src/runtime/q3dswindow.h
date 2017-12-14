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
#include <QElapsedTimer>
#include <Qt3DCore/QAspectEngine>
#include <Qt3DStudioRuntime2/Q3DSUipDocument>
#include <Qt3DStudioRuntime2/q3dsscenemanager.h>

QT_BEGIN_NAMESPACE

class Q3DSV_EXPORT Q3DStudioWindow : public QWindow
{
    Q_OBJECT
public:
    Q3DStudioWindow();
    ~Q3DStudioWindow();

    enum InitFlag {
        Force4xMSAA = 0x01
    };
    Q_DECLARE_FLAGS(InitFlags, InitFlag)

    static void initStaticPreApp();
    static void initStaticPostApp(InitFlags flags);

    bool setSource(const QString &uipOrUiaFileName);
    QString source() const;

    QString uipFileName(int index = 0) const;
    Q3DSUipDocument *uipDocument(int index = 0) const;
    Q3DSSceneManager *sceneManager(int index = 0) const;

    // for testing purposes
    void setOnDemandRendering(bool enabled);

Q_SIGNALS:
    void sceneUpdated();

protected:
    void exposeEvent(QExposeEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    struct Presentation {
        QString uipFileName;
        Q3DSUipDocument *uipDocument = nullptr;
        Q3DSSceneManager *sceneManager = nullptr;
        Q3DSSceneManager::Scene q3dscene;
        Q3DSSubPresentation subPres;
    };

    void createAspectEngine();
    bool loadPresentation(Presentation *pres);
    bool loadSubPresentation(Presentation *pres);

    QString m_source; // uip or uia file
    QVector<Presentation> m_presentations;

    QScopedPointer<Qt3DCore::QAspectEngine> m_aspectEngine;

    QElapsedTimer m_profilerActivateTimer;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Q3DStudioWindow::InitFlags)

QT_END_NAMESPACE

#endif // Q3DSTUDIOWINDOW_H
