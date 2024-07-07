#pragma once
#include "Core/All.h"

using namespace Core;


class Asteroid : public ModelEntity
{
public:
	Asteroid(MeshPtr mesh, std::vector<MaterialPtr> materials);
	~Asteroid();
public:
	void onCreate() override;
	void onUpdate(float deltaTime) override;
	void onDestroy() override;
public:
	void setHealth(float health);
	void setDamage(float damage);
	float getHealth() const;
	float getDamage() const;
private:
	float m_health = 10.0f;
	float m_damage = 1.0f;
};

