#pragma once
#include "RenderPass.h"
#include <vector>
#include <memory>
#include <algorithm>

namespace Core
{
    /// Manages and executes render passes in priority order
    class RenderPipeline
    {
    public:
        RenderPipeline() = default;
        ~RenderPipeline() = default;
        
        /// Add a render pass to the pipeline
        /// Passes are automatically sorted by priority
        template<typename T, typename... Args>
        T* addPass(Args&&... args)
        {
            static_assert(std::is_base_of_v<IRenderPass, T>, "T must inherit from IRenderPass");
            auto pass = std::make_unique<T>(std::forward<Args>(args)...);
            T* ptr = pass.get();
            m_passes.push_back(std::move(pass));
            sortPasses();
            return ptr;
        }
        
        /// Add an existing pass (takes ownership)
        IRenderPass* addPass(std::unique_ptr<IRenderPass> pass)
        {
            IRenderPass* ptr = pass.get();
            m_passes.push_back(std::move(pass));
            sortPasses();
            return ptr;
        }
        
        /// Remove a pass by pointer
        void removePass(IRenderPass* pass)
        {
            m_passes.erase(
                std::remove_if(m_passes.begin(), m_passes.end(),
                    [pass](const auto& p) { return p.get() == pass; }),
                m_passes.end()
            );
        }
        
        /// Execute all enabled passes in priority order
        void execute(GraphicsEngine* gfx, ECS* ecs, const RenderContext& ctx)
        {
            for (auto& pass : m_passes)
            {
                if (pass && pass->enabled)
                {
                    pass->execute(gfx, ecs, ctx);
                }
            }
        }
        
        /// Get all passes (for debugging/inspection)
        const std::vector<std::unique_ptr<IRenderPass>>& getPasses() const
        {
            return m_passes;
        }
        
        /// Get pass by name (returns nullptr if not found)
        IRenderPass* getPass(const char* name) const
        {
            for (const auto& pass : m_passes)
            {
                if (pass && strcmp(pass->name(), name) == 0)
                {
                    return pass.get();
                }
            }
            return nullptr;
        }
        
        /// Clear all passes
        void clear()
        {
            m_passes.clear();
        }
        
    private:
        void sortPasses()
        {
            std::sort(m_passes.begin(), m_passes.end(),
                [](const auto& a, const auto& b)
                {
                    return a->priority() < b->priority();
                });
        }
        
        std::vector<std::unique_ptr<IRenderPass>> m_passes;
    };
}
