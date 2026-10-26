#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Scene.h"
#include <memory>

namespace Audio
{
	struct AudioSystemImpl;

	class DF_API AudioSystem
	{
	public:
		AudioSystem();
		~AudioSystem();

		AudioSystem(const AudioSystem&) = delete;
		AudioSystem& operator=(const AudioSystem&) = delete;
		AudioSystem(AudioSystem&&) noexcept;
		AudioSystem& operator=(AudioSystem&&) noexcept;

		bool init();
		void shutdown();
		void update(Core::Scene& scene, float deltaTime);
		bool isInitialized() const noexcept;

		static AudioSystem& Get();

	private:
		std::unique_ptr<AudioSystemImpl> m_impl;
	};
}

namespace Core
{
	using Audio::AudioSystem;
}
