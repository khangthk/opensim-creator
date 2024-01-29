#include "LOGLDeferredShadingTab.hpp"

#include <imgui.h>
#include <oscar/Graphics/AntiAliasingLevel.hpp>
#include <oscar/Graphics/Camera.hpp>
#include <oscar/Graphics/Color.hpp>
#include <oscar/Graphics/ColorSpace.hpp>
#include <oscar/Graphics/Graphics.hpp>
#include <oscar/Graphics/GraphicsHelpers.hpp>
#include <oscar/Graphics/ImageLoadingFlags.hpp>
#include <oscar/Graphics/Material.hpp>
#include <oscar/Graphics/Mesh.hpp>
#include <oscar/Graphics/MeshGenerators.hpp>
#include <oscar/Graphics/RenderBufferLoadAction.hpp>
#include <oscar/Graphics/RenderBufferStoreAction.hpp>
#include <oscar/Graphics/RenderTarget.hpp>
#include <oscar/Graphics/RenderTargetColorAttachment.hpp>
#include <oscar/Graphics/RenderTargetDepthAttachment.hpp>
#include <oscar/Graphics/RenderTexture.hpp>
#include <oscar/Graphics/RenderTextureFormat.hpp>
#include <oscar/Graphics/Shader.hpp>
#include <oscar/Graphics/Texture2D.hpp>
#include <oscar/Maths/Angle.hpp>
#include <oscar/Maths/Eulers.hpp>
#include <oscar/Maths/MathHelpers.hpp>
#include <oscar/Maths/Rect.hpp>
#include <oscar/Maths/Transform.hpp>
#include <oscar/Maths/Vec2.hpp>
#include <oscar/Maths/Vec3.hpp>
#include <oscar/Platform/App.hpp>
#include <oscar/UI/Tabs/StandardTabImpl.hpp>
#include <oscar/UI/ImGuiHelpers.hpp>
#include <oscar/Utils/CStringView.hpp>
#include <SDL_events.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace osc::literals;
using osc::AntiAliasingLevel;
using osc::App;
using osc::Camera;
using osc::Color;
using osc::CStringView;
using osc::Material;
using osc::RenderBufferLoadAction;
using osc::RenderBufferStoreAction;
using osc::RenderTarget;
using osc::RenderTargetDepthAttachment;
using osc::RenderTargetColorAttachment;
using osc::RenderTexture;
using osc::RenderTextureDescriptor;
using osc::RenderTextureFormat;
using osc::Shader;
using osc::ToLinear;
using osc::Vec2;
using osc::Vec3;

namespace
{
    constexpr CStringView c_TabStringID = "LearnOpenGL/DeferredShading";

    constexpr auto c_ObjectPositions = std::to_array<Vec3>({
        {-3.0,  -0.5, -3.0},
        { 0.0,  -0.5, -3.0},
        { 3.0,  -0.5, -3.0},
        {-3.0,  -0.5,  0.0},
        { 0.0,  -0.5,  0.0},
        { 3.0,  -0.5,  0.0},
        {-3.0,  -0.5,  3.0},
        { 0.0,  -0.5,  3.0},
        { 3.0,  -0.5,  3.0},
    });
    constexpr size_t c_NumLights = 32;

    Vec3 GenerateSceneLightPosition(std::default_random_engine& rng)
    {
        std::uniform_real_distribution<float> dist{-3.0f, 3.0f};
        return {dist(rng), dist(rng), dist(rng)};
    }

    Color GenerateSceneLightColor(std::default_random_engine& rng)
    {
        std::uniform_real_distribution<float> dist{0.5f, 1.0f};
        return {dist(rng), dist(rng), dist(rng), 1.0f};
    }

    std::vector<Vec3> GenerateNSceneLightPositions(size_t n)
    {
        auto const generator = [rng = std::default_random_engine{std::random_device{}()}]() mutable
        {
            return GenerateSceneLightPosition(rng);
        };

        std::vector<Vec3> rv;
        rv.reserve(n);
        std::generate_n(std::back_inserter(rv), n, generator);
        return rv;
    }

