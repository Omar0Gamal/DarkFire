#pragma once
#include "Core/Graphics/Prerequisites.h"
#include "Resource.h"
#include <vector>

namespace Core
{
	class Material : public Resource
	{
	public:
		Material(const wchar_t* full_path, ResourceManager* manager);
		Material(const MaterialPtr& material, ResourceManager* manager);

		void addTexture(const  TexturePtr& texture);
		void removeTexture(unsigned int index);

		void setData(void* data, unsigned int size);
		void setUserData(void* data, unsigned int size);

		void setCullMode(const  CullMode& mode);
		CullMode getCullMode();
	private:
		ShaderPtr m_shader;
		ConstantBufferPtr m_constant_buffer;
		ConstantBufferPtr m_userBuffer;
		std::vector<TexturePtr> m_vec_textures;
		CullMode m_cull_mode = CullMode::Back;
	private:
		friend class  GraphicsEngine;
	};
}

