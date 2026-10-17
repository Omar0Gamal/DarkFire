#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <df_net.h>

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

class GameServer : public df::net::server_interface<NetworkMsgTypes>
{
public:
	GameServer(uint16_t nPort) : df::net::server_interface<NetworkMsgTypes>(nPort)
	{
	}

protected:
	bool OnClientConnect(std::shared_ptr<df::net::connection<NetworkMsgTypes>> client) override
	{
		df::net::message<NetworkMsgTypes> msg;
		msg.header.id = NetworkMsgTypes::ServerAccept;
		client->Send(msg);

		std::cout << "[SERVER] Client connection accepted: " << client->GetID() << "\n";
		return true;
	}

	void OnClientValidated(std::shared_ptr<df::net::connection<NetworkMsgTypes>> client) override
	{
		uint32_t newId = client->GetID();

		// 1. Tell the new client its assigned ID
		df::net::message<NetworkMsgTypes> msgAssign;
		msgAssign.header.id = NetworkMsgTypes::ClientAssignID;
		msgAssign << newId;
		client->Send(msgAssign);

		// 2. Inform existing players about the new player
		df::net::message<NetworkMsgTypes> msgNewPlayer;
		msgNewPlayer.header.id = NetworkMsgTypes::PlayerConnect;
		msgNewPlayer << newId;
		MessageAllClients(msgNewPlayer, client);

		// 3. Inform the new player about all existing players
		for (const auto& [id, state] : m_players)
		{
			df::net::message<NetworkMsgTypes> msgExisting;
			msgExisting.header.id = NetworkMsgTypes::PlayerConnect;
			msgExisting << id;
			client->Send(msgExisting);

			df::net::message<NetworkMsgTypes> msgMove;
			msgMove.header.id = NetworkMsgTypes::PlayerMove;
			msgMove << state;
			client->Send(msgMove);
		}

		// Initialize default state for this player
		PlayerNetState initial{};
		initial.clientID = newId;
		initial.scale = 1.0f;
		m_players[newId] = initial;

		std::cout << "[SERVER] Client " << newId << " fully joined. Total players: " << m_players.size() << "\n";
	}

	void OnClientDisconnect(std::shared_ptr<df::net::connection<NetworkMsgTypes>> client) override
	{
		uint32_t id = client->GetID();
		m_players.erase(id);

		std::cout << "[SERVER] Client disconnected: " << id << ". Total players: " << m_players.size() << "\n";

		df::net::message<NetworkMsgTypes> msg;
		msg.header.id = NetworkMsgTypes::PlayerDisconnect;
		msg << id;
		MessageAllClients(msg, client);
	}

	void OnMessage(std::shared_ptr<df::net::connection<NetworkMsgTypes>> client, df::net::message<NetworkMsgTypes>& msg) override
	{
		uint32_t senderId = client->GetID();

		switch (msg.header.id)
		{
		case NetworkMsgTypes::PlayerMove:
		{
			PlayerNetState state{};
			msg >> state;
			state.clientID = senderId;
			m_players[senderId] = state;

			// Broadcast updated movement to all other clients
			df::net::message<NetworkMsgTypes> outMsg;
			outMsg.header.id = NetworkMsgTypes::PlayerMove;
			outMsg << state;
			MessageAllClients(outMsg, client);
			break;
		}

		case NetworkMsgTypes::PlayerShoot:
		{
			PlayerShootData shoot{};
			msg >> shoot;
			shoot.clientID = senderId;

			// Broadcast shot to all other clients
			df::net::message<NetworkMsgTypes> outMsg;
			outMsg.header.id = NetworkMsgTypes::PlayerShoot;
			outMsg << shoot;
			MessageAllClients(outMsg, client);
			break;
		}

		case NetworkMsgTypes::ProjectileHit:
		{
			uint32_t asteroidId = 0;
			msg >> asteroidId;

			// Re-broadcast hit to synchronize asteroid destruction
			df::net::message<NetworkMsgTypes> outMsg;
			outMsg.header.id = NetworkMsgTypes::ProjectileHit;
			outMsg << asteroidId;
			MessageAllClients(outMsg, client);
			break;
		}

		default:
			break;
		}
	}

private:
	std::unordered_map<uint32_t, PlayerNetState> m_players;
};

#include <csignal>
#include <atomic>

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
		uint16_t port = 60000;
		if (argc > 1)
		{
			port = static_cast<uint16_t>(std::atoi(argv[1]));
		}

		std::cout << "========================================\n";
		std::cout << "   DarkFire SpaceShooter Dedicated Server\n";
		std::cout << "   Port: " << port << "\n";
		std::cout << "========================================" << std::endl;

		GameServer server(port);

		if (!server.Start())
		{
			std::cerr << "[ERROR] Failed to start server on port " << port << "!" << std::endl;
			return 1;
		}

		std::cout << "[INFO] Server started successfully. Waiting for clients...\n";
		std::cout << "[INFO] Press Ctrl+C to stop the server." << std::endl;

		while (g_serverRunning)
		{
			server.Update(64, true);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		std::cout << "\n[INFO] Shutting down server gracefully..." << std::endl;
		server.Stop();
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
