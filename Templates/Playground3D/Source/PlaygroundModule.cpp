#include "Engine/Runtime/Module.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Platform/Input.h"
#include "Engine/Core/Log/Log.h"

namespace Playground
{
	using Core::Vector3;

	class PlayerController : public Core::ScriptableEntity
	{
	public:
		void onStart() override
		{
			Core::Log::Get().info("Playground3D", "PlayerController script started on entity: '{}'", getEntity().getName());
		}

		void onUpdate(float deltaTime) override
		{
			Vector3 moveDir(0.0f, 0.0f, 0.0f);
			if (Platform::Input::isKeyPressed(Platform::KeyCode::W) || Platform::Input::isKeyPressed(Platform::KeyCode::Up))
			{
				moveDir.z += 1.0f;
			}
			if (Platform::Input::isKeyPressed(Platform::KeyCode::S) || Platform::Input::isKeyPressed(Platform::KeyCode::Down))
			{
				moveDir.z -= 1.0f;
			}
			if (Platform::Input::isKeyPressed(Platform::KeyCode::D) || Platform::Input::isKeyPressed(Platform::KeyCode::Right))
			{
				moveDir.x += 1.0f;
			}
			if (Platform::Input::isKeyPressed(Platform::KeyCode::A) || Platform::Input::isKeyPressed(Platform::KeyCode::Left))
			{
				moveDir.x -= 1.0f;
			}

			if (moveDir.LengthSquared() > 0.0f)
			{
				moveDir.Normalize();
				auto& trans = getEntity().getTransform();
				trans.position += moveDir * (m_speed * deltaTime);
			}

			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Space))
			{
				auto* state = getEntity().tryGetComponent<Core::RigidbodyRuntimeState>();
				if (state)
				{
					state->linearVelocity.y = m_jumpForce;
					Core::Log::Get().info("Playground3D", "Player jumped!");
				}
			}
		}

		void onCollisionEnter(Core::Entity other) override
		{
			Core::Log::Get().info("Playground3D", "PlayerController: Hit '{}'", other.getName());
		}

	private:
		float m_speed = 5.0f;
		float m_jumpForce = 6.0f;
	};

	class Rotator : public Core::ScriptableEntity
	{
	public:
		void onUpdate(float deltaTime) override
		{
			auto& trans = getEntity().getTransform();
			Vector3 degs = trans.eulerDegrees;
			degs.y += m_rotationSpeed * deltaTime;
			if (degs.y >= 360.0f)
			{
				degs.y -= 360.0f;
			}
			trans.setEulerDegrees(degs);
		}

	private:
		float m_rotationSpeed = 60.0f; // degrees/sec
	};

	class TriggerZone : public Core::ScriptableEntity
	{
	public:
		void onTriggerEnter(Core::Entity other) override
		{
			Core::Log::Get().info("Playground3D", "TriggerZone: Entity '{}' ENTERED trigger area!", other.getName());
		}

		void onTriggerExit(Core::Entity other) override
		{
			Core::Log::Get().info("Playground3D", "TriggerZone: Entity '{}' EXITED trigger area!", other.getName());
		}
	};

	class Bouncer : public Core::ScriptableEntity
	{
	public:
		void onCollisionEnter(Core::Entity other) override
		{
			Core::Log::Get().info("Playground3D", "Bouncer: Bounced off '{}'!", other.getName());
		}
	};
}

extern "C"
{
	DF_MODULE_API Core::ModuleInfo DF_GetModuleInfo()
	{
		return Core::ModuleInfo{
			"Playground3D",
			"1.0.0",
			Core::DARKFIRE_API_VERSION
		};
	}

	DF_MODULE_API void DF_RegisterTypes(Core::EngineContext& ctx)
	{
		ctx.log.info("Playground3D", "Registering Playground3D scripts into ScriptRegistry...");
		Core::ScriptRegistry::Get().registerScript<Playground::PlayerController>("PlayerController");
		Core::ScriptRegistry::Get().registerScript<Playground::Rotator>("Rotator");
		Core::ScriptRegistry::Get().registerScript<Playground::TriggerZone>("TriggerZone");
		Core::ScriptRegistry::Get().registerScript<Playground::Bouncer>("Bouncer");
	}

	DF_MODULE_API void DF_OnLoad(Core::EngineContext& ctx)
	{
		ctx.log.info("Playground3D", "Playground3D module loaded successfully across DLL boundary!");
	}

	DF_MODULE_API void DF_OnUnload()
	{
		Core::Log::Get().info("Playground3D", "Playground3D module unloaded.");
	}

	DF_MODULE_API void DF_RegisterEditorTools(Core::EditorContext& ctx)
	{
		(void)ctx;
	}
}
