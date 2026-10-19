#pragma once
#include "Core/Graphics/Prerequisites.h"
#include "Resource.h"
#include "Core/Math/Vector4d.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace Core
{
	enum class BlendMode
	{
		Opaque,
		AlphaBlend,
		Additive
	};

	enum class DepthMode
	{
		Default,
		ReadOnly,
		None
	};

	class Material : public Resource
	{
	public:
		Material(const wchar_t* full_path, ResourceManager* manager);
		Material(const MaterialPtr& material, ResourceManager* manager);
		Material(const ShaderPtr& shader, ResourceManager* manager);

		void addTexture(const TexturePtr& texture);
		void removeTexture(unsigned int index);
		const std::vector<TexturePtr>& getTextures() const { return m_vec_textures; }

		void setData(void* data, unsigned int size);
		void setUserData(void* data, unsigned int size);

		void setCullMode(const CullMode& mode);
		CullMode getCullMode() const;

		void setBlendMode(BlendMode mode) { m_blendMode = mode; }
		BlendMode getBlendMode() const { return m_blendMode; }

		void setDepthMode(DepthMode mode) { m_depthMode = mode; }
		DepthMode getDepthMode() const { return m_depthMode; }

		// Named parameter support
		void setFloat(const std::string& name, float value);
		float getFloat(const std::string& name, float defaultValue = 0.0f) const;

		void setColor(const std::string& name, const Vector4d& color);
		Vector4d getColor(const std::string& name, const Vector4d& defaultValue = Vector4d(1.0f, 1.0f, 1.0f, 1.0f)) const;

		void setVector(const std::string& name, const Vector4d& vec);
		Vector4d getVector(const std::string& name, const Vector4d& defaultValue = Vector4d()) const;

		void setTexture(const std::string& name, const TexturePtr& texture);
		TexturePtr getTexture(const std::string& name) const;

		const ShaderPtr& getShader() const { return m_shader; }
		void setShader(const ShaderPtr& shader) { m_shader = shader; }

	private:
		ShaderPtr m_shader;
		ConstantBufferPtr m_constant_buffer;
		ConstantBufferPtr m_userBuffer;
		std::vector<TexturePtr> m_vec_textures;
		CullMode m_cull_mode = CullMode::Back;
		BlendMode m_blendMode = BlendMode::Opaque;
		DepthMode m_depthMode = DepthMode::Default;

		std::unordered_map<std::string, float> m_floatParams;
		std::unordered_map<std::string, Vector4d> m_vectorParams;
		std::unordered_map<std::string, TexturePtr> m_namedTextures;

	private:
		friend class GraphicsEngine;
		friend class UIRender;
	};

	// Get Default Material
	MaterialPtr GetDefaultMaterial(ResourceManager* manager);
}

