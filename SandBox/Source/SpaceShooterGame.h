#pragma once
#include "Core/All.h"
#include <darkfire/darkfire.hpp>
#include <functional>

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

enum class NetworkMsgTypes : uint16_t
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

DARKFIRE_DECLARE_TRIVIALLY_SERIALIZABLE(PlayerNetState);
DARKFIRE_DECLARE_TRIVIALLY_SERIALIZABLE(PlayerShootData);

class NetworkClient : public darkfire::net::DualClient<NetworkMsgTypes>
{
public:
	using PacketCallback = std::function<void(darkfire::net::Packet&)>;

	NetworkClient() : darkfire::net::DualClient<NetworkMsgTypes>("DarkFireSecretAuthKey") {}

	void setPacketHandler(PacketCallback cb)
	{
		m_packetHandler = std::move(cb);
	}

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

		darkfire::net::Packet pkt(NetworkMsgTypes::PlayerMove);
		pkt << state;
		SendUDPSequenced(pkt);
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

		darkfire::net::Packet pkt(NetworkMsgTypes::PlayerShoot);
		pkt << shoot;
		SendUDPReliable(pkt);
	}

	void sendHit(uint32_t asteroidID)
	{
		if (!IsConnected()) return;
		darkfire::net::Packet pkt(NetworkMsgTypes::ProjectileHit);
		pkt << asteroidID;
		SendTCP(pkt);
	}

	void setMyClientID(uint32_t id)
	{
		m_myClientID = id;
		RegisterUDP(id);
	}

	uint32_t getMyClientID() const { return m_myClientID; }

protected:
	void OnMessage(darkfire::net::Packet& pkt) override
	{
		if (m_packetHandler) m_packetHandler(pkt);
	}

	void OnUDPMessage(darkfire::net::Packet& pkt) override
	{
		if (m_packetHandler) m_packetHandler(pkt);
	}

private:
	uint32_t m_myClientID = 0;
	PacketCallback m_packetHandler;
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
	void processPacket(darkfire::net::Packet& pkt);
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
