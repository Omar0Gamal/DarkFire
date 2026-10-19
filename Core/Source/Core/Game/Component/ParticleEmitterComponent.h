#pragma once
#include "Core/Game/IComponent.h"
#include "Core/Math/Vector3d.h"
#include "Core/Math/Vector4d.h"
#include "Core/Graphics/Prerequisites.h"
#include <vector>
#include <string>
#include <random>
#include <cmath>
#include <algorithm>

namespace Core
{
	struct Particle
	{
		Vector3d position = { 0, 0, 0 };
		Vector3d velocity = { 0, 0, 0 };
		Vector4d color = { 1, 1, 1, 1 };
		float size = 0.5f;
		float rotation = 0.0f;
		float rotationSpeed = 0.0f;
		float lifetime = 1.0f;
		float age = 0.0f;

		bool isAlive() const { return age < lifetime; }
		float getNormalizedAge() const { return lifetime > 0.0f ? age / lifetime : 1.0f; }
	};

	enum class EmitterShape
	{
		Point,
		Sphere,
		Box,
		Cone
	};

	enum class ParticleBlendMode
	{
		Additive,
		AlphaBlend
	};

	class ParticleEmitterComponent : public IComponent
	{
	public:
		ParticleEmitterComponent() = default;
		virtual ~ParticleEmitterComponent() = default;

		void update(float deltaTime, const Vector3d& emitterWorldPos, const Vector3d& emitterWorldRot)
		{
			// 1. Spawning
			if (isPlaying)
			{
				if (isLooping)
				{
					emissionAccumulator += emissionRate * deltaTime;
					while (emissionAccumulator >= 1.0f && particles.size() < maxParticles)
					{
						spawnParticle(emitterWorldPos, emitterWorldRot);
						emissionAccumulator -= 1.0f;
					}
				}

				while (burstCount > 0 && particles.size() < maxParticles)
				{
					spawnParticle(emitterWorldPos, emitterWorldRot);
					--burstCount;
				}
			}

			// 2. Simulation
			for (size_t i = 0; i < particles.size(); ++i)
			{
				auto& p = particles[i];
				p.age += deltaTime;

				if (p.isAlive())
				{
					float t = p.getNormalizedAge();

					// Physics integration
					p.velocity += gravity * deltaTime;
					p.velocity *= (1.0f - (std::min)(1.0f, drag * deltaTime));
					p.position += p.velocity * deltaTime;
					p.rotation += p.rotationSpeed * deltaTime;

					// Color and size interpolation
					p.color = Vector4d::lerp(startColor, endColor, t);
					p.size = startSize * (1.0f - t) + endSize * t;
				}
			}

			// 3. Removal of dead particles (swap-and-pop)
			for (size_t i = 0; i < particles.size(); )
			{
				if (!particles[i].isAlive())
				{
					particles[i] = particles.back();
					particles.pop_back();
				}
				else
				{
					++i;
				}
			}
		}

		void burst(int count)
		{
			burstCount += count;
		}

		void play() { isPlaying = true; }
		void pause() { isPlaying = false; }
		void stop() { isPlaying = false; particles.clear(); emissionAccumulator = 0.0f; burstCount = 0; }

	public:
		static ParticleEmitterComponent CreateSparks(const Vector4d& color = Vector4d(1.0f, 0.85f, 0.2f, 1.0f))
		{
			ParticleEmitterComponent emitter;
			emitter.blendMode = ParticleBlendMode::Additive;
			emitter.shape = EmitterShape::Cone;
			emitter.emissionRate = 80.0f;
			emitter.lifetimeMin = 0.3f;
			emitter.lifetimeMax = 0.7f;
			emitter.speedMin = 4.0f;
			emitter.speedMax = 12.0f;
			emitter.direction = Vector3d(0, 1, 0);
			emitter.coneAngle = 0.45f;
			emitter.startColor = color;
			emitter.endColor = Vector4d(color.x * 0.5f, color.y * 0.2f, 0.0f, 0.0f);
			emitter.startSize = 0.35f;
			emitter.endSize = 0.05f;
			emitter.gravity = Vector3d(0, -14.0f, 0);
			emitter.drag = 0.4f;
			return emitter;
		}

		static ParticleEmitterComponent CreateSmoke(const Vector4d& color = Vector4d(0.55f, 0.55f, 0.58f, 0.45f))
		{
			ParticleEmitterComponent emitter;
			emitter.blendMode = ParticleBlendMode::AlphaBlend;
			emitter.shape = EmitterShape::Sphere;
			emitter.sphereRadius = 0.3f;
			emitter.emissionRate = 35.0f;
			emitter.lifetimeMin = 0.8f;
			emitter.lifetimeMax = 1.8f;
			emitter.speedMin = 1.0f;
			emitter.speedMax = 3.0f;
			emitter.direction = Vector3d(0, 1, 0);
			emitter.startColor = color;
			emitter.endColor = Vector4d(color.x, color.y, color.z, 0.0f);
			emitter.startSize = 0.4f;
			emitter.endSize = 1.8f;
			emitter.gravity = Vector3d(0, 0.8f, 0); // Gentle buoyancy
			emitter.drag = 1.2f;
			return emitter;
		}

