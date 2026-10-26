#include "Reflection.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include <nlohmann/json.hpp>

namespace Core
{
	using TypeRegistry = ComponentRegistry;

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

		inline void SerializeVec2(nlohmann::ordered_json& j, const std::string& key, const Vector2& v)
		{
			j[key] = { v.x, v.y };
		}

		inline void DeserializeVec2(const nlohmann::ordered_json& j, const std::string& key, Vector2& v)
		{
			if (j.contains(key) && j[key].is_array() && j[key].size() == 2)
			{
				v = Vector2(j[key][0].get<float>(), j[key][1].get<float>());
			}
		}

		inline void attachSerializer(TypeRegistry& registry, std::string_view name,
			std::function<void(const Scene&, entt::entity, void*)> ser,
			std::function<void(Scene&, entt::entity, const void*)> deser)
		{
			if (const auto* constDesc = registry.findByName(name))
			{
				auto* desc = const_cast<ComponentDesc*>(constDesc);
				desc->serializeToJson = std::move(ser);
				desc->deserializeFromJson = std::move(deser);
			}
		}
	}

	void registerComponentSerializers(TypeRegistry& registry)
	{
		// 1. IdentityComponent
		attachSerializer(registry, "IdentityComponent",
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
			});

		// 2. TransformComponent
		attachSerializer(registry, "TransformComponent",
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
			});

		// 3. CameraComponent
		attachSerializer(registry, "CameraComponent",
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
			});

		// 4. MeshRendererComponent
		attachSerializer(registry, "MeshRendererComponent",
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
			});

		// 5. LightComponent
		attachSerializer(registry, "LightComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<LightComponent>(e);
				j["type"] = static_cast<uint8_t>(comp.type);
				SerializeColor(j, "color", comp.color);
				j["intensity"] = comp.intensity;
				j["range"] = comp.range;
				j["spotInnerAngle"] = comp.spotInnerAngle;
				j["spotOuterAngle"] = comp.spotOuterAngle;
				j["castShadows"] = comp.castShadows;
				j["shadowBias"] = comp.shadowBias;
				j["shadowNormalBias"] = comp.shadowNormalBias;
				j["shadowSoftness"] = comp.shadowSoftness;
				j["shadowDistance"] = comp.shadowDistance;
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
				if (j.contains("castShadows")) comp.castShadows = j["castShadows"].get<bool>();
				if (j.contains("shadowBias")) comp.shadowBias = j["shadowBias"].get<float>();
				if (j.contains("shadowNormalBias")) comp.shadowNormalBias = j["shadowNormalBias"].get<float>();
				if (j.contains("shadowSoftness")) comp.shadowSoftness = j["shadowSoftness"].get<float>();
				if (j.contains("shadowDistance")) comp.shadowDistance = j["shadowDistance"].get<float>();
			});

		// 6. RigidbodyComponent
		attachSerializer(registry, "RigidbodyComponent",
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
			});

		// 7. ColliderComponent
		attachSerializer(registry, "ColliderComponent",
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
			});

		// 8. ParticleEmitterComponent
		attachSerializer(registry, "ParticleEmitterComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<ParticleEmitterComponent>(e);
				j["maxParticles"] = comp.maxParticles;
				j["emissionRate"] = comp.emissionRate;
				j["lifetime"] = comp.lifetime;
				SerializeVec3(j, "startVelocity", comp.startVelocity);
				j["velocityRandomness"] = comp.velocityRandomness;
				j["gravityScale"] = comp.gravityScale;
				j["startSize"] = comp.startSize;
				j["endSize"] = comp.endSize;
				SerializeColor(j, "startColor", comp.startColor);
				SerializeColor(j, "endColor", comp.endColor);
				j["blendMode"] = static_cast<uint8_t>(comp.blendMode);
				j["simulationSpace"] = static_cast<uint8_t>(comp.simulationSpace);
				j["looping"] = comp.looping;
				j["playOnStart"] = comp.playOnStart;
				j["texturePath"] = comp.texturePath;
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<ParticleEmitterComponent>(e);
				if (j.contains("maxParticles")) comp.maxParticles = j["maxParticles"].get<int>();
				if (j.contains("emissionRate")) comp.emissionRate = j["emissionRate"].get<float>();
				if (j.contains("lifetime")) comp.lifetime = j["lifetime"].get<float>();
				DeserializeVec3(j, "startVelocity", comp.startVelocity);
				if (j.contains("velocityRandomness")) comp.velocityRandomness = j["velocityRandomness"].get<float>();
				if (j.contains("gravityScale")) comp.gravityScale = j["gravityScale"].get<float>();
				if (j.contains("startSize")) comp.startSize = j["startSize"].get<float>();
				if (j.contains("endSize")) comp.endSize = j["endSize"].get<float>();
				DeserializeColor(j, "startColor", comp.startColor);
				DeserializeColor(j, "endColor", comp.endColor);
				if (j.contains("blendMode")) comp.blendMode = static_cast<ParticleBlendMode>(j["blendMode"].get<uint8_t>());
				if (j.contains("simulationSpace")) comp.simulationSpace = static_cast<SimulationSpace>(j["simulationSpace"].get<uint8_t>());
				if (j.contains("looping")) comp.looping = j["looping"].get<bool>();
				if (j.contains("playOnStart")) comp.playOnStart = j["playOnStart"].get<bool>();
				if (j.contains("texturePath")) comp.texturePath = j["texturePath"].get<std::string>();
			});

		// 9. ScriptsComponent
		attachSerializer(registry, "ScriptsComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<ScriptsComponent>(e);
				auto scriptsArray = nlohmann::ordered_json::array();
				for (const auto& scr : comp.scripts)
				{
					nlohmann::ordered_json item;
					item["name"] = scr.scriptName;
					item["enabled"] = scr.isEnabled;
					if (scr.instance)
					{
						std::string fields = scr.instance->serializeExposedFields();
						if (!fields.empty() && fields != "{}")
						{
							item["fields"] = fields;
						}
					}
					else if (!scr.serializedFieldsJson.empty())
					{
						item["fields"] = scr.serializedFieldsJson;
					}
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
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
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
						std::string fields = item.value("fields", "");
						comp.scripts.push_back(ScriptInstance{ name, nullptr, enabled, false, fields });
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
			});

		// 10. PrefabInstanceComponent
		attachSerializer(registry, "PrefabInstanceComponent",
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
			});

		// 13. EnvironmentComponent
		attachSerializer(registry, "EnvironmentComponent",
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
				j["enableFog"] = comp.enableFog;
				SerializeColor(j, "fogColor", comp.fogColor);
				j["fogDensity"] = comp.fogDensity;
				j["fogHeightFalloff"] = comp.fogHeightFalloff;
				j["fogSunScattering"] = comp.fogSunScattering;
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
				if (j.contains("enableFog")) comp.enableFog = j["enableFog"].get<bool>();
				DeserializeColor(j, "fogColor", comp.fogColor);
				if (j.contains("fogDensity")) comp.fogDensity = j["fogDensity"].get<float>();
				if (j.contains("fogHeightFalloff")) comp.fogHeightFalloff = j["fogHeightFalloff"].get<float>();
				if (j.contains("fogSunScattering")) comp.fogSunScattering = j["fogSunScattering"].get<float>();
			});

		// 14. PostProcessingComponent
		attachSerializer(registry, "PostProcessingComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<PostProcessingComponent>(e);
				j["renderPipeline"] = comp.renderPipeline;
				j["enableShadows"] = comp.enableShadows;
				j["enablePCSS"] = comp.enablePCSS;
				j["enableContactShadows"] = comp.enableContactShadows;
				j["enableTessellation"] = comp.enableTessellation;
				j["tessellationFactor"] = comp.tessellationFactor;
				j["meshLodBias"] = comp.meshLodBias;
				j["enableTonemapping"] = comp.enableTonemapping;
				j["tonemapper"] = static_cast<uint8_t>(comp.tonemapper);
				j["exposure"] = comp.exposure;
				j["gamma"] = comp.gamma;
				j["enableAutoExposure"] = comp.enableAutoExposure;
				j["autoExposureMinEV"] = comp.autoExposureMinEV;
				j["autoExposureMaxEV"] = comp.autoExposureMaxEV;
				j["autoExposureSpeed"] = comp.autoExposureSpeed;
				j["contrast"] = comp.contrast;
				j["saturation"] = comp.saturation;
				j["vignetteIntensity"] = comp.vignetteIntensity;
				j["colorTemperature"] = comp.colorTemperature;
				j["enableBoxProjectedIBL"] = comp.enableBoxProjectedIBL;
				SerializeVec3(j, "iblBoxCenter", comp.iblBoxCenter);
				SerializeVec3(j, "iblBoxExtents", comp.iblBoxExtents);
				j["enableFXAA"] = comp.enableFXAA;
				j["fxaaSubpix"] = comp.fxaaSubpix;
				j["fxaaEdgeThreshold"] = comp.fxaaEdgeThreshold;
				j["fxaaEdgeThresholdMin"] = comp.fxaaEdgeThresholdMin;
				j["casSharpening"] = comp.casSharpening;
				j["enableSSAO"] = comp.enableSSAO;
				j["ssaoRadius"] = comp.ssaoRadius;
				j["ssaoBias"] = comp.ssaoBias;
				j["ssaoIntensity"] = comp.ssaoIntensity;
				j["ssaoDirectLightInfluence"] = comp.ssaoDirectLightInfluence;
				j["enableBloom"] = comp.enableBloom;
				j["bloomThreshold"] = comp.bloomThreshold;
				j["bloomIntensity"] = comp.bloomIntensity;
				j["enableSSR"] = comp.enableSSR;
				j["ssrMaxDistance"] = comp.ssrMaxDistance;
				j["ssrThickness"] = comp.ssrThickness;
				j["ssrRoughnessCutoff"] = comp.ssrRoughnessCutoff;
				j["ssrIntensity"] = comp.ssrIntensity;
				j["ssrStepCount"] = comp.ssrStepCount;
				j["enableSSS"] = comp.enableSSS;
				j["sssRadius"] = comp.sssRadius;
				j["sssMaxDepthDiff"] = comp.sssMaxDepthDiff;
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<PostProcessingComponent>(e);
				if (j.contains("renderPipeline")) comp.renderPipeline = j["renderPipeline"].get<uint8_t>();
				if (j.contains("enableShadows")) comp.enableShadows = j["enableShadows"].get<bool>();
				if (j.contains("enablePCSS")) comp.enablePCSS = j["enablePCSS"].get<bool>();
				if (j.contains("enableContactShadows")) comp.enableContactShadows = j["enableContactShadows"].get<bool>();
				if (j.contains("enableTessellation")) comp.enableTessellation = j["enableTessellation"].get<bool>();
				if (j.contains("tessellationFactor")) comp.tessellationFactor = j["tessellationFactor"].get<float>();
				if (j.contains("meshLodBias")) comp.meshLodBias = j["meshLodBias"].get<int32_t>();
				if (j.contains("enableTonemapping")) comp.enableTonemapping = j["enableTonemapping"].get<bool>();
				if (j.contains("tonemapper")) comp.tonemapper = static_cast<TonemapperType>(j["tonemapper"].get<uint8_t>());
				if (j.contains("exposure")) comp.exposure = j["exposure"].get<float>();
				if (j.contains("gamma")) comp.gamma = j["gamma"].get<float>();
				if (j.contains("enableAutoExposure")) comp.enableAutoExposure = j["enableAutoExposure"].get<bool>();
				if (j.contains("autoExposureMinEV")) comp.autoExposureMinEV = j["autoExposureMinEV"].get<float>();
				if (j.contains("autoExposureMaxEV")) comp.autoExposureMaxEV = j["autoExposureMaxEV"].get<float>();
				if (j.contains("autoExposureSpeed")) comp.autoExposureSpeed = j["autoExposureSpeed"].get<float>();
				if (j.contains("contrast")) comp.contrast = j["contrast"].get<float>();
				if (j.contains("saturation")) comp.saturation = j["saturation"].get<float>();
				if (j.contains("vignetteIntensity")) comp.vignetteIntensity = j["vignetteIntensity"].get<float>();
				if (j.contains("colorTemperature")) comp.colorTemperature = j["colorTemperature"].get<float>();
				if (j.contains("enableBoxProjectedIBL")) comp.enableBoxProjectedIBL = j["enableBoxProjectedIBL"].get<bool>();
				DeserializeVec3(j, "iblBoxCenter", comp.iblBoxCenter);
				DeserializeVec3(j, "iblBoxExtents", comp.iblBoxExtents);
				if (j.contains("enableFXAA")) comp.enableFXAA = j["enableFXAA"].get<bool>();
				if (j.contains("fxaaSubpix")) comp.fxaaSubpix = j["fxaaSubpix"].get<float>();
				if (j.contains("fxaaEdgeThreshold")) comp.fxaaEdgeThreshold = j["fxaaEdgeThreshold"].get<float>();
				if (j.contains("fxaaEdgeThresholdMin")) comp.fxaaEdgeThresholdMin = j["fxaaEdgeThresholdMin"].get<float>();
				if (j.contains("casSharpening")) comp.casSharpening = j["casSharpening"].get<float>();
				if (j.contains("enableSSAO")) comp.enableSSAO = j["enableSSAO"].get<bool>();
				if (j.contains("ssaoRadius")) comp.ssaoRadius = j["ssaoRadius"].get<float>();
				if (j.contains("ssaoBias")) comp.ssaoBias = j["ssaoBias"].get<float>();
				if (j.contains("ssaoIntensity")) comp.ssaoIntensity = j["ssaoIntensity"].get<float>();
				if (j.contains("ssaoDirectLightInfluence")) comp.ssaoDirectLightInfluence = j["ssaoDirectLightInfluence"].get<float>();
				if (j.contains("enableBloom")) comp.enableBloom = j["enableBloom"].get<bool>();
				if (j.contains("bloomThreshold")) comp.bloomThreshold = j["bloomThreshold"].get<float>();
				if (j.contains("bloomIntensity")) comp.bloomIntensity = j["bloomIntensity"].get<float>();
				if (j.contains("enableSSR")) comp.enableSSR = j["enableSSR"].get<bool>();
				if (j.contains("ssrMaxDistance")) comp.ssrMaxDistance = j["ssrMaxDistance"].get<float>();
				if (j.contains("ssrThickness")) comp.ssrThickness = j["ssrThickness"].get<float>();
				if (j.contains("ssrRoughnessCutoff")) comp.ssrRoughnessCutoff = j["ssrRoughnessCutoff"].get<float>();
				if (j.contains("ssrIntensity")) comp.ssrIntensity = j["ssrIntensity"].get<float>();
				if (j.contains("ssrStepCount")) comp.ssrStepCount = j["ssrStepCount"].get<uint32_t>();
				if (j.contains("enableSSS")) comp.enableSSS = j["enableSSS"].get<bool>();
				if (j.contains("sssRadius")) comp.sssRadius = j["sssRadius"].get<float>();
				if (j.contains("sssMaxDepthDiff")) comp.sssMaxDepthDiff = j["sssMaxDepthDiff"].get<float>();
			});

		// 15. TerrainComponent
		attachSerializer(registry, "TerrainComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<TerrainComponent>(e);
				j["resolution"] = comp.resolution;
				j["worldSize"] = comp.worldSize;
				j["maxHeight"] = comp.maxHeight;
				j["seed"] = comp.seed;
				j["octaves"] = comp.octaves;
				j["frequency"] = comp.frequency;
				j["persistence"] = comp.persistence;
				j["lacunarity"] = comp.lacunarity;
				j["ridgeWeight"] = comp.ridgeWeight;
				j["isIsland"] = comp.isIsland;
				j["islandRadius"] = comp.islandRadius;
				j["coastlineFalloff"] = comp.coastlineFalloff;
				j["seaFloorDepth"] = comp.seaFloorDepth;
				j["sandHeight"] = comp.sandHeight;
				j["cliffSlopeThreshold"] = comp.cliffSlopeThreshold;
				j["snowHeight"] = comp.snowHeight;
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<TerrainComponent>(e);
				if (j.contains("resolution")) comp.resolution = j["resolution"].get<uint32_t>();
				if (j.contains("worldSize")) comp.worldSize = j["worldSize"].get<float>();
				if (j.contains("maxHeight")) comp.maxHeight = j["maxHeight"].get<float>();
				if (j.contains("seed")) comp.seed = j["seed"].get<uint32_t>();
				if (j.contains("octaves")) comp.octaves = j["octaves"].get<uint32_t>();
				if (j.contains("frequency")) comp.frequency = j["frequency"].get<float>();
				if (j.contains("persistence")) comp.persistence = j["persistence"].get<float>();
				if (j.contains("lacunarity")) comp.lacunarity = j["lacunarity"].get<float>();
				if (j.contains("ridgeWeight")) comp.ridgeWeight = j["ridgeWeight"].get<float>();
				if (j.contains("isIsland")) comp.isIsland = j["isIsland"].get<bool>();
				if (j.contains("islandRadius")) comp.islandRadius = j["islandRadius"].get<float>();
				if (j.contains("coastlineFalloff")) comp.coastlineFalloff = j["coastlineFalloff"].get<float>();
				if (j.contains("seaFloorDepth")) comp.seaFloorDepth = j["seaFloorDepth"].get<float>();
				if (j.contains("sandHeight")) comp.sandHeight = j["sandHeight"].get<float>();
				if (j.contains("cliffSlopeThreshold")) comp.cliffSlopeThreshold = j["cliffSlopeThreshold"].get<float>();
				if (j.contains("snowHeight")) comp.snowHeight = j["snowHeight"].get<float>();
			});

		// 16. FoliageComponent
		attachSerializer(registry, "FoliageComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<FoliageComponent>(e);
				j["bladeCount"] = comp.bladeCount;
				SerializeVec2(j, "patchDimensions", comp.patchDimensions);
				j["bladeHeightMin"] = comp.bladeHeightMin;
				j["bladeHeightMax"] = comp.bladeHeightMax;
				j["bladeWidth"] = comp.bladeWidth;
				j["windSpeed"] = comp.windSpeed;
				j["windStrength"] = comp.windStrength;
				j["windFrequency"] = comp.windFrequency;
				j["maxDrawDistance"] = comp.maxDrawDistance;
				SerializeColor(j, "rootColor", comp.rootColor);
				SerializeColor(j, "tipColor", comp.tipColor);
				j["snapToTerrain"] = comp.snapToTerrain;
				j["minAltitude"] = comp.minAltitude;
				j["maxAltitude"] = comp.maxAltitude;
				j["maxSlope"] = comp.maxSlope;
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<FoliageComponent>(e);
				if (j.contains("bladeCount")) comp.bladeCount = j["bladeCount"].get<uint32_t>();
				DeserializeVec2(j, "patchDimensions", comp.patchDimensions);
				if (j.contains("bladeHeightMin")) comp.bladeHeightMin = j["bladeHeightMin"].get<float>();
				if (j.contains("bladeHeightMax")) comp.bladeHeightMax = j["bladeHeightMax"].get<float>();
				if (j.contains("bladeWidth")) comp.bladeWidth = j["bladeWidth"].get<float>();
				if (j.contains("windSpeed")) comp.windSpeed = j["windSpeed"].get<float>();
				if (j.contains("windStrength")) comp.windStrength = j["windStrength"].get<float>();
				if (j.contains("windFrequency")) comp.windFrequency = j["windFrequency"].get<float>();
				if (j.contains("maxDrawDistance")) comp.maxDrawDistance = j["maxDrawDistance"].get<float>();
				DeserializeColor(j, "rootColor", comp.rootColor);
				DeserializeColor(j, "tipColor", comp.tipColor);
				if (j.contains("snapToTerrain")) comp.snapToTerrain = j["snapToTerrain"].get<bool>();
				if (j.contains("minAltitude")) comp.minAltitude = j["minAltitude"].get<float>();
				if (j.contains("maxAltitude")) comp.maxAltitude = j["maxAltitude"].get<float>();
				if (j.contains("maxSlope")) comp.maxSlope = j["maxSlope"].get<float>();
			});

		// 17. WaterComponent
		attachSerializer(registry, "WaterComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<WaterComponent>(e);
				j["waterLevel"] = comp.waterLevel;
				j["planeSize"] = comp.planeSize;
				j["gridResolution"] = comp.gridResolution;
				j["waveAmplitude"] = comp.waveAmplitude;
				j["waveFrequency"] = comp.waveFrequency;
				j["waveSpeed"] = comp.waveSpeed;
				j["waveSteepness"] = comp.waveSteepness;
				SerializeColor(j, "shallowColor", comp.shallowColor);
				SerializeColor(j, "deepColor", comp.deepColor);
				SerializeColor(j, "foamColor", comp.foamColor);
				j["depthAbsorption"] = comp.depthAbsorption;
				j["foamDepthThreshold"] = comp.foamDepthThreshold;
				j["refractionStrength"] = comp.refractionStrength;
				j["causticsIntensity"] = comp.causticsIntensity;
				j["buoyancyDensity"] = comp.buoyancyDensity;
				j["fluidDrag"] = comp.fluidDrag;
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<WaterComponent>(e);
				if (j.contains("waterLevel")) comp.waterLevel = j["waterLevel"].get<float>();
				if (j.contains("planeSize")) comp.planeSize = j["planeSize"].get<float>();
				if (j.contains("gridResolution")) comp.gridResolution = j["gridResolution"].get<uint32_t>();
				if (j.contains("waveAmplitude")) comp.waveAmplitude = j["waveAmplitude"].get<float>();
				if (j.contains("waveFrequency")) comp.waveFrequency = j["waveFrequency"].get<float>();
				if (j.contains("waveSpeed")) comp.waveSpeed = j["waveSpeed"].get<float>();
				if (j.contains("waveSteepness")) comp.waveSteepness = j["waveSteepness"].get<float>();
				DeserializeColor(j, "shallowColor", comp.shallowColor);
				DeserializeColor(j, "deepColor", comp.deepColor);
				DeserializeColor(j, "foamColor", comp.foamColor);
				if (j.contains("depthAbsorption")) comp.depthAbsorption = j["depthAbsorption"].get<float>();
				if (j.contains("foamDepthThreshold")) comp.foamDepthThreshold = j["foamDepthThreshold"].get<float>();
				if (j.contains("refractionStrength")) comp.refractionStrength = j["refractionStrength"].get<float>();
				if (j.contains("causticsIntensity")) comp.causticsIntensity = j["causticsIntensity"].get<float>();
				if (j.contains("buoyancyDensity")) comp.buoyancyDensity = j["buoyancyDensity"].get<float>();
				if (j.contains("fluidDrag")) comp.fluidDrag = j["fluidDrag"].get<float>();
			});

		// 18. AudioSourceComponent
		attachSerializer(registry, "AudioSourceComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<AudioSourceComponent>(e);
				j["clipPath"] = comp.clipPath;
				j["volume"] = comp.volume;
				j["pitch"] = comp.pitch;
				j["loop"] = comp.loop;
				j["spatialize3D"] = comp.spatialize3D;
				j["minDistance"] = comp.minDistance;
				j["maxDistance"] = comp.maxDistance;
				j["playOnStart"] = comp.playOnStart;
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<AudioSourceComponent>(e);
				if (j.contains("clipPath")) comp.clipPath = j["clipPath"].get<std::string>();
				if (j.contains("volume")) comp.volume = j["volume"].get<float>();
				if (j.contains("pitch")) comp.pitch = j["pitch"].get<float>();
				if (j.contains("loop")) comp.loop = j["loop"].get<bool>();
				if (j.contains("spatialize3D")) comp.spatialize3D = j["spatialize3D"].get<bool>();
				if (j.contains("minDistance")) comp.minDistance = j["minDistance"].get<float>();
				if (j.contains("maxDistance")) comp.maxDistance = j["maxDistance"].get<float>();
				if (j.contains("playOnStart")) comp.playOnStart = j["playOnStart"].get<bool>();
			});

		// 19. AudioListenerComponent
		attachSerializer(registry, "AudioListenerComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<AudioListenerComponent>(e);
				j["active"] = comp.active;
				j["masterVolume"] = comp.masterVolume;
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<AudioListenerComponent>(e);
				if (j.contains("active")) comp.active = j["active"].get<bool>();
				if (j.contains("masterVolume")) comp.masterVolume = j["masterVolume"].get<float>();
			});

		// 20. HUDTelemetryComponent
		attachSerializer(registry, "HUDTelemetryComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<HUDTelemetryComponent>(e);
				j["showOverlay"] = comp.showOverlay;
				j["showHotkeys"] = comp.showHotkeys;
				j["customBannerText"] = comp.customBannerText;
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<HUDTelemetryComponent>(e);
				if (j.contains("showOverlay")) comp.showOverlay = j["showOverlay"].get<bool>();
				if (j.contains("showHotkeys")) comp.showHotkeys = j["showHotkeys"].get<bool>();
				if (j.contains("customBannerText")) comp.customBannerText = j["customBannerText"].get<std::string>();
			});

		// 21. ReflectionProbeComponent
		attachSerializer(registry, "ReflectionProbeComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<ReflectionProbeComponent>(e);
				SerializeVec3(j, "boxExtents", comp.boxExtents);
				j["intensity"] = comp.intensity;
				j["boxProjection"] = comp.boxProjection;
				j["captureSceneGeometry"] = comp.captureSceneGeometry;
				j["resolution"] = comp.resolution;
				j["bakedCubemapPath"] = comp.bakedCubemapPath;
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<ReflectionProbeComponent>(e);
				DeserializeVec3(j, "boxExtents", comp.boxExtents);
				if (j.contains("intensity")) comp.intensity = j["intensity"].get<float>();
				if (j.contains("boxProjection")) comp.boxProjection = j["boxProjection"].get<bool>();
				if (j.contains("captureSceneGeometry")) comp.captureSceneGeometry = j["captureSceneGeometry"].get<bool>();
				if (j.contains("resolution")) comp.resolution = j["resolution"].get<uint32_t>();
				if (j.contains("bakedCubemapPath")) comp.bakedCubemapPath = j["bakedCubemapPath"].get<std::string>();
			});

		// 22. DecalComponent
		attachSerializer(registry, "DecalComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<DecalComponent>(e);
				j["albedoTexturePath"] = comp.albedoTexturePath;
				j["normalTexturePath"] = comp.normalTexturePath;
				SerializeColor(j, "color", comp.color);
				j["normalStrength"] = comp.normalStrength;
				j["roughness"] = comp.roughness;
				j["fadeDistance"] = comp.fadeDistance;
				SerializeVec3(j, "extents", comp.extents);
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<DecalComponent>(e);
				if (j.contains("albedoTexturePath")) comp.albedoTexturePath = j["albedoTexturePath"].get<std::string>();
				if (j.contains("normalTexturePath")) comp.normalTexturePath = j["normalTexturePath"].get<std::string>();
				DeserializeColor(j, "color", comp.color);
				if (j.contains("normalStrength")) comp.normalStrength = j["normalStrength"].get<float>();
				if (j.contains("roughness")) comp.roughness = j["roughness"].get<float>();
				if (j.contains("fadeDistance")) comp.fadeDistance = j["fadeDistance"].get<float>();
				DeserializeVec3(j, "extents", comp.extents);
			});

		// 23. SplineComponent
		attachSerializer(registry, "SplineComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<SplineComponent>(e);
				auto pts = nlohmann::ordered_json::array();
				for (const auto& pt : comp.controlPoints)
				{
					pts.push_back({ pt.x, pt.y, pt.z });
				}
				j["controlPoints"] = pts;
				j["isClosedLoop"] = comp.isClosedLoop;
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<SplineComponent>(e);
				if (j.contains("controlPoints") && j["controlPoints"].is_array())
				{
					comp.controlPoints.clear();
					for (const auto& item : j["controlPoints"])
					{
						if (item.is_array() && item.size() == 3)
						{
							comp.controlPoints.emplace_back(item[0].get<float>(), item[1].get<float>(), item[2].get<float>());
						}
					}
				}
				if (j.contains("isClosedLoop")) comp.isClosedLoop = j["isClosedLoop"].get<bool>();
			});

		// 24. DDGIProbeVolumeComponent
		attachSerializer(registry, "DDGIProbeVolumeComponent",
			[](const Scene& scene, entt::entity e, void* jsonNode) {
				auto& j = *static_cast<nlohmann::ordered_json*>(jsonNode);
				const auto& comp = scene.getRegistry().get<DDGIProbeVolumeComponent>(e);
				j["enabled"] = comp.enabled;
				SerializeVec3(j, "origin", comp.origin);
				SerializeVec3(j, "probeCounts", comp.probeCounts);
				j["probeSpacing"] = comp.probeSpacing;
				j["normalBias"] = comp.normalBias;
				j["hysteresis"] = comp.hysteresis;
				j["intensity"] = comp.intensity;
				j["energyPreservation"] = comp.energyPreservation;
			},
			[](Scene& scene, entt::entity e, const void* jsonNode) {
				const auto& j = *static_cast<const nlohmann::ordered_json*>(jsonNode);
				auto& comp = scene.getRegistry().get_or_emplace<DDGIProbeVolumeComponent>(e);
				if (j.contains("enabled")) comp.enabled = j["enabled"].get<bool>();
				DeserializeVec3(j, "origin", comp.origin);
				DeserializeVec3(j, "probeCounts", comp.probeCounts);
				if (j.contains("probeSpacing")) comp.probeSpacing = j["probeSpacing"].get<float>();
				if (j.contains("normalBias")) comp.normalBias = j["normalBias"].get<float>();
				if (j.contains("hysteresis")) comp.hysteresis = j["hysteresis"].get<float>();
				if (j.contains("intensity")) comp.intensity = j["intensity"].get<float>();
				if (j.contains("energyPreservation")) comp.energyPreservation = j["energyPreservation"].get<float>();
			});
	}
}
