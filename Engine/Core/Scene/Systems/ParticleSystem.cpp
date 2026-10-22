#include "ParticleSystem.h"
#include "Engine/Core/Scene/Components.h"
#include <algorithm>
#include <cmath>

namespace Core::Systems
{
	// Linear Congruential Generator for fast deterministic RNG
	static float NextRandomFloat(uint32_t& seed)
	{
		seed = seed * 1664525u + 1013904223u;
		return static_cast<float>(seed & 0x00FFFFFF) / static_cast<float>(0x00FFFFFF);
	}

	static Vector3 NextRandomUnitVector(uint32_t& seed)
	{
		float z = NextRandomFloat(seed) * 2.0f - 1.0f;
		float a = NextRandomFloat(seed) * 6.2831853f;
		float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
		return Vector3(r * std::cos(a), z, r * std::sin(a));
	}

	void ParticleSystem::Tick(Scene& scene, float dt)
	{
		if (dt <= 0.0f) return;

		auto& registry = scene.getRegistry();

		// Ensure all entities with ParticleEmitterComponent have ParticleRuntimeState
		auto emitterOnly = registry.view<ParticleEmitterComponent>(entt::exclude<ParticleRuntimeState>);
		for (auto entity : emitterOnly)
		{
			registry.emplace<ParticleRuntimeState>(entity);
		}

		auto view = registry.view<ParticleEmitterComponent, ParticleRuntimeState, WorldTransform>();
		for (auto entity : view)
		{
			const auto& emitter = view.get<ParticleEmitterComponent>(entity);
			auto& state = view.get<ParticleRuntimeState>(entity);
			const auto& transform = view.get<WorldTransform>(entity);

			const Vector3 emitterWorldPos = Math::getTranslation(transform.worldMatrix);

			// 1. Update existing active particles
			for (int i = 0; i < state.activeCount; )
			{
				auto& p = state.particles[i];
				p.age += dt;

				if (p.age >= p.lifetime)
				{
					// Swap with last active particle and decrement
					std::swap(state.particles[i], state.particles[state.activeCount - 1]);
					state.activeCount--;
					continue;
				}

				// Apply gravity
				p.velocity.y -= 9.81f * emitter.gravityScale * dt;
				p.position += p.velocity * dt;

				// Interpolate size & color
				float t = std::clamp(p.age / std::max(p.lifetime, 0.0001f), 0.0f, 1.0f);
				p.size = std::lerp(emitter.startSize, emitter.endSize, t);

				p.color = Color(
					std::lerp(emitter.startColor.R(), emitter.endColor.R(), t),
					std::lerp(emitter.startColor.G(), emitter.endColor.G(), t),
					std::lerp(emitter.startColor.B(), emitter.endColor.B(), t),
					std::lerp(emitter.startColor.A(), emitter.endColor.A(), t)
				);

				++i;
			}

			// 2. Spawn new particles
			if (emitter.looping || emitter.playOnStart)
			{
				state.emitAccumulator += emitter.emissionRate * dt;
				int toSpawn = static_cast<int>(state.emitAccumulator);
				state.emitAccumulator -= static_cast<float>(toSpawn);

				int maxP = std::max(emitter.maxParticles, 1);
				if (static_cast<int>(state.particles.size()) < maxP)
				{
					state.particles.resize(maxP);
				}

				for (int s = 0; s < toSpawn && state.activeCount < maxP; ++s)
				{
					auto& newP = state.particles[state.activeCount];
					newP.age = 0.0f;
					newP.lifetime = std::max(emitter.lifetime, 0.01f);
					newP.size = emitter.startSize;
					newP.color = emitter.startColor;

					// Position
					if (emitter.simulationSpace == SimulationSpace::World)
					{
						newP.position = emitterWorldPos;
					}
					else
					{
						newP.position = Vector3(0.0f, 0.0f, 0.0f);
					}

					// Velocity with randomness
					Vector3 randDir = NextRandomUnitVector(state.rngSeed);
					Vector3 vel = emitter.startVelocity + randDir * emitter.velocityRandomness;
					newP.velocity = vel;

					state.activeCount++;
				}
			}
		}
	}

	void ParticleSystem::Reset(Scene& scene)
	{
		auto& registry = scene.getRegistry();
		auto view = registry.view<ParticleRuntimeState>();
		for (auto entity : view)
		{
			auto& state = view.get<ParticleRuntimeState>(entity);
			state.activeCount = 0;
			state.emitAccumulator = 0.0f;
		}
	}
}
