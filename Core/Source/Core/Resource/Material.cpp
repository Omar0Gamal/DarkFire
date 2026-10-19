#include "Material.h"

#include "Core/Graphics/ConstantBuffer.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "ResourceManager.h"
#include "Core/Game/Game.h"

namespace Core
{
	Material::Material(const wchar_t* full_path, ResourceManager* manager) : Resource(full_path, manager)
	{
		auto engine = m_resManager->getGame()->getGraphicsEngine();
		m_shader = engine->CreateShader({ full_path, "vsmain" , full_path, "psmain" });
	}

	Material::Material(const MaterialPtr& material, ResourceManager* manager) : Resource(L"", manager)
	{
		m_shader = material->m_shader;
		m_cull_mode = material->m_cull_mode;
		m_blendMode = material->m_blendMode;
		m_depthMode = material->m_depthMode;
		m_vec_textures = material->m_vec_textures;
		m_floatParams = material->m_floatParams;
		m_vectorParams = material->m_vectorParams;
		m_namedTextures = material->m_namedTextures;
	}

	Material::Material(const ShaderPtr& shader, ResourceManager* manager) : Resource(L"", manager), m_shader(shader)
	{
	}

	void Material::addTexture(const TexturePtr& texture)
	{
		m_vec_textures.push_back(texture);
	}

	void Material::removeTexture(unsigned int index)
	{
		if (index >= this->m_vec_textures.size()) return;
		m_vec_textures.erase(m_vec_textures.begin() + index);
	}

	void Material::setData(void* data, unsigned int size)
	{
		auto engine = m_resManager->getGame()->getGraphicsEngine();

		if (!m_constant_buffer)
			m_constant_buffer = engine->CreateConstantBuffer({ data, size });
		else
			m_constant_buffer->update(data);
	}

	void Material::setUserData(void* data, unsigned int size)
	{
		auto engine = m_resManager->getGame()->getGraphicsEngine();

		if (!m_userBuffer)
			m_userBuffer = engine->CreateConstantBuffer({ data, size });
		else
			m_userBuffer->update(data);
	}

	void Material::setCullMode(const CullMode& mode)
	{
		m_cull_mode = mode;
	}

	CullMode Material::getCullMode() const
	{
		return m_cull_mode;
	}

	void Material::setFloat(const std::string& name, float value)
	{
		m_floatParams[name] = value;
	}

	float Material::getFloat(const std::string& name, float defaultValue) const
	{
		auto it = m_floatParams.find(name);
		return (it != m_floatParams.end()) ? it->second : defaultValue;
	}

	void Material::setColor(const std::string& name, const Vector4d& color)
	{
		m_vectorParams[name] = color;
	}

	Vector4d Material::getColor(const std::string& name, const Vector4d& defaultValue) const
	{
		auto it = m_vectorParams.find(name);
		return (it != m_vectorParams.end()) ? it->second : defaultValue;
	}

	void Material::setVector(const std::string& name, const Vector4d& vec)
	{
		m_vectorParams[name] = vec;
	}

	Vector4d Material::getVector(const std::string& name, const Vector4d& defaultValue) const
	{
		auto it = m_vectorParams.find(name);
		return (it != m_vectorParams.end()) ? it->second : defaultValue;
	}

	void Material::setTexture(const std::string& name, const TexturePtr& texture)
	{
		m_namedTextures[name] = texture;
		// Keep in m_vec_textures for compatibility with older passes
		bool found = false;
		for (const auto& t : m_vec_textures)
		{
			if (t == texture) { found = true; break; }
		}
		if (!found && texture) m_vec_textures.push_back(texture);
	}

	TexturePtr Material::getTexture(const std::string& name) const
	{
		auto it = m_namedTextures.find(name);
		return (it != m_namedTextures.end()) ? it->second : nullptr;
	}

	MaterialPtr GetDefaultMaterial(ResourceManager* manager)
	{
		if (!manager) return nullptr;
		return manager->getDefaultMaterial();
	}
}
