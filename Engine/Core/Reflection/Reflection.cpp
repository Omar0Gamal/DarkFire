#include "Reflection.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include <nlohmann/json.hpp>

namespace Core
{
	namespace
	{
		inline void SerializeVec3(nlohmann::ordered_json& j, const std::string& key, const Vector3& v)
		{
			j[key] = { v.x, v.y, v.z };
		}

		inline void DeserializeVec3(const nlohmann::ordered_json& j, const std::string& key, Vector3& v)
		{
			if (j.contains(key) && j[key].is_array() && j[key].size() == 3)
			{
				v = Vector3(j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>());
			}
		}

		inline void SerializeColor(nlohmann::ordered_json& j, const std::string& key, const Color& c)
		{
			j[key] = { c.R(), c.G(), c.B(), c.A() };
		}

		inline void DeserializeColor(const nlohmann::ordered_json& j, const std::string& key, Color& c)
		{
			if (j.contains(key) && j[key].is_array() && j[key].size() == 4)
			{
				c = Color(j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>(), j[key][3].get<float>());
			}
		}
	}

	ComponentRegistry& ComponentRegistry::Get()
	{
		static ComponentRegistry instance;
		return instance;
	}

	void ComponentRegistry::registerComponent(ComponentDesc desc)
	{
		for (auto& existing : m_components)
		{
			if (existing.typeId == desc.typeId)
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
		ComponentBuilder<IdentityComponent>("IdentityComponent")
			.field("id", &IdentityComponent::id)
			.field("name", &IdentityComponent::name)
			.field("tag", &IdentityComponent::tag)
			.serialize(
				[](const Scene& scene, entt::entity e, void* jsonNode) {
					auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
					const auto& comp = scene.getRegistry().get<IdentityComponent>(e);
					j["id"] = comp.id.value();
					j["name"] = comp.name;
					j["tag"] = comp.tag;
				},
				[](Scene& scene, entt::entity e, const void* jsonNode) {
					const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
					auto& comp = scene.getRegistry().get_or_emplace<IdentityComponent>(e);
					if (j.contains("id")) comp.id = UUID(j["id"].get<uint64_t>());
					if (j.contains("name")) comp.name = j["name"].get<std::string>();
					if (j.contains("tag")) comp.tag = j["tag"].get<std::string>();
				})
			.registerSelf();

		// 2. TransformComponent
		ComponentBuilder<TransformComponent>("TransformComponent")
			.field("position", &TransformComponent::position)
			.field("rotation", &TransformComponent::rotation)
			.field("scale", &TransformComponent::scale)
			.field("eulerDegrees", &TransformComponent::eulerDegrees)
			.serialize(
				[](const Scene& scene, entt::entity e, void* jsonNode) {
					auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
					const auto& comp = scene.getRegistry().get<TransformComponent>(e);
					SerializeVec3(j, "position", comp.position);
					j["rotation"] = { comp.rotation.x, comp.rotation.y, comp.rotation.z, comp.rotation.w };
					SerializeVec3(j, "scale", comp.scale);
					SerializeVec3(j, "eulerDegrees", comp.eulerDegrees);
				},
				[](Scene& scene, entt::entity e, const void* jsonNode) {
					const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
					auto& comp = scene.getRegistry().get_or_emplace<TransformComponent>(e);
					DeserializeVec3(j, "position", comp.position);
					if (j.contains("rotation") && j["rotation"].is_array() && j["rotation"].size() == 4)
					{
						comp.rotation = Quaternion(j["rotation"][0], j["rotation"][1], j["rotation"][2], j["rotation"][3]);
					}
					DeserializeVec3(j, "scale", comp.scale);
					DeserializeVec3(j, "eulerDegrees", comp.eulerDegrees);
				})
			.registerSelf();

		// 3. CameraComponent
		ComponentBuilder<CameraComponent>("CameraComponent")
			.field("fov", &CameraComponent::fov, { true, 1.0f, 179.0f, "Field of view in degrees" })
			.field("nearZ", &CameraComponent::nearZ, { true, 0.001f, 100.0f, "Near clipping plane" })
			.field("farZ", &CameraComponent::farZ, { true, 1.0f, 10000.0f, "Far clipping plane" })
			.field("isPrimary", &CameraComponent::isPrimary)
			.field("isOrthographic", &CameraComponent::isOrthographic)
			.field("orthoWidth", &CameraComponent::orthoWidth)
			.field("orthoHeight", &CameraComponent::orthoHeight)
			.serialize(
				[](const Scene& scene, entt::entity e, void* jsonNode) {
					auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
					const auto& comp = scene.getRegistry().get<CameraComponent>(e);
					j["fov"] = comp.fov;
					j["nearZ"] = comp.nearZ;
					j["farZ"] = comp.farZ;
					j["isPrimary"] = comp.isPrimary;
					j["isOrthographic"] = comp.isOrthographic;
					j["orthoWidth"] = comp.orthoWidth;
					j["orthoHeight"] = comp.orthoHeight;
				},
				[](Scene& scene, entt::entity e, const void* jsonNode) {
					const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
					auto& comp = scene.getRegistry().get_or_emplace<CameraComponent>(e);
					if (j.contains("fov")) comp.fov = j["fov"].get<float>();
					if (j.contains("nearZ")) comp.nearZ = j["nearZ"].get<float>();
					if (j.contains("farZ")) comp.farZ = j["farZ"].get<float>();
					if (j.contains("isPrimary")) comp.isPrimary = j["isPrimary"].get<bool>();
					if (j.contains("isOrthographic")) comp.isOrthographic = j["isOrthographic"].get<bool>();
					if (j.contains("orthoWidth")) comp.orthoWidth = j["orthoWidth"].get<float>();
					if (j.contains("orthoHeight")) comp.orthoHeight = j["orthoHeight"].get<float>();
				})
			.registerSelf();

		// 4. MeshRendererComponent
		ComponentBuilder<MeshRendererComponent>("MeshRendererComponent")
			.field("meshPath", &MeshRendererComponent::meshPath)
			.field("materialPath", &MeshRendererComponent::materialPath)
			.field("castShadows", &MeshRendererComponent::castShadows)
			.field("receiveShadows", &MeshRendererComponent::receiveShadows)
			.serialize(
				[](const Scene& scene, entt::entity e, void* jsonNode) {
					auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
					const auto& comp = scene.getRegistry().get<MeshRendererComponent>(e);
					j["meshPath"] = comp.meshPath;
					j["materialPath"] = comp.materialPath;
					j["castShadows"] = comp.castShadows;
					j["receiveShadows"] = comp.receiveShadows;
				},
				[](Scene& scene, entt::entity e, const void* jsonNode) {
					const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
					auto& comp = scene.getRegistry().get_or_emplace<MeshRendererComponent>(e);
					if (j.contains("meshPath")) comp.meshPath = j["meshPath"].get<std::string>();
					if (j.contains("materialPath")) comp.materialPath = j["materialPath"].get<std::string>();
					if (j.contains("castShadows")) comp.castShadows = j["castShadows"].get<bool>();
					if (j.contains("receiveShadows")) comp.receiveShadows = j["receiveShadows"].get<bool>();
				})
			.registerSelf();

		// 5. LightComponent
		ComponentBuilder<LightComponent>("LightComponent")
			.enumField("type", &LightComponent::type, { "Directional", "Point", "Spot" })
			.field("color", &LightComponent::color, { false, 0, 0, "", true })
			.field("intensity", &LightComponent::intensity, { true, 0.0f, 100.0f })
			.field("range", &LightComponent::range, { true, 0.1f, 1000.0f })
			.field("spotInnerAngle", &LightComponent::spotInnerAngle)
			.field("spotOuterAngle", &LightComponent::spotOuterAngle)
			.serialize(
				[](const Scene& scene, entt::entity e, void* jsonNode) {
					auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
					const auto& comp = scene.getRegistry().get<LightComponent>(e);
					j["type"] = static_cast<uint8_t>(comp.type);
					SerializeColor(j, "color", comp.color);
					j["intensity"] = comp.intensity;
					j["range"] = comp.range;
					j["spotInnerAngle"] = comp.spotInnerAngle;
					j["spotOuterAngle"] = comp.spotOuterAngle;
				},
				[](Scene& scene, entt::entity e, const void* jsonNode) {
					const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
					auto& comp = scene.getRegistry().get_or_emplace<LightComponent>(e);
					if (j.contains("type")) comp.type = static_cast<LightType>(j["type"].get<uint8_t>());
					DeserializeColor(j, "color", comp.color);
					if (j.contains("intensity")) comp.intensity = j["intensity"].get<float>();
					if (j.contains("range")) comp.range = j["range"].get<float>();
					if (j.contains("spotInnerAngle")) comp.spotInnerAngle = j["spotInnerAngle"].get<float>();
					if (j.contains("spotOuterAngle")) comp.spotOuterAngle = j["spotOuterAngle"].get<float>();
				})
			.registerSelf();

		// 6. RigidbodyComponent
		ComponentBuilder<RigidbodyComponent>("RigidbodyComponent")
			.field("mass", &RigidbodyComponent::mass, { true, 0.001f, 10000.0f })
			.field("drag", &RigidbodyComponent::drag, { true, 0.0f, 100.0f })
			.field("angularDrag", &RigidbodyComponent::angularDrag, { true, 0.0f, 100.0f })
			.field("isKinematic", &RigidbodyComponent::isKinematic)
			.field("useGravity", &RigidbodyComponent::useGravity)
			.field("restitution", &RigidbodyComponent::restitution, { true, 0.0f, 1.0f })
			.field("friction", &RigidbodyComponent::friction, { true, 0.0f, 10.0f })
			.serialize(
				[](const Scene& scene, entt::entity e, void* jsonNode) {
					auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
					const auto& comp = scene.getRegistry().get<RigidbodyComponent>(e);
					j["mass"] = comp.mass;
					j["drag"] = comp.drag;
					j["angularDrag"] = comp.angularDrag;
					j["isKinematic"] = comp.isKinematic;
					j["useGravity"] = comp.useGravity;
					j["restitution"] = comp.restitution;
					j["friction"] = comp.friction;
				},
				[](Scene& scene, entt::entity e, const void* jsonNode) {
					const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
					auto& comp = scene.getRegistry().get_or_emplace<RigidbodyComponent>(e);
					if (j.contains("mass")) comp.mass = j["mass"].get<float>();
					if (j.contains("drag")) comp.drag = j["drag"].get<float>();
					if (j.contains("angularDrag")) comp.angularDrag = j["angularDrag"].get<float>();
					if (j.contains("isKinematic")) comp.isKinematic = j["isKinematic"].get<bool>();
					if (j.contains("useGravity")) comp.useGravity = j["useGravity"].get<bool>();
					if (j.contains("restitution")) comp.restitution = j["restitution"].get<float>();
					if (j.contains("friction")) comp.friction = j["friction"].get<float>();
				})
			.registerSelf();

		// 7. ColliderComponent
		ComponentBuilder<ColliderComponent>("ColliderComponent")
			.enumField("shape", &ColliderComponent::shape, { "Box", "Sphere", "Capsule", "Mesh" })
			.field("size", &ColliderComponent::size)
			.field("radius", &ColliderComponent::radius)
			.field("height", &ColliderComponent::height)
			.field("isTrigger", &ColliderComponent::isTrigger)
			.field("layer", &ColliderComponent::layer)
			.serialize(
				[](const Scene& scene, entt::entity e, void* jsonNode) {
					auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
					const auto& comp = scene.getRegistry().get<ColliderComponent>(e);
					j["shape"] = static_cast<uint8_t>(comp.shape);
					SerializeVec3(j, "size", comp.size);
					j["radius"] = comp.radius;
					j["height"] = comp.height;
					j["isTrigger"] = comp.isTrigger;
					j["layer"] = comp.layer;
				},
				[](Scene& scene, entt::entity e, const void* jsonNode) {
					const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
					auto& comp = scene.getRegistry().get_or_emplace<ColliderComponent>(e);
					if (j.contains("shape")) comp.shape = static_cast<ColliderShape>(j["shape"].get<uint8_t>());
					DeserializeVec3(j, "size", comp.size);
					if (j.contains("radius")) comp.radius = j["radius"].get<float>();
					if (j.contains("height")) comp.height = j["height"].get<float>();
					if (j.contains("isTrigger")) comp.isTrigger = j["isTrigger"].get<bool>();
					if (j.contains("layer")) comp.layer = j["layer"].get<uint8_t>();
				})
			.registerSelf();

		// 8. ParticleEmitterComponent
		ComponentBuilder<ParticleEmitterComponent>("ParticleEmitterComponent")
			.field("maxParticles", &ParticleEmitterComponent::maxParticles)
			.field("emissionRate", &ParticleEmitterComponent::emissionRate)
			.field("lifetime", &ParticleEmitterComponent::lifetime)
			.field("startVelocity", &ParticleEmitterComponent::startVelocity)
			.field("startColor", &ParticleEmitterComponent::startColor, { false, 0, 0, "", true })
			.field("endColor", &ParticleEmitterComponent::endColor, { false, 0, 0, "", true })
			.serialize(
				[](const Scene& scene, entt::entity e, void* jsonNode) {
					auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
					const auto& comp = scene.getRegistry().get<ParticleEmitterComponent>(e);
					j["maxParticles"] = comp.maxParticles;
					j["emissionRate"] = comp.emissionRate;
					j["lifetime"] = comp.lifetime;
					SerializeVec3(j, "startVelocity", comp.startVelocity);
					SerializeColor(j, "startColor", comp.startColor);
					SerializeColor(j, "endColor", comp.endColor);
				},
				[](Scene& scene, entt::entity e, const void* jsonNode) {
					const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
					auto& comp = scene.getRegistry().get_or_emplace<ParticleEmitterComponent>(e);
					if (j.contains("maxParticles")) comp.maxParticles = j["maxParticles"].get<int>();
					if (j.contains("emissionRate")) comp.emissionRate = j["emissionRate"].get<float>();
					if (j.contains("lifetime")) comp.lifetime = j["lifetime"].get<float>();
					DeserializeVec3(j, "startVelocity", comp.startVelocity);
					DeserializeColor(j, "startColor", comp.startColor);
					DeserializeColor(j, "endColor", comp.endColor);
				})
			.registerSelf();

		// 9. ScriptsComponent (Custom serialization)
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
			desc.getComponentRaw = [](Scene& scene, entt::entity e) -> void* {
				return scene.getRegistry().try_get<ScriptsComponent>(e);
			};
			desc.getComponentRawConst = [](const Scene& scene, entt::entity e) -> const void* {
				return scene.getRegistry().try_get<ScriptsComponent>(e);
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
					for (const auto& ms : comp.missingScripts)
					{
						nlohmann::ordered_json mItem;
						mItem["name"] = ms.scriptName;
						mItem["rawJson"] = ms.rawJson;
						missingArray.push_back(mItem);
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
						MissingScriptData mData;
						mData.scriptName = item["name"].get<std::string>();
						mData.rawJson = item.value("rawJson", "");
						comp.missingScripts.push_back(std::move(mData));
					}
				}
			};
			registerComponent(std::move(desc));
		}

