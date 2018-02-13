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
#include <QLoggingCategory>
#include <Qt3DRender/QTexture>
#include <Qt3DRender/QPaintedTextureImage>
#include <Qt3DRender/QGeometry>
#include <Qt3DRender/QAttribute>
#include <Qt3DRender/QBuffer>
#include <Qt3DRender/QShaderProgram>

#include <imgui.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcProf, "q3ds.profileui")

const int MAX_FRAME_DELTA_COUNT = 1000; // plot the last 1000 frame deltas at most

class Q3DSProfileView
{
public:
    Q3DSProfileView(Q3DSProfiler *profiler)
        : m_profiler(profiler)
    { }

    void frame();

    void openLog() { m_logWindowOpen = true; }

private:
    void addQt3DObjectsWindow();
    void addLayerWindow();
    void addLogWindow();
    void addAlterSceneStuff();
    void addPresentationSelector();
    Q3DSProfiler *selectedProfiler() const;

    Q3DSProfiler *m_profiler;
    int m_frameDeltaCount = 100; // last 100 frames
    float m_frameDeltaPlotMin = 0; // bottom 0 ms
    float m_frameDeltaPlotMax = 100; // top 100 ms

    QVector<Q3DSLightNode *> m_disabledShadowCasters;

    bool m_qt3dObjectsWindowOpen = false;
    bool m_layerWindowOpen = false;
    bool m_logWindowOpen = false;
    bool m_logScrollToBottomOnChange = true;
    int m_currentPresentationIndex = 0;
};

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
        const Q3DSGraphicsLimits *limits = m_profiler->graphicsLimits();
        ImGui::Text("RENDERER: %s", limits->renderer.constData());
        ImGui::Text("VENDOR: %s", limits->vendor.constData());
        ImGui::Text("VERSION: %s", limits->version.constData());
        ImGui::Text("Multisample textures supported: %s", (limits->multisampleTextureSupported ? "yes" : "no"));
        ImGui::Text("MAX_DRAW_BUFFERS: %d", limits->maxDrawBuffers);
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
        ImGui::Separator();
        const QVector<Q3DSProfiler::FrameData> *frameData = m_profiler->frameData();
        const Q3DSProfiler::FrameData *lastFrameData = !frameData->isEmpty() ? &frameData->last() : nullptr;
        // life is too short to figure out why mingw does not like %lld so stick with %u
        uint frameNo = lastFrameData ? lastFrameData->globalFrameCounter : 0;
        ImGui::Text("Frame %u", frameNo);
        ImGui::Text("Scene dirty: %s", lastFrameData ? (lastFrameData->wasDirty ? "true" : "false") : "unknown");
    }

    if (ImGui::CollapsingHeader("Scene info")) {
        if (ImGui::Button("Qt 3D object list"))
            m_qt3dObjectsWindowOpen = !m_qt3dObjectsWindowOpen;
        if (ImGui::Button("Layer list"))
            m_layerWindowOpen = !m_layerWindowOpen;
        if (ImGui::Button("Log window"))
            m_logWindowOpen = !m_logWindowOpen;
    }

    if (ImGui::CollapsingHeader("Alter scene"))
        addAlterSceneStuff();

    ImGui::End();

    if (m_qt3dObjectsWindowOpen)
        addQt3DObjectsWindow();

    if (m_layerWindowOpen)
        addLayerWindow();

    if (m_logWindowOpen)
        addLogWindow();
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
        ImGui::Columns(5, "tex2dcols");
        ImGui::Separator();
        ImGui::Text("Index"); ImGui::SetColumnWidth(-1, 50); ImGui::NextColumn();
        ImGui::Text("Description"); ImGui::NextColumn();
        ImGui::Text("Size (pixels)"); ImGui::NextColumn();
        ImGui::Text("Format"); ImGui::NextColumn();
        ImGui::Text("Samples"); ImGui::NextColumn();
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
                        useTexture = false;
                    }
                }
                if (useTexture) {
                    // ### QTBUG-65775
                    if (t->width() == 1 && t->height() == 1)
                        ImGui::Text("unknown");
                    else
                        ImGui::Text("%dx%d", t->width(), t->height());
                    ImGui::NextColumn();
                    if (t->format() == 0 || t->format() == 1)
                        ImGui::Text("unknown");
                    else
                        ImGui::Text("0x%x", t->format());
                    ImGui::NextColumn();
                    ImGui::Text("%d", t->samples());
                    ImGui::NextColumn();
                }
            } else {
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
    ImGui::Columns(6, "layercols");
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

void Q3DSProfileView::addLogWindow()
{
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Log", &m_logWindowOpen, ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::Button("Clear"))
        m_profiler->clearLog();
    ImGui::SameLine();
    ImGui::Checkbox("Scroll on change", &m_logScrollToBottomOnChange);
    ImGui::Separator();

    ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const QString &msg : m_profiler->log()) {
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
}

static void changeProperty(Q3DSGraphObject *obj, const QString &name, const QString &value)
{
    Q3DSPropertyChangeList changeList;
    changeList.append(Q3DSPropertyChange(name, value));
    obj->applyPropertyChanges(&changeList);
    obj->notifyPropertyChanges(&changeList);
}

void Q3DSProfileView::addAlterSceneStuff()
{
    Q3DSUipPresentation *pres = m_profiler->presentation();
    if (!pres)
        return;

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
}

Q3DSProfileUi::Q3DSProfileUi(Q3DSGuiData *guiData, Q3DSProfiler *profiler)
    : m_data(guiData)
{
    m_guiMgr = new Q3DSImguiManager;
    m_view = new Q3DSProfileView(profiler);
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

void Q3DSProfileUi::openLog()
{
    m_view->openLog();
}

QT_END_NAMESPACE
