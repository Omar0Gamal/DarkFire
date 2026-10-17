#pragma once
#include "Core/Graphics/Prerequisites.h"
#include "Core/Math/Mat4x4.h"
#include "Core/Math/Vector4d.h"
#include <vector>

namespace Core
{
    /// Light data for shaders - must match HLSL struct
    __declspec(align(16))
    struct LightData
    {
        Vector4d lightColor;
        Vector4d lightDirection;
        Vector4d lightPosition;
    };

    /// Render context passed to all render passes
    struct RenderContext
    {
        Mat4x4 view;
        Mat4x4 projection;
        Vector4d cameraPosition;
        std::vector<LightData> lights;
        float deltaTime = 0.0f;
        float totalTime = 0.0f;
        
        // Viewport info
        unsigned int viewportWidth = 0;
        unsigned int viewportHeight = 0;
    };

    /// Graphics data sent to shaders - must match HLSL cbuffer
    __declspec(align(16))
    struct GraphicsData
    {
        Mat4x4 world;
        Mat4x4 view;
        Mat4x4 proj;
        Vector4d cameraPosition;
        LightData lights[32];
        int numLights = 0;
        float time = 0.0f;
    };

    class GraphicsEngine;
    class ECS;

    /// Base class for render passes
    /// Implement this to create custom rendering stages (shadows, post-process, etc.)
    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;
        
        /// Execute the render pass
        /// @param gfx Graphics engine for draw calls
        /// @param ecs Entity component system for querying entities
        /// @param ctx Render context with camera/light data
        virtual void execute(GraphicsEngine* gfx, ECS* ecs, const RenderContext& ctx) = 0;
        
        /// Priority determines execution order (lower = earlier)
        /// Suggested ranges:
        ///   0-99: Pre-render (shadows, etc.)
        ///   100-199: Opaque geometry
        ///   200-299: Transparent geometry
        ///   300-399: Post-processing
        ///   400+: UI
        virtual int priority() const { return 100; }
        
        /// Name for debugging
        virtual const char* name() const { return "UnnamedPass"; }
        
        /// Enable/disable pass at runtime
        bool enabled = true;
    };
}
