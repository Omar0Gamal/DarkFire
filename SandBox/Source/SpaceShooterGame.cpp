#include "SpaceShooterGame.h"
#include "Core/Graphics/SwapChain.h"
#include "Spaceship.h"
#include <unordered_map>
#include <functional>
#include "Asteroid.h"
#include "Projectile.h"


SpaceShooterGame::SpaceShooterGame(const wchar_t* titles, int width, int height, NetworkClient* client)
	: Core::Game(titles, width, height), m_client(client)
{
	if (m_client)
	{
		m_client->setPacketHandler([this](darkfire::net::Packet& pkt) {
			processPacket(pkt);
		});
	}
}

SpaceShooterGame::~SpaceShooterGame()
{
}

void SpaceShooterGame::onCreate()
{
	getActiveWorld()->setGravity(0);

	auto font = createFont(L"Assets/Fonts/arial.font");

	//Adding SkyBox
	{
		auto mat = createMaterial(L"Assets/Shaders/skybox.hlsl");
		mat->addTexture(createTexture(L"Assets/Textures/stars_map.jpg"));
		mat->setCullMode(Core::CullMode::Front);

		auto entity = getActiveWorld()->createEntity<ModelEntity>("SkyBox", createMesh(L"Assets/Meshes/sphere.obj"), std::vector<MaterialPtr> {mat});
		entity->getComponent<TransformComponent>().setScale({ 40000,40000,40000 });
	}

	//Adding DLightEntity
	{
		auto entity = getActiveWorld()->createEntity<LightEntity>("Light1", Vector3d( 1, 1, 1 ));
		entity->getComponent<TransformComponent>().setRotation({ -0.707f, 0.707f, 0 });
	}

	//Adding DLightEntity
	{
		auto entity = getActiveWorld()->createEntity<LightEntity>("Light2", Vector3d( 1 , 0, 0 ));
		entity->getComponent<TransformComponent>().setScale({ 0.707f, 0.707f, 0 });
	}

	//Adding Asteroids
	{
		srand((unsigned int)time(NULL));

		auto mesh = createMesh(L"Assets/Meshes/asteroid.obj");
		auto mat = createMaterial(L"Assets/Shaders/base.hlsl");
		mat->addTexture(createTexture(L"Assets/Textures/asteroid.jpg"));

		for (unsigned int i = 0; i < 200; i++)
		{
			float scale = rand() % 20 + (1.0f);
			auto entity = getActiveWorld()->createEntity<Asteroid>("asteroid" + i, mesh, std::vector<MaterialPtr>{mat});
			entity->getComponent<TransformComponent>()
				.setPosition(Core::Vector3d((rand() % 4000) + (-2000.0f), 2000 + (rand() % 4000) + (-2000.0f), (rand() % 4000) + (-2000.0f)))
				.setRotation(Core::Vector3d((rand() % 628) / 100.0f, (rand() % 628) / 100.0f, (rand() % 628) / 100.0f))
				.setScale(Core::Vector3d(scale, scale, scale));
			entity->setHealth(5.0f * scale);
		}
	}

	{
		auto mat = createMaterial(L"Assets/Shaders/base.hlsl");
		mat->addTexture(createTexture(L"Assets/Textures/grass.jpg"));

		auto entity = getActiveWorld()->createEntity<ModelEntity>("earth", createMesh(L"Assets/Meshes/box.obj"), std::vector<MaterialPtr>{mat});
		entity->getComponent<TransformComponent>()
			.setPosition(Core::Vector3d(0, -400, 0))
			.setRotation(Core::Vector3d(0, 0, 0))
			.setScale(Core::Vector3d(8000, 20, 8000));
	}


	//Adding Spaceship
	{
		auto cam = getActiveWorld()->createEntity<CameraEntity>("Camera",40000.0);
		getActiveWorld()->setMainCamera(cam->getID());

		auto mat = createMaterial(L"Assets/Shaders/base.hlsl");
		mat->addTexture(createTexture(L"Assets/Textures/spaceship.jpg"));

		m_remoteShipMesh = createMesh(L"Assets/Meshes/spaceship.obj");
		m_remoteShipMaterials = { mat };

		auto entity = getActiveWorld()->createEntity<Spaceship>("Spaceship", m_remoteShipMesh, std::vector<MaterialPtr> {mat}, cam->getID(), m_client);
		m_spaceID = entity->getID();
	}

	//Text
	{
		auto entity = getActiveWorld()->createEntity<Entity>("Text");
		std::wstringstream ss;
		ss << "FPS: {FPS}";
		ss << "\nEntities: {Entities}";
		ss << "\nAsteroids: {Asteroids}";
		ss << "\nSpaceship: {Spaceship}";

		entity->addComponent<TextComponent>(TextComponent(ss.str(), font));
		entity->getComponent<TextComponent>().setPosition({ 0,0 }).setScale({1.0f, 1.0f}).setColor(DirectX::Colors::Aqua);

		m_UI = entity->getID();
	}

	//UI Cross

	{
		auto tex = createTexture(L"Assets/Textures/UI/cross.png");
		auto entity = getActiveWorld()->createEntity<Entity>("Cross");
		entity->addComponent<ImageComponent>(ImageComponent(tex->getTexture2D(), Box(0, 0, 50, 50)));
		entity->getComponent<ImageComponent>().SetPosition({ getDisplay()->getClientSize().width / 2.0F - 25, getDisplay()->getClientSize().height / 2.0F - 25 });

		m_Cross = entity->getID();
	}



	//Enable Play Mode - Cursor locked at the center of screen and invisible
	getInputManager()->enablePlayMode(m_input);
}

