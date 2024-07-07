#include "Projectile.h"
#include "Asteroid.h"

Projectile::Projectile()
	: m_direction({ 0,0,0 }), m_speed(0.0f), m_lifeTime(0.0f), m_damage(0.0f), m_owner(0)
	, ModelEntity()
{
}

Projectile::~Projectile()
{
}

void Projectile::onCreate()
{
	setMesh(m_World->getGame()->createMesh(L"Assets/Meshes/sphere.obj"));
	setMaterials(std::vector<MaterialPtr>{m_World->getGame()->createMaterial(L"Assets/Shaders/projectile.hlsl")});
	ModelEntity::onCreate();
	getComponent<TransformComponent>().setScale({ 2.0f,2.0f,2.0f });
}

void Projectile::onUpdate(float deltaTime)
{
	m_elapsed += deltaTime;

	//Move the projectile along the defined direction (spaceship direction)
	getComponent<TransformComponent>().setPosition(getComponent<TransformComponent>().m_Position + m_direction * deltaTime * m_speed);

	for (auto entity : m_World->getAllEntities<Asteroid>()) {
		
		if (has_Hit(entity->getComponent<TransformComponent>()))
		{
			if(entity->getHealth() - m_damage <= 0)
				entity->remove();
			else
				entity->setHealth(entity->getHealth() - m_damage);
			IEntity::remove();
			break;
		}
	}

	//After 3 seconds, delete the projectile
	if (m_elapsed > m_lifeTime)
	{
		IEntity::remove();
	}
}

void Projectile::onDestroy()
{
}

void Projectile::setDirection(Vector3d direction)
{
	m_direction = direction;
}

void Projectile::setSpeed(float speed)
{
	m_speed = speed;
}

void Projectile::setLifeTime(float lifeTime)
{
	m_lifeTime = lifeTime;
}

void Projectile::setDamage(float damage)
{
	m_damage = damage;
}

void Projectile::setOwner(EntityID owner)
{
	m_owner = owner;
}

double distanceBetween(const Vector3d& a, const Vector3d& b) {
	return std::sqrt(std::pow(a.x - b.x, 2) +
		std::pow(a.y - b.y, 2) +
		std::pow(a.z - b.z, 2));
}

bool Projectile::has_Hit(TransformComponent& asteroidT)
{
	auto projT = getComponent<TransformComponent>();

	double distance = distanceBetween(projT.m_Position,asteroidT.m_Position);
	return distance < (projT.m_Scale.x + asteroidT.m_Scale.x * 2.8);
}
