#include <iostream>
#include <df_net.h>

enum class NetworkMsgTypes : uint32_t
{
	ServerAccept,
	ServerDeny,
	WorldData,
	PlayerMove,
	PlayerShoot,
	ProjectileHit
};

class CustomServer : public df::net::server_interface<NetworkMsgTypes>
{
public:
	CustomServer(uint16_t nPort) : df::net::server_interface<NetworkMsgTypes>(nPort)
	{

	}
protected:
	virtual bool OnClientConnect(std::shared_ptr<df::net::connection<NetworkMsgTypes>> client)
	{
		df::net::message<NetworkMsgTypes> msg;
		msg.header.id = NetworkMsgTypes::ServerAccept;
		client->Send(msg);
		if (!m_first)
		{
			client->Send(world);
		}
		return true;
	}
	virtual void OnClientDisconnect(std::shared_ptr<df::net::connection<NetworkMsgTypes>> client)
	{
		std::cout << "Removing client [" << client->GetID() << "]\n";
	}
	virtual void OnMessage(std::shared_ptr<df::net::connection<NetworkMsgTypes>> client, df::net::message<NetworkMsgTypes>& msg)
	{
		switch (msg.header.id)
		{
		case NetworkMsgTypes::PlayerMove:
		{
			std::cout << "[" << client->GetID() << "]: Player Moved\n";

			df::net::message<NetworkMsgTypes> msg;
			msg.header.id = NetworkMsgTypes::PlayerMove;
			MessageAllClients(msg, client);
			break;
		}

		case NetworkMsgTypes::PlayerShoot:
		{
			std::cout << "[" << client->GetID() << "]: Player Shoot\n";
			df::net::message<NetworkMsgTypes> msg;
			msg.header.id = NetworkMsgTypes::PlayerShoot;
			msg << client->GetID();
			MessageAllClients(msg, client);
			break;
		}
		case NetworkMsgTypes::ProjectileHit:
		{
			std::cout << "[" << client->GetID() << "]: Projectile Hit \n";
			df::net::message<NetworkMsgTypes> msg;
			msg.header.id = NetworkMsgTypes::ProjectileHit;
			msg << client->GetID();
			MessageAllClients(msg, client);
			break;
		}
		case NetworkMsgTypes::WorldData:
		{
			std::cout << "[" << client->GetID() << "]: World Data\n";
			df::net::message<NetworkMsgTypes> msg;
			msg.header.id = NetworkMsgTypes::WorldData;
			world = msg;
			m_first = false;
			break;
		}
		}
	}
	private:
		bool m_first = true;
		df::net::message<NetworkMsgTypes> world;
};

int main()
{
	CustomServer server(60000);
	server.Start();
	while (1)
	{
		server.Update(-1, true);
	}
	return 0;
}