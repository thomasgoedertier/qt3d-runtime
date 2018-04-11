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

#include "q3dsprofileui_p.h"
#include "q3dsimguimanager_p.h"
#include "q3dsprofiler_p.h"
#include "q3dsuippresentation_p.h"
#include "q3dsmesh_p.h"
#include "q3dsenummaps_p.h"
#include "q3dsimagemanager_p.h"
#include "q3dsgraphicslimits_p.h"
#include <QLoggingCategory>
#include <QGuiApplication>
#include <QMetaObject>
#include <QClipboard>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QPaintedTextureImage>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QLayerFilter>
#include <Qt3DRender/QLayer>
#include <Qt3DRender/QRenderPassFilter>
#include <Qt3DRender/QFilterKey>

#include <imgui.h>
#include "q3dsconsole_p.h"

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcProf, "q3ds.profileui")

static const int MAX_FRAME_DELTA_COUNT = 1000; // plot the last 1000 frame deltas at most
static const int MAX_LOG_FILTER_ENTRIES = 20;

class Q3DSProfileView
{
public:
    Q3DSProfileView(Q3DSProfiler *profiler, Q3DSProfileUi::ConsoleInitFunc consoleInitFunc);
    ~Q3DSProfileView();

    void frame();

    void openLogAndConsole() { m_logWindowOpen = m_consoleWindowOpen = true; }

private:
    void addQt3DObjectsWindow();
    void addLayerWindow();
    void addBehaviorWindow();
    void addLogWindow();
    void addConsoleWindow();
    void addFrameGraphWindow();
    void addAlterSceneStuff();
    void addPresentationSelector();
    Q3DSProfiler *selectedProfiler() const;
    bool isFiltered(const QString &s) const;
    void addFrameGraphNode(Qt3DRender::QFrameGraphNode *fg, const QSet<Qt3DRender::QFrameGraphNode *> &stopNodes);

    Q3DSProfiler *m_profiler;
    Q3DSProfileUi::ConsoleInitFunc m_consoleInitFunc;

    int m_frameDeltaCount = 100; // last 100 frames
    float m_frameDeltaPlotMin = 0; // bottom 0 ms
    float m_frameDeltaPlotMax = 100; // top 100 ms

    QVector<Q3DSLightNode *> m_disabledShadowCasters;

    bool m_qt3dObjectsWindowOpen = false;
    bool m_layerWindowOpen = false;
    bool m_behaviorWindowOpen = false;
    bool m_logWindowOpen = false;
    bool m_consoleWindowOpen = false;
    bool m_logScrollToBottomOnChange = true;
    int m_currentPresentationIndex = 0;
    bool m_logFilterWindowOpen = false;
    const char *m_logFilterPrefixes[MAX_LOG_FILTER_ENTRIES];
    bool m_logFilterEnabled[MAX_LOG_FILTER_ENTRIES];
    bool m_dataInputWindowOpen = false;
    bool m_frameGraphWindowOpen = false;
    QHash<QString, QByteArray> m_dataInputTextBuf;
    QHash<QString, float> m_dataInputFloatBuf;
    QHash<QString, QVector2D> m_dataInputVec2Buf;
    QHash<QString, QVector3D> m_dataInputVec3Buf;
    Q3DSConsole *m_console = nullptr;
    bool m_layerCaching = true;
};

Q3DSProfileView::Q3DSProfileView(Q3DSProfiler *profiler, Q3DSProfileUi::ConsoleInitFunc consoleInitFunc)
    : m_profiler(profiler),
      m_consoleInitFunc(consoleInitFunc)
{
    int i = 0;
    m_logFilterPrefixes[i] = "q3ds.perf";
    m_logFilterEnabled[i] = true;
    ++i;
    m_logFilterPrefixes[i] = "q3ds.uip";
    m_logFilterEnabled[i] = true;
    ++i;
    m_logFilterPrefixes[i] = "q3ds.uipprop";
    m_logFilterEnabled[i] = true;
    ++i;
    m_logFilterPrefixes[i] = "q3ds.scene";
    m_logFilterEnabled[i] = true;
    ++i;
    m_logFilterPrefixes[i] = "q3ds.slideplayer";
    m_logFilterEnabled[i] = true;
    ++i;
    m_logFilterPrefixes[i] = "q3ds.profileui";
    m_logFilterEnabled[i] = true;
    ++i;
    m_logFilterPrefixes[i] = "q3ds.studio3d";
    m_logFilterEnabled[i] = true;
    ++i;
    m_logFilterPrefixes[i] = "q3ds.surface";
    m_logFilterEnabled[i] = true;
    ++i;
    m_logFilterPrefixes[i] = "q3ds.widget";
    m_logFilterEnabled[i] = true;
    ++i;
    m_logFilterPrefixes[i] = "qml";
    m_logFilterEnabled[i] = true;
    ++i;
    m_logFilterPrefixes[i] = nullptr;
}

Q3DSProfileView::~Q3DSProfileView()
{
    delete m_console;
}

static void addTip(const char *s)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(300);
        ImGui::TextUnformatted(s);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void Q3DSProfileView::addPresentationSelector()
{
    QByteArray presNames = m_profiler->presentationName().toUtf8();
    presNames += '\0';
    for (Q3DSProfiler *subPresProfiler : *m_profiler->subPresentationProfilers()) {
        presNames += subPresProfiler->presentationName().toUtf8();
        presNames += '\0';
    }
    ImGui::Combo("Presentation", &m_currentPresentationIndex, presNames.constData());
}

