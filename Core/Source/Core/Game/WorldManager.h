#pragma once
#include "Core/Graphics/Prerequisites.h"
#include <string>
#include <map>
namespace Core
{
	class WorldManager
	{
	public:
		WorldManager(GamePtr game);
		~WorldManager();
	public:
		WorldPtr createWorld(const char* name);
		WorldPtr getWorld(const char* name);
		void destroyWorld(const char* name);
		void destroyWorld(World world);
		void destroyAllWorlds();
	public:
		GamePtr getGame() { return m_Game; }
	private:
		std::map<std::string, WorldPtr> m_Worlds;
		GamePtr m_Game;
	};
}

