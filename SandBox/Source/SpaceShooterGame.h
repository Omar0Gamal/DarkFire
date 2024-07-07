#pragma once
#include "Core/All.h"
#include "df_net.h"

using namespace Core;

struct AsteroidData
{
	EntityID id;
	float x, y, z;
	float rx, ry, rz;
	float scale;
};

struct WorldData
{
	std::vector<AsteroidData> asteroids;
};

enum class NetworkMsgTypes : uint32_t
{
	ServerAccept,
	ServerDeny,
	WorldData,
	PlayerMove,
	PlayerShoot,
	ProjectileHit
};

class NetworkClient : public df::net::client_interface<NetworkMsgTypes>
{
public:
	void sendMove(TransformComponent& transform)
	{
		df::net::message<NetworkMsgTypes> msg;
		msg.header.id = NetworkMsgTypes::PlayerMove;

		msg << transform.m_Position.x;
		msg << transform.m_Position.y;
		msg << transform.m_Position.z;
		msg << transform.m_Rotation.x;
		msg << transform.m_Rotation.y;
		msg << transform.m_Rotation.z;
		msg << transform.m_Scale.x;
		Send(msg);
	}

	void sendShoot()
	{
		df::net::message<NetworkMsgTypes> msg;
		msg.header.id = NetworkMsgTypes::PlayerShoot;
		Send(msg);
	}

	void sendHit(EntityID asteroidID)
	{
		df::net::message<NetworkMsgTypes> msg;
		msg.header.id = NetworkMsgTypes::ProjectileHit;

		msg << asteroidID;
		Send(msg);
	}

	void sendWorldData(WorldData& data)
	{
		df::net::message<NetworkMsgTypes> msg;
		msg.header.id = NetworkMsgTypes::ServerAccept;

		msg << data.asteroids.size();
		for (auto& asteroid : data.asteroids)
		{
			msg << asteroid.id;
			msg << asteroid.x;
			msg << asteroid.y;
			msg << asteroid.z;
			msg << asteroid.rx;
			msg << asteroid.ry;
			msg << asteroid.rz;
			msg << asteroid.scale;
		}

		Send(msg);
	}
};

class SpaceShooterGame : public Core::Game
{
public:
	SpaceShooterGame(const wchar_t* titles, int width, int height, NetworkClient* client);
	~SpaceShooterGame();
public:
	virtual void onCreate();
	virtual void onUpdate(float deltaTime);
	virtual void onQuit();
private:
	bool m_input = true;
	bool m_fullscreen = false;
	EntityID m_spaceID;
	EntityID m_EnmeyID;
	WorldData wdata;
	NetworkClient* m_client;
};

