#include "SpaceShooterGame.h"
#include <iostream>


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR lpCmdLine, int nCmdShow)
{
	try
	{
		NetworkClient client;
		client.Connect("127.0.0.1", 60000);
		SpaceShooterGame game(L"Space Shooter", 1680, 1000,&client);
		game.run();
	}
	catch (const std::exception& e)
	{
		std::string s = "Error: ";
		s += e.what();
		MessageBoxA(NULL, s.c_str(), "DarkFire | DirectX 3D Game | Error", MB_ICONERROR | MB_OK);
	}

	return 0;
}