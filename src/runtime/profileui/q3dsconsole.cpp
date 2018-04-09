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

#include "q3dsconsole_p.h"

#include <imgui.h>

QT_BEGIN_NAMESPACE

// console window implementation, based on imgui_demo.cpp

Q3DSConsole::Q3DSConsole()
{
    m_inputBuf[0] = '\0';
}

void Q3DSConsole::clear()
{
    m_contents.clear();
    m_scrollToBottom = true;
}

void Q3DSConsole::addMessage(const QString &msg, const QColor &color)
{
    Item item;
    item.text = msg.toUtf8();
    item.color = color;
    m_contents.append(item);
    m_scrollToBottom = true;
}

void Q3DSConsole::addMessageFmt(const QColor &color, const char *msg, ...)
{
    va_list ap;
    va_start(ap, msg);
    const QString s = QString::vasprintf(msg, ap);
    va_end(ap);
    addMessage(s, color);
}

void Q3DSConsole::addCommand(const Command &command)
{
    m_commands.append(command);
}

void Q3DSConsole::setCommandRecorder(CommandFunc f)
{
    m_recorder = f;
}

void Q3DSConsole::setRecording(bool enabled)
{
    m_recording = enabled;
}

void Q3DSConsole::draw()
{
    ImGui::TextWrapped("Enter 'help' for help, press TAB for completion, UP/DOWN for history.");
    if (ImGui::SmallButton("Clear"))
        clear();

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -ImGui::GetItemsLineHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));

    for (const auto &item : m_contents) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(item.color.redF(), item.color.greenF(), item.color.blueF(), 1.0f));
        ImGui::TextUnformatted(item.text.constData());
        ImGui::PopStyleColor();
    }

    if (m_scrollToBottom) {
        m_scrollToBottom = false;
        ImGui::SetScrollHere();
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::Separator();

    if (m_recording) {
        ImGui::Text("REC");
        ImGui::SameLine();
    }
    if (ImGui::InputText("Input", m_inputBuf, InputBufSize,
                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
                         editCallbackStatic, this))
    {
        const QByteArray fullCmd = QByteArray(m_inputBuf).trimmed();
        if (!fullCmd.isEmpty()) {
            m_historyPos = -1;
            for (int i = m_history.count() - 1; i >= 0; --i) {
                if (m_history[i] == fullCmd) {
                    m_history.removeAt(i);
                    break;
                }
            }
            m_history.append(fullCmd);

            runCommand(fullCmd);
        }
        m_inputBuf[0] = '\0';
    }

    if (ImGui::IsItemHovered() || (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
        ImGui::SetKeyboardFocusHere(-1);
}

void Q3DSConsole::runCommand(const QByteArray &fullCmd, bool fromRecording)
{
    // split to cmd(args)
    const int argsStart = fullCmd.indexOf('(');
    const int argsEnd = fullCmd.lastIndexOf(')');
    QByteArray cmd = fullCmd;
    QByteArray args;
    if (argsStart >= 0 && argsEnd > argsStart) {
        cmd = fullCmd.left(argsStart);
        args = fullCmd.mid(argsStart + 1, argsEnd - argsStart - 1);
    }
    auto it = std::find_if(m_commands.cbegin(), m_commands.cend(), [cmd](const Command &c) { return c.name == cmd; });
    if (it != m_commands.cend()) {
        if (!fromRecording && m_recording && it->flags.testFlag(CmdRecordable)) {
            if (m_recorder)
                m_recorder(fullCmd.constData());
        } else if (it->callback) {
            if (!fromRecording)
                addMessageFmt(Qt::white, "\n > %s\n", fullCmd.constData());
            it->callback(args);
        }
    } else {
        addMessageFmt(Qt::red, "\nUnknown command '%s'", cmd.constData());
    }
}

void Q3DSConsole::runRecordedCommand(const QByteArray &fullCmd)
{
    runCommand(fullCmd, true);
}

int Q3DSConsole::editCallbackStatic(ImGuiTextEditCallbackData *data)
{
    return static_cast<Q3DSConsole *>(data->UserData)->editCallback(data);
}

int Q3DSConsole::editCallback(ImGuiTextEditCallbackData *data)
{
    switch (data->EventFlag) {
    case ImGuiInputTextFlags_CallbackHistory:
    {
        const int prevPos = m_historyPos;
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (m_historyPos == -1)
                m_historyPos = m_history.count() - 1;
            else if (m_historyPos > 0)
                --m_historyPos;
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (m_historyPos != -1)
                m_historyPos = qMin(m_historyPos + 1, m_history.count() - 1);
        }
        if (m_historyPos != prevPos) {
            data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen =
                    int(snprintf(data->Buf, size_t(data->BufSize), "%s",
                                 m_historyPos >= 0 ? m_history[m_historyPos].constData() : ""));
            data->BufDirty = true;
        }
    }
        break;

    case ImGuiInputTextFlags_CallbackCompletion:
    {
        const char *wordEnd = data->Buf + data->CursorPos;
        const char *wordStart = wordEnd;
        while (wordStart > data->Buf)
        {
            const char c = wordStart[-1];
            if (c == ' ' || c == '\t' || c == ',' || c == ';')
                break;
            --wordStart;
        }
        const int matchLen = wordEnd - wordStart;
        if (matchLen == 0)
            break;

        QByteArrayList candidates;
        for (const Command &cmd : m_commands) {
            if (!strncmp(cmd.name.constData(), wordStart, matchLen))
                candidates.append(cmd.name);
        }

        if (candidates.isEmpty()) {
            addMessageFmt(Qt::green, "No matching command");
            break;
        }

        if (candidates.count() == 1) {
            data->DeleteChars(wordStart - data->Buf, matchLen);
            data->InsertChars(data->CursorPos, candidates.first().constData());
            data->InsertChars(data->CursorPos, " ");
            break;
        }

        int candMatchLen = matchLen;
        for ( ; ; ) {
            int c = 0;
            bool allMatch = true;
            for (int i = 0; i < candidates.count() && allMatch; ++i) {
                const char *p = candidates[i].constData();
                if (i == 0)
                    c = toupper(p[candMatchLen]);
                else if (c == 0 || c != toupper(p[candMatchLen]))
                    allMatch = false;
            }
            if (!allMatch)
                break;
            ++candMatchLen;
        }

        if (candMatchLen > 0) {
            data->DeleteChars(wordStart - data->Buf, matchLen);
            const char *p = candidates.first().constData();
            data->InsertChars(data->CursorPos, p, p + candMatchLen);
        }

        addMessageFmt(Qt::green, "Possible matches:\n");
        for (const QByteArray &cand : candidates)
            addMessageFmt(Qt::green, "  %s\n", cand.constData());
    }
        break;

    default:
        break;
    }

    return 0;
}

QT_END_NAMESPACE
