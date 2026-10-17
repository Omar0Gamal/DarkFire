#include "Spaceship.h"
#include "Asteroid.h"
#include "Projectile.h"
#include <algorithm>

Spaceship::Spaceship(MeshPtr mesh, std::vector<MaterialPtr> materials, EntityID camID,NetworkClient* client)
	:ModelEntity(mesh, materials), m_camID(camID), m_client(client)
{

}

Spaceship::~Spaceship()
{
}

void Spaceship::onCreate()
{
	ModelEntity::onCreate();
}

void Spaceship::onUpdate(float deltaTime)
{
	auto ecs = m_World->getECS();
	auto input = m_World->getGame()->getInputManager();

	float targetForward = 0.0f;
	static float currentForward = 0.0f;
	
	float targetRightward = 0.0f;
	static float currentRightward = 0.0f;

	float speed = 1.0f;
	bool turbo = false;

	float mouseSensitivity = 0.01f;
	float smoothedYaw = 0.0f;
	float smoothedPitch = 0.0f;

	//Spaceship controls
	if (input->isKeyDown(Key::W))
	{
		targetForward = 1.0f;
	}
	if (input->isKeyDown(Key::S))
	{
		targetForward = -1.0f;
	}
	if (input->isKeyDown(Key::A))
	{
		targetRightward = -1.0f;
	}
	if (input->isKeyDown(Key::D))
	{
		targetRightward = 1.0f;
	}
	if (input->isKeyDown(Key::Shift))
	{
		speed = 3.0f;
		turbo = true;
	}

	//Handle position and rotation of spaceship and camera
	//With smooth movements, thanks to the lerp function

	currentForward = std::lerp(currentForward, targetForward, 6.0f * deltaTime);
	currentRightward = std::lerp(currentRightward, targetRightward, 6.0f * deltaTime);

	float targetDistance = 18.0f; // default

	if (targetForward > 0.0f) {
		targetDistance = turbo ? 25.0f : 20.0f;
	}
	else if (targetForward < 0.0f) {
		targetDistance = turbo ? 5.0f : 9.0f;
	}

	// Smooth interpolation
	m_cam_distance = std::lerp(m_cam_distance, targetDistance, 5.0f * deltaTime);


	auto vec = Vector3d::lerp(Vector3d(m_current_cam_distance, 0, 0),
		Vector3d(m_cam_distance, 0, 0), 2.0f * deltaTime);
	m_current_cam_distance = vec.x;

	float mouseX = input->getMouseXAxis();
	float mouseY = input->getMouseYAxis();


	if (input->isPlayModeEnabled())
	{
		m_yaw += mouseX * mouseSensitivity * deltaTime * 60.0f;
		m_pitch += mouseY * mouseSensitivity * deltaTime * 60.0f;
	}

	m_pitch = std::clamp(m_pitch, -1.57f, 1.57f);

	TransformComponent& trans = getComponent<TransformComponent>();

	CameraComponent& cam = ecs->Get<CameraComponent>(m_camID);
	TransformComponent& camTrans = ecs->Get<TransformComponent>(m_camID);

	auto curr = Vector3d::lerp(Vector3d(m_oldPitch, m_oldYaw, 0), Vector3d(m_pitch, m_yaw, 0), 5.0f * deltaTime);
	m_oldPitch = curr.x;
	m_oldYaw = curr.y;

	trans.setRotation(Vector3d(m_oldPitch, m_oldYaw, 0));

	auto curr_cam = Vector3d::lerp(Vector3d(m_camPitch, m_camYaw, 0), Vector3d(m_pitch, m_yaw, 0), 3.0f * deltaTime);
	m_camPitch = curr_cam.x;
	m_camYaw = curr_cam.y;

	camTrans.setRotation(Vector3d(m_camPitch, m_camYaw, 0));

	Mat4x4 w = trans.m_worldMatrix;
	auto zdir = w.getForwardDirection();
	auto xdir = w.getRightwardDirection();
	auto ydir = w.getUpDirection();

	auto pos = trans.m_Position + zdir * currentForward * speed * 100 * deltaTime;
	pos = pos + xdir * currentRightward * speed * 100 * deltaTime;

	pos = will_collide(pos);

	trans.setPosition(pos);

	if (m_client && m_client->IsConnected())
	{
		m_client->sendMove(trans);
	}

	Mat4x4 w2 = camTrans.m_worldMatrix;
	zdir = w2.getForwardDirection();
	xdir = w2.getRightwardDirection();
	ydir = w2.getUpDirection();

	auto camPos = Vector3d(pos + zdir * -m_current_cam_distance);
	camPos = camPos + ydir * 6.5f;

	camTrans.setPosition(camPos);

	//On left mouse click, spawn the projectile along the spaceship direction
	if (input->isMouseUp(MouseButton::Left))
	{
		if (!input->isPlayModeEnabled()) return;
		auto proj = m_World->createEntity<Projectile>("pr");
		TransformComponent& projTrans = proj->getComponent<TransformComponent>();
		projTrans.m_Position = pos;
		proj->setDirection(zdir);
		proj->setSpeed(500.0f);
		proj->setLifeTime(15.0f);
		proj->setDamage(10.0f);
		proj->setOwner(getID());

		if (m_client && m_client->IsConnected())
		{
			m_client->sendShoot(pos, zdir, 500.0f);
		}
	}
}