    std::vector<Vec3> GenerateNSceneLightColors(size_t n)
    {
        auto const generator = [rng = std::default_random_engine{std::random_device{}()}]() mutable
        {
            Color const sRGBColor = GenerateSceneLightColor(rng);
            Color const linearColor = ToLinear(sRGBColor);
            return Vec3{linearColor.r, linearColor.g, linearColor.b};
        };

        std::vector<Vec3> rv;
        rv.reserve(n);
        std::generate_n(std::back_inserter(rv), n, generator);
        return rv;
    }

    Material LoadGBufferMaterial()
    {
        return Material{Shader{
            App::slurp("oscar_learnopengl/shaders/AdvancedLighting/deferred_shading/GBuffer.vert"),
            App::slurp("oscar_learnopengl/shaders/AdvancedLighting/deferred_shading/GBuffer.frag"),
        }};
    }

    RenderTexture RenderTextureWithColorFormat(RenderTextureFormat f)
    {
        RenderTexture rv;
        rv.setColorFormat(f);
        return rv;
    }

    Camera CreateCameraThatMatchesLearnOpenGL()
    {
        Camera rv;
        rv.setPosition({0.0f, 0.0f, 5.0f});
        rv.setCameraFOV(45_deg);
        rv.setNearClippingPlane(0.1f);
        rv.setFarClippingPlane(100.0f);
        rv.setBackgroundColor(Color::black());
        return rv;
    }

    struct GBufferRenderingState final {
        Material material = LoadGBufferMaterial();
        RenderTexture albedo = RenderTextureWithColorFormat(RenderTextureFormat::ARGB32);
        RenderTexture normal = RenderTextureWithColorFormat(RenderTextureFormat::ARGBFloat16);
        RenderTexture position = RenderTextureWithColorFormat(RenderTextureFormat::ARGBFloat16);
        RenderTarget renderTarget{
            {
                RenderTargetColorAttachment{
                    albedo.updColorBuffer(),
                    RenderBufferLoadAction::Clear,
                    RenderBufferStoreAction::Resolve,
                    Color::black(),
                },
                RenderTargetColorAttachment{
                    normal.updColorBuffer(),
                    RenderBufferLoadAction::Clear,
                    RenderBufferStoreAction::Resolve,
                    Color::black(),
                },
                RenderTargetColorAttachment{
                    position.updColorBuffer(),
                    RenderBufferLoadAction::Clear,
                    RenderBufferStoreAction::Resolve,
                    Color::black(),
                },
            },

            RenderTargetDepthAttachment{
                albedo.updDepthBuffer(),
                RenderBufferLoadAction::Clear,
                RenderBufferStoreAction::DontCare,
            },
        };

        void reformat(Vec2 dims, AntiAliasingLevel antiAliasingLevel)
        {
            RenderTextureDescriptor desc{dims};
            desc.setAntialiasingLevel(antiAliasingLevel);

            for (RenderTexture* tex : {&albedo, &normal, &position}) {
                desc.setColorFormat(tex->getColorFormat());
                tex->reformat(desc);
            }
        }
    };

    struct LightPassState final {
        Material material{Shader{
            App::slurp("oscar_learnopengl/shaders/AdvancedLighting/deferred_shading/LightingPass.vert"),
            App::slurp("oscar_learnopengl/shaders/AdvancedLighting/deferred_shading/LightingPass.frag"),
        }};
    };
}

class osc::LOGLDeferredShadingTab::Impl final : public StandardTabImpl {
public:
    Impl() : StandardTabImpl{c_TabStringID}
    {}

private:
    void implOnMount() final
    {
        App::upd().makeMainEventLoopPolling();
        m_IsMouseCaptured = true;
    }

