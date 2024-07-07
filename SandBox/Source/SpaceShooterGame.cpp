#include "SpaceShooterGame.h"
#include "Core/Graphics/SwapChain.h"
#include "Spaceship.h"
#include "Asteroid.h"

SpaceShooterGame::SpaceShooterGame(const wchar_t* titles, int width, int height,NetworkClient * client)
	: Core::Game(titles, width, height), m_client(client)
{

}

SpaceShooterGame::~SpaceShooterGame()
{
}

void SpaceShooterGame::onCreate()
{
	getActiveWorld()->setGravity(0);
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

		auto entity = getActiveWorld()->createEntity<Spaceship>("Spaceship", createMesh(L"Assets/Meshes/spaceship.obj"),std::vector<MaterialPtr> {mat}, cam->getID(), m_client);
		m_spaceID = entity->getID();
	}

	//Enable Play Mode - Cursor locked at the center of screen and invisible
	getInputManager()->enablePlayMode(m_input);
}


void SpaceShooterGame::onUpdate(float deltaTime)
{

	if (getInputManager()->isKeyUp(Core::Key::Escape))
	{
		m_input = !m_input;
		getInputManager()->enablePlayMode(m_input);
	}

	if(getInputManager()->isKeyUp(Core::Key::Space))
	{

		m_fullscreen = !m_fullscreen;
		getDisplay()->getSwapChain()->setFullScreen(m_fullscreen, getDisplay()->getScreenSize());

	}

	if (m_client->IsConnected())
	{
		if (!m_client->Incoming().empty())
		{
			auto msg = m_client->Incoming().pop_front().msg;

			switch (msg.header.id)
			{
			case NetworkMsgTypes::ServerAccept:
			{
				std::cout << "Server Accepted Connection\n";
				break;
			}
			case NetworkMsgTypes::ServerDeny:
			{
				std::cout << "Server Denied Connection\n";
				break;
			}
			}
		}
	}
}

void SpaceShooterGame::onQuit()
{
	m_client->Disconnect();
}