void Spaceship::onDestroy()
{
}


inline float squared(float v) { return v * v; }
bool sphereCubeIntersect(const Vector3d& S, float R,
	const Vector3d& C1, const Vector3d& C2) {
	float dist_squared = R * R;
	/* assume C1 and C2 are element-wise sorted, if not, do that now */
	if (S.x < C1.x) dist_squared -= squared(S.x - C1.x);
	else if (S.x > C2.x) dist_squared -= squared(S.x - C2.x);
	if (S.y < C1.y) dist_squared -= squared(S.y - C1.y);
	else if (S.y > C2.y) dist_squared -= squared(S.y - C2.y);
	if (S.z < C1.z) dist_squared -= squared(S.z - C1.z);
	else if (S.z > C2.z) dist_squared -= squared(S.z - C2.z);
	return dist_squared > 0;
}



Vector3d findIntersectionPoints(const Vector3d& sphereCenter, float sphereRadius,
	const Vector3d& cubeCorner1, const Vector3d& cubeCorner2) {
	Vector3d pMax;
	for (int i = 0; i < 3; ++i) {
		pMax.x = (std::max)(sphereCenter.x + sphereRadius, (std::max)(cubeCorner1.x, cubeCorner2.x));
		pMax.y = (std::max)(sphereCenter.y + sphereRadius, (std::max)(cubeCorner1.y, cubeCorner2.y));
		pMax.z = (std::max)(sphereCenter.z + sphereRadius, (std::max)(cubeCorner1.z, cubeCorner2.z));
	}
	return pMax;
}

Vector3d Spaceship::will_collide(Vector3d newpos)
{
	Vector3d minCorner = getComponent<MeshComponent>().m_Mesh->getMinCorner() + newpos;
	Vector3d maxCorner = getComponent<MeshComponent>().m_Mesh->getMaxCorner() + newpos;

	for (auto entity : m_World->getAllEntities<Asteroid>()) 
	{
		auto t = entity->getComponent<TransformComponent>();
		float radius = t.m_Scale.x * 2.8f;
		if (sphereCubeIntersect(t.m_Position, radius, minCorner,maxCorner))
		{
			Vector3d intersection = findIntersectionPoints(t.m_Position, radius, minCorner, maxCorner);
			Vector3d diff = diff.normalize(intersection - t.m_Position);
			newpos = newpos + diff;
			return newpos;
		}
	}

	return newpos;
}