    void implOnUnmount() final
    {
        App::upd().setShowCursor(true);
        App::upd().makeMainEventLoopWaiting();

        // un-capture the mouse when un-mounting this tab
        m_IsMouseCaptured = false;
    }

    bool implOnEvent(SDL_Event const& e) final
    {
        // handle mouse input
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            m_IsMouseCaptured = false;
            return true;
        }
        else if (e.type == SDL_MOUSEBUTTONDOWN && IsMouseInMainViewportWorkspaceScreenRect()) {
            m_IsMouseCaptured = true;
            return true;
        }
        return false;
    }

    void implOnDraw() final
    {
        // handle mouse capturing
        if (m_IsMouseCaptured) {
            UpdateEulerCameraFromImGuiUserInput(m_Camera, m_CameraEulers);
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
            App::upd().setShowCursor(false);
        }
        else {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            App::upd().setShowCursor(true);
        }

        draw3DScene();
    }

    void draw3DScene()
    {
        Rect const viewportRect = GetMainViewportWorkspaceScreenRect();
        Vec2 const viewportDims = Dimensions(viewportRect);
        AntiAliasingLevel const antiAliasingLevel = App::get().getCurrentAntiAliasingLevel();

        // ensure textures/buffers have correct dimensions
        {
            m_GBuffer.reformat(viewportDims, antiAliasingLevel);
            m_OutputTexture.setDimensions(viewportDims);
            m_OutputTexture.setAntialiasingLevel(antiAliasingLevel);
        }

        renderSceneToGBuffers();
        renderLightingPass();
        renderLightCubes();
        Graphics::BlitToScreen(m_OutputTexture, viewportRect);
        drawGBufferOverlays(viewportRect);
    }

    void renderSceneToGBuffers()
    {
        m_GBuffer.material.setTexture("uDiffuseMap", m_DiffuseMap);
        m_GBuffer.material.setTexture("uSpecularMap", m_SpecularMap);

        // render scene cubes
        for (Vec3 const& objectPosition : c_ObjectPositions)
        {
            Graphics::DrawMesh(
                m_CubeMesh,
                {.scale = Vec3{0.5f}, .position = objectPosition},
                m_GBuffer.material,
                m_Camera
            );
        }
        m_Camera.renderTo(m_GBuffer.renderTarget);
    }

    void drawGBufferOverlays(Rect const& viewportRect) const
    {
        Graphics::BlitToScreen(
            m_GBuffer.albedo,
            Rect{viewportRect.p1, viewportRect.p1 + 200.0f}
        );
        Graphics::BlitToScreen(
            m_GBuffer.normal,
            Rect{viewportRect.p1 + Vec2{200.0f, 0.0f}, viewportRect.p1 + Vec2{200.0f, 0.0f} + 200.0f}
        );
        Graphics::BlitToScreen(
            m_GBuffer.position,
            Rect{viewportRect.p1 + Vec2{400.0f, 0.0f}, viewportRect.p1 + Vec2{400.0f, 0.0f} + 200.0f}
        );
    }

    void renderLightingPass()
    {
        m_LightPass.material.setRenderTexture("uPositionTex", m_GBuffer.position);
        m_LightPass.material.setRenderTexture("uNormalTex", m_GBuffer.normal);
        m_LightPass.material.setRenderTexture("uAlbedoTex", m_GBuffer.albedo);
        m_LightPass.material.setVec3Array("uLightPositions", m_LightPositions);
        m_LightPass.material.setVec3Array("uLightColors", m_LightColors);
        m_LightPass.material.setFloat("uLightLinear", 0.7f);
        m_LightPass.material.setFloat("uLightQuadratic", 1.8f);
        m_LightPass.material.setVec3("uViewPos", m_Camera.getPosition());

        Graphics::DrawMesh(m_QuadMesh, Identity<Transform>(), m_LightPass.material, m_Camera);

        m_Camera.renderTo(m_OutputTexture);

        m_LightPass.material.clearRenderTexture("uPositionTex");
        m_LightPass.material.clearRenderTexture("uNormalTex");
        m_LightPass.material.clearRenderTexture("uAlbedoTex");
    }

    void renderLightCubes()
    {
        OSC_ASSERT(m_LightPositions.size() == m_LightColors.size());

        for (size_t i = 0; i < m_LightPositions.size(); ++i) {
            m_LightBoxMaterial.setVec3("uLightColor", m_LightColors[i]);
            Graphics::DrawMesh(m_CubeMesh, {.scale = Vec3{0.125f}, .position = m_LightPositions[i]}, m_LightBoxMaterial, m_Camera);
        }

        RenderTarget t{
            {
                RenderTargetColorAttachment{
                    m_OutputTexture.updColorBuffer(),
                    RenderBufferLoadAction::Load,
                    RenderBufferStoreAction::Resolve,
                    Color::clear(),
                },
            },
            RenderTargetDepthAttachment{
                m_GBuffer.albedo.updDepthBuffer(),
                RenderBufferLoadAction::Load,
                RenderBufferStoreAction::DontCare,
            },
        };
        m_Camera.renderTo(t);
    }

    // scene state
    std::vector<Vec3> m_LightPositions = GenerateNSceneLightPositions(c_NumLights);
    std::vector<Vec3> m_LightColors = GenerateNSceneLightColors(c_NumLights);
    Camera m_Camera = CreateCameraThatMatchesLearnOpenGL();
    bool m_IsMouseCaptured = true;
    Eulers m_CameraEulers = {};
    Mesh m_CubeMesh = GenerateCubeMesh();
    Mesh m_QuadMesh = GenerateTexturedQuadMesh();
    Texture2D m_DiffuseMap = LoadTexture2DFromImage(
        App::resource("oscar_learnopengl/textures/container2.png"),
        ColorSpace::sRGB,
        ImageLoadingFlags::FlipVertically
    );
    Texture2D m_SpecularMap = LoadTexture2DFromImage(
        App::resource("oscar_learnopengl/textures/container2_specular.png"),
        ColorSpace::sRGB,
        ImageLoadingFlags::FlipVertically
    );

    // rendering state
    GBufferRenderingState m_GBuffer;
    LightPassState m_LightPass;

    Material m_LightBoxMaterial{Shader{
        App::slurp("oscar_learnopengl/shaders/AdvancedLighting/deferred_shading/LightBox.vert"),
        App::slurp("oscar_learnopengl/shaders/AdvancedLighting/deferred_shading/LightBox.frag"),
    }};

    RenderTexture m_OutputTexture;
};


