#include "World.h"
namespace Core
{
	World::World(const char* name, GamePtr game)
		: m_Name(name), m_Game(game), mainCameraID(0)
	{
		m_ECS = std::make_unique<ECS>(m_Game.get());
	}

	World::~World()
	{
		m_Entities.clear();
		m_ECS.reset();
	}

	const char* World::getName() const
	{
		return m_Name.c_str();
	}
	void World::setMainCamera(EntityID id)
	{
		mainCameraID = id;
	}
	EntityID World::getMainCameraID() const
	{
		return mainCameraID;
	}
}