#pragma once
#include "Core/Graphics/Prerequisites.h"
#include "Component/RigidbodyComponent.h"

namespace Core
{
	class PhysicsEngine
	{
	public:
		PhysicsEngine(GamePtr game);
		~PhysicsEngine();
	public:
		void update(float deltaTime);
		void ResolveCollisions(float deltaTime);
	public:
		GamePtr getGame() { return m_Game; }
	private:
		GamePtr m_Game;
	};
}
