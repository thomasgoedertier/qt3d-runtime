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

#ifndef Q3DSREMOTEDEPLOYMENTSERVER_H
#define Q3DSREMOTEDEPLOYMENTSERVER_H

#include <QtCore/QObject>
#include <QtCore/QDataStream>
#include <QtCore/QTemporaryDir>
#include <QtNetwork/QTcpServer>

QT_BEGIN_NAMESPACE

class Q3DSEngine;

class Q3DSRemoteDeploymentServer : public QObject
{
    Q_OBJECT
public:
    explicit Q3DSRemoteDeploymentServer(int serverPort, QObject *parent = nullptr);
    ~Q3DSRemoteDeploymentServer();

    QString startServer();
    void disconnectRemote();

    QHostAddress hostAddress() const { return m_hostAddress; }
    int serverPort() const { return m_serverPort; }
    void setServerPort(int port) { m_serverPort = port; }
    bool isConnected() const { return m_connection; }
    bool isProjectDeployed() const { return m_connection && m_projectDeployed; }
    QString fileName() const { return m_projectFile; }

Q_SIGNALS:
    void projectChanged();
    void projectChanging();
    void remoteConnected();
    void remoteDisconnected();

private Q_SLOTS:
    void acceptRemoteConnection();
    void acceptRemoteDisconnection();
    void readProject();
    void setPort(int value);

private:
    Q3DSEngine *m_engine = nullptr;
    QTcpServer *m_tcpServer = nullptr;
    QTcpSocket *m_connection = nullptr;
    QHostAddress m_hostAddress;
    QDataStream m_incoming;
    QTemporaryDir *m_temporaryDir = nullptr;
    QString m_projectFile;
    bool m_projectDeployed;
    int m_serverPort;
};

QT_END_NAMESPACE

#endif // Q3DSREMOTEDEPLOYMENTRECEIVER_H
