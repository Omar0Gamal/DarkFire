#pragma once
#include "RenderPass.h"
#include "Core/Game/ECS.h"
#include "Core/Game/Component/ImageComponent.h"
#include "Core/Game/Component/TextComponent.h"
#include "Core/Resource/Font.h"
#include "Core/Graphics/Font2D.h"
#include "GraphicsEngine.h"

namespace Core
{
    /// Renders UI elements (images and text)
    class UIRenderPass : public IRenderPass
    {
    public:
        void execute(GraphicsEngine* gfx, ECS* ecs, const RenderContext& ctx) override
        {
            // Render images
            ecs->ForEach<ImageComponent>([&](ImageComponent& image)
            {
                auto texture = image.GetTexture();
                if (!texture) return;
                
                Box rc = {
                    static_cast<unsigned int>(image.GetPosition().x),
                    static_cast<unsigned int>(image.GetPosition().y),
                    static_cast<unsigned int>(image.GetSize().width),
                    static_cast<unsigned int>(image.GetSize().height)
                };
                gfx->drawImage(texture, rc);
            });
            
            // Render text
            ecs->ForEach<TextComponent>([&](TextComponent& text)
            {
                auto fontRes = text.getFont();
                if (!fontRes) return;
                
                auto font = fontRes->getFont();
                if (!font || !font->m_batch || !font->m_font) return;
                
                font->m_batch->Begin();
                font->m_font->DrawString(
                    font->m_batch.get(),
                    text.getText(),
                    text.getPosition(),
                    text.getColor(),
                    text.getRotation(),
                    DirectX::XMFLOAT2(0, 0),
                    text.getScale()
                );
                font->m_batch->End();
            });
        }
        
        int priority() const override { return 500; } // UI renders last
        const char* name() const override { return "UIRenderPass"; }
    };
}
