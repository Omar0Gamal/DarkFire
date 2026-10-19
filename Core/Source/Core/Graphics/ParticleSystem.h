#pragma once
#include "Core/Game/ECS.h"

namespace Core
{
	class Game;

	class ParticleSystem
	{
	public:
		ParticleSystem(Game* game = nullptr);
		~ParticleSystem() = default;

		void update(float deltaTime, ECS* ecs);

	private:
		Game* m_game = nullptr;
	};
}
