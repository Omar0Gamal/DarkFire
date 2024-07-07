#include "PhysicsEngine.h"
#include "Game.h"
#include "World.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"

namespace Core
{
	PhysicsEngine::PhysicsEngine(GamePtr game)
		: m_Game(game) {}

	PhysicsEngine::~PhysicsEngine()
	{
	}

    void PhysicsEngine::update(float deltaTime)
    {
		Vector3d m_gravity = Vector3d(0, -m_Game->getActiveWorld()->getGravity(), 0);
        // Update all rigidbodies
    //    m_Game->getActiveWorld()->getECS()->ForEach<TransformComponent, RigidbodyComponent>
    //        ([&](TransformComponent& transform, RigidbodyComponent& rigidbody)
    //        {
				//if (!rigidbody.isKinematic) return;
				//rigidbody.Force += m_gravity * rigidbody.mass ; // apply a force
 
				//rigidbody.velocity += rigidbody.Force / rigidbody.mass * deltaTime;
				//transform.m_Position += rigidbody.velocity * deltaTime;
 
				//rigidbody.Force = Vector3d(0, 0, 0);
    //        });
    }

	void PhysicsEngine::ResolveCollisions(float deltaTime)
	{

	}
}