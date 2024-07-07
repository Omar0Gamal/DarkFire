#pragma once
#include "Core/All.h"

using namespace Core;

class Projectile : public ModelEntity
{
public:
	Projectile();
	~Projectile();
public:
	void onCreate() override;
	void onUpdate(float deltaTime) override;
	void onDestroy() override;
public:
	void setDirection(Vector3d direction);
	void setSpeed(float speed);
	void setLifeTime(float lifeTime);
	void setDamage(float damage);
	void setOwner(EntityID owner);
private:
	bool has_Hit(TransformComponent& transform);
private:
	Vector3d m_direction = { 0,0,0 };
	float m_speed = 0.0f;
	float m_lifeTime = 0.0f;
	float m_damage = 0.0f;
	EntityID m_owner = 0;
	float m_elapsed = 0.0f;
};

