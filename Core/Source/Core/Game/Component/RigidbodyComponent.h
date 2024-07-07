#pragma once
#include "Core/Game/IComponent.h"
#include "ColliderComponent.h"
#include "Core/Math/Vector3d.h"
namespace Core
{
    class RigidbodyComponent : public IComponent
    {
    public:
        RigidbodyComponent(float mass, bool isKinematic)
            : velocity({ 0,0,0 }), mass(mass), Force({ 0,0,0 }), isKinematic(isKinematic) {}
    public:
        Vector3d velocity = {0,0,0};
		float mass = 1.0f;
        Vector3d Force = {0,0,0};
		bool isKinematic = true;
    };
}