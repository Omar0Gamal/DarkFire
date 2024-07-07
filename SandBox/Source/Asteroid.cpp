#include "Asteroid.h"

Asteroid::Asteroid(MeshPtr mesh, std::vector<MaterialPtr> materials)
	:ModelEntity(mesh , materials)
{
}

Asteroid::~Asteroid()
{
}

void Asteroid::onCreate()
{
	ModelEntity::onCreate();
}

void Asteroid::onUpdate(float deltaTime)
{
	ModelEntity::onUpdate(deltaTime);
	float g = 9.8f;
	TransformComponent& trans = getComponent<TransformComponent>();
	auto pos = trans.m_Position;
	pos.y -= g * 2.0f * deltaTime;

	trans.setPosition(pos);
}

void Asteroid::onDestroy()
{
}

void Asteroid::setHealth(float health)
{
	m_health = health;
}

void Asteroid::setDamage(float damage)
{
	m_damage = damage;
}

float Asteroid::getHealth() const
{
	return m_health;
}

float Asteroid::getDamage() const
{
	return m_damage;
}
