#pragma once

#include <memory>
#include <common/application.h>
#include <scene/camera/camera.h>
#include <scene/light/light.h>
#include <graphics/gl_program.h>
#include <graphics/sampler.h>
#include <graphics/vertex_array.h>
#include "cpu_renderer.h"
#include "gpu_renderer.h"
#include "raster_scene.h"

class DebugScene;

class SoftwareRendererApp : public Application {
public:
    explicit SoftwareRendererApp(Config const& config);

    ~SoftwareRendererApp() override;

private:
    enum class RenderBackend { Cpu = 0, Gpu = 1 };

    // Which draw-item list is rendered: the glTF model, or one of the procedural
    // pages that exercise individual pipeline stages (README 8.2).
    enum class SceneMode { Helmet = 0, DebugStage = 1, DebugClip = 2 };

private:
    // scene
    std::unique_ptr<RasterScene> m_scene;
    std::unique_ptr<DebugScene> m_debugScene;
    SceneMode m_sceneMode = SceneMode::Helmet;
    // Light intensity used for the model, restored when leaving the debug pages.
    float m_modelLightIntensity = 10.0f;
    PerspectiveCamera m_camera{glm::radians(45.0f), m_window.aspectRatio(), 0.1f, 100.0f};
    DirectionalLight m_directionalLight;
    // Model spin is integrated here so it can be frozen from the UI: the CPU and GPU
    // screenshots in the report must share one pose (README 3 and 6.3).
    float m_modelAngle = 0.0f;
    bool m_pauseRotation = false;
    // Staged-debugging view (README 8.1); 0 keeps the full PBR shading.
    int m_debugView = 0;

    // renderer
    std::unique_ptr<CpuRenderer> m_cpuRenderer;
    std::unique_ptr<GpuRenderer> m_gpuRenderer;
    RenderBackend m_renderBackend = RenderBackend::Cpu;

    // display
    std::unique_ptr<gfx::GLProgram> m_displayProgram;
    gfx::Sampler m_displaySampler{gfx::Sampler::Description{
        .minFilter = gfx::Sampler::Filter::Nearest,
        .magFilter = gfx::Sampler::Filter::Nearest,
        .addressModeU = gfx::Sampler::AddressMode::ClampToEdge,
        .addressModeV = gfx::Sampler::AddressMode::ClampToEdge,
    }};
    gfx::VertexArray m_fullscreenVao;

private:
    void handleEvent(Event& event) override;

    void renderFrame() override;

    static void validateDrawItem(DrawItem const& drawItem);
    static FrameState frameForDrawItem(FrameState frame, DrawItem const& drawItem);

    void renderUi();
};
