#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Platform/Input.h"
#include "Engine/Platform/KeyCodes.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>

namespace Core
{
	enum class ScriptPropertyType
	{
		Float,
		Int,
		Bool,
		Vec2,
		Vec3,
		Vec4,
		Color,
		String
	};

	struct ScriptProperty
	{
		std::string name;
		ScriptPropertyType type = ScriptPropertyType::Float;
		void* dataPtr = nullptr;
		float minVal = 0.0f;
		float maxVal = 100.0f;
		float speed = 0.1f;
	};

	class DF_API ScriptableEntity
	{
	public:
		virtual ~ScriptableEntity() = default;

		void bind(Entity entity, Scene* scene)
		{
			m_entity = entity;
			m_scene = scene;
		}

		Entity getEntity() const noexcept { return m_entity; }
		Scene* getScene() const noexcept { return m_scene; }

		template<typename T>
		T& getComponent() { return m_entity.getComponent<T>(); }

		template<typename T>
		const T& getComponent() const { return m_entity.getComponent<T>(); }

		template<typename T>
		T* tryGetComponent() { return m_entity.tryGetComponent<T>(); }

		template<typename T>
		const T* tryGetComponent() const { return m_entity.tryGetComponent<T>(); }

		template<typename T>
		bool hasComponent() const { return m_entity.hasComponent<T>(); }

		template<typename T, typename... Args>
		T& addComponent(Args&&... args) { return m_entity.addComponent<T>(std::forward<Args>(args)...); }

		template<typename T>
		void removeComponent() { m_entity.removeComponent<T>(); }

		// Lifecycle hooks
		virtual void onCreate() {}
		virtual void onStart() {}
		virtual void onUpdate(float deltaTime) { (void)deltaTime; }
		virtual void onFixedUpdate(float fixedDeltaTime) { (void)fixedDeltaTime; }
		virtual void onLateUpdate(float deltaTime) { (void)deltaTime; }
		virtual void onEnable() {}
		virtual void onDisable() {}
		virtual void onDestroy() {}
		virtual void onCollisionEnter(Entity other) { (void)other; }
		virtual void onCollisionExit(Entity other) { (void)other; }
		virtual void onTriggerEnter(Entity other) { (void)other; }
		virtual void onTriggerExit(Entity other) { (void)other; }
		virtual void onValidate() {}

		// Property Exposure for Editor Inspector & Serialization
		void exposeFloat(const std::string& name, float& val, float minVal = 0.0f, float maxVal = 100.0f, float speed = 0.1f)
		{
			m_exposedProperties.push_back({ name, ScriptPropertyType::Float, &val, minVal, maxVal, speed });
		}

		void exposeInt(const std::string& name, int& val, int minVal = 0, int maxVal = 100)
		{
			m_exposedProperties.push_back({ name, ScriptPropertyType::Int, &val, static_cast<float>(minVal), static_cast<float>(maxVal), 1.0f });
		}

		void exposeBool(const std::string& name, bool& val)
		{
			m_exposedProperties.push_back({ name, ScriptPropertyType::Bool, &val, 0.0f, 1.0f, 1.0f });
		}

		void exposeVec2(const std::string& name, Vector2& val, float speed = 0.1f)
		{
			m_exposedProperties.push_back({ name, ScriptPropertyType::Vec2, &val, 0.0f, 0.0f, speed });
		}

		void exposeVec3(const std::string& name, Vector3& val, float speed = 0.1f)
		{
			m_exposedProperties.push_back({ name, ScriptPropertyType::Vec3, &val, 0.0f, 0.0f, speed });
		}

		void exposeColor(const std::string& name, Color& val)
		{
			m_exposedProperties.push_back({ name, ScriptPropertyType::Color, &val, 0.0f, 1.0f, 0.01f });
		}

		void exposeString(const std::string& name, std::string& val)
		{
			m_exposedProperties.push_back({ name, ScriptPropertyType::String, &val, 0.0f, 0.0f, 0.0f });
		}

		[[nodiscard]] const std::vector<ScriptProperty>& getExposedProperties() const noexcept { return m_exposedProperties; }
		std::vector<ScriptProperty>& getExposedProperties() noexcept { return m_exposedProperties; }

