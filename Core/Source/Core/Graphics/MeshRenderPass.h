#pragma once
#include "RenderPass.h"
#include "Core/Game/ECS.h"
#include "Core/Game/Component/TransformComponent.h"
#include "Core/Game/Component/MeshComponent.h"
#include "Core/Resource/Material.h"
#include "GraphicsEngine.h"

namespace Core
{
    /// Renders all entities with TransformComponent and MeshComponent
    class MeshRenderPass : public IRenderPass
    {
    public:
        void execute(GraphicsEngine* gfx, ECS* ecs, const RenderContext& ctx) override
        {
            ecs->ForEach<TransformComponent, MeshComponent>(
                [&](TransformComponent& transform, MeshComponent& mesh)
                {
                    if (!mesh.m_Mesh) return;
                    
                    GraphicsData gdata;
                    gdata.view = ctx.view;
                    gdata.proj = ctx.projection;
                    gdata.cameraPosition = ctx.cameraPosition;
                    gdata.time = ctx.totalTime;
                    
                    transform.proccessMatrix();
                    gdata.world = transform.m_worldMatrix;
                    
                    // Copy lights (max 32)
                    gdata.numLights = static_cast<int>((std::min)(ctx.lights.size(), size_t(32)));
                    for (int i = 0; i < gdata.numLights; i++)
                    {
                        gdata.lights[i] = ctx.lights[i];
                    }
                    
                    // Update material constant buffers
                    for (auto& material : mesh.m_Materials)
                    {
                        if (material)
                        {
                            material->setData(&gdata, sizeof(GraphicsData));
                        }
                    }
                    
                    gfx->drawMesh(mesh.m_Mesh, mesh.m_Materials);
                });
        }
        
        int priority() const override { return 100; }
        const char* name() const override { return "MeshRenderPass"; }
    };
}
