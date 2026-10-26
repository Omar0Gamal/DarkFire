#include "Engine/Audio/AudioSystem.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/VFS/AssetPathResolver.h"

#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <filesystem>
#include <algorithm>

#if defined(_MSC_VER)
	#pragma warning(push, 0)
	#pragma warning(disable: 4100 4244 4245 4267 4310 4701 4706 4996 4189)
#endif

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

namespace Audio
{
	enum class SoundSourceType
	{
		Unknown,
		ProceduralOceanWaves,
		ProceduralWind,
		File
	};

	struct ActiveSound
	{
		SoundSourceType type = SoundSourceType::Unknown;
		std::string clipPath;
		ma_sound sound{};
		ma_noise noise{};
		bool isNoiseInit = false;
		bool isSoundInit = false;
		bool isPlaying = false;
		float accumulatedTime = 0.0f;
		float currentVolume = 0.8f;
		float currentPitch = 1.0f;
		Core::Vector3 prevPosition = Core::Vector3(0.0f, 0.0f, 0.0f);
		bool hasPrevPos = false;
	};

	struct AudioSystemImpl
	{
		ma_engine engine{};
		bool isInitialized = false;
		bool noDevice = false;
		float globalTime = 0.0f;

		ma_sound_group soundGroups[static_cast<size_t>(Core::AudioBus::Count)]{};
		float busVolumes[static_cast<size_t>(Core::AudioBus::Count)]{ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
		bool soundGroupsInitialized = false;

		Core::Vector3 prevListenerPos = Core::Vector3(0.0f, 0.0f, 0.0f);
		bool hasPrevListenerPos = false;

		std::unordered_map<uint64_t, std::unique_ptr<ActiveSound>> activeSounds;

		bool init()
		{
			if (isInitialized) return true;

			// 1. Try standard playback hardware device
			ma_result result = ma_engine_init(nullptr, &engine);
			if (result == MA_SUCCESS)
			{
				isInitialized = true;
				noDevice = false;
				Core::Log::Get().info("Audio", "AudioSystem initialized with hardware playback device.");
			}
			else
			{
				// 2. Headless / CI fallback: initialize with noDevice = true
				ma_engine_config config = ma_engine_config_init();
				config.noDevice = MA_TRUE;
				config.channels = 2;
				config.sampleRate = 48000;
				result = ma_engine_init(&config, &engine);
				if (result == MA_SUCCESS)
				{
					isInitialized = true;
					noDevice = true;
					Core::Log::Get().info("Audio", "AudioSystem initialized with no-device fallback (headless/CI).");
				}
				else
				{
					Core::Log::Get().warn("Audio", "Failed to initialize miniaudio engine: code = {}", static_cast<int>(result));
					isInitialized = false;
					return false;
				}
			}

			// Initialize submix buses: Master is root; SFX, Ambient, Engine, Music route to Master
			if (ma_sound_group_init(&engine, 0, nullptr, &soundGroups[static_cast<size_t>(Core::AudioBus::Master)]) == MA_SUCCESS)
			{
				soundGroupsInitialized = true;
				for (size_t b = 1; b < static_cast<size_t>(Core::AudioBus::Count); ++b)
				{
					ma_sound_group_init(&engine, 0, &soundGroups[static_cast<size_t>(Core::AudioBus::Master)], &soundGroups[b]);
				}
				for (size_t b = 0; b < static_cast<size_t>(Core::AudioBus::Count); ++b)
				{
					ma_sound_group_set_volume(&soundGroups[b], busVolumes[b]);
				}
			}

			hasPrevListenerPos = false;
			return true;
		}

		void shutdown()
		{
			if (!isInitialized) return;

			for (auto& [id, s] : activeSounds)
			{
				cleanupSound(*s);
			}
			activeSounds.clear();

			if (soundGroupsInitialized)
			{
				for (size_t b = 0; b < static_cast<size_t>(Core::AudioBus::Count); ++b)
				{
					ma_sound_group_uninit(&soundGroups[b]);
				}
				soundGroupsInitialized = false;
			}

			ma_engine_uninit(&engine);
			isInitialized = false;
			hasPrevListenerPos = false;
			Core::Log::Get().info("Audio", "AudioSystem shut down cleanly.");
		}

		void setBusVolume(Core::AudioBus bus, float volume)
		{
			size_t idx = static_cast<size_t>(bus);
			if (idx < static_cast<size_t>(Core::AudioBus::Count))
			{
				busVolumes[idx] = volume;
				if (soundGroupsInitialized)
				{
					ma_sound_group_set_volume(&soundGroups[idx], volume);
				}
			}
		}

		float getBusVolume(Core::AudioBus bus) const
		{
			size_t idx = static_cast<size_t>(bus);
			if (idx < static_cast<size_t>(Core::AudioBus::Count))
			{
				return busVolumes[idx];
			}
			return 1.0f;
		}

		void cleanupSound(ActiveSound& s)
		{
			if (s.isSoundInit)
			{
				ma_sound_uninit(&s.sound);
				s.isSoundInit = false;
			}
			if (s.isNoiseInit)
			{
				ma_noise_uninit(&s.noise, nullptr);
				s.isNoiseInit = false;
			}
			s.isPlaying = false;
			s.hasPrevPos = false;
		}

		bool setupSound(ActiveSound& s, const Core::AudioSourceComponent& comp)
		{
			cleanupSound(s);
			s.clipPath = comp.clipPath;
			s.accumulatedTime = 0.0f;
			s.currentVolume = comp.volume;
			s.currentPitch = comp.pitch;

			ma_uint32 flags = 0;
			if (!comp.spatialize3D)
			{
				flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
			}

			size_t busIdx = static_cast<size_t>(comp.bus);
			if (busIdx >= static_cast<size_t>(Core::AudioBus::Count))
			{
				busIdx = static_cast<size_t>(Core::AudioBus::SFX);
			}
			ma_sound_group* parentGroup = soundGroupsInitialized ? &soundGroups[busIdx] : nullptr;

			if (comp.clipPath == "builtin:ocean_waves")
			{
				s.type = SoundSourceType::ProceduralOceanWaves;
				ma_noise_config nCfg = ma_noise_config_init(ma_format_f32, 2, ma_noise_type_pink, 1337, 0.4);
				if (ma_noise_init(&nCfg, nullptr, &s.noise) != MA_SUCCESS)
				{
					return false;
				}
				s.isNoiseInit = true;

				if (ma_sound_init_from_data_source(&engine, reinterpret_cast<ma_data_source*>(&s.noise.ds), flags, parentGroup, &s.sound) != MA_SUCCESS)
				{
					cleanupSound(s);
					return false;
				}
				s.isSoundInit = true;
			}
			else if (comp.clipPath == "builtin:wind")
			{
				s.type = SoundSourceType::ProceduralWind;
				ma_noise_config nCfg = ma_noise_config_init(ma_format_f32, 2, ma_noise_type_brownian, 4242, 0.3);
				if (ma_noise_init(&nCfg, nullptr, &s.noise) != MA_SUCCESS)
				{
					return false;
				}
				s.isNoiseInit = true;

				if (ma_sound_init_from_data_source(&engine, reinterpret_cast<ma_data_source*>(&s.noise.ds), flags, parentGroup, &s.sound) != MA_SUCCESS)
				{
					cleanupSound(s);
					return false;
				}
				s.isSoundInit = true;
			}
			else
			{
				s.type = SoundSourceType::File;
				std::string resolvedPath = comp.clipPath;
				if (!std::filesystem::exists(resolvedPath))
				{
					auto res = Core::AssetPathResolver::resolve(comp.clipPath);
					if (res.has_value() && !res->physicalPath.empty() && std::filesystem::exists(res->physicalPath))
					{
						resolvedPath = res->physicalPath.string();
					}
				}

				ma_result sRes = ma_sound_init_from_file(&engine, resolvedPath.c_str(), flags, parentGroup, nullptr, &s.sound);
				if (sRes != MA_SUCCESS)
				{
					// Fallback to procedural pink noise
					ma_noise_config nCfg = ma_noise_config_init(ma_format_f32, 2, ma_noise_type_pink, 0, 0.3);
					if (ma_noise_init(&nCfg, nullptr, &s.noise) == MA_SUCCESS)
					{
						s.isNoiseInit = true;
						if (ma_sound_init_from_data_source(&engine, reinterpret_cast<ma_data_source*>(&s.noise.ds), flags, parentGroup, &s.sound) == MA_SUCCESS)
						{
							s.isSoundInit = true;
						}
					}
				}
				else
				{
					s.isSoundInit = true;
				}
			}

			if (s.isSoundInit)
			{
				ma_sound_set_looping(&s.sound, comp.loop ? MA_TRUE : MA_FALSE);
				ma_sound_set_volume(&s.sound, comp.volume);
				ma_sound_set_pitch(&s.sound, comp.pitch);
				ma_sound_set_doppler_factor(&s.sound, comp.dopplerFactor);

				if (comp.spatialize3D)
				{
					ma_sound_set_spatialization_enabled(&s.sound, MA_TRUE);
					ma_sound_set_min_distance(&s.sound, comp.minDistance);
					ma_sound_set_max_distance(&s.sound, comp.maxDistance);
					ma_sound_set_attenuation_model(&s.sound, ma_attenuation_model_inverse);
				}
				else
				{
					ma_sound_set_spatialization_enabled(&s.sound, MA_FALSE);
				}

				if (comp.playOnStart)
				{
					ma_sound_start(&s.sound);
					s.isPlaying = true;
				}
				return true;
			}
			return false;
		}

		void update(Core::Scene& scene, float deltaTime)
		{
			if (!isInitialized) return;

			globalTime += deltaTime;

			// 1. Update Listener (AudioListenerComponent or primary CameraComponent)
			bool foundListener = false;
			auto listenerView = scene.view<Core::AudioListenerComponent, Core::TransformComponent>();
			for (auto e : listenerView)
			{
				const auto& lComp = listenerView.get<Core::AudioListenerComponent>(e);
				if (!lComp.active) continue;

				const auto& tc = listenerView.get<Core::TransformComponent>(e);
				Core::Vector3 pos = tc.position;
				Core::Vector3 fwd = tc.getForward();

				Core::Vector3 listenerVel(0.0f, 0.0f, 0.0f);
				if (hasPrevListenerPos && deltaTime > 0.0001f)
				{
					listenerVel = (pos - prevListenerPos) / deltaTime;
				}
				prevListenerPos = pos;
				hasPrevListenerPos = true;

				ma_engine_listener_set_position(&engine, 0, pos.x, pos.y, pos.z);
				ma_engine_listener_set_direction(&engine, 0, fwd.x, fwd.y, fwd.z);
				ma_engine_listener_set_velocity(&engine, 0, listenerVel.x, listenerVel.y, listenerVel.z);
				ma_engine_listener_set_world_up(&engine, 0, 0.0f, 1.0f, 0.0f);
				ma_engine_listener_set_enabled(&engine, 0, MA_TRUE);
				ma_engine_set_volume(&engine, lComp.masterVolume);

				foundListener = true;
				break;
			}

			if (!foundListener)
			{
				auto camView = scene.view<Core::CameraComponent, Core::TransformComponent>();
				for (auto e : camView)
				{
					const auto& cam = camView.get<Core::CameraComponent>(e);
					if (cam.isPrimary || !foundListener)
					{
						const auto& tc = camView.get<Core::TransformComponent>(e);
						Core::Vector3 pos = tc.position;
						Core::Vector3 fwd = tc.getForward();

						Core::Vector3 listenerVel(0.0f, 0.0f, 0.0f);
						if (hasPrevListenerPos && deltaTime > 0.0001f)
						{
							listenerVel = (pos - prevListenerPos) / deltaTime;
						}
						prevListenerPos = pos;
						hasPrevListenerPos = true;

						ma_engine_listener_set_position(&engine, 0, pos.x, pos.y, pos.z);
						ma_engine_listener_set_direction(&engine, 0, fwd.x, fwd.y, fwd.z);
						ma_engine_listener_set_velocity(&engine, 0, listenerVel.x, listenerVel.y, listenerVel.z);
						ma_engine_listener_set_world_up(&engine, 0, 0.0f, 1.0f, 0.0f);
						ma_engine_listener_set_enabled(&engine, 0, MA_TRUE);

						foundListener = true;
						if (cam.isPrimary) break;
					}
				}
			}

			// 2. Update Audio Sources
			std::unordered_set<uint64_t> currentEntities;
			auto sourceView = scene.view<Core::AudioSourceComponent>();
			for (auto handle : sourceView)
			{
				uint64_t entId = static_cast<uint64_t>(handle);
				currentEntities.insert(entId);

				const auto& comp = sourceView.get<Core::AudioSourceComponent>(handle);
				auto it = activeSounds.find(entId);
				if (it == activeSounds.end())
				{
					auto newSound = std::make_unique<ActiveSound>();
					if (setupSound(*newSound, comp))
					{
						activeSounds[entId] = std::move(newSound);
					}
				}
				else
				{
					auto& active = *it->second;
					if (active.clipPath != comp.clipPath)
					{
						setupSound(active, comp);
					}
				}

				it = activeSounds.find(entId);
				if (it != activeSounds.end() && it->second->isSoundInit)
				{
					auto& active = *it->second;
					active.accumulatedTime += deltaTime;

					// Spatial 3D positioning & velocity tracking
					if (scene.getRegistry().all_of<Core::TransformComponent>(handle))
					{
						const auto& tc = scene.getRegistry().get<Core::TransformComponent>(handle);
						ma_sound_set_position(&active.sound, tc.position.x, tc.position.y, tc.position.z);

						Core::Vector3 srcVel(0.0f, 0.0f, 0.0f);
						if (const auto* rbState = scene.getRegistry().try_get<Core::RigidbodyRuntimeState>(handle))
						{
							srcVel = rbState->linearVelocity;
						}
						else if (active.hasPrevPos && deltaTime > 0.0001f)
						{
							srcVel = (tc.position - active.prevPosition) / deltaTime;
						}
						active.prevPosition = tc.position;
						active.hasPrevPos = true;

						ma_sound_set_velocity(&active.sound, srcVel.x, srcVel.y, srcVel.z);
						ma_sound_set_doppler_factor(&active.sound, comp.dopplerFactor);
					}

					if (comp.spatialize3D)
					{
						ma_sound_set_spatialization_enabled(&active.sound, MA_TRUE);
						ma_sound_set_min_distance(&active.sound, comp.minDistance);
						ma_sound_set_max_distance(&active.sound, comp.maxDistance);
					}
					else
					{
						ma_sound_set_spatialization_enabled(&active.sound, MA_FALSE);
					}

					// Parameter smoothing (exponential slew rate)
					float vAlpha = (comp.volumeSlewRateHz > 0.0f && deltaTime > 0.0f)
						? (1.0f - std::exp(-deltaTime * comp.volumeSlewRateHz))
						: 1.0f;
					active.currentVolume += (comp.targetVolume - active.currentVolume) * std::clamp(vAlpha, 0.0f, 1.0f);

					float pAlpha = (comp.pitchSlewRateHz > 0.0f && deltaTime > 0.0f)
						? (1.0f - std::exp(-deltaTime * comp.pitchSlewRateHz))
						: 1.0f;
					active.currentPitch += (comp.targetPitch - active.currentPitch) * std::clamp(pAlpha, 0.0f, 1.0f);

					// Dynamic volume modulation for procedural wave swells & wind gusts
					float modVolume = active.currentVolume;
					if (active.type == SoundSourceType::ProceduralOceanWaves)
					{
						float swell = 0.6f + 0.4f * std::sin(active.accumulatedTime * 0.8f);
						modVolume *= swell;
					}
					else if (active.type == SoundSourceType::ProceduralWind)
					{
						float gust = 0.75f + 0.25f * std::sin(active.accumulatedTime * 1.3f) * std::cos(active.accumulatedTime * 0.5f);
						modVolume *= gust;
					}

					ma_sound_set_volume(&active.sound, modVolume);
					ma_sound_set_pitch(&active.sound, active.currentPitch);
					ma_sound_set_looping(&active.sound, comp.loop ? MA_TRUE : MA_FALSE);

					if (comp.playOnStart && !active.isPlaying)
					{
						ma_sound_start(&active.sound);
						active.isPlaying = true;
					}
				}
			}

			// Clean up removed audio source entities
			for (auto it = activeSounds.begin(); it != activeSounds.end(); )
			{
				if (currentEntities.find(it->first) == currentEntities.end())
				{
					cleanupSound(*it->second);
					it = activeSounds.erase(it);
				}
				else
				{
					++it;
				}
			}
		}
	};

	AudioSystem::AudioSystem()
		: m_impl(std::make_unique<AudioSystemImpl>())
	{
	}

	AudioSystem::~AudioSystem() = default;

	AudioSystem::AudioSystem(AudioSystem&&) noexcept = default;
	AudioSystem& AudioSystem::operator=(AudioSystem&&) noexcept = default;

	bool AudioSystem::init()
	{
		return m_impl->init();
	}

	void AudioSystem::shutdown()
	{
		m_impl->shutdown();
	}

	void AudioSystem::update(Core::Scene& scene, float deltaTime)
	{
		m_impl->update(scene, deltaTime);
	}

	bool AudioSystem::isInitialized() const noexcept
	{
		return m_impl->isInitialized;
	}

	void AudioSystem::setBusVolume(Core::AudioBus bus, float volume)
	{
		m_impl->setBusVolume(bus, volume);
	}

	float AudioSystem::getBusVolume(Core::AudioBus bus) const
	{
		return m_impl->getBusVolume(bus);
	}

	AudioSystem& AudioSystem::Get()
	{
		static AudioSystem s_instance;
		return s_instance;
	}
}
