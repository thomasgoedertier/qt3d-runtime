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

#ifndef Q3DSIMGUIMANAGER_P_H
#define Q3DSIMGUIMANAGER_P_H

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

#include <functional>
#include <QSize>
#include <QEntity>

QT_BEGIN_NAMESPACE

class Q3DSImguiInputEventFilter;

namespace Qt3DRender {
class QBuffer;
class QTexture2D;
class QMaterial;
class QLayer;
class QGeometryRenderer;
class QShaderProgram;
class QScissorTest;
class QParameter;
class QFilterKey;
class QDepthTest;
class QNoDepthMask;
class QBlendEquation;
class QBlendEquationArguments;
class QCullFace;
}

class Q3DSImguiManager
{
public:
    ~Q3DSImguiManager();

    typedef std::function<void()> FrameFunc;
    void setFrameFunc(FrameFunc f) { m_frame = f; }

    void setInputEventSource(QObject *src);

    struct OutputInfo {
        QSize size;
        qreal dpr;
        Qt3DRender::QLayer *guiTag;
        Qt3DRender::QLayer *activeGuiTag;
        Qt3DRender::QFilterKey *guiTechniqueFilterKey;
    };
    typedef std::function<OutputInfo()> OutputInfoFunc;
    void setOutputInfoFunc(OutputInfoFunc f) { m_outputInfoFunc = f; }

    void initialize(Qt3DCore::QEntity *rootEntity);
    void releaseResources();

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

private:
    struct CmdListEntry;
    void resizePool(CmdListEntry *e, int newSize);
    Qt3DRender::QMaterial *buildMaterial(Qt3DRender::QScissorTest **scissor);
    void updateGeometry(CmdListEntry *e, int idx, uint elemCount, int vertexCount,
                        int indexCount, const void *indexOffset);
    void update3D();
    void updateInput();

    FrameFunc m_frame = nullptr;
    OutputInfoFunc m_outputInfoFunc = nullptr;
    OutputInfo m_outputInfo;
    QObject *m_inputEventSource = nullptr;
    Q3DSImguiInputEventFilter *m_inputEventFilter = nullptr;
    bool m_inputInitialized = false;
    bool m_enabled = true;

    Qt3DCore::QEntity *m_rootEntity = nullptr;
    Qt3DRender::QTexture2D *m_atlasTex = nullptr;
    struct SharedRenderPassData {
        bool valid = false;
        Qt3DRender::QShaderProgram *progES2;
        Qt3DRender::QShaderProgram *progGL3;
        Qt3DRender::QFilterKey *techniqueFilterKey;
        Qt3DRender::QParameter *texParam;
        Qt3DRender::QDepthTest *depthTest;
        Qt3DRender::QNoDepthMask *noDepthWrite;
        Qt3DRender::QBlendEquation *blendFunc;
        Qt3DRender::QBlendEquationArguments *blendArgs;
        Qt3DRender::QCullFace *cullFace;
        QVector<Qt3DCore::QNode *> enabledToggle;
    } rpd;

    struct CmdEntry {
        Qt3DCore::QEntity *entity = nullptr;
        Qt3DRender::QGeometryRenderer *geomRenderer = nullptr;
        Qt3DRender::QScissorTest *scissor = nullptr;
    };

    struct CmdListEntry {
        Qt3DRender::QBuffer *vbuf = nullptr;
        Qt3DRender::QBuffer *ibuf = nullptr;
        QVector<CmdEntry> cmds;
        int activeSize = 0;
    };

    QVector<CmdListEntry> m_cmdList;
};

QT_END_NAMESPACE

#endif
