/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef Q3DSCONSOLE_P_H
#define Q3DSCONSOLE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of a number of Qt sources files.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <QString>
#include <QVector>
#include <QColor>
#include <functional>

struct ImGuiTextEditCallbackData;

QT_BEGIN_NAMESPACE

class Q3DSConsole
{
public:
    Q3DSConsole();
    void draw();
    void addMessage(const QString &msg, const QColor &color = Qt::white);
    void addMessageFmt(const QColor &color, const char *msg, ...);

    typedef std::function<void(const QByteArray &)> CommandFunc;
    struct Command {
        QByteArray name;
        CommandFunc callback = nullptr;
    };
    static Command makeCommand(const char *name, CommandFunc cb) {
        Command c;
        c.name = name;
        c.callback = cb;
        return c;
    }
    void addCommand(const Command &command);

    void clear();

private:
    static int editCallbackStatic(ImGuiTextEditCallbackData *data);
    int editCallback(ImGuiTextEditCallbackData *data);

    struct Item {
        QByteArray text;
        QColor color;
    };

    QVector<Item> m_contents;
    QVector<Command> m_commands;
    bool m_scrollToBottom = true;
    static const int InputBufSize = 512;
    char m_inputBuf[InputBufSize];
    QByteArrayList m_history;
    int m_historyPos = -1;
};

QT_END_NAMESPACE

#endif
