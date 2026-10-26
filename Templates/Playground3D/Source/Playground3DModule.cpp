#include "Engine/Runtime/Module.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Platform/Input.h"
#include "Engine/Core/Log/Log.h"
#include <cmath>

namespace Playground
{
	using Core::Vector3;
	using Core::Vector2;

	class PlayerController : public Core::ScriptableEntity
	{
	public:
		void onCreate() override
		{
			exposeFloat("Move Speed", m_speed, 1.0f, 30.0f, 0.2f);
			exposeFloat("Jump Force", m_jumpForce, 1.0f, 25.0f, 0.2f);
		}

		void onStart() override
		{
			Core::Log::Get().info("Playground3D", "PlayerController script started on entity: '{}'", getEntity().getName());
		}

		void onUpdate(float deltaTime) override
		{
			Vector2 moveInput = getMovementInput2D();
			Vector3 moveDir(moveInput.x, 0.0f, moveInput.y);

			if (hasRigidbody())
			{
				setHorizontalVelocity(moveDir * m_speed);
				if (isKeyJustPressed(Platform::KeyCode::Space))
				{
					addImpulse(Vector3(0.0f, m_jumpForce, 0.0f));
					Core::Log::Get().info("Playground3D", "Player jumped with force: {}", m_jumpForce);
				}
			}
			else
			{
				translate(moveDir * (m_speed * deltaTime));
			}
		}

		void onCollisionEnter(Core::Entity other) override
		{
			Core::Log::Get().info("Playground3D", "PlayerController: Collided with '{}'", other.getName());
		}

	private:
		float m_speed = 7.0f;
		float m_jumpForce = 7.5f;
	};

	class Rotator : public Core::ScriptableEntity
	{
	public:
		void onCreate() override
		{
			exposeFloat("Rotation Speed", m_rotationSpeed, -360.0f, 360.0f, 1.0f);
		}

		void onUpdate(float deltaTime) override
		{
			rotateY(m_rotationSpeed * deltaTime);
		}

	private:
		float m_rotationSpeed = 60.0f; // degrees/sec
	};

	class FloatingPickup : public Core::ScriptableEntity
	{
	public:
		void onCreate() override
		{
			exposeFloat("Spin Speed", m_spinSpeed, -360.0f, 360.0f, 5.0f);
			exposeFloat("Bob Frequency", m_bobFreq, 0.1f, 10.0f, 0.1f);
			exposeFloat("Bob Amplitude", m_bobAmp, 0.01f, 2.0f, 0.05f);
		}

		void onStart() override
		{
			Core::Log::Get().info("Playground3D", "FloatingPickup initialized on entity '{}'", getEntity().getName());
		}

		void onUpdate(float deltaTime) override
		{
			rotateY(m_spinSpeed * deltaTime);
			m_timer += deltaTime;
			translate(Vector3(0.0f, std::cos(m_timer * m_bobFreq) * m_bobAmp * deltaTime, 0.0f));
		}

		void onTriggerEnter(Core::Entity other) override
		{
			Core::Log::Get().info("Playground3D", "FloatingPickup: Collected by '{}'!", other.getName());
		}

	private:
		float m_spinSpeed = 90.0f; // degrees/sec
		float m_bobFreq = 2.5f;
		float m_bobAmp = 0.5f;
		float m_timer = 0.0f;
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
		void onCreate() override
		{
			exposeFloat("Bounce Multiplier", m_bounceMultiplier, 0.1f, 5.0f, 0.05f);
		}

		void onCollisionEnter(Core::Entity other) override
		{
			addImpulse(Vector3(0.0f, 7.5f * m_bounceMultiplier, 0.0f));
			Core::Log::Get().info("Playground3D", "Bouncer: Propelled entity '{}' upwards!", other.getName());
		}

	private:
		float m_bounceMultiplier = 1.25f;
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
		Core::ScriptRegistry::Get().registerScript<Playground::FloatingPickup>("FloatingPickup");
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
