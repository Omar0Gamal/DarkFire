#include "Engine/Runtime/Module.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include "Engine/Core/Log/Log.h"

namespace Empty3D
{
	class Rotator : public Core::ScriptableEntity
	{
	public:
		void onCreate() override
		{
			exposeFloat("Rotation Speed", m_rotationSpeed, -360.0f, 360.0f, 1.0f);
		}

		void onStart() override
		{
			Core::Log::Get().info("Empty3D", "Rotator script started on entity '{}'", getEntity().getName());
		}

		void onUpdate(float deltaTime) override
		{
			rotateY(m_rotationSpeed * deltaTime);
		}

	private:
		float m_rotationSpeed = 45.0f;
	};
}

extern "C"
{
	DF_MODULE_API Core::ModuleInfo DF_GetModuleInfo()
	{
		return Core::ModuleInfo{
			"Empty3D",
			"1.0.0",
			Core::DARKFIRE_API_VERSION
		};
	}

	DF_MODULE_API void DF_RegisterTypes(Core::EngineContext& ctx)
	{
		ctx.log.info("Empty3D", "Empty3D module type registration.");
		Core::ScriptRegistry::Get().registerScript<Empty3D::Rotator>("Rotator");
	}

	DF_MODULE_API void DF_OnLoad(Core::EngineContext& ctx)
	{
		ctx.log.info("Empty3D", "Empty3D module loaded successfully.");
	}

	DF_MODULE_API void DF_OnUnload()
	{
		Core::Log::Get().info("Empty3D", "Empty3D module unloaded.");
	}

	DF_MODULE_API void DF_RegisterEditorTools(Core::EditorContext& ctx)
	{
		(void)ctx;
	}
}
