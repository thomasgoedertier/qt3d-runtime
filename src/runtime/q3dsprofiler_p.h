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
#include <QMultiMap>
#include <QObject>
#include <QHash>
#include <QSet>

QT_BEGIN_NAMESPACE

class Q3DSSceneManager;
class Q3DSUipPresentation;
class Q3DSMesh;

namespace Qt3DCore {
class QEntity;
}

namespace Qt3DRender {
class QFrameGraphNode;
}

class Q3DSProfiler
{
public:
    Q3DSProfiler();
    ~Q3DSProfiler();
    void resetForNewScene(Q3DSSceneManager *sceneManager);

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    void reportNewFrame(float deltaMs);
    void updateFrameStats(qint64 globalFrameCounter);

    enum ObjectType {
        UnknownObject,
        RenderTargetObject,
        Texture2DObject,
        TextureCubeObject,
        MeshObject,
        ShaderProgramObject
    };
    void trackNewObject(QObject *obj, ObjectType type, const char *info, ...);
    void vtrackNewObject(QObject *obj, ObjectType type, const char *info, va_list args);
    void updateObjectInfo(QObject *obj, ObjectType type, const char *info, ...);

    struct FrameData {
        float deltaMs = 0;
        qint64 globalFrameCounter = 0;
        bool wasDirty = false;
    };

    const QVector<FrameData> *frameData() const { return &m_frameData; }

    struct ObjectData {
        ObjectData() { }
        ObjectData(QObject *obj_, ObjectType type_)
            : obj(obj_), type(type_)
        { }
        QObject *obj = nullptr;
        ObjectType type = UnknownObject;
        QString info;
    };

    const QMultiMap<ObjectType, ObjectData> *objectData() const { return &m_objectData; }

    void reportTotalParseBuildTime(qint64 ms);
    qint64 totalParseBuildTime() const { return m_totalParseBuildTime; }
    void reportBehaviorStats(qint64 loadTimeMs, int loadCount);
    qint64 behaviorLoadTime() const { return m_behaviorLoadTime; }
    int behaviorActiveCount() const { return m_behaviorActiveCount; }
    void reportTimeAfterBuildUntilFirstFrameAction(qint64 ms);
    qint64 timeAfterBuildUntilFirstFrameAction() const { return m_firstFrameActionTime; }

    struct SubMeshData {
        bool needsBlending = false;
    };
    void reportSubMeshData(Q3DSMesh *mesh, const SubMeshData &data);
    SubMeshData subMeshData(Q3DSMesh *mesh) const { return m_subMeshData.value(mesh); }

    void registerSubPresentationProfiler(Q3DSProfiler *p);
    const QVector<Q3DSProfiler *> *subPresentationProfilers() const { return &m_subPresProfilers; }
    Q3DSProfiler *mainPresentationProfiler();

    void reportQt3DSceneGraphRoot(Qt3DCore::QEntity *rootEntity); // there can be multiple ones
    QVector<Qt3DCore::QEntity *> qt3dSceneGraphRoots() const { return m_qt3dSceneGraphRoots; }
    void reportFrameGraphRoot(Qt3DRender::QFrameGraphNode *fgNode);
    void reportFrameGraphStopNode(Qt3DRender::QFrameGraphNode *fgNode);
    Qt3DRender::QFrameGraphNode *frameGraphRoot() const { return m_frameGraphRoot; }
    QSet<Qt3DRender::QFrameGraphNode *> frameGraphStopNodes() const { return m_fgStopNodes; }

    const Q3DSUipPresentation *presentation() const { return m_presentation; }
    Q3DSUipPresentation *presentation() { return m_presentation; }
    QString presentationName() const { return m_presentationName; }

    // note that we do not expose the scene manager to the profile ui. instead,
    // such data is expected to be mediated through the profiler object.

    QStringList log() const { return m_log; }
    bool hasLogChanged();
    void clearLog();
    void addLog(const QString &msg);

    void sendDataInputValueChange(const QString &dataInputName, const QVariant &value);
    void setLayerCaching(bool enabled);

    float cpuLoadForCurrentProcess();
    QPair<qint64, qint64> memUsageForCurrentProcess();

private:
    bool m_enabled = false; // disabled by default, profiling is opt-in
    QVector<FrameData> m_frameData;
    QMultiMap<ObjectType, ObjectData> m_objectData;
    QVector<QMetaObject::Connection> m_objectDestroyConnections;
    Q3DSSceneManager *m_sceneManager = nullptr;
    Q3DSUipPresentation *m_presentation = nullptr;
    QString m_presentationName;
    qint64 m_totalParseBuildTime = 0;
    qint64 m_behaviorLoadTime = 0;
    int m_behaviorActiveCount = 0;
    qint64 m_firstFrameActionTime = 0;
    QHash<Q3DSMesh *, SubMeshData> m_subMeshData;
    QVector<Q3DSProfiler *> m_subPresProfilers;
    QStringList m_log;
    bool m_logChanged = false;
    Q3DSProfiler *m_mainProfiler = nullptr;
    QVector<Qt3DCore::QEntity *> m_qt3dSceneGraphRoots;
    Qt3DRender::QFrameGraphNode *m_frameGraphRoot = nullptr;
    QSet<Qt3DRender::QFrameGraphNode *> m_fgStopNodes;

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
Q_DECLARE_TYPEINFO(Q3DSProfiler::ObjectData, Q_MOVABLE_TYPE);
Q_DECLARE_TYPEINFO(Q3DSProfiler::SubMeshData, Q_MOVABLE_TYPE);

inline bool operator==(const Q3DSProfiler::ObjectData &lhs, const Q3DSProfiler::ObjectData &rhs) Q_DECL_NOTHROW
{
    return lhs.type == rhs.type && lhs.obj == rhs.obj;
}

inline bool operator!=(const Q3DSProfiler::ObjectData &lhs, const Q3DSProfiler::ObjectData &rhs) Q_DECL_NOTHROW
{
    return !(lhs == rhs);
}

QT_END_NAMESPACE

#endif // Q3DSPROFILER_P_H
