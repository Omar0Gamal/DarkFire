#include "WorldManager.h"
#include "World.h"

namespace Core
{
    WorldManager::WorldManager(GamePtr game)
		: m_Game(game) {}

    WorldManager::~WorldManager()
    {
    }

    WorldPtr WorldManager::createWorld(const char* name)
    {
		WorldPtr world = std::make_shared<World>(name,m_Game);
		m_Worlds[name] = world;
		return world;
    }

    WorldPtr WorldManager::getWorld(const char* name)
    {
		return m_Worlds[name];
    }

    void WorldManager::destroyWorld(const char* name)
    {
        if (m_Worlds.find(name) == m_Worlds.end()) return;
		m_Worlds.erase(name);
    }

    void WorldManager::destroyWorld(World world)
    {
		if (m_Worlds.find(world.getName()) == m_Worlds.end()) return;
		m_Worlds.erase(world.getName());
    }

    void WorldManager::destroyAllWorlds()
    {
		m_Worlds.clear();
    }
}
