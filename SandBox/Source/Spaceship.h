#pragma once
#include "Core/All.h"
#include "SpaceShooterGame.h"

using namespace Core;

class Spaceship : public ModelEntity
{
public:
	Spaceship(MeshPtr mesh,std::vector<MaterialPtr> materials, EntityID camID, NetworkClient* client);
	~Spaceship();
public:
	void onCreate() override;
	void onUpdate(float deltaTime) override;
	void onDestroy() override;
public:
	Vector3d will_collide(Vector3d newpos);
private:
	float m_pitch = 0.0f;
	float m_yaw = 0.0f;

	float m_oldPitch = 0;
	float m_oldYaw = 0;

	float m_camPitch = 0;
	float m_camYaw = 0;

	float m_oldCamPitch = 0;
	float m_oldCamYaw = 0;


	bool m_turbo_mode = false;

	float m_current_cam_distance = 0.0f;
	float m_cam_distance = 18.0f;


	EntityID m_camID;
	NetworkClient* m_client;
};

