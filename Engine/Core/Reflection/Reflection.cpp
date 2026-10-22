#include "Reflection.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include <nlohmann/json.hpp>

namespace Core
{
	ComponentRegistry& ComponentRegistry::Get()
	{
		static ComponentRegistry instance;
		return instance;
	}

	void ComponentRegistry::registerComponent(ComponentDesc desc)
	{
		for (auto& existing : m_components)
		{
			if (existing.name == desc.name)
			{
				existing = std::move(desc);
				return;
			}
		}
		m_components.push_back(std::move(desc));
	}

	const ComponentDesc* ComponentRegistry::findByName(std::string_view name) const
	{
		for (const auto& desc : m_components)
		{
			if (desc.name == name)
			{
				return &desc;
			}
		}
		return nullptr;
	}

	const ComponentDesc* ComponentRegistry::findByTypeId(uint64_t typeId) const
	{
		for (const auto& desc : m_components)
		{
			if (desc.typeId == typeId)
			{
				return &desc;
			}
		}
		return nullptr;
	}

	void ComponentRegistry::registerBuiltinTypes()
	{
		// 1. IdentityComponent
		{
			ComponentDesc desc;
			desc.name = "IdentityComponent";
			desc.typeId = entt::type_hash<IdentityComponent>::value();
			desc.isAuthored = true;
			desc.fields = {
				{ "id", FieldType::String, {} },
				{ "name", FieldType::String, {} },
				{ "tag", FieldType::String, {} }
			};
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<IdentityComponent>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<IdentityComponent>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<IdentityComponent>(e);
			};
			desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				dst.getRegistry().emplace_or_replace<IdentityComponent>(d, src.getRegistry().get<IdentityComponent>(s));
			};
			desc.serializeToJson = [](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<IdentityComponent>(e);
				j["id"] = comp.id.value();
				j["name"] = comp.name;
				j["tag"] = comp.tag;
			};
			desc.deserializeFromJson = [](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<IdentityComponent>(e);
				if (j.contains("id")) comp.id = UUID(j["id"].get<uint64_t>());
				if (j.contains("name")) comp.name = j["name"].get<std::string>();
				if (j.contains("tag")) comp.tag = j["tag"].get<std::string>();
			};
			registerComponent(std::move(desc));
		}

		// 2. TransformComponent
		{
			ComponentDesc desc;
			desc.name = "TransformComponent";
			desc.typeId = entt::type_hash<TransformComponent>::value();
			desc.isAuthored = true;
			desc.fields = {
				{ "position", FieldType::Vec3, {} },
				{ "rotation", FieldType::Quat, {} },
				{ "scale", FieldType::Vec3, {} },
				{ "eulerDegrees", FieldType::Vec3, {} }
			};
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<TransformComponent>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<TransformComponent>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<TransformComponent>(e);
			};
			desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				dst.getRegistry().emplace_or_replace<TransformComponent>(d, src.getRegistry().get<TransformComponent>(s));
			};
			desc.serializeToJson = [](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<TransformComponent>(e);
				j["position"] = { comp.position.x, comp.position.y, comp.position.z };
				j["rotation"] = { comp.rotation.x, comp.rotation.y, comp.rotation.z, comp.rotation.w };
				j["scale"] = { comp.scale.x, comp.scale.y, comp.scale.z };
				j["eulerDegrees"] = { comp.eulerDegrees.x, comp.eulerDegrees.y, comp.eulerDegrees.z };
			};
			desc.deserializeFromJson = [](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<TransformComponent>(e);
				if (j.contains("position") && j["position"].is_array() && j["position"].size() == 3)
				{
					comp.position = Vector3(j["position"][0], j["position"][1], j["position"][2]);
				}
				if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() == 4)
				{
					comp.rotation = Quaternion(j["rotation"][0], j["rotation"][1], j["rotation"][2], j["rotation"][3]);
				}
				if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() == 3)
				{
					comp.scale = Vector3(j["scale"][0], j["scale"][1], j["scale"][2]);
				}
				if (j.contains("eulerDegrees") && j["eulerDegrees"].is_array() && j["eulerDegrees"].size() == 3)
				{
					comp.eulerDegrees = Vector3(j["eulerDegrees"][0], j["eulerDegrees"][1], j["eulerDegrees"][2]);
				}
			};
			registerComponent(std::move(desc));
		}

		// 3. CameraComponent
		{
			ComponentDesc desc;
			desc.name = "CameraComponent";
			desc.typeId = entt::type_hash<CameraComponent>::value();
			desc.isAuthored = true;
			desc.fields = {
				{ "fov", FieldType::Float, { true, 1.0f, 179.0f, "Field of view in degrees" } },
				{ "nearZ", FieldType::Float, { true, 0.001f, 100.0f, "Near clipping plane" } },
				{ "farZ", FieldType::Float, { true, 1.0f, 10000.0f, "Far clipping plane" } },
				{ "isPrimary", FieldType::Bool, {} },
				{ "isOrthographic", FieldType::Bool, {} },
				{ "orthoWidth", FieldType::Float, {} },
				{ "orthoHeight", FieldType::Float, {} }
			};
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<CameraComponent>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<CameraComponent>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<CameraComponent>(e);
			};
			desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				dst.getRegistry().emplace_or_replace<CameraComponent>(d, src.getRegistry().get<CameraComponent>(s));
			};
			desc.serializeToJson = [](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<CameraComponent>(e);
				j["fov"] = comp.fov;
				j["nearZ"] = comp.nearZ;
				j["farZ"] = comp.farZ;
				j["isPrimary"] = comp.isPrimary;
				j["isOrthographic"] = comp.isOrthographic;
				j["orthoWidth"] = comp.orthoWidth;
				j["orthoHeight"] = comp.orthoHeight;
			};
			desc.deserializeFromJson = [](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<CameraComponent>(e);
				if (j.contains("fov")) comp.fov = j["fov"].get<float>();
				if (j.contains("nearZ")) comp.nearZ = j["nearZ"].get<float>();
				if (j.contains("farZ")) comp.farZ = j["farZ"].get<float>();
				if (j.contains("isPrimary")) comp.isPrimary = j["isPrimary"].get<bool>();
				if (j.contains("isOrthographic")) comp.isOrthographic = j["isOrthographic"].get<bool>();
				if (j.contains("orthoWidth")) comp.orthoWidth = j["orthoWidth"].get<float>();
				if (j.contains("orthoHeight")) comp.orthoHeight = j["orthoHeight"].get<float>();
			};
			registerComponent(std::move(desc));
		}

		// 4. MeshRendererComponent
		{
			ComponentDesc desc;
			desc.name = "MeshRendererComponent";
			desc.typeId = entt::type_hash<MeshRendererComponent>::value();
			desc.isAuthored = true;
			desc.fields = {
				{ "meshPath", FieldType::String, {} },
				{ "materialPath", FieldType::String, {} },
				{ "castShadows", FieldType::Bool, {} },
				{ "receiveShadows", FieldType::Bool, {} }
			};
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<MeshRendererComponent>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<MeshRendererComponent>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<MeshRendererComponent>(e);
			};
			desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				dst.getRegistry().emplace_or_replace<MeshRendererComponent>(d, src.getRegistry().get<MeshRendererComponent>(s));
			};
			desc.serializeToJson = [](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<MeshRendererComponent>(e);
				j["meshPath"] = comp.meshPath;
				j["materialPath"] = comp.materialPath;
				j["castShadows"] = comp.castShadows;
				j["receiveShadows"] = comp.receiveShadows;
			};
			desc.deserializeFromJson = [](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<MeshRendererComponent>(e);
				if (j.contains("meshPath")) comp.meshPath = j["meshPath"].get<std::string>();
				if (j.contains("materialPath")) comp.materialPath = j["materialPath"].get<std::string>();
				if (j.contains("castShadows")) comp.castShadows = j["castShadows"].get<bool>();
				if (j.contains("receiveShadows")) comp.receiveShadows = j["receiveShadows"].get<bool>();
			};
			registerComponent(std::move(desc));
		}

		// 5. LightComponent
		{
			ComponentDesc desc;
			desc.name = "LightComponent";
			desc.typeId = entt::type_hash<LightComponent>::value();
			desc.isAuthored = true;
			desc.fields = {
				{ "type", FieldType::Enum, {} },
				{ "color", FieldType::Color, { false, 0, 0, "", true } },
				{ "intensity", FieldType::Float, { true, 0.0f, 100.0f } },
				{ "range", FieldType::Float, { true, 0.1f, 1000.0f } },
				{ "spotInnerAngle", FieldType::Float, {} },
				{ "spotOuterAngle", FieldType::Float, {} }
			};
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<LightComponent>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<LightComponent>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<LightComponent>(e);
			};
			desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				dst.getRegistry().emplace_or_replace<LightComponent>(d, src.getRegistry().get<LightComponent>(s));
			};
			desc.serializeToJson = [](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<LightComponent>(e);
				j["type"] = static_cast<uint8_t>(comp.type);
				j["color"] = { comp.color.R(), comp.color.G(), comp.color.B(), comp.color.A() };
				j["intensity"] = comp.intensity;
				j["range"] = comp.range;
				j["spotInnerAngle"] = comp.spotInnerAngle;
				j["spotOuterAngle"] = comp.spotOuterAngle;
			};
			desc.deserializeFromJson = [](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<LightComponent>(e);
				if (j.contains("type")) comp.type = static_cast<LightType>(j["type"].get<uint8_t>());
				if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4)
				{
					comp.color = Color(j["color"][0], j["color"][1], j["color"][2], j["color"][3]);
				}
				if (j.contains("intensity")) comp.intensity = j["intensity"].get<float>();
				if (j.contains("range")) comp.range = j["range"].get<float>();
				if (j.contains("spotInnerAngle")) comp.spotInnerAngle = j["spotInnerAngle"].get<float>();
				if (j.contains("spotOuterAngle")) comp.spotOuterAngle = j["spotOuterAngle"].get<float>();
			};
			registerComponent(std::move(desc));
		}

		// 6. RigidbodyComponent
		{
			ComponentDesc desc;
			desc.name = "RigidbodyComponent";
			desc.typeId = entt::type_hash<RigidbodyComponent>::value();
			desc.isAuthored = true;
			desc.fields = {
				{ "mass", FieldType::Float, { true, 0.001f, 10000.0f } },
				{ "drag", FieldType::Float, { true, 0.0f, 100.0f } },
				{ "angularDrag", FieldType::Float, { true, 0.0f, 100.0f } },
				{ "isKinematic", FieldType::Bool, {} },
				{ "useGravity", FieldType::Bool, {} },
				{ "restitution", FieldType::Float, { true, 0.0f, 1.0f } },
				{ "friction", FieldType::Float, { true, 0.0f, 10.0f } }
			};
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<RigidbodyComponent>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<RigidbodyComponent>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<RigidbodyComponent>(e);
			};
			desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				dst.getRegistry().emplace_or_replace<RigidbodyComponent>(d, src.getRegistry().get<RigidbodyComponent>(s));
			};
			desc.serializeToJson = [](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<RigidbodyComponent>(e);
				j["mass"] = comp.mass;
				j["drag"] = comp.drag;
				j["angularDrag"] = comp.angularDrag;
				j["isKinematic"] = comp.isKinematic;
				j["useGravity"] = comp.useGravity;
				j["restitution"] = comp.restitution;
				j["friction"] = comp.friction;
			};
			desc.deserializeFromJson = [](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<RigidbodyComponent>(e);
				if (j.contains("mass")) comp.mass = j["mass"].get<float>();
				if (j.contains("drag")) comp.drag = j["drag"].get<float>();
				if (j.contains("angularDrag")) comp.angularDrag = j["angularDrag"].get<float>();
				if (j.contains("isKinematic")) comp.isKinematic = j["isKinematic"].get<bool>();
				if (j.contains("useGravity")) comp.useGravity = j["useGravity"].get<bool>();
				if (j.contains("restitution")) comp.restitution = j["restitution"].get<float>();
				if (j.contains("friction")) comp.friction = j["friction"].get<float>();
			};
			registerComponent(std::move(desc));
		}

		// 7. ColliderComponent
		{
			ComponentDesc desc;
			desc.name = "ColliderComponent";
			desc.typeId = entt::type_hash<ColliderComponent>::value();
			desc.isAuthored = true;
			desc.fields = {
				{ "shape", FieldType::Enum, {} },
				{ "size", FieldType::Vec3, {} },
				{ "radius", FieldType::Float, {} },
				{ "height", FieldType::Float, {} },
				{ "isTrigger", FieldType::Bool, {} },
				{ "layer", FieldType::Int, {} }
			};
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<ColliderComponent>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<ColliderComponent>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<ColliderComponent>(e);
			};
			desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				dst.getRegistry().emplace_or_replace<ColliderComponent>(d, src.getRegistry().get<ColliderComponent>(s));
			};
			desc.serializeToJson = [](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<ColliderComponent>(e);
				j["shape"] = static_cast<uint8_t>(comp.shape);
				j["size"] = { comp.size.x, comp.size.y, comp.size.z };
				j["radius"] = comp.radius;
				j["height"] = comp.height;
				j["isTrigger"] = comp.isTrigger;
				j["layer"] = comp.layer;
			};
			desc.deserializeFromJson = [](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<ColliderComponent>(e);
				if (j.contains("shape")) comp.shape = static_cast<ColliderShape>(j["shape"].get<uint8_t>());
				if (j.contains("size") && j["size"].is_array() && j["size"].size() == 3)
				{
					comp.size = Vector3(j["size"][0], j["size"][1], j["size"][2]);
				}
				if (j.contains("radius")) comp.radius = j["radius"].get<float>();
				if (j.contains("height")) comp.height = j["height"].get<float>();
				if (j.contains("isTrigger")) comp.isTrigger = j["isTrigger"].get<bool>();
				if (j.contains("layer")) comp.layer = j["layer"].get<uint8_t>();
			};
			registerComponent(std::move(desc));
		}

		// 8. ParticleEmitterComponent
		{
			ComponentDesc desc;
			desc.name = "ParticleEmitterComponent";
			desc.typeId = entt::type_hash<ParticleEmitterComponent>::value();
			desc.isAuthored = true;
			desc.fields = {
				{ "maxParticles", FieldType::Int, {} },
				{ "emissionRate", FieldType::Float, {} },
				{ "lifetime", FieldType::Float, {} },
				{ "startVelocity", FieldType::Vec3, {} },
				{ "startColor", FieldType::Color, { false, 0, 0, "", true } },
				{ "endColor", FieldType::Color, { false, 0, 0, "", true } }
			};
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<ParticleEmitterComponent>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<ParticleEmitterComponent>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<ParticleEmitterComponent>(e);
			};
			desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				dst.getRegistry().emplace_or_replace<ParticleEmitterComponent>(d, src.getRegistry().get<ParticleEmitterComponent>(s));
			};
			desc.serializeToJson = [](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<ParticleEmitterComponent>(e);
				j["maxParticles"] = comp.maxParticles;
				j["emissionRate"] = comp.emissionRate;
				j["lifetime"] = comp.lifetime;
				j["startVelocity"] = { comp.startVelocity.x, comp.startVelocity.y, comp.startVelocity.z };
				j["startColor"] = { comp.startColor.R(), comp.startColor.G(), comp.startColor.B(), comp.startColor.A() };
				j["endColor"] = { comp.endColor.R(), comp.endColor.G(), comp.endColor.B(), comp.endColor.A() };
			};
			desc.deserializeFromJson = [](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<ParticleEmitterComponent>(e);
				if (j.contains("maxParticles")) comp.maxParticles = j["maxParticles"].get<int>();
				if (j.contains("emissionRate")) comp.emissionRate = j["emissionRate"].get<float>();
				if (j.contains("lifetime")) comp.lifetime = j["lifetime"].get<float>();
				if (j.contains("startVelocity") && j["startVelocity"].is_array() && j["startVelocity"].size() == 3)
				{
					comp.startVelocity = Vector3(j["startVelocity"][0], j["startVelocity"][1], j["startVelocity"][2]);
				}
				if (j.contains("startColor") && j["startColor"].is_array() && j["startColor"].size() == 4)
				{
					comp.startColor = Color(j["startColor"][0], j["startColor"][1], j["startColor"][2], j["startColor"][3]);
				}
				if (j.contains("endColor") && j["endColor"].is_array() && j["endColor"].size() == 4)
				{
					comp.endColor = Color(j["endColor"][0], j["endColor"][1], j["endColor"][2], j["endColor"][3]);
				}
			};
			registerComponent(std::move(desc));
		}

		// 9. ScriptsComponent
		{
			ComponentDesc desc;
			desc.name = "ScriptsComponent";
			desc.typeId = entt::type_hash<ScriptsComponent>::value();
			desc.isAuthored = true;
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<ScriptsComponent>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<ScriptsComponent>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<ScriptsComponent>(e);
			};
			desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				const auto& srcComp = src.getRegistry().get<ScriptsComponent>(s);
				auto& dstComp = dst.getRegistry().emplace_or_replace<ScriptsComponent>(d);
				dstComp.missingScripts = srcComp.missingScripts;
				for (const auto& scr : srcComp.scripts)
				{
					ScriptInstance copyInst;
					copyInst.scriptName = scr.scriptName;
					copyInst.isEnabled = scr.isEnabled;
					copyInst.isStarted = false;
					dstComp.scripts.push_back(std::move(copyInst));
				}
			};
			desc.serializeToJson = [](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<ScriptsComponent>(e);
				auto scriptsArray = nlohmann::ordered_json::array();
				for (const auto& scr : comp.scripts)
				{
					nlohmann::ordered_json item;
					item["name"] = scr.scriptName;
					item["enabled"] = scr.isEnabled;
					scriptsArray.push_back(item);
				}
				j["scripts"] = scriptsArray;

				if (!comp.missingScripts.empty())
				{
					auto missingArray = nlohmann::ordered_json::array();
					for (const auto& m : comp.missingScripts)
					{
						nlohmann::ordered_json item;
						item["name"] = m.scriptName;
						item["rawJson"] = m.rawJson;
						missingArray.push_back(item);
					}
					j["missingScripts"] = missingArray;
				}
			};
			desc.deserializeFromJson = [](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<ScriptsComponent>(e);
				comp.scripts.clear();
				comp.missingScripts.clear();

				if (j.contains("scripts") && j["scripts"].is_array())
				{
					for (const auto& item : j["scripts"])
					{
						std::string name = item["name"].get<std::string>();
						bool enabled = item.value("enabled", true);
						comp.scripts.push_back(ScriptInstance{ name, nullptr, enabled, false });
					}
				}

				if (j.contains("missingScripts") && j["missingScripts"].is_array())
				{
					for (const auto& item : j["missingScripts"])
					{
						MissingScriptData m;
						m.scriptName = item["name"].get<std::string>();
						m.rawJson = item.value("rawJson", "");
						comp.missingScripts.push_back(std::move(m));
					}
				}
			};
			registerComponent(std::move(desc));
		}

		// 10. PrefabInstanceComponent
		{
			ComponentDesc desc;
			desc.name = "PrefabInstanceComponent";
			desc.typeId = entt::type_hash<PrefabInstanceComponent>::value();
			desc.isAuthored = true;
			desc.fields = {
				{ "prefabGuid", FieldType::String, {} },
				{ "prefabPath", FieldType::String, {} }
			};
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<PrefabInstanceComponent>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<PrefabInstanceComponent>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<PrefabInstanceComponent>(e);
			};
			desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				dst.getRegistry().emplace_or_replace<PrefabInstanceComponent>(d, src.getRegistry().get<PrefabInstanceComponent>(s));
			};
			desc.serializeToJson = [](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<PrefabInstanceComponent>(e);
				j["prefabGuid"] = comp.prefabGuid;
				j["prefabPath"] = comp.prefabPath;
			};
			desc.deserializeFromJson = [](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<PrefabInstanceComponent>(e);
				if (j.contains("prefabGuid")) comp.prefabGuid = j["prefabGuid"].get<uint64_t>();
				if (j.contains("prefabPath")) comp.prefabPath = j["prefabPath"].get<std::string>();
			};
			registerComponent(std::move(desc));
		}

		// 11. RigidbodyRuntimeState (Runtime only)
		{
			ComponentDesc desc;
			desc.name = "RigidbodyRuntimeState";
			desc.typeId = entt::type_hash<RigidbodyRuntimeState>::value();
			desc.isAuthored = false;
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<RigidbodyRuntimeState>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<RigidbodyRuntimeState>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<RigidbodyRuntimeState>(e);
			};
			registerComponent(std::move(desc));
		}

		// 12. ParticleRuntimeState (Runtime only)
		{
			ComponentDesc desc;
			desc.name = "ParticleRuntimeState";
			desc.typeId = entt::type_hash<ParticleRuntimeState>::value();
			desc.isAuthored = false;
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<ParticleRuntimeState>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<ParticleRuntimeState>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<ParticleRuntimeState>(e);
			};
			registerComponent(std::move(desc));
		}
	}
}
