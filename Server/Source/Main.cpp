#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <darkfire/darkfire.hpp>

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

class GameServer : public darkfire::net::DualServer<NetworkMsgTypes>
{
public:
	GameServer(uint16_t tcpPort, uint16_t udpPort)
		: darkfire::net::DualServer<NetworkMsgTypes>(tcpPort, udpPort, "DarkFireSecretAuthKey")
	{
	}

protected:
	bool OnClientConnect(std::shared_ptr<darkfire::net::TCPConnection> client) override
	{
		std::cout << "[SERVER] Client connection requested, assigning ID " << client->GetId() << "\n";
		return true;
	}

	void OnClientValidated(std::shared_ptr<darkfire::net::TCPConnection> client) override
	{
		uint32_t newId = client->GetId();

		// 1. Tell the new client its assigned ID via TCP
		darkfire::net::Packet msgAssign(NetworkMsgTypes::ClientAssignID);
		msgAssign << newId;
		Send(client, msgAssign);

		// 2. Inform existing players about the new player via TCP
		darkfire::net::Packet msgNewPlayer(NetworkMsgTypes::PlayerConnect);
		msgNewPlayer << newId;
		Broadcast(msgNewPlayer, client);

		// 3. Inform the new player about all existing players
		for (const auto& [id, state] : m_players)
		{
			darkfire::net::Packet msgExisting(NetworkMsgTypes::PlayerConnect);
			msgExisting << id;
			Send(client, msgExisting);

			darkfire::net::Packet msgMove(NetworkMsgTypes::PlayerMove);
			msgMove << state;
			Send(client, msgMove);
		}

		// Initialize default state for this player
		PlayerNetState initial{};
		initial.clientID = newId;
		initial.scale = 1.0f;
		m_players[newId] = initial;

		std::cout << "[SERVER] Client " << newId << " authenticated & joined. Total players: " << m_players.size() << "\n";
	}

	void OnClientDisconnect(std::shared_ptr<darkfire::net::TCPConnection> client) override
	{
		uint32_t id = client->GetId();
		m_players.erase(id);

		std::cout << "[SERVER] Client disconnected: " << id << ". Total players: " << m_players.size() << "\n";

		darkfire::net::Packet msg(NetworkMsgTypes::PlayerDisconnect);
		msg << id;
		Broadcast(msg, client);

		darkfire::net::DualServer<NetworkMsgTypes>::OnClientDisconnect(client);
	}

	// Reliable TCP message handler (critical game events)
	void OnMessage(std::shared_ptr<darkfire::net::TCPConnection> client, darkfire::net::Packet& msg) override
	{
		uint32_t senderId = client->GetId();
		auto msgId = msg.GetIdAs<NetworkMsgTypes>();
		auto reader = msg.GetReader();

		switch (msgId)
		{
		case NetworkMsgTypes::ProjectileHit:
		{
			uint32_t asteroidId = 0;
			reader >> asteroidId;

			// Re-broadcast hit to synchronize asteroid destruction
			darkfire::net::Packet outMsg(NetworkMsgTypes::ProjectileHit);
			outMsg << asteroidId;
			Broadcast(outMsg, client);
			break;
		}

		default:
			break;
		}
	}

	// High-frequency UDP message handler (fast flight transforms, shooting)
	void OnUDPMessage(const asio::ip::udp::endpoint& sender, darkfire::net::Packet& msg) override
	{
		(void)sender;
		auto msgId = msg.GetIdAs<NetworkMsgTypes>();
		auto reader = msg.GetReader();

		switch (msgId)
		{
		case NetworkMsgTypes::PlayerMove:
		{
			PlayerNetState state{};
			reader >> state;
			uint32_t senderId = state.clientID;
			m_players[senderId] = state;

			// Broadcast updated movement to all other clients via UDP
			darkfire::net::Packet outMsg(NetworkMsgTypes::PlayerMove);
			outMsg << state;
			BroadcastUDP(outMsg, senderId);
			break;
		}

		case NetworkMsgTypes::PlayerShoot:
		{
			PlayerShootData shoot{};
			reader >> shoot;
			uint32_t senderId = shoot.clientID;

			// Broadcast projectile shot to all other clients via UDP
			darkfire::net::Packet outMsg(NetworkMsgTypes::PlayerShoot);
			outMsg << shoot;
			BroadcastUDP(outMsg, senderId);
			break;
		}

		default:
			break;
		}
	}

private:
	std::unordered_map<uint32_t, PlayerNetState> m_players;
};

static std::atomic<bool> g_serverRunning{ true };

void handleSignal(int signum)
{
	(void)signum;
	g_serverRunning = false;
}

int main(int argc, char* argv[])
{
	std::signal(SIGINT, handleSignal);
	std::signal(SIGTERM, handleSignal);

	try
	{
		uint16_t tcpPort = 60000;
		uint16_t udpPort = 60001;
		if (argc > 1)
		{
			tcpPort = static_cast<uint16_t>(std::atoi(argv[1]));
			udpPort = tcpPort + 1;
		}

		std::cout << "========================================\n";
		std::cout << "   DarkFire v2 Dedicated Dual-Server\n";
		std::cout << "   TCP Port: " << tcpPort << " | UDP Port: " << udpPort << "\n";
		std::cout << "========================================" << std::endl;

		GameServer server(tcpPort, udpPort);

		if (!server.StartDual())
		{
			std::cerr << "[ERROR] Failed to start dual server on ports TCP:" << tcpPort << " UDP:" << udpPort << "!" << std::endl;
			return 1;
		}

		std::cout << "[INFO] Dual-server started successfully. Waiting for clients...\n";
		std::cout << "[INFO] Press Ctrl+C to stop the server." << std::endl;

		while (g_serverRunning)
		{
			server.UpdateDual(64, false);
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}

		std::cout << "\n[INFO] Shutting down server gracefully..." << std::endl;
		server.StopDual();
	}
	catch (const std::exception& e)
	{
		std::cerr << "[FATAL ERROR] Server crashed with exception: " << e.what() << std::endl;
		return 1;
	}
	catch (...)
	{
		std::cerr << "[FATAL ERROR] Server crashed with unknown exception!" << std::endl;
		return 1;
	}

	return 0;
}