		// 10. PrefabInstanceComponent
		ComponentBuilder<PrefabInstanceComponent>("PrefabInstanceComponent")
			.field("prefabGuid", &PrefabInstanceComponent::prefabGuid)
			.field("prefabPath", &PrefabInstanceComponent::prefabPath)
			.serialize(
				[](const Scene& scene, entt::entity e, void* jsonNode) {
					auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
					const auto& comp = scene.getRegistry().get<PrefabInstanceComponent>(e);
					j["prefabGuid"] = comp.prefabGuid;
					j["prefabPath"] = comp.prefabPath;
				},
				[](Scene& scene, entt::entity e, const void* jsonNode) {
					const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
					auto& comp = scene.getRegistry().get_or_emplace<PrefabInstanceComponent>(e);
					if (j.contains("prefabGuid")) comp.prefabGuid = j["prefabGuid"].get<uint64_t>();
					if (j.contains("prefabPath")) comp.prefabPath = j["prefabPath"].get<std::string>();
				})
			.registerSelf();

		// 11. RigidbodyRuntimeState (Runtime only)
		ComponentBuilder<RigidbodyRuntimeState>("RigidbodyRuntimeState", false).registerSelf();

		// 12. ParticleRuntimeState (Runtime only)
		ComponentBuilder<ParticleRuntimeState>("ParticleRuntimeState", false).registerSelf();

