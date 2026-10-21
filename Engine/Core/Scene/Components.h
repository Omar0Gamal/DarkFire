#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Scene/UUID.h"
#include <entt/entt.hpp>
#include <string>
#include <vector>

namespace Core
{
	// ==========================================
	// Core Identity & Hierarchy Components
	// ==========================================

	struct DF_API IdentityComponent
	{
		UUID id;
		std::string name = "Entity";
		std::string tag = "Default";

		IdentityComponent() = default;
		explicit IdentityComponent(UUID uuid, std::string n = "Entity", std::string t = "Default")
			: id(uuid), name(std::move(n)), tag(std::move(t)) {}
	};

	struct DF_API HierarchyComponent
	{
		entt::entity parent = entt::null;
		std::vector<entt::entity> children;

		HierarchyComponent() = default;
	};

	// Authored local transform
	struct DF_API TransformComponent
	{
		Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
		Quaternion rotation = Quaternion::Identity;
		Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);
		Vector3 eulerDegrees = Vector3(0.0f, 0.0f, 0.0f); // Authoring & Inspector presentation

		TransformComponent() = default;
		TransformComponent(const Vector3& pos, const Quaternion& rot, const Vector3& scl)
			: position(pos), rotation(rot), scale(scl)
		{
			Vector3 rads = rot.ToEuler();
			eulerDegrees = Vector3(Math::toDegrees(rads.x), Math::toDegrees(rads.y), Math::toDegrees(rads.z));
		}

		Matrix getLocalMatrix() const noexcept
		{
			return Math::createTransformTRS(position, rotation, scale);
		}

		void setEulerDegrees(const Vector3& degs) noexcept
		{
			eulerDegrees = degs;
			rotation = Math::eulerToQuaternion(Math::toRadians(degs.x), Math::toRadians(degs.y), Math::toRadians(degs.z));
		}

		Vector3 getForward() const noexcept
		{
			return Vector3::Transform(Vector3(0.0f, 0.0f, 1.0f), rotation);
		}

		Vector3 getUp() const noexcept
		{
			return Vector3::Transform(Vector3(0.0f, 1.0f, 0.0f), rotation);
		}

		Vector3 getRight() const noexcept
		{
			return Vector3::Transform(Vector3(1.0f, 0.0f, 0.0f), rotation);
		}
	};

	// ==========================================
	// Runtime-Only Components (Ignored by Serializer & Scene Copy)
	// ==========================================

	struct DF_API WorldTransform
	{
		Matrix worldMatrix = DirectX::XMMatrixIdentity();
		bool isDirty = true;
	};

	struct DF_API RigidbodyRuntimeState
	{
		Vector3 linearVelocity = Vector3(0.0f, 0.0f, 0.0f);
		Vector3 angularVelocity = Vector3(0.0f, 0.0f, 0.0f);
	};

	struct DF_API ParticleRuntimeState
	{
		int activeCount = 0;
	};

	// ==========================================
	// Authored Gameplay & Engine Components
	// ==========================================

	struct DF_API CameraComponent
	{
		float fov = 60.0f; // vertical FOV in degrees
		float nearZ = 0.1f;
		float farZ = 1000.0f;
		bool isPrimary = true;
		bool isOrthographic = false;
		float orthoWidth = 10.0f;
		float orthoHeight = 10.0f;

		Matrix getProjection(float aspectRatio) const noexcept
		{
			if (isOrthographic)
			{
				return Math::orthographicLH_ReversedZ(orthoWidth, orthoHeight, nearZ, farZ);
			}
			return Math::perspectiveFovLH_ReversedZ(Math::toRadians(fov), aspectRatio, nearZ, farZ);
		}
	};

	struct DF_API MeshRendererComponent
	{
		std::string meshPath;
		std::string materialPath;
		bool castShadows = true;
		bool receiveShadows = true;
	};

	enum class LightType : uint8_t
	{
		Directional = 0,
		Point,
		Spot
	};

	struct DF_API LightComponent
	{
		LightType type = LightType::Directional;
		Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);
		float intensity = 1.0f;
		float range = 10.0f;
		float spotInnerAngle = 30.0f; // degrees
		float spotOuterAngle = 45.0f; // degrees
	};

	struct DF_API RigidbodyComponent
	{
		float mass = 1.0f;
		float drag = 0.05f;
		float angularDrag = 0.05f;
		bool isKinematic = false;
		bool useGravity = true;
		float restitution = 0.2f;
		float friction = 0.5f;
	};

	enum class ColliderShape : uint8_t
	{
		Box = 0,
		Sphere,
		Capsule
	};

	struct DF_API ColliderComponent
	{
		ColliderShape shape = ColliderShape::Box;
		Vector3 size = Vector3(1.0f, 1.0f, 1.0f);
		float radius = 0.5f;
		float height = 1.0f;
		bool isTrigger = false;
		uint8_t layer = 0; // Layer index 0..15 for collision filtering
	};

	struct DF_API ParticleEmitterComponent
	{
		int maxParticles = 1000;
		float emissionRate = 50.0f;
		float lifetime = 2.0f;
		Vector3 startVelocity = Vector3(0.0f, 5.0f, 0.0f);
		Color startColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
		Color endColor = Color(1.0f, 1.0f, 1.0f, 0.0f);
	};

	// ==========================================
	// Scripting Component
	// ==========================================

	class ScriptableEntity;

	struct DF_API ScriptInstance
	{
		std::string scriptName;
		std::shared_ptr<ScriptableEntity> instance = nullptr;
		bool isEnabled = true;
		bool isStarted = false;
	};

	struct DF_API MissingScriptData
	{
		std::string scriptName;
		std::string rawJson;
	};

	struct DF_API ScriptsComponent
	{
		std::vector<ScriptInstance> scripts;
		std::vector<MissingScriptData> missingScripts;

		ScriptsComponent() = default;

		template<typename T>
		T* getScript()
		{
			for (auto& s : scripts)
			{
				if (s.instance)
				{
					if (auto casted = dynamic_cast<T*>(s.instance.get()))
					{
						return casted;
					}
				}
			}
			return nullptr;
		}

		void addScript(const std::string& name, std::shared_ptr<ScriptableEntity> inst = nullptr)
		{
			scripts.push_back(ScriptInstance{ name, std::move(inst), true, false });
		}
	};
}

