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

#include "q3dsprofiler_p.h"

#if defined(Q_OS_WIN) && !defined(Q_OS_WINRT)
#include <qt_windows.h>
#include <Psapi.h>
#endif

QT_BEGIN_NAMESPACE

Q3DSProfiler::Q3DSProfiler(const Q3DSGraphicsLimits &limits)
    : m_gfxLimits(limits)
{
    m_frameData.reserve(64 * 1024);
}

void Q3DSProfiler::resetForNewScene(Q3DSPresentation *presentation)
{
    m_frameData.clear();
    m_presentation = presentation;
}

void Q3DSProfiler::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
}

void Q3DSProfiler::reportNewFrame(float deltaMs)
{
    if (!m_enabled)
        return;

    FrameData d;
    d.deltaMs = deltaMs;
    m_frameData.append(d);
}

float Q3DSProfiler::cpuLoadForCurrentProcess()
{
#if defined(Q_OS_WIN) && !defined(Q_OS_WINRT)
    if (!m_cpuLoadTimer.isValid()) {
        m_cpuLoadTimer.start();
    } else {
        // query and calculate every 500 ms only, return the cached value otherwise
        if (m_cpuLoadTimer.elapsed() < 500)
            return m_lastLoad;
        m_cpuLoadTimer.restart();
    }

    quint64 kernelTimeSys, userTimeSys, kernelTimeProc, userTimeProc;
    FILETIME creationFTime, exitFTime, kernelFTime, userFTime, idleFTime;
    if (!GetSystemTimes(&idleFTime, &kernelFTime, &userFTime))
        return m_lastLoad;
    kernelTimeSys = quint64(kernelFTime.dwHighDateTime) << 32 | quint64(kernelFTime.dwLowDateTime);
    userTimeSys = quint64(userFTime.dwHighDateTime) << 32 | quint64(userFTime.dwLowDateTime);
    if (!GetProcessTimes(GetCurrentProcess(), &creationFTime, &exitFTime, &kernelFTime, &userFTime))
        return m_lastLoad;
    kernelTimeProc = quint64(kernelFTime.dwHighDateTime) << 32 | quint64(kernelFTime.dwLowDateTime);
    userTimeProc = quint64(userFTime.dwHighDateTime) << 32 | quint64(userFTime.dwLowDateTime);

    float d = (kernelTimeSys - m_lastKernelTimeSys) + (userTimeSys - m_lastUserTimeSys);
    if (d)
        m_lastLoad = ((kernelTimeProc - m_lastKernelTimeProc) + (userTimeProc - m_lastUserTimeProc)) / d * 100.0f;

    m_lastKernelTimeSys = kernelTimeSys;
    m_lastUserTimeSys = userTimeSys;
    m_lastKernelTimeProc = kernelTimeProc;
    m_lastUserTimeProc = userTimeProc;

    return m_lastLoad;
#else
    return 0.0f;
#endif
}

QPair<qint64, qint64> Q3DSProfiler::memUsageForCurrentProcess()
{
#if defined(Q_OS_WIN) && !defined(Q_OS_WINRT)
    if (!m_memUsageTimer.isValid()) {
        m_memUsageTimer.start();
    } else {
        // query and calculate every 500 ms only, return the cached value otherwise
        if (m_memUsageTimer.elapsed() < 500)
            return m_lastMemUsage;
        m_memUsageTimer.restart();
    }

    PROCESS_MEMORY_COUNTERS_EX memInfo;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memInfo), sizeof(memInfo)))
        return { 0, 0 };

    qint64 physMappedSize = memInfo.WorkingSetSize;
    qint64 commitCharge = memInfo.PrivateUsage;
    m_lastMemUsage = { physMappedSize, commitCharge };
    return m_lastMemUsage;
#else
    return { 0, 0 };
#endif
}

QT_END_NAMESPACE
