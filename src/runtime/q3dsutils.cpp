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

#include "q3dsutils.h"
#include "q3dsgraphexplorer.h"
#include <QMessageBox>
#include <QInputDialog>

QT_BEGIN_NAMESPACE

static bool q3ds_dialogsEnabled = true;

// autotests will not want to pop up message boxes, obviously
void Q3DSUtils::setDialogsEnabled(bool enable)
{
    q3ds_dialogsEnabled = enable;
}

void Q3DSUtils::showMessage(const QString &msg)
{
    qWarning("%s", qPrintable(msg));
    if (q3ds_dialogsEnabled)
        QMessageBox::information(nullptr, QObject::tr("Viewer"), msg);
}

QString Q3DSUtils::getInput(const QString &msg)
{
    if (q3ds_dialogsEnabled)
        return QInputDialog::getText(nullptr, QObject::tr("Input"), msg);

    return QString();
}

QString Q3DSUtils::resourcePrefix()
{
#if 0
    // Allow overriding the res folder locations by setting an env var
    QString prefix;
    if (qEnvironmentVariableIsSet("Q3DS_RES"))
        prefix = (qgetenv("Q3DS_RES") + QByteArrayLiteral("/"));
    return prefix;
#endif
    return QLatin1String(":/q3ds/");
}

void Q3DSUtils::showObjectGraph(Q3DSGraphObject *obj)
{
    Q3DSGraphExplorer *w = new Q3DSGraphExplorer(obj);
    w->show();
}

QT_END_NAMESPACE
