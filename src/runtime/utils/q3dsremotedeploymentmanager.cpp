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

#include "q3dsremotedeploymentmanager_p.h"
#include "q3dsremotedeploymentserver_p.h"
#include <private/q3dsengine_p.h>

#include <Q3DSPresentation>
#include <Q3DSDataInput>

namespace {
QString c_introPresentation() { return QStringLiteral(":/q3ds/res/intro/Intro.uia"); }
QString c_connectionTextDataInput() { return QStringLiteral("connectionInfo"); }
QString c_connectionErrorDataInput() { return QStringLiteral("errorInfo"); }
}

QT_BEGIN_NAMESPACE

Q3DSRemoteDeploymentManager::Q3DSRemoteDeploymentManager(Q3DSEngine *engine, int port, bool hasWidgets)
    : m_engine(engine)
    , m_port(port)
    , m_hasWidgets(hasWidgets)
{
    // setup the server
    m_server = new Q3DSRemoteDeploymentServer(m_port, this);

    connect(m_server, &Q3DSRemoteDeploymentServer::remoteConnected,
            this, &Q3DSRemoteDeploymentManager::remoteConnected);
    connect(m_server, &Q3DSRemoteDeploymentServer::remoteDisconnected,
            this, &Q3DSRemoteDeploymentManager::remoteDisconnected);
    connect(m_server, &Q3DSRemoteDeploymentServer::projectChanging,
            this, &Q3DSRemoteDeploymentManager::remoteProjectChanging);
    connect(m_server, &Q3DSRemoteDeploymentServer::projectChanged, [this] {
        m_isRemoteProjectLoaded = true;
        loadRemoteProject();
    });
    connect(m_server, &Q3DSRemoteDeploymentServer::updateProgress,
            this, &Q3DSRemoteDeploymentManager::setProgress);

    m_connectionInfoTimer.setInterval(1000);
    m_connectionInfoTimer.setSingleShot(true);
    connect(&m_connectionInfoTimer, &QTimer::timeout, [this] {
        m_isReadyToShow = true;
        loadRemoteProject();
    });
}

Q3DSRemoteDeploymentManager::~Q3DSRemoteDeploymentManager()
{
}

void Q3DSRemoteDeploymentManager::setConnectionPort(int port)
{
    if (port == m_port)
        return;

    m_port = port;
    if (m_server && m_server->isConnected()) {
        m_server->disconnectRemote();
        m_server->setServerPort(m_port);
        m_errorMessage = m_server->startServer();
    } else {
        m_server->setServerPort(m_port);
    }
}

void Q3DSRemoteDeploymentManager::startServer()
{
    if (!m_server->isConnected())
        m_errorMessage = m_server->startServer();
}

void Q3DSRemoteDeploymentManager::stopServer()
{
    if (m_server->isConnected())
        m_server->disconnectRemote();
}

void Q3DSRemoteDeploymentManager::showConnectionSetup()
{
    startServer();
    setupConnectionScene();
}

void Q3DSRemoteDeploymentManager::setState(Q3DSRemoteDeploymentManager::State state)
{
    if (state == m_state)
        return;

    m_state = state;
    stateChanged(state);
}

Q3DSRemoteDeploymentManager::State Q3DSRemoteDeploymentManager::state() const
{
    return m_state;
}

void Q3DSRemoteDeploymentManager::remoteConnected()
{
    if (m_state == LocalProject || m_state == RemoteProject)
        showConnectionSetup();

    setState(RemoteConnected);
    setErrorMessage(tr("Connected!"));
}

void Q3DSRemoteDeploymentManager::remoteDisconnected()
{
    if (m_state == LocalProject || m_state == RemoteProject)
        showConnectionSetup();
    setErrorMessage(tr("Disconnected from Qt 3D Studio..."));
}

void Q3DSRemoteDeploymentManager::remoteProjectChanging()
{
    // Only set this state the first time (until text bug is fixed)
    // it would be possible to impliment a progress bar but right
    // now we can't change text every frame in Qt3D.
    if (m_state == RemoteLoading)
        return;

    if (m_state == LocalProject || m_state == RemoteProject)
        showConnectionSetup();
    setState(RemoteLoading);
    m_connectionInfoTimer.start();
    setErrorMessage(tr("Loading Remote Project..."));
}

void Q3DSRemoteDeploymentManager::loadRemoteProject()
{
    setErrorMessage(tr("Loading Presentation..."));
    if (m_isReadyToShow && m_isRemoteProjectLoaded) {
        setState(RemoteProject);
        const QString remoteProject = m_server->fileName();
        loadFile(remoteProject);
        m_isReadyToShow = false;
        m_isRemoteProjectLoaded = false;
    }
}

void Q3DSRemoteDeploymentManager::loadFile(const QString &filename)
{
    QString targetFilename = filename;
    // Try to find the application (*.uia) file for loading instead of the presentation (*.uip)
    // in case we are connected to remote sender.
    if (targetFilename.endsWith(QStringLiteral(".uip"))) {
        targetFilename.chop(4);
        targetFilename.append(QStringLiteral(".uia"));
        QFileInfo targetfileInfo(targetFilename);
        // uia not found, revert to given uip
        if (!targetfileInfo.exists())
            targetFilename = filename;
    }

    QFileInfo fileInfo(targetFilename);
    if (!fileInfo.exists()) {
        setupConnectionScene();
        setErrorMessage(QString(tr("Tried to load nonexistent file %1").arg(targetFilename)));
        return;
    }

    m_engine->setSource(fileInfo.absoluteFilePath());
}

void Q3DSRemoteDeploymentManager::setupConnectionScene()
{
    // start the connection information scene
    m_engine->setSource(c_introPresentation());
    m_engine->setDataInputValue(c_connectionTextDataInput(), generateConnectionInfo());
    m_engine->setDataInputValue(c_connectionErrorDataInput(), m_errorMessage);
    if (!m_server->isConnected())
        setState(ConnectionInfo);
}

QString Q3DSRemoteDeploymentManager::generateConnectionInfo()
{
    QString msg = QObject::tr("Use IP: %1 and Port: %2\n"
                              "in Qt 3D Studio Editor to connect to this viewer.\n\n")
            .arg(m_server->hostAddress().toString())
            .arg(QString::number(m_port));

    if (m_hasWidgets)
        msg += QObject::tr("Use File/Open... to open a local presentation.");

    return msg;
}

void Q3DSRemoteDeploymentManager::setErrorMessage(const QString &errorString)
{
    m_errorMessage = errorString;
    m_engine->setDataInputValue(c_connectionErrorDataInput(), errorString);
}

void Q3DSRemoteDeploymentManager::setProgress(int progress)
{
    // Don't wait for 100%, as it'll already start loading and text isn't updated anymore
    if (progress >= 98)
        setErrorMessage(tr("Loading Presentation..."));
    else
        setErrorMessage(tr("Receiving (%1%)").arg(progress));
}

QT_END_NAMESPACE
