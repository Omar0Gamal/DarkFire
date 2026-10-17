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
	ClientAssignID,
	WorldData,
	PlayerConnect,
	PlayerDisconnect,
	PlayerMove,
	PlayerShoot,
	ProjectileHit
};

#pragma pack(push, 1)
struct PlayerNetState
{
	uint32_t clientID;
	float posX, posY, posZ;
	float rotX, rotY, rotZ;
	float scale;
};

struct PlayerShootData
{
	uint32_t clientID;
	float posX, posY, posZ;
	float dirX, dirY, dirZ;
	float speed;
};
#pragma pack(pop)

class NetworkClient : public df::net::client_interface<NetworkMsgTypes>
{
public:
	void sendMove(const TransformComponent& transform)
	{
		if (!IsConnected()) return;
		PlayerNetState state{};
		state.clientID = m_myClientID;
		state.posX = transform.m_Position.x;
		state.posY = transform.m_Position.y;
		state.posZ = transform.m_Position.z;
		state.rotX = transform.m_Rotation.x;
		state.rotY = transform.m_Rotation.y;
		state.rotZ = transform.m_Rotation.z;
		state.scale = transform.m_Scale.x;

		df::net::message<NetworkMsgTypes> msg;
		msg.header.id = NetworkMsgTypes::PlayerMove;
		msg << state;
		Send(msg);
	}

	void sendShoot(const Vector3d& pos, const Vector3d& dir, float speed)
	{
		if (!IsConnected()) return;
		PlayerShootData shoot{};
		shoot.clientID = m_myClientID;
		shoot.posX = pos.x;
		shoot.posY = pos.y;
		shoot.posZ = pos.z;
		shoot.dirX = dir.x;
		shoot.dirY = dir.y;
		shoot.dirZ = dir.z;
		shoot.speed = speed;

		df::net::message<NetworkMsgTypes> msg;
		msg.header.id = NetworkMsgTypes::PlayerShoot;
		msg << shoot;
		Send(msg);
	}

	void sendHit(uint32_t asteroidID)
	{
		if (!IsConnected()) return;
		df::net::message<NetworkMsgTypes> msg;
		msg.header.id = NetworkMsgTypes::ProjectileHit;
		msg << asteroidID;
		Send(msg);
	}

	void setMyClientID(uint32_t id) { m_myClientID = id; }
	uint32_t getMyClientID() const { return m_myClientID; }

private:
	uint32_t m_myClientID = 0;
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
public:
	int getFPS() { return m_fps; }
private:
	void handleNetworkMessages();
	void spawnRemotePlayer(uint32_t id);
	void removeRemotePlayer(uint32_t id);
private:
	bool m_input = true;
	bool m_fullscreen = false;
	int m_fps = 0;
	EntityID m_spaceID;
	EntityID m_EnmeyID;
	WorldData wdata;
	NetworkClient* m_client = nullptr;
	EntityID m_UI;
	EntityID m_Cross;
	std::unordered_map<uint32_t, EntityID> m_remotePlayers;
	MeshPtr m_remoteShipMesh = nullptr;
	std::vector<MaterialPtr> m_remoteShipMaterials;
};

