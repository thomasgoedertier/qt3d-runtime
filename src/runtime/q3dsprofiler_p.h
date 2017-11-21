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

#ifndef Q3DSPROFILER_P_H
#define Q3DSPROFILER_P_H

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

#include <QVector>
#include <QElapsedTimer>
#include "q3dsgraphicslimits.h"

QT_BEGIN_NAMESPACE

class Q3DSPresentation;

class Q3DSProfiler
{
public:
    Q3DSProfiler(const Q3DSGraphicsLimits &limits);
    void resetForNewScene(Q3DSPresentation *presentation);

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    void reportNewFrame(float deltaMs);

    struct FrameData {
        float deltaMs = 0;
    };

    const QVector<FrameData> *frameData() const { return &m_frameData; }
    const Q3DSGraphicsLimits *graphicsLimits() const { return &m_gfxLimits; }
    const Q3DSPresentation *presentation() const { return m_presentation; }
    Q3DSPresentation *presentation() { return m_presentation; }

    float cpuLoadForCurrentProcess();
    QPair<qint64, qint64> memUsageForCurrentProcess();

private:
    bool m_enabled = false; // disabled by default, profiling is opt-in
    Q3DSGraphicsLimits m_gfxLimits;
    QVector<FrameData> m_frameData;
    Q3DSPresentation *m_presentation = nullptr;

    QElapsedTimer m_cpuLoadTimer;
    float m_lastCpuLoad = 0;
    QElapsedTimer m_memUsageTimer;
    QPair<qint64, qint64> m_lastMemUsage;
#if defined(Q_OS_WIN) && !defined(Q_OS_WINRT)
    quint64 m_lastKernelTimeSys = 0;
    quint64 m_lastUserTimeSys = 0;
    quint64 m_lastKernelTimeProc = 0;
    quint64 m_lastUserTimeProc = 0;
#elif defined(Q_OS_LINUX)
    int m_numCpus = 0;
    qint64 m_lastTimestamp = 0;
    qint64 m_lastKernel = 0;
    qint64 m_lastUser = 0;
#endif
};

Q_DECLARE_TYPEINFO(Q3DSProfiler::FrameData, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif // Q3DSPROFILER_P_H