Q3DSProfiler *Q3DSProfileView::selectedProfiler() const
{
    if (m_currentPresentationIndex == 0)
        return m_profiler;

    return m_profiler->subPresentationProfilers()->at(m_currentPresentationIndex - 1);
}

bool Q3DSProfileView::isFiltered(const QString &s) const
{
    for (int i = 0; i < MAX_LOG_FILTER_ENTRIES; ++i) {
        if (!m_logFilterPrefixes[i])
            break;
        if (!m_logFilterEnabled[i] && s.startsWith(QString::fromLatin1(m_logFilterPrefixes[i])))
            return true;
    }
    return false;
}

void Q3DSProfileView::frame()
{
    ImGui::Begin("Profile", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

    if (!m_profiler->isEnabled()) {
        if (ImGui::Button("Enable profiling")) {
            qCDebug(lcProf, "Profiling enabled");
            m_profiler->setEnabled(true);
        }
    } else {
        if (ImGui::Button("Disable profiling")) {
            qCDebug(lcProf, "Profiling disabled");
            m_profiler->setEnabled(false);
        }
    }
    addTip("Toggles profiling. Note that some features, like Qt 3D object statistics, "
           "need profiling enabled upon opening the presentation and will give "
           "partial results only when enabled afterwards at runtime.");

    if (ImGui::CollapsingHeader("System load")) {
        ImGui::Text("Current process");
        ImGui::Text("CPU: %.1f%%", m_profiler->cpuLoadForCurrentProcess());
        auto memUsage = m_profiler->memUsageForCurrentProcess();
        float physMappedSize = (memUsage.first / 1000) / 1000.0f;
        float commitCharge = (memUsage.second / 1000) / 1000.0f;
        ImGui::Text("Mapped physical memory: %.1f MB", physMappedSize);
        ImGui::Text("Commit charge: %.1f MB", commitCharge);
    }

    if (ImGui::CollapsingHeader("OpenGL")) {
        Q3DSGraphicsLimits gfxLimits = Q3DS::graphicsLimits();
        ImGui::Text("RENDERER: %s", gfxLimits.renderer.constData());
        ImGui::Text("VENDOR: %s", gfxLimits.vendor.constData());
        ImGui::Text("VERSION: %s", gfxLimits.version.constData());
        ImGui::Text("Multisample textures supported: %s", (gfxLimits.multisampleTextureSupported ? "yes" : "no"));
        ImGui::Text("MAX_DRAW_BUFFERS: %d", gfxLimits.maxDrawBuffers);
    }

    if (ImGui::CollapsingHeader("Frame rate")) {
        const QVector<Q3DSProfiler::FrameData> *frameData = m_profiler->frameData();
        float v[MAX_FRAME_DELTA_COUNT];
        int count = 0;
        float avgFrameDelta = 0, minFrameDelta = 0, maxFrameDelta = 0;

        ImGui::SliderInt("# last frames", &m_frameDeltaCount, 2, MAX_FRAME_DELTA_COUNT);

        for (int i = qMax(0, frameData->count() - m_frameDeltaCount); i < frameData->count(); ++i) {
            float deltaMs = (*frameData)[i].deltaMs;
            if (deltaMs < minFrameDelta || minFrameDelta == 0)
                minFrameDelta = deltaMs;
            if (deltaMs > maxFrameDelta)
                maxFrameDelta = deltaMs;
            avgFrameDelta += deltaMs;
            v[count++] = deltaMs;
        }
        if (count)
            avgFrameDelta /= count;

        ImGui::Text("Min: %.3f ms Max: %.3f ms Gui avg: %.3f", minFrameDelta, maxFrameDelta, 1000.0f / ImGui::GetIO().Framerate);

        ImGui::SliderFloat("plot min", &m_frameDeltaPlotMin, 0.0f, 100.f);
        ImGui::SliderFloat("plot max", &m_frameDeltaPlotMax, 0.0f, 100.f);

        char overlayText[256];
        qsnprintf(overlayText, sizeof(overlayText), "Avg frame delta: %.3f ms", avgFrameDelta);
        ImGui::PlotLines("", v, count, 0, overlayText, m_frameDeltaPlotMin, m_frameDeltaPlotMax, ImVec2(0, 60));
        addTip("Elapsed time between frames on the main thread (QFrameAction callback)");
    }

    if (ImGui::CollapsingHeader("Perf. stats")) {
        qint64 totalTime = m_profiler->totalParseBuildTime();
        ImGui::Text("Combined parse/build time: %u ms", (uint) m_profiler->totalParseBuildTime());
        ImGui::Text("Per presentation times:");
        ImGui::Text("parse/build - of which mesh I/O - then to first frame");
        const QString mainPresName = m_profiler->presentationName();
        ImGui::Text("  %s - %u ms - %u ms - %u ms", qPrintable(mainPresName),
                    (uint) m_profiler->presentation()->loadTimeMsecs(),
                    (uint) m_profiler->presentation()->meshesLoadTimeMsecs(),
                    (uint) m_profiler->timeAfterBuildUntilFirstFrameAction());
        totalTime += m_profiler->timeAfterBuildUntilFirstFrameAction();
        for (Q3DSProfiler *subPresProfiler : *m_profiler->subPresentationProfilers()) {
            const QString subPresName = subPresProfiler->presentationName();
            ImGui::Text("  %s - %u ms - %u ms - %u ms", qPrintable(subPresName),
                        (uint) subPresProfiler->presentation()->loadTimeMsecs(),
                        (uint) subPresProfiler->presentation()->meshesLoadTimeMsecs(),
                        (uint) subPresProfiler->timeAfterBuildUntilFirstFrameAction());
            totalTime += subPresProfiler->timeAfterBuildUntilFirstFrameAction();
        }
        ImGui::Text("Total: %u ms", (uint) totalTime);
        ImGui::Text("  of which image file I/O: %u ms\n  IBL mipmap gen: %u ms",
                    (uint) Q3DSImageManager::instance().ioTimeMsecs(),
                    (uint) Q3DSImageManager::instance().iblTimeMsecs());
        ImGui::Text("  Active behavior QML comp.: %d, total load time %u ms",
                    m_profiler->behaviorActiveCount(), (uint) m_profiler->behaviorLoadTime());
        ImGui::Separator();
        const QVector<Q3DSProfiler::FrameData> *frameData = m_profiler->frameData();
        const Q3DSProfiler::FrameData *lastFrameData = !frameData->isEmpty() ? &frameData->last() : nullptr;
        // life is too short to figure out why mingw does not like %lld so stick with %u
        uint frameNo = lastFrameData ? lastFrameData->globalFrameCounter : 0;
        ImGui::Text("Frame %u", frameNo);
        ImGui::Text("Scene dirty: %s", lastFrameData ? (lastFrameData->wasDirty ? "true" : "false") : "unknown");
    }

    if (ImGui::CollapsingHeader("Scene info")) {
        if (ImGui::Button("Log window"))
            m_logWindowOpen = !m_logWindowOpen;
        if (ImGui::Button("Console window"))
            m_consoleWindowOpen = !m_consoleWindowOpen;
        if (ImGui::Button("Layer list"))
            m_layerWindowOpen = !m_layerWindowOpen;
        if (ImGui::Button("Qt 3D object list"))
            m_qt3dObjectsWindowOpen = !m_qt3dObjectsWindowOpen;
        if (ImGui::Button("Qt 3D frame graph"))
            m_frameGraphWindowOpen = !m_frameGraphWindowOpen;
        if (ImGui::Button("Behavior list"))
            m_behaviorWindowOpen = !m_behaviorWindowOpen;
    }

    if (ImGui::CollapsingHeader("Alter scene"))
        addAlterSceneStuff();

    ImGui::End();

    if (m_qt3dObjectsWindowOpen)
        addQt3DObjectsWindow();

    if (m_layerWindowOpen)
        addLayerWindow();

    if (m_behaviorWindowOpen)
        addBehaviorWindow();

    if (m_logWindowOpen)
        addLogWindow();

    if (m_consoleWindowOpen)
        addConsoleWindow();

    if (m_frameGraphWindowOpen)
        addFrameGraphWindow();
}

void Q3DSProfileView::addQt3DObjectsWindow()
{
    ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Qt 3D objects", &m_qt3dObjectsWindowOpen, ImGuiWindowFlags_NoSavedSettings);

    addPresentationSelector();

    Q3DSProfiler *p = selectedProfiler();
    const QMultiMap<Q3DSProfiler::ObjectType, Q3DSProfiler::ObjectData> *objs = p->objectData();

    auto meshes = objs->values(Q3DSProfiler::MeshObject);
    ImGui::Text("Meshes: %d", meshes.count());
    if (ImGui::TreeNodeEx("Mesh details", meshes.isEmpty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(6, "meshcols");
        ImGui::Separator();
        ImGui::Text("Index"); ImGui::SetColumnWidth(-1, 50); ImGui::NextColumn();
        ImGui::Text("Description"); ImGui::NextColumn();
        ImGui::Text("Draw vertex count"); ImGui::NextColumn();
        ImGui::Text("Vertex bytes"); ImGui::NextColumn();
        ImGui::Text("Index bytes"); ImGui::NextColumn();
        ImGui::Text("Blending");
        addTip("When true, the entity belongs to the transparent pass (back-to-front sorting, blending enabled) "
               "instead of the opaque pass");
        ImGui::NextColumn();
        ImGui::Separator();
        int idx = 0;
        for (const Q3DSProfiler::ObjectData &objd : meshes) {
            ImGui::Text("%d", idx);
            ++idx;
            ImGui::NextColumn();
            const QByteArray info = objd.info.toUtf8();
            ImGui::Text("%s", info.constData());
            ImGui::NextColumn();

            // show only what's relevant; for now q3dsmeshloader always creates 2 buffers (vertex, index) for each submesh
            int drawVertexCount = 0;
            int vertexBufferByteSize = 0;
            int indexBufferByteSize = 0;
            bool blending = false;
            if (auto gr = qobject_cast<Q3DSMesh *>(objd.obj)) {
                drawVertexCount = gr->vertexCount();
                auto g = gr->geometry();
                for (auto attr : g->attributes()) {
                    if (attr->attributeType() == Qt3DRender::QAttribute::IndexAttribute) {
                        if (!drawVertexCount)
                            drawVertexCount = attr->count();
                        auto buf = attr->buffer();
                        indexBufferByteSize = buf->data().size();
                    } else if (attr->name() == QStringLiteral("attr_pos")) {
                        auto buf = attr->buffer();
                        vertexBufferByteSize = buf->data().size();
                    }
                }
                auto sd = p->subMeshData(gr);
                blending = sd.needsBlending;
            }

            ImGui::Text("%d", drawVertexCount);
            ImGui::NextColumn();
            ImGui::Text("%d", vertexBufferByteSize);
            ImGui::NextColumn();
            ImGui::Text("%d", indexBufferByteSize);
            ImGui::NextColumn();
            ImGui::Text("%s", blending ? "true" : "false");
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::TreePop();
    }

    auto tex2d = objs->values(Q3DSProfiler::Texture2DObject);
    ImGui::Text("2D textures: %d", tex2d.count());
    if (ImGui::TreeNodeEx("2D texture details", tex2d.isEmpty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(6, "tex2dcols");
        ImGui::Separator();
        ImGui::Text("Index"); ImGui::SetColumnWidth(-1, 50); ImGui::NextColumn();
        ImGui::Text("Description"); ImGui::NextColumn();
        ImGui::Text("Size (pixels)"); ImGui::NextColumn();
        ImGui::Text("Format"); ImGui::NextColumn();
        ImGui::Text("Samples"); ImGui::NextColumn();
        ImGui::Text("Shares data");
        addTip("When 'yes', the image referred to an already loaded file and thus the pixel data was reused. "
               "This can also mean that there is only one underlying OpenGL texture, but note that that is "
               "only possible when all texture (Image) parameters (like wrap mode) match.");
        ImGui::NextColumn();
        ImGui::Separator();
        int idx = 0;
        for (const Q3DSProfiler::ObjectData &objd : tex2d) {
            ImGui::Text("%d", idx);
            ++idx;
            ImGui::NextColumn();
            const QByteArray info = objd.info.toUtf8();
            ImGui::Text("%s", info.constData());
            ImGui::NextColumn();
            if (auto t = qobject_cast<Qt3DRender::QAbstractTexture *>(objd.obj)) {
                bool useTexture = true;
                const QVector<Qt3DRender::QAbstractTextureImage *> textureImages = t->textureImages();
                if (!textureImages.isEmpty()) {
                    // handle Text textures specially since the data may be dummy on the texture itself
                    if (auto ti = qobject_cast<Qt3DRender::QPaintedTextureImage *>(textureImages[0])) {
                        ImGui::Text("%dx%d", ti->width(), ti->height());
                        ImGui::NextColumn();
                        ImGui::Text("0x8058");
                        ImGui::NextColumn();
                        ImGui::Text("1");
                        ImGui::NextColumn();
                        ImGui::Text("no");
                        ImGui::NextColumn();
                        useTexture = false;
                    }
                }
                if (useTexture) {
                    // 2D textures may come from files, in which case we should
                    // get the info via Q3DSImageManager, not directly.
                    const QSize size = Q3DSImageManager::instance().size(t);
                    auto format = Q3DSImageManager::instance().format(t);
                    bool wasCached = Q3DSImageManager::instance().wasCached(t);
                    ImGui::Text("%dx%d", size.width(), size.height());
                    ImGui::NextColumn();
                    ImGui::Text("0x%x", format);
                    ImGui::NextColumn();
                    ImGui::Text("%d", t->samples());
                    ImGui::NextColumn();
                    ImGui::Text("%s", wasCached ? "yes" : "no");
                    ImGui::NextColumn();
                }
            } else {
                ImGui::NextColumn();
                ImGui::NextColumn();
                ImGui::NextColumn();
                ImGui::NextColumn();
            }
        }
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::TreePop();
    }

    auto texcube = objs->values(Q3DSProfiler::TextureCubeObject);
    ImGui::Text("Cube textures: %d", texcube.count());
    if (ImGui::TreeNodeEx("Cube texture details", texcube.isEmpty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(3, "texcubecols");
        ImGui::Separator();
        ImGui::Text("Index"); ImGui::SetColumnWidth(-1, 50); ImGui::NextColumn();
        ImGui::Text("Description"); ImGui::NextColumn();
        ImGui::Text("Size"); ImGui::NextColumn();
        ImGui::Separator();
        int idx = 0;
        for (const Q3DSProfiler::ObjectData &objd : texcube) {
            ImGui::Text("%d", idx);
            ++idx;
            ImGui::NextColumn();
            const QByteArray info = objd.info.toUtf8();
            ImGui::Text("%s", info.constData());
            ImGui::NextColumn();
            if (auto t = qobject_cast<Qt3DRender::QAbstractTexture *>(objd.obj)) {
                const QVector<Qt3DRender::QAbstractTextureImage *> textureImages = t->textureImages();
                if (textureImages.isEmpty()) {
                    ImGui::Text("%dx%dx%d", t->width(), t->height(), t->depth());
                    ImGui::NextColumn();
                } else {
                    ImGui::NextColumn();
                }
            }
        }
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::TreePop();
    }

    auto rts = objs->values(Q3DSProfiler::RenderTargetObject);
    ImGui::Text("Render targets (FBOs): %d", rts.count());
    if (ImGui::TreeNodeEx("Render target details", rts.isEmpty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(2, "rtcols");
        ImGui::Separator();
        ImGui::Text("Index"); ImGui::SetColumnWidth(-1, 50); ImGui::NextColumn();
        ImGui::Text("Description"); ImGui::NextColumn();
        ImGui::Separator();
        int idx = 0;
        for (const Q3DSProfiler::ObjectData &objd : rts) {
            ImGui::Text("%d", idx);
            ++idx;
            ImGui::NextColumn();
            const QByteArray info = objd.info.toUtf8();
            ImGui::Text("%s", info.constData());
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::TreePop();
    }

    if (m_currentPresentationIndex == 0) {
        auto progs = objs->values(Q3DSProfiler::ShaderProgramObject);
        ImGui::Text("Shader programs (from all presentations): %d", progs.count());
        if (ImGui::TreeNodeEx("Shader program details", progs.isEmpty() ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Columns(3, "progcols");
            ImGui::Separator();
            ImGui::Text("Index"); ImGui::SetColumnWidth(-1, 50); ImGui::NextColumn();
            ImGui::Text("Description"); ImGui::NextColumn();
            ImGui::Text("Status"); ImGui::NextColumn();
            ImGui::Separator();
            int idx = 0;
            for (const Q3DSProfiler::ObjectData &objd : progs) {
                ImGui::Text("%d", idx);
                ++idx;
                ImGui::NextColumn();
                const QByteArray info = objd.info.toUtf8();
                ImGui::Text("%s", info.constData());
                ImGui::NextColumn();
                QString status;
                if (auto p = qobject_cast<Qt3DRender::QShaderProgram *>(objd.obj)) {
                    switch (p->status()) {
                    case Qt3DRender::QShaderProgram::NotReady:
                        status = QLatin1String("Not ready");
                        break;
                    case Qt3DRender::QShaderProgram::Ready:
                        status = QLatin1String("Ready");
                        break;
                    case Qt3DRender::QShaderProgram::Error:
                    {
                        status = QLatin1String("Error: ");
                        QString log = p->log().left(500);
                        log.replace(QLatin1Char('\n'), QLatin1String(" "));
                        status += log;
                    }
                        break;
                    default:
                        break;
                    }
                }
                ImGui::Text("%s", qPrintable(status));
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::Separator();
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void Q3DSProfileView::addLayerWindow()
{
    ImGui::SetNextWindowSize(ImVec2(640, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Layers", &m_layerWindowOpen, ImGuiWindowFlags_NoSavedSettings);

    addPresentationSelector();

    ImGui::Text("Layers");
    ImGui::Columns(7, "layercols");
    ImGui::Separator();
    ImGui::Text("ID"); ImGui::NextColumn();
    ImGui::Text("Visible"); ImGui::NextColumn();
    ImGui::Text("Size"); ImGui::NextColumn();
    ImGui::Text("Blend mode");
    addTip("Advanced blend modes (*) are more expensive due to "
           "using additional render passes and targets.");
    ImGui::NextColumn();
    ImGui::Text("AA");
    addTip("Multisample / Supersample / Progressive / Temporal antialiasing");
    ImGui::NextColumn();
    ImGui::Text("Dirty"); ImGui::NextColumn();
    ImGui::Text("Cached");
    addTip("When in cached mode, a layer does not render its content into the "
           "associated offscreen render target. Rather, the previous content is "
           "used for composition. This can only happen while Dirty is false.");
    ImGui::NextColumn();
    ImGui::Separator();

    Q3DSProfiler *p = selectedProfiler();
    int activeCount = 0;
    Q3DSUipPresentation::forAllLayers(p->presentation()->scene(), [&](Q3DSLayerNode *layer3DS) {
        Q3DSLayerAttached *data = static_cast<Q3DSLayerAttached *>(layer3DS->attached());
        ImGui::Text("%s", layer3DS->id().constData());
        ImGui::NextColumn();
        const bool isActive = layer3DS->flags().testFlag(Q3DSNode::Active);
        if (isActive)
            ++activeCount;
        ImGui::Text("%s", isActive ? "true" : "false");
        ImGui::NextColumn();
        if (data)
            ImGui::Text("%dx%d", data->layerSize.width(), data->layerSize.height());
        else
            ImGui::Text("unknown");
        ImGui::NextColumn();
        const char *blendType = Q3DSEnumMap::strFromEnum(layer3DS->blendType());
        ImGui::Text("%s", blendType);
        ImGui::NextColumn();
        const QByteArray aa =
                QByteArrayLiteral("M/SSAA: ") + Q3DSEnumMap::strFromEnum(layer3DS->multisampleAA())
                + QByteArrayLiteral("\nPAA: ") + Q3DSEnumMap::strFromEnum(layer3DS->progressiveAA())
                + QByteArrayLiteral("\nTAA: ") + (layer3DS->layerFlags().testFlag(Q3DSLayerNode::TemporalAA) ? "Yes": "No");
        ImGui::Text("%s", aa.constData());
        ImGui::NextColumn();
        ImGui::Text("%s", data->wasDirty ? "true" : "false");
        ImGui::NextColumn();
        if (data->layerFgRoot) {
            const bool isCached = data->layerFgRoot->parentNode() == data->layerFgDummyParent;
            ImGui::Text("%s", isCached ? "true" : "false");
        } else {
            ImGui::Text("N/A");
        }
        ImGui::NextColumn();
    });
    ImGui::Columns(1);
    ImGui::Separator();

    ImGui::Text("Total active layers: %d", activeCount);
    addTip("Layers are rendered into textures and then composed together by drawing textured quads. "
           "Therefore a large number of layers lead to lower performance. The size matters too since "
           "a larger layer means more fragment processing work for the GPU. Antialiasing methods can also "
           "heavily affect the performance.");

    ImGui::End();
}

void Q3DSProfileView::addBehaviorWindow()
{
    ImGui::SetNextWindowSize(ImVec2(640, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Behaviors", &m_behaviorWindowOpen, ImGuiWindowFlags_NoSavedSettings);

    addPresentationSelector();

    ImGui::Text("behaviors");
    ImGui::Columns(3, "behaviorcols");
    ImGui::Separator();
    ImGui::Text("ID"); ImGui::NextColumn();
    ImGui::Text("Active");
    addTip("Corresponds to the eyeball in the editor. "
           "Inactive behavior instances have no QML component and QObject instantiated. "
           "Note that only behavior instances with no error (next column) are really alive.");
    ImGui::NextColumn();
    ImGui::Text("QML error string"); ImGui::NextColumn();
    ImGui::Separator();

    Q3DSProfiler *p = selectedProfiler();
    Q3DSUipPresentation::forAllObjectsOfType(p->presentation()->scene(), Q3DSGraphObject::Behavior, [&](Q3DSGraphObject *obj) {
        Q3DSBehaviorInstance *behaviorInstance = static_cast<Q3DSBehaviorInstance *>(obj);
        ImGui::Text("%s", behaviorInstance->id().constData());
        ImGui::NextColumn();
        ImGui::Text("%s", behaviorInstance->active() ? "true" : "false");
        ImGui::NextColumn();
        QString err = behaviorInstance->qmlErrorString();
        for (int i = 0; i < err.count(); ++i) {
            if (i && !(i % 60))
                err.insert(i, QLatin1Char('\n'));
        }
        ImGui::Text("%s", qPrintable(err));
        ImGui::NextColumn();
    });
    ImGui::Columns(1);
    ImGui::Separator();

    ImGui::End();
}

void Q3DSProfileView::addLogWindow()
{
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(200, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Log", &m_logWindowOpen, ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::Button("Clear"))
        m_profiler->clearLog();
    ImGui::SameLine();
#ifndef QT_NO_CLIPBOARD
    if (ImGui::Button("Copy"))
        QGuiApplication::clipboard()->setText(m_profiler->log().join('\n'));
    ImGui::SameLine();
#endif
    if (ImGui::Button("Filter"))
        m_logFilterWindowOpen = !m_logFilterWindowOpen;
    ImGui::SameLine();
    ImGui::Checkbox("Scroll on change", &m_logScrollToBottomOnChange);
    ImGui::Separator();

    ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const QString &msg : m_profiler->log()) {
        if (isFiltered(msg))
            continue;
        const QByteArray msgBa = msg.toUtf8() + '\n';
        static const ImVec4 perfHighlightColor(1, 0, 0, 1);
        if (msgBa.startsWith(QByteArrayLiteral("q3ds.perf")))
            ImGui::TextColored(perfHighlightColor, "%s", msgBa.constData());
        else
            ImGui::TextUnformatted(msgBa.constData());
    }
    if (m_logScrollToBottomOnChange && m_profiler->hasLogChanged())
        ImGui::SetScrollHere(1.0f);
    ImGui::EndChild();
    ImGui::Separator();

    ImGui::End();

    if (m_logFilterWindowOpen) {
        ImGui::Begin("Filter log", &m_logFilterWindowOpen,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("Select which log entries to show");
        ImGui::Separator();
        for (int i = 0; i < MAX_LOG_FILTER_ENTRIES; ++i) {
            if (!m_logFilterPrefixes[i])
                break;
            ImGui::Selectable(m_logFilterPrefixes[i], &m_logFilterEnabled[i]);
        }
        ImGui::Separator();
        ImGui::End();
    }
}

void Q3DSProfileView::addConsoleWindow()
{
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPosCenter(ImGuiCond_FirstUseEver);
    ImGui::Begin("Console", &m_consoleWindowOpen, ImGuiWindowFlags_NoSavedSettings);

    if (!m_console) {
        m_console = new Q3DSConsole;
        m_consoleInitFunc(m_console);
    }
    m_console->draw();

    ImGui::End();
}

void Q3DSProfileView::addFrameGraphNode(Qt3DRender::QFrameGraphNode *fg, const QSet<Qt3DRender::QFrameGraphNode *> &stopNodes)
{
    if (stopNodes.contains(fg))
        return;

    QString extraInfo;
    Qt3DRender::QLayerFilter *layerFilter = qobject_cast<Qt3DRender::QLayerFilter *>(fg);
    if (layerFilter) {
        switch (layerFilter->filterMode()) {
        case Qt3DRender::QLayerFilter::AcceptAnyMatchingLayers:
            extraInfo += QLatin1String("\naccepts any of");
            break;
        case Qt3DRender::QLayerFilter::AcceptAllMatchingLayers:
            extraInfo += QLatin1String("\naccepts when matches all of");
            break;
        case Qt3DRender::QLayerFilter::DiscardAnyMatchingLayers:
            extraInfo += QLatin1String("\ndiscards any of");
            break;
        case Qt3DRender::QLayerFilter::DiscardAllMatchingLayers:
            extraInfo += QLatin1String("\ndiscards when matches all of");
            break;
        default:
            Q_UNREACHABLE();
            break;
        }
        for (Qt3DRender::QLayer *layer : layerFilter->layers()) {
            extraInfo += QLatin1String(" \"");
            extraInfo += layer->objectName();
            extraInfo += QLatin1Char('\"');
        }
    }

    Qt3DRender::QRenderPassFilter *renderPassFilter = qobject_cast<Qt3DRender::QRenderPassFilter *>(fg);
    if (renderPassFilter) {
        QVector<Qt3DRender::QFilterKey *> keys = renderPassFilter->matchAny();
        extraInfo += QLatin1String("\naccepts any of");
        for (Qt3DRender::QFilterKey *key : keys) {
            extraInfo += QLatin1String(" \"");
            extraInfo += key->name();
            extraInfo += QLatin1Char('=');
            extraInfo += key->value().toString();
            extraInfo += QLatin1Char('\"');
        }
    }

    QByteArray label = QString(QLatin1String("%1 (0x%2)%3"))
            .arg(QString::fromUtf8(fg->metaObject()->className()))
            .arg((quintptr) fg, 0, 16)
            .arg(extraInfo)
            .toUtf8();
    int f = 0;
    QVarLengthArray<Qt3DRender::QFrameGraphNode *, 32> fgChildren;

    for (QObject *obj : fg->children()) {
        auto child = qobject_cast<Qt3DRender::QFrameGraphNode *>(obj);
        if (child)
            fgChildren.append(child);
    }

    if (fgChildren.isEmpty())
        f |= ImGuiTreeNodeFlags_Leaf;

    if (ImGui::TreeNodeEx(label.constData(), f)) {
        for (auto child : fgChildren)
            addFrameGraphNode(child, stopNodes);
        ImGui::TreePop();
    }
}

void Q3DSProfileView::addFrameGraphWindow()
{
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Frame graph", &m_frameGraphWindowOpen, ImGuiWindowFlags_NoSavedSettings);

    addPresentationSelector();
    ImGui::Text("Frame graph for the above presentation,\n"
                "excluding nodes for this UI\n"
                "but including the layer composition\n"
                "(cached layers are excluded since these do not re-render)");
    ImGui::Separator();
    addFrameGraphNode(selectedProfiler()->frameGraphRoot(), selectedProfiler()->frameGraphStopNodes());

    ImGui::End();
}

static void changeProperty(Q3DSGraphObject *obj, const QString &name, const QString &value)
{
    Q3DSPropertyChangeList changeList;
    changeList.append(Q3DSPropertyChange(name, value));
    obj->applyPropertyChanges(changeList);
    obj->notifyPropertyChanges(changeList);
}

void Q3DSProfileView::addAlterSceneStuff()
{
    Q3DSUipPresentation *pres = m_profiler->presentation();
    if (!pres)
        return;

    ImGui::Checkbox("Layer caching", &m_layerCaching);
    m_profiler->setLayerCaching(m_layerCaching);

    if (ImGui::Button("Data input"))
        m_dataInputWindowOpen = !m_dataInputWindowOpen;

    if (m_disabledShadowCasters.isEmpty()) {
        if (ImGui::Button("Disable shadows for..."))
            ImGui::OpenPopup("shdwdis");

        if (ImGui::BeginPopup("shdwdis")) {
            if (ImGui::Selectable("all shadow casting lights")) {
                Q3DSUipPresentation::forAllObjectsOfType(pres->scene(), Q3DSGraphObject::Light, [this](Q3DSGraphObject *obj) {
                    Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(obj);
                    if (light3DS->castShadow()) {
                        m_disabledShadowCasters.append(light3DS);
                        changeProperty(light3DS, QLatin1String("castshadow"), QLatin1String("false"));
                    }
                });
            }
            // omnidirectional shadow mapping uses a cube map with multiple
            // passes, while directional lights are lighter (heh) -> allow
            // toggling these separately.
            if (ImGui::Selectable("point and area only")) {
                Q3DSUipPresentation::forAllObjectsOfType(pres->scene(), Q3DSGraphObject::Light, [this](Q3DSGraphObject *obj) {
                    Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(obj);
                    if (light3DS->castShadow() && light3DS->lightType() != Q3DSLightNode::Directional) {
                        m_disabledShadowCasters.append(light3DS);
                        changeProperty(light3DS, QLatin1String("castshadow"), QLatin1String("false"));
                    }
                });
            }
            if (ImGui::Selectable("directional only")) {
                Q3DSUipPresentation::forAllObjectsOfType(pres->scene(), Q3DSGraphObject::Light, [this](Q3DSGraphObject *obj) {
                    Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(obj);
                    if (light3DS->castShadow() && light3DS->lightType() == Q3DSLightNode::Directional) {
                        m_disabledShadowCasters.append(light3DS);
                        changeProperty(light3DS, QLatin1String("castshadow"), QLatin1String("false"));
                    }
                });
            }
            ImGui::EndPopup();
        }
    } else {
        if (ImGui::Button("Re-enable shadows")) {
            for (Q3DSLightNode *light3DS : m_disabledShadowCasters)
                changeProperty(light3DS, QLatin1String("castshadow"), QLatin1String("true"));
            m_disabledShadowCasters.clear();
        }
    }

    if (m_dataInputWindowOpen) {
        ImGui::Begin("Data input", &m_dataInputWindowOpen,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

        addPresentationSelector();

        Q3DSProfiler *p = selectedProfiler();
        const Q3DSDataInputEntry::Map *diMetaMap = p->presentation()->dataInputEntries();
        const Q3DSUipPresentation::DataInputMap *diL1 = p->presentation()->dataInputMap();
        int idx = 0;
        for (const QString &diName : diL1->uniqueKeys()) {
            if (!diMetaMap || !diMetaMap->contains(diName))
                continue;

            ImGui::PushID(idx++); // because of the Apply buttons with the same label/id
            const Q3DSDataInputEntry &diMeta((*diMetaMap)[diName]);
            switch (diMeta.type) {
            case Q3DSDataInputEntry::TypeString:
            {
                QByteArray *buf;
                if (m_dataInputTextBuf.contains(diName)) {
                    buf = &m_dataInputTextBuf[diName];
                } else {
                    buf = &m_dataInputTextBuf[diName];
                    buf->resize(255);
                    buf->data()[0] = '\0';
                }
                ImGui::InputText(qPrintable(diName), buf->data(), 255);
                ImGui::SameLine();
                if (ImGui::Button("Apply"))
                    p->sendDataInputValueChange(diName, QString::fromUtf8(*buf));
            }
                break;
            case Q3DSDataInputEntry::TypeRangedNumber:
            {
                float &buf(m_dataInputFloatBuf[diName]);
                ImGui::InputFloat(qPrintable(diName), &buf);
                ImGui::SameLine();
                if (ImGui::Button("Apply"))
                    p->sendDataInputValueChange(diName, buf);
            }
                break;
            case Q3DSDataInputEntry::TypeVec2:
            {
                QVector2D &buf(m_dataInputVec2Buf[diName]); // QVector2D is two floats in practice
                ImGui::InputFloat2(qPrintable(diName), reinterpret_cast<float *>(&buf));
                ImGui::SameLine();
                if (ImGui::Button("Apply"))
                    p->sendDataInputValueChange(diName, buf);
            }
                break;
            case Q3DSDataInputEntry::TypeVec3:
            {
                QVector3D &buf(m_dataInputVec3Buf[diName]); // QVector3D is three floats in practice
                ImGui::InputFloat3(qPrintable(diName), reinterpret_cast<float *>(&buf));
                ImGui::SameLine();
                if (ImGui::Button("Apply"))
                    p->sendDataInputValueChange(diName, buf);
            }
                break;
            default:
                break;
            }

            for (Q3DSGraphObject *diTarget : diL1->values(diName)) {
                const Q3DSGraphObject::DataInputControlledProperties *diL2 = diTarget->dataInputControlledProperties();
                for (const QString &propName : diL2->values(diName))
                    ImGui::Text("  target: %s.%s", diTarget->id().constData(), qPrintable(propName));
            }

            ImGui::PopID();
        }

        ImGui::End();
    }
}

Q3DSProfileUi::Q3DSProfileUi(Q3DSGuiData *guiData, Q3DSProfiler *profiler, ConsoleInitFunc consoleInitFunc)
    : m_data(guiData)
{
    m_guiMgr = new Q3DSImguiManager;
    m_view = new Q3DSProfileView(profiler, consoleInitFunc);
    m_guiMgr->setFrameFunc(std::bind(&Q3DSProfileView::frame, m_view));
    m_guiMgr->setOutputInfoFunc([this]() {
        Q3DSImguiManager::OutputInfo inf;
        inf.size = m_data->outputSize;
        inf.dpr = m_data->outputDpr;
        inf.guiTag = m_data->tag;
        inf.activeGuiTag = m_data->activeTag;
        inf.guiTechniqueFilterKey = m_data->techniqueFilterKey;
        return inf;
    });
}

Q3DSProfileUi::~Q3DSProfileUi()
{
    delete m_guiMgr;
    delete m_view;
}

void Q3DSProfileUi::setInputEventSource(QObject *obj)
{
    m_guiMgr->setInputEventSource(obj);
}

void Q3DSProfileUi::configure(float scale)
{
    m_guiMgr->setScale(scale);
}

void Q3DSProfileUi::releaseResources()
{
    m_guiMgr->releaseResources();
    m_inited = false;
    m_visible = false;
}

void Q3DSProfileUi::setVisible(bool visible)
{
    if (visible == m_visible)
        return;

    m_visible = visible;

    if (!m_inited) {
        if (!m_visible)
            return;
        m_inited = true;
        qCDebug(lcProf, "Initializing");
        m_guiMgr->initialize(m_data->rootEntity);
    }

    qCDebug(lcProf, "Visible = %d", m_visible);
    m_guiMgr->setEnabled(m_visible);
}

void Q3DSProfileUi::openLogAndConsole()
{
    m_view->openLogAndConsole();
}

QT_END_NAMESPACE