		static ParticleEmitterComponent CreateFlames()
		{
			ParticleEmitterComponent emitter;
			emitter.blendMode = ParticleBlendMode::Additive;
			emitter.shape = EmitterShape::Cone;
			emitter.emissionRate = 100.0f;
			emitter.lifetimeMin = 0.25f;
			emitter.lifetimeMax = 0.6f;
			emitter.speedMin = 3.0f;
			emitter.speedMax = 8.0f;
			emitter.direction = Vector3d(0, 0, -1);
			emitter.coneAngle = 0.2f;
			emitter.startColor = Vector4d(1.0f, 0.9f, 0.3f, 1.0f);
			emitter.endColor = Vector4d(0.9f, 0.2f, 0.02f, 0.0f);
			emitter.startSize = 0.5f;
			emitter.endSize = 0.1f;
			emitter.gravity = Vector3d(0, 1.5f, 0);
			emitter.drag = 0.5f;
			return emitter;
		}

		static ParticleEmitterComponent CreateExplosionBurst(const Vector4d& color = Vector4d(1.0f, 0.6f, 0.1f, 1.0f))
		{
			ParticleEmitterComponent emitter;
			emitter.blendMode = ParticleBlendMode::Additive;
			emitter.shape = EmitterShape::Sphere;
			emitter.sphereRadius = 0.2f;
			emitter.isLooping = false;
			emitter.burstCount = 120;
			emitter.lifetimeMin = 0.4f;
			emitter.lifetimeMax = 0.9f;
			emitter.speedMin = 8.0f;
			emitter.speedMax = 22.0f;
			emitter.startColor = color;
			emitter.endColor = Vector4d(0.8f, 0.1f, 0.0f, 0.0f);
			emitter.startSize = 0.7f;
			emitter.endSize = 0.05f;
			emitter.gravity = Vector3d(0, -9.8f, 0);
			emitter.drag = 1.0f;
			return emitter;
		}

	private:
		void spawnParticle(const Vector3d& origin, const Vector3d& rotation)
		{
			static thread_local std::mt19937 rng(1337);
			std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
			std::uniform_real_distribution<float> distSym(-1.0f, 1.0f);

			Particle p;
			p.position = origin;
			p.age = 0.0f;
			p.lifetime = lifetimeMin + dist01(rng) * (lifetimeMax - lifetimeMin);
			p.size = startSize;
			p.color = startColor;
			p.rotation = dist01(rng) * 6.2831853f;
			p.rotationSpeed = distSym(rng) * 4.0f;

			// Position offset based on shape
			if (shape == EmitterShape::Sphere)
			{
				Vector3d offset(distSym(rng), distSym(rng), distSym(rng));
				offset = Vector3d::normalize(offset) * (dist01(rng) * sphereRadius);
				p.position += offset;
			}
			else if (shape == EmitterShape::Box)
			{
				Vector3d offset(distSym(rng) * boxExtents.x, distSym(rng) * boxExtents.y, distSym(rng) * boxExtents.z);
				p.position += offset;
			}

			// Initial velocity
			float speed = speedMin + dist01(rng) * (speedMax - speedMin);
			Vector3d dir = direction;

			if (shape == EmitterShape::Cone)
			{
				// Spread within cone angle
				float theta = dist01(rng) * coneAngle;
				float phi = dist01(rng) * 6.2831853f;

				Vector3d spread(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
				dir = Vector3d::normalize(dir + spread * coneAngle);
			}
			else if (shape == EmitterShape::Sphere)
			{
				dir = Vector3d(distSym(rng), distSym(rng), distSym(rng));
				dir = Vector3d::normalize(dir);
			}

			p.velocity = dir * speed;
			particles.push_back(p);
		}

	public:
		bool isPlaying = true;
		bool isLooping = true;
		EmitterShape shape = EmitterShape::Cone;
		ParticleBlendMode blendMode = ParticleBlendMode::Additive;

		float emissionRate = 60.0f;
		int burstCount = 0;

		float lifetimeMin = 0.4f;
		float lifetimeMax = 1.0f;
		float speedMin = 2.0f;
		float speedMax = 6.0f;

		Vector3d direction = { 0, 1, 0 };
		float coneAngle = 0.35f;

		Vector3d boxExtents = { 0.5f, 0.5f, 0.5f };
		float sphereRadius = 0.5f;

		Vector4d startColor = { 1.0f, 0.85f, 0.2f, 1.0f };
		Vector4d endColor = { 1.0f, 0.1f, 0.0f, 0.0f };

		float startSize = 0.4f;
		float endSize = 0.05f;

		Vector3d gravity = { 0, -9.8f, 0 };
		float drag = 0.3f;

		Texture2DPtr texture = nullptr;
		std::wstring texturePath = L"";

		size_t maxParticles = 2500;
		std::vector<Particle> particles;

	private:
		float emissionAccumulator = 0.0f;
	};
}