		std::string serializeExposedFields() const
		{
			nlohmann::ordered_json j = nlohmann::ordered_json::object();
			for (const auto& prop : m_exposedProperties)
			{
				if (!prop.dataPtr) continue;
				switch (prop.type)
				{
				case ScriptPropertyType::Float:
					j[prop.name] = *reinterpret_cast<float*>(prop.dataPtr);
					break;
				case ScriptPropertyType::Int:
					j[prop.name] = *reinterpret_cast<int*>(prop.dataPtr);
					break;
				case ScriptPropertyType::Bool:
					j[prop.name] = *reinterpret_cast<bool*>(prop.dataPtr);
					break;
				case ScriptPropertyType::Vec2:
				{
					const auto& v = *reinterpret_cast<Vector2*>(prop.dataPtr);
					j[prop.name] = { v.x, v.y };
					break;
				}
				case ScriptPropertyType::Vec3:
				{
					const auto& v = *reinterpret_cast<Vector3*>(prop.dataPtr);
					j[prop.name] = { v.x, v.y, v.z };
					break;
				}
				case ScriptPropertyType::Vec4:
				{
					const auto& v = *reinterpret_cast<Vector4*>(prop.dataPtr);
					j[prop.name] = { v.x, v.y, v.z, v.w };
					break;
				}
				case ScriptPropertyType::Color:
				{
					const auto& c = *reinterpret_cast<Color*>(prop.dataPtr);
					j[prop.name] = { c.R(), c.G(), c.B(), c.A() };
					break;
				}
				case ScriptPropertyType::String:
					j[prop.name] = *reinterpret_cast<std::string*>(prop.dataPtr);
					break;
				}
			}
			return j.dump();
		}

		void deserializeExposedFields(const std::string& jsonStr)
		{
			if (jsonStr.empty()) return;
			try
			{
				auto j = nlohmann::json::parse(jsonStr);
				for (auto& prop : m_exposedProperties)
				{
					if (!prop.dataPtr || !j.contains(prop.name)) continue;
					const auto& val = j[prop.name];
					switch (prop.type)
					{
					case ScriptPropertyType::Float:
						if (val.is_number()) *reinterpret_cast<float*>(prop.dataPtr) = val.get<float>();
						break;
					case ScriptPropertyType::Int:
						if (val.is_number_integer()) *reinterpret_cast<int*>(prop.dataPtr) = val.get<int>();
						break;
					case ScriptPropertyType::Bool:
						if (val.is_boolean()) *reinterpret_cast<bool*>(prop.dataPtr) = val.get<bool>();
						break;
					case ScriptPropertyType::Vec2:
						if (val.is_array() && val.size() >= 2)
						{
							auto* v = reinterpret_cast<Vector2*>(prop.dataPtr);
							v->x = val[0].get<float>();
							v->y = val[1].get<float>();
						}
						break;
					case ScriptPropertyType::Vec3:
						if (val.is_array() && val.size() >= 3)
						{
							auto* v = reinterpret_cast<Vector3*>(prop.dataPtr);
							v->x = val[0].get<float>();
							v->y = val[1].get<float>();
							v->z = val[2].get<float>();
						}
						break;
					case ScriptPropertyType::Vec4:
						if (val.is_array() && val.size() >= 4)
						{
							auto* v = reinterpret_cast<Vector4*>(prop.dataPtr);
							v->x = val[0].get<float>();
							v->y = val[1].get<float>();
							v->z = val[2].get<float>();
							v->w = val[3].get<float>();
						}
						break;
					case ScriptPropertyType::Color:
						if (val.is_array() && val.size() >= 3)
						{
							auto* c = reinterpret_cast<Color*>(prop.dataPtr);
							float r = val[0].get<float>();
							float g = val[1].get<float>();
							float b = val[2].get<float>();
							float a = (val.size() >= 4) ? val[3].get<float>() : 1.0f;
							*c = Color(r, g, b, a);
						}
						break;
					case ScriptPropertyType::String:
						if (val.is_string()) *reinterpret_cast<std::string*>(prop.dataPtr) = val.get<std::string>();
						break;
					}
				}
			}
			catch (...) {}
		}

		// High-Level Gameplay Helpers
		TransformComponent& transform()
		{
			return m_entity.getTransform();
		}

		const TransformComponent& transform() const
		{
			return m_entity.getTransform();
		}

		Vector3 getPosition() const
		{
			return m_entity.getTransform().position;
		}

		void setPosition(const Vector3& pos)
		{
			m_entity.getTransform().position = pos;
		}

