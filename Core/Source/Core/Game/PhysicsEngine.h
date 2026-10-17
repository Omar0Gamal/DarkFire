#pragma once
#include "Core/Graphics/Prerequisites.h"
#include "Component/RigidbodyComponent.h"

namespace Core
{
	class PhysicsEngine
	{
	public:
		PhysicsEngine(Game* game);
		~PhysicsEngine();
	public:
		void update(float deltaTime);
		void ResolveCollisions(float deltaTime);
	public:
		Game* getGame() { return m_Game; }
	private:
		Game* m_Game = nullptr;
	};
}
