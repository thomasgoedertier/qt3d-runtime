/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
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

#ifndef Q3DSREMOTEDEPLOYMENTMANAGER_H
#define Q3DSREMOTEDEPLOYMENTMANAGER_H

#include <QObject>

QT_BEGIN_NAMESPACE

class Q3DSEngine;
class Q3DSRemoteDeploymentServer;

class Q3DSRemoteDeploymentManager : public QObject
{
    Q_OBJECT
public:
    enum State {
        LocalProject,
        ConnectionInfo,
        RemoteConnected,
        RemoteLoading,
        RemoteProject
    };

    explicit Q3DSRemoteDeploymentManager(Q3DSEngine *engine, int port = 36000);
    ~Q3DSRemoteDeploymentManager();

    void setConnectionPort(int port);

    void startServer();
    void stopServer();

    void showConnectionSetup();

    void setState(Q3DSRemoteDeploymentManager::State state);
    Q3DSRemoteDeploymentManager::State state() const;

private Q_SLOTS:
    void remoteConnected();
    void remoteDisconnected();
    void remoteProjectChanging();
    void loadRemoteProject();
    void loadFile(const QString &filename);

Q_SIGNALS:
    void stateChanged(Q3DSRemoteDeploymentManager::State state);

private:
    void setupConnectionScene();
    QString generateConnectionInfo();
    void setErrorMessage(const QString &errorString);

    Q3DSEngine *m_engine = nullptr;
    Q3DSRemoteDeploymentServer *m_server = nullptr;
    int m_port;
    QString m_errorMessage;
    State m_state = LocalProject;
};

QT_END_NAMESPACE

#endif // Q3DSREMOTEDEPLOYMENTMANAGER_H