		Vector3 getRotationEuler() const
		{
			return m_entity.getTransform().eulerDegrees;
		}

		void setRotationEuler(const Vector3& eulerDeg)
		{
			m_entity.getTransform().setEulerDegrees(eulerDeg);
		}

		Vector3 getScale() const
		{
			return m_entity.getTransform().scale;
		}

		void setScale(const Vector3& s)
		{
			m_entity.getTransform().scale = s;
		}

		Vector3 getForward() const
		{
			return m_entity.getTransform().getForward();
		}

		Vector3 getRight() const
		{
			return m_entity.getTransform().getRight();
		}

		Vector3 getUp() const
		{
			return m_entity.getTransform().getUp();
		}

		void translate(const Vector3& delta)
		{
			m_entity.getTransform().position += delta;
		}

		void rotateY(float deltaDeg)
		{
			auto& t = m_entity.getTransform();
			Vector3 degs = t.eulerDegrees;
			degs.y += deltaDeg;
			while (degs.y >= 360.0f) degs.y -= 360.0f;
			while (degs.y < 0.0f) degs.y += 360.0f;
			t.setEulerDegrees(degs);
		}

		bool hasRigidbody() const
		{
			return m_entity.hasComponent<RigidbodyComponent>() || m_entity.hasComponent<RigidbodyRuntimeState>();
		}

		void setHorizontalVelocity(const Vector3& planarVel)
		{
			auto* state = m_entity.tryGetComponent<RigidbodyRuntimeState>();
			if (state)
			{
				state->linearVelocity.x = planarVel.x;
				state->linearVelocity.z = planarVel.z;
			}
			else
			{
				m_entity.getTransform().position.x += planarVel.x;
				m_entity.getTransform().position.z += planarVel.z;
			}
		}

		void setLinearVelocity(const Vector3& vel)
		{
			auto* state = m_entity.tryGetComponent<RigidbodyRuntimeState>();
			if (state)
			{
				state->linearVelocity = vel;
			}
		}

		Vector3 getLinearVelocity() const
		{
			auto* state = m_entity.tryGetComponent<RigidbodyRuntimeState>();
			if (state)
			{
				return state->linearVelocity;
			}
			return Vector3::Zero;
		}

		void addForce(const Vector3& force, float dt = 0.016666f)
		{
			auto* state = m_entity.tryGetComponent<RigidbodyRuntimeState>();
			auto* rb = m_entity.tryGetComponent<RigidbodyComponent>();
			if (state && rb && rb->mass > 0.0f)
			{
				state->linearVelocity += (force / rb->mass) * dt;
			}
		}

		void addImpulse(const Vector3& impulse)
		{
			auto* state = m_entity.tryGetComponent<RigidbodyRuntimeState>();
			auto* rb = m_entity.tryGetComponent<RigidbodyComponent>();
			if (state && rb && rb->mass > 0.0f)
			{
				state->linearVelocity += impulse / rb->mass;
			}
		}

		// Input Helpers
		bool isKeyDown(Platform::KeyCode key) const
		{
			return Platform::Input::isKeyPressed(key);
		}

		bool isKeyJustPressed(Platform::KeyCode key) const
		{
			return Platform::Input::isKeyJustPressed(key);
		}

		bool isKeyJustReleased(Platform::KeyCode key) const
		{
			return Platform::Input::isKeyJustReleased(key);
		}

		bool isMouseButtonDown(Platform::MouseCode button) const
		{
			return Platform::Input::isMouseButtonPressed(button);
		}

		Vector2 getMovementInput2D() const
		{
			Vector2 move(0.0f, 0.0f);
			if (isKeyDown(Platform::KeyCode::W) || isKeyDown(Platform::KeyCode::Up)) move.y += 1.0f;
			if (isKeyDown(Platform::KeyCode::S) || isKeyDown(Platform::KeyCode::Down)) move.y -= 1.0f;
			if (isKeyDown(Platform::KeyCode::D) || isKeyDown(Platform::KeyCode::Right)) move.x += 1.0f;
			if (isKeyDown(Platform::KeyCode::A) || isKeyDown(Platform::KeyCode::Left)) move.x -= 1.0f;
			if (move.LengthSquared() > 0.0f)
			{
				move.Normalize();
			}
			return move;
		}

	protected:
		Entity m_entity{};
		Scene* m_scene = nullptr;
		std::vector<ScriptProperty> m_exposedProperties;
	};
}