		// 13. EnvironmentComponent
		ComponentBuilder<EnvironmentComponent>("EnvironmentComponent")
			.enumField("skyType", &EnvironmentComponent::skyType, { "Procedural Gradient", "Skybox Texture", "Custom Color" })
			.field("skyColor", &EnvironmentComponent::skyColor, { false, 0, 0, "", true })
			.field("horizonColor", &EnvironmentComponent::horizonColor, { false, 0, 0, "", true })
			.field("groundColor", &EnvironmentComponent::groundColor, { false, 0, 0, "", true })
			.field("ambientColor", &EnvironmentComponent::ambientColor, { false, 0, 0, "", true })
			.field("ambientEnergy", &EnvironmentComponent::ambientEnergy, { true, 0.0f, 5.0f })
			.field("skyboxTexturePath", &EnvironmentComponent::skyboxTexturePath)
			.serialize(
				[](const Scene& scene, entt::entity e, void* jsonNode) {
					auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
					const auto& comp = scene.getRegistry().get<EnvironmentComponent>(e);
					j["skyType"] = static_cast<uint8_t>(comp.skyType);
					SerializeColor(j, "skyColor", comp.skyColor);
					SerializeColor(j, "horizonColor", comp.horizonColor);
					SerializeColor(j, "groundColor", comp.groundColor);
					SerializeColor(j, "ambientColor", comp.ambientColor);
					j["ambientEnergy"] = comp.ambientEnergy;
					j["skyboxTexturePath"] = comp.skyboxTexturePath;
				},
				[](Scene& scene, entt::entity e, const void* jsonNode) {
					const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
					auto& comp = scene.getRegistry().get_or_emplace<EnvironmentComponent>(e);
					if (j.contains("skyType")) comp.skyType = static_cast<SkyType>(j["skyType"].get<uint8_t>());
					DeserializeColor(j, "skyColor", comp.skyColor);
					DeserializeColor(j, "horizonColor", comp.horizonColor);
					DeserializeColor(j, "groundColor", comp.groundColor);
					DeserializeColor(j, "ambientColor", comp.ambientColor);
					if (j.contains("ambientEnergy")) comp.ambientEnergy = j["ambientEnergy"].get<float>();
					if (j.contains("skyboxTexturePath")) comp.skyboxTexturePath = j["skyboxTexturePath"].get<std::string>();
				})
			.registerSelf();

