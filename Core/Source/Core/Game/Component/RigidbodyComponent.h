#pragma once
#include "Core/Game/IComponent.h"
#include "Core/Math/Vector3d.h"

namespace Core
{
	class RigidbodyComponent : public IComponent
	{
	public:
		RigidbodyComponent(float mass = 1.0f, bool isDynamic = true)
			: mass(mass), isKinematic(!isDynamic), useGravity(isDynamic) {}

	public:
		Vector3d velocity = { 0, 0, 0 };
		Vector3d angularVelocity = { 0, 0, 0 };
		Vector3d force = { 0, 0, 0 };
		Vector3d torque = { 0, 0, 0 };

		float mass = 1.0f;
		bool isKinematic = false; // When false, simulated by physics forces; when true, controlled directly or static
		bool useGravity = true;

		float linearDrag = 0.05f;
		float angularDrag = 0.05f;
		float bounciness = 0.25f;
		float friction = 0.6f;

	public:
		void addForce(const Vector3d& f)
		{
			force = force + f;
		}

		void addImpulse(const Vector3d& impulse)
		{
			if (mass > 0.0001f && !isKinematic)
			{
				velocity = velocity + (impulse / mass);
			}
		}

		void setVelocity(const Vector3d& vel)
		{
			velocity = vel;
		}
	};
}