void SpaceShooterGame::spawnRemotePlayer(uint32_t id)
{
	if (m_remotePlayers.find(id) != m_remotePlayers.end()) return;
	std::string name = "RemoteShip_" + std::to_string(id);
	auto remoteShip = getActiveWorld()->createEntity<ModelEntity>(name.c_str(), m_remoteShipMesh, m_remoteShipMaterials);
	m_remotePlayers[id] = remoteShip->getID();
}

void SpaceShooterGame::removeRemotePlayer(uint32_t id)
{
	auto it = m_remotePlayers.find(id);
	if (it != m_remotePlayers.end())
	{
		getActiveWorld()->destroyEntity<ModelEntity>(it->second);
		m_remotePlayers.erase(it);
	}
}

void SpaceShooterGame::handleNetworkMessages()
{
	if (!m_client || !m_client->IsConnected()) return;
	m_client->UpdateDual();
}

void SpaceShooterGame::processPacket(darkfire::net::Packet& pkt)
{
	auto msgId = pkt.GetIdAs<NetworkMsgTypes>();
	auto reader = pkt.GetReader();

	switch (msgId)
	{
	case NetworkMsgTypes::ServerAccept:
	{
		std::cout << "[CLIENT] Server Accepted Connection\n";
		break;
	}
	case NetworkMsgTypes::ServerDeny:
	{
		std::cout << "[CLIENT] Server Denied Connection\n";
		break;
	}
	case NetworkMsgTypes::ClientAssignID:
	{
		uint32_t myId = 0;
		reader >> myId;
		if (m_client) m_client->setMyClientID(myId);
		std::cout << "[CLIENT] Assigned Client ID: " << myId << "\n";
		break;
	}
	case NetworkMsgTypes::PlayerConnect:
	{
		uint32_t newPlayerId = 0;
		reader >> newPlayerId;
		if (m_client && newPlayerId != m_client->getMyClientID())
		{
			spawnRemotePlayer(newPlayerId);
			std::cout << "[CLIENT] Remote Player Joined: " << newPlayerId << "\n";
		}
		break;
	}
	case NetworkMsgTypes::PlayerDisconnect:
	{
		uint32_t leftPlayerId = 0;
		reader >> leftPlayerId;
		removeRemotePlayer(leftPlayerId);
		std::cout << "[CLIENT] Remote Player Left: " << leftPlayerId << "\n";
		break;
	}
	case NetworkMsgTypes::PlayerMove:
	{
		PlayerNetState state{};
		reader >> state;
		if (m_client && state.clientID != m_client->getMyClientID())
		{
			if (m_remotePlayers.find(state.clientID) == m_remotePlayers.end())
			{
				spawnRemotePlayer(state.clientID);
			}
			auto ent = getActiveWorld()->getEntity<ModelEntity>(m_remotePlayers[state.clientID]);
			if (ent)
			{
				ent->getComponent<TransformComponent>()
					.setPosition(Vector3d(state.posX, state.posY, state.posZ))
					.setRotation(Vector3d(state.rotX, state.rotY, state.rotZ))
					.setScale(Vector3d(state.scale, state.scale, state.scale));
			}
		}
		break;
	}
	case NetworkMsgTypes::PlayerShoot:
	{
		PlayerShootData shoot{};
		reader >> shoot;
		if (m_client && shoot.clientID != m_client->getMyClientID())
		{
			auto proj = getActiveWorld()->createEntity<Projectile>("RemoteProj");
			TransformComponent& projTrans = proj->getComponent<TransformComponent>();
			projTrans.m_Position = Vector3d(shoot.posX, shoot.posY, shoot.posZ);
			proj->setDirection(Vector3d(shoot.dirX, shoot.dirY, shoot.dirZ));
			proj->setSpeed(shoot.speed > 0 ? shoot.speed : 500.0f);
			proj->setLifeTime(15.0f);
			proj->setDamage(10.0f);
		}
		break;
	}
	default:
		break;
	}
}

void SpaceShooterGame::onUpdate(float deltaTime)
{
	if (getInputManager()->isKeyUp(Core::Key::Escape))
	{
		m_input = !m_input;
		getInputManager()->enablePlayMode(m_input);
	}

	if (getInputManager()->isKeyUp(Core::Key::Space))
	{
		m_fullscreen = !m_fullscreen;
		getDisplay()->getSwapChain()->setFullScreen(m_fullscreen, getDisplay()->getScreenSize());
	}

	handleNetworkMessages();

	auto entity = getActiveWorld()->getEntity<Entity>(m_UI);
	auto pos = getActiveWorld()->getEntity<Spaceship>(m_spaceID)->getComponent<TransformComponent>().m_Position;

	auto entityCross = getActiveWorld()->getEntity<Entity>(m_Cross);
	entityCross->getComponent<ImageComponent>().SetPosition({ getDisplay()->getClientSize().width / 2.0F - 25, getDisplay()->getClientSize().height / 2.0F - 25 });
	
	std::wstringstream ss;
	ss << "FPS: " << (int)(1.0f / deltaTime);
	if (m_client && m_client->IsConnected())
	{
		ss << " | Multiplayer: ONLINE (ID: " << m_client->getMyClientID() << ", Remote Players: " << m_remotePlayers.size() << ")";
	}
	else
	{
		ss << " | Multiplayer: OFFLINE (Single Player)";
	}
	ss << "\n\nPress 'ESC' to toggle cursor\nPress 'SPACE' to toggle fullscreen";
	ss << "\nEntities: " << getActiveWorld()->getEntities().size()
	   << "\nAsteroids: " << getActiveWorld()->getAllEntities<Asteroid>().size()
	   << "\nSpaceship: " << pos.x << ", " << pos.y << ", " << pos.z;
	entity->getComponent<TextComponent>().setText(ss.str().c_str());	
}

void SpaceShooterGame::onQuit()
{
	if (m_client)
		m_client->DisconnectDual();
}