		// 14. PostProcessingComponent
		ComponentBuilder<PostProcessingComponent>("PostProcessingComponent")
			.field("enableTonemapping", &PostProcessingComponent::enableTonemapping)
			.enumField("tonemapper", &PostProcessingComponent::tonemapper, { "ACES Filmic", "Reinhard", "Linear" })
			.field("exposure", &PostProcessingComponent::exposure, { true, 0.01f, 10.0f })
			.field("gamma", &PostProcessingComponent::gamma, { true, 1.0f, 3.0f })
			.field("enableFXAA", &PostProcessingComponent::enableFXAA)
			.field("fxaaSubpix", &PostProcessingComponent::fxaaSubpix, { true, 0.0f, 1.0f })
			.field("fxaaEdgeThreshold", &PostProcessingComponent::fxaaEdgeThreshold, { true, 0.063f, 0.333f })
			.field("fxaaEdgeThresholdMin", &PostProcessingComponent::fxaaEdgeThresholdMin, { true, 0.0312f, 0.0833f })
			.field("enableSSAO", &PostProcessingComponent::enableSSAO)
			.field("ssaoRadius", &PostProcessingComponent::ssaoRadius, { true, 0.1f, 3.0f })
			.field("ssaoBias", &PostProcessingComponent::ssaoBias, { true, 0.001f, 0.1f })
			.field("ssaoIntensity", &PostProcessingComponent::ssaoIntensity, { true, 0.0f, 5.0f })
			.field("enableBloom", &PostProcessingComponent::enableBloom)
			.field("bloomThreshold", &PostProcessingComponent::bloomThreshold, { true, 0.1f, 5.0f })
			.field("bloomIntensity", &PostProcessingComponent::bloomIntensity, { true, 0.0f, 1.0f })
			.serialize(
				[](const Scene& scene, entt::entity e, void* jsonNode) {
					auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
					const auto& comp = scene.getRegistry().get<PostProcessingComponent>(e);
					j["enableTonemapping"] = comp.enableTonemapping;
					j["tonemapper"] = static_cast<uint8_t>(comp.tonemapper);
					j["exposure"] = comp.exposure;
					j["gamma"] = comp.gamma;
					j["enableFXAA"] = comp.enableFXAA;
					j["fxaaSubpix"] = comp.fxaaSubpix;
					j["fxaaEdgeThreshold"] = comp.fxaaEdgeThreshold;
					j["fxaaEdgeThresholdMin"] = comp.fxaaEdgeThresholdMin;
					j["enableSSAO"] = comp.enableSSAO;
					j["ssaoRadius"] = comp.ssaoRadius;
					j["ssaoBias"] = comp.ssaoBias;
					j["ssaoIntensity"] = comp.ssaoIntensity;
					j["enableBloom"] = comp.enableBloom;
					j["bloomThreshold"] = comp.bloomThreshold;
					j["bloomIntensity"] = comp.bloomIntensity;
				},
				[](Scene& scene, entt::entity e, const void* jsonNode) {
					const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
					auto& comp = scene.getRegistry().get_or_emplace<PostProcessingComponent>(e);
					if (j.contains("enableTonemapping")) comp.enableTonemapping = j["enableTonemapping"].get<bool>();
					if (j.contains("tonemapper")) comp.tonemapper = static_cast<TonemapperType>(j["tonemapper"].get<uint8_t>());
					if (j.contains("exposure")) comp.exposure = j["exposure"].get<float>();
					if (j.contains("gamma")) comp.gamma = j["gamma"].get<float>();
					if (j.contains("enableFXAA")) comp.enableFXAA = j["enableFXAA"].get<bool>();
					if (j.contains("fxaaSubpix")) comp.fxaaSubpix = j["fxaaSubpix"].get<float>();
					if (j.contains("fxaaEdgeThreshold")) comp.fxaaEdgeThreshold = j["fxaaEdgeThreshold"].get<float>();
					if (j.contains("fxaaEdgeThresholdMin")) comp.fxaaEdgeThresholdMin = j["fxaaEdgeThresholdMin"].get<float>();
					if (j.contains("enableSSAO")) comp.enableSSAO = j["enableSSAO"].get<bool>();
					if (j.contains("ssaoRadius")) comp.ssaoRadius = j["ssaoRadius"].get<float>();
					if (j.contains("ssaoBias")) comp.ssaoBias = j["ssaoBias"].get<float>();
					if (j.contains("ssaoIntensity")) comp.ssaoIntensity = j["ssaoIntensity"].get<float>();
					if (j.contains("enableBloom")) comp.enableBloom = j["enableBloom"].get<bool>();
					if (j.contains("bloomThreshold")) comp.bloomThreshold = j["bloomThreshold"].get<float>();
					if (j.contains("bloomIntensity")) comp.bloomIntensity = j["bloomIntensity"].get<float>();
				})
			.registerSelf();
	}
}
