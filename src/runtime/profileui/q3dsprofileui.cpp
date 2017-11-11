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
#include "q3dspresentation.h"
#include <QLoggingCategory>

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

private:
    void addAlterSceneStuff();

    Q3DSProfiler *m_profiler;
    int m_frameDeltaCount = 100; // last 100 frames
    float m_frameDeltaPlotMin = 0; // bottom 0 ms
    float m_frameDeltaPlotMax = 100; // top 100 ms

    QVector<Q3DSLightNode *> m_disabledShadowCasters;
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

void Q3DSProfileView::frame()
{
    ImGuiWindowFlags wflags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("Profile", nullptr, wflags);

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

    if (ImGui::CollapsingHeader("Alter scene"))
        addAlterSceneStuff();

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
    Q3DSPresentation *pres = m_profiler->presentation();
    if (!pres)
        return;

    if (m_disabledShadowCasters.isEmpty()) {
        if (ImGui::Button("Disable shadows for..."))
            ImGui::OpenPopup("shdwdis");

        if (ImGui::BeginPopup("shdwdis")) {
            if (ImGui::Selectable("all shadow casting lights")) {
                Q3DSPresentation::forAllObjectsOfType(pres->scene(), Q3DSGraphObject::Light, [this](Q3DSGraphObject *obj) {
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
                Q3DSPresentation::forAllObjectsOfType(pres->scene(), Q3DSGraphObject::Light, [this](Q3DSGraphObject *obj) {
                    Q3DSLightNode *light3DS = static_cast<Q3DSLightNode *>(obj);
                    if (light3DS->castShadow() && light3DS->lightType() != Q3DSLightNode::Directional) {
                        m_disabledShadowCasters.append(light3DS);
                        changeProperty(light3DS, QLatin1String("castshadow"), QLatin1String("false"));
                    }
                });
            }
            if (ImGui::Selectable("directional only")) {
                Q3DSPresentation::forAllObjectsOfType(pres->scene(), Q3DSGraphObject::Light, [this](Q3DSGraphObject *obj) {
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

QT_END_NAMESPACE