// public API

CStringView osc::LOGLDeferredShadingTab::id()
{
    return c_TabStringID;
}

osc::LOGLDeferredShadingTab::LOGLDeferredShadingTab(ParentPtr<ITabHost> const&) :
    m_Impl{std::make_unique<Impl>()}
{
}

osc::LOGLDeferredShadingTab::LOGLDeferredShadingTab(LOGLDeferredShadingTab&&) noexcept = default;
osc::LOGLDeferredShadingTab& osc::LOGLDeferredShadingTab::operator=(LOGLDeferredShadingTab&&) noexcept = default;
osc::LOGLDeferredShadingTab::~LOGLDeferredShadingTab() noexcept = default;

osc::UID osc::LOGLDeferredShadingTab::implGetID() const
{
    return m_Impl->getID();
}

CStringView osc::LOGLDeferredShadingTab::implGetName() const
{
    return m_Impl->getName();
}

void osc::LOGLDeferredShadingTab::implOnMount()
{
    m_Impl->onMount();
}

void osc::LOGLDeferredShadingTab::implOnUnmount()
{
    m_Impl->onUnmount();
}

bool osc::LOGLDeferredShadingTab::implOnEvent(SDL_Event const& e)
{
    return m_Impl->onEvent(e);
}

void osc::LOGLDeferredShadingTab::implOnDraw()
{
    m_Impl->onDraw();
}
