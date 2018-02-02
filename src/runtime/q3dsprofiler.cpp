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
#include "q3dsscenemanager_p.h"

#if defined(Q_OS_WIN) && !defined(Q_OS_WINRT)
#include <qt_windows.h>
#include <Psapi.h>
#endif

#ifdef Q_OS_LINUX
#include <QFile>
#include <sys/times.h>
#endif

QT_BEGIN_NAMESPACE

Q3DSProfiler::Q3DSProfiler(const Q3DSGraphicsLimits &limits)
    : m_gfxLimits(limits)
{
    m_frameData.reserve(64 * 1024);
}

Q3DSProfiler::~Q3DSProfiler()
{
    for (auto c : m_objectDestroyConnections)
        QObject::disconnect(c);
}

void Q3DSProfiler::resetForNewScene(Q3DSSceneManager *sceneManager)
{
    for (auto c : m_objectDestroyConnections)
        QObject::disconnect(c);

    m_subMeshData.clear();
    m_subPresProfilers.clear();
    m_frameData.clear();
    m_objectData.clear();
    m_objectDestroyConnections.clear();

    m_sceneManager = sceneManager;
    m_presentation = m_sceneManager->m_presentation;
    Q_ASSERT(m_presentation);
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

void Q3DSProfiler::updateFrameStats(qint64 globalFrameCounter)
{
    if (!m_enabled)
        return;

    Q_ASSERT(!m_frameData.isEmpty());
    FrameData &d(m_frameData.last());
    d.globalFrameCounter = globalFrameCounter;
    d.wasDirty = m_sceneManager->m_wasDirty;
}

void Q3DSProfiler::trackNewObject(QObject *obj, ObjectType type, const char *info, ...)
{
    if (!m_enabled)
        return;

    ObjectData objd(obj, type);
    va_list ap;
    va_start(ap, info);
    objd.info = QString::vasprintf(info, ap);
    va_end(ap);
    m_objectData.insert(type, objd);

    m_objectDestroyConnections.append(QObject::connect(obj, &QObject::destroyed, [this, obj, type]() {
        m_objectData.remove(type, ObjectData(obj, type));
    }));
}

void Q3DSProfiler::reportTotalParseBuildTime(qint64 ms)
{
    m_totalParseBuildTime = ms;
}

void Q3DSProfiler::reportTimeAfterBuildUntilFirstFrameAction(qint64 ms)
{
    m_firstFrameActionTime = ms;
}

void Q3DSProfiler::reportSubMeshData(Q3DSMesh *mesh, const SubMeshData &data)
{
    if (!m_enabled)
        return;

    m_subMeshData.insert(mesh, data);
}

void Q3DSProfiler::registerSubPresentationProfiler(Q3DSPresentation *subPres, Q3DSProfiler *p)
{
    SubPresentationProfiler sp;
    sp.presentation = subPres;
    sp.profiler = p;
    m_subPresProfilers.append(sp);
}

float Q3DSProfiler::cpuLoadForCurrentProcess()
{
    if (!m_cpuLoadTimer.isValid()) {
        m_cpuLoadTimer.start();
    } else {
        // query and calculate every 500 ms only, return the cached value otherwise
        if (m_cpuLoadTimer.elapsed() < 500)
            return m_lastCpuLoad;
        m_cpuLoadTimer.restart();
    }

#if defined(Q_OS_WIN) && !defined(Q_OS_WINRT)
    quint64 kernelTimeSys, userTimeSys, kernelTimeProc, userTimeProc;
    FILETIME creationFTime, exitFTime, kernelFTime, userFTime, idleFTime;
    if (!GetSystemTimes(&idleFTime, &kernelFTime, &userFTime))
        return m_lastCpuLoad;
    kernelTimeSys = quint64(kernelFTime.dwHighDateTime) << 32 | quint64(kernelFTime.dwLowDateTime);
    userTimeSys = quint64(userFTime.dwHighDateTime) << 32 | quint64(userFTime.dwLowDateTime);
    if (!GetProcessTimes(GetCurrentProcess(), &creationFTime, &exitFTime, &kernelFTime, &userFTime))
        return m_lastCpuLoad;
    kernelTimeProc = quint64(kernelFTime.dwHighDateTime) << 32 | quint64(kernelFTime.dwLowDateTime);
    userTimeProc = quint64(userFTime.dwHighDateTime) << 32 | quint64(userFTime.dwLowDateTime);

    float d = (kernelTimeSys - m_lastKernelTimeSys) + (userTimeSys - m_lastUserTimeSys);
    if (d)
        m_lastCpuLoad = ((kernelTimeProc - m_lastKernelTimeProc) + (userTimeProc - m_lastUserTimeProc)) / d * 100.0f;

    m_lastKernelTimeSys = kernelTimeSys;
    m_lastUserTimeSys = userTimeSys;
    m_lastKernelTimeProc = kernelTimeProc;
    m_lastUserTimeProc = userTimeProc;

    return m_lastCpuLoad;
#elif defined(Q_OS_LINUX)
    if (!m_numCpus) {
        QFile f(QLatin1String("/proc/cpuinfo"));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return m_lastCpuLoad;
        for ( ; ; ) {
            QByteArray line = f.readLine();
            if (line.isEmpty())
                break;
            if (line.startsWith(QByteArrayLiteral("processor")))
                ++m_numCpus;
        }
    }
    if (!m_numCpus)
        return m_lastCpuLoad;

    tms processTimes;
    clock_t timestamp = times(&processTimes);
    if (timestamp > m_lastTimestamp) {
        float d = (timestamp - m_lastTimestamp);
        // behave like adb top, max is 100% so divide by number of cores
        m_lastCpuLoad = ((processTimes.tms_stime - m_lastKernel) + (processTimes.tms_utime - m_lastUser)) / d / m_numCpus * 100.0f;
    }
    m_lastTimestamp = timestamp;
    m_lastKernel = processTimes.tms_stime;
    m_lastUser = processTimes.tms_utime;
    return m_lastCpuLoad;
#else
    return m_lastCpuLoad;
#endif
}

QPair<qint64, qint64> Q3DSProfiler::memUsageForCurrentProcess()
{
    if (!m_memUsageTimer.isValid()) {
        m_memUsageTimer.start();
    } else {
        // query and calculate every 500 ms only, return the cached value otherwise
        if (m_memUsageTimer.elapsed() < 500)
            return m_lastMemUsage;
        m_memUsageTimer.restart();
    }

#if defined(Q_OS_WIN) && !defined(Q_OS_WINRT)
    PROCESS_MEMORY_COUNTERS_EX memInfo;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memInfo), sizeof(memInfo)))
        return m_lastMemUsage;

    qint64 physMappedSize = memInfo.WorkingSetSize;
    qint64 commitCharge = memInfo.PrivateUsage;
    m_lastMemUsage = { physMappedSize, commitCharge };
    return m_lastMemUsage;
#elif defined(Q_OS_LINUX)
    QFile f(QLatin1String("/proc/self/status"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return m_lastMemUsage;
    qint64 physMappedSize = 0, commitCharge = 0;
    for ( ; ; ) {
        QByteArray line = f.readLine();
        if (line.isEmpty())
            break;
        if (line.startsWith(QByteArrayLiteral("VmSize:"))) {
            QByteArrayList c = line.mid(7).trimmed().split(' ');
            if (!c.isEmpty())
                commitCharge = c[0].toLongLong() * 1000;
        } else if (line.startsWith(QByteArrayLiteral("VmRSS:"))) {
            QByteArrayList c = line.mid(6).trimmed().split(' ');
            if (!c.isEmpty())
                physMappedSize = c[0].toLongLong() * 1000;
        }
        if (physMappedSize && commitCharge)
            break;
    }
    f.close();
    m_lastMemUsage = { physMappedSize, commitCharge };
    return m_lastMemUsage;
#else
    return m_lastMemUsage;
#endif
}

QT_END_NAMESPACE
