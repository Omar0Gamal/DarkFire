#include "ParticleSystem.h"
#include "Core/Game/Component/TransformComponent.h"
#include "Core/Game/Component/ParticleEmitterComponent.h"

namespace Core
{
	ParticleSystem::ParticleSystem(Game* game)
		: m_game(game)
	{
	}

	void ParticleSystem::update(float deltaTime, ECS* ecs)
	{
		if (!ecs)
			return;

		ecs->ForEach<TransformComponent, ParticleEmitterComponent>(
			[&](TransformComponent& transform, ParticleEmitterComponent& emitter)
			{
				emitter.update(deltaTime, transform.m_Position, transform.m_Rotation);
			});
	}
}
