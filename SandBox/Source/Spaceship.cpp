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

	float forward = 0.0f;
	float rightward = 0.0f;

	float speed = 1.0f;
	bool turbo = false;
	bool shouldSend = false;

	//Spaceship controls
	if (input->isKeyDown(Key::W))
	{
		forward = 1.0f;
		shouldSend = true;
	}
	if (input->isKeyDown(Key::S))
	{
		forward = -1.0f;
		shouldSend = true;
	}
	if (input->isKeyDown(Key::A))
	{
		rightward = -1.0f;
		shouldSend = true;
	}
	if (input->isKeyDown(Key::D))
	{
		rightward = 1.0f;
		shouldSend = true;
	}
	if (input->isKeyDown(Key::Shift))
	{
		speed = 3.0f;
		turbo = true;
		shouldSend = true;
	}

	//Handle position and rotation of spaceship and camera
	//With smooth movements, thanks to the lerp function

	if (forward)
	{
		if (turbo)
		{
			if (forward > 0.0f) m_cam_distance = 25.0f;
			else m_cam_distance = 5.0f;
		}
		else
		{
			if (forward > 0.0f) m_cam_distance = 20.0f;
			else m_cam_distance = 9.0f;
		}
	}
	else
	{
		m_cam_distance = 18.0f;
	}

	auto vec = Vector3d::lerp(Vector3d(m_current_cam_distance, 0, 0),
		Vector3d(m_cam_distance, 0, 0), 2.0f * deltaTime);
	m_current_cam_distance = vec.x;


	if (input->isPlayModeEnabled())
	{
		m_yaw += input->getMouseXAxis() * 0.001f;
		m_pitch += input->getMouseYAxis() * 0.001f;
	}

	if (m_pitch < -1.57f)
		m_pitch = -1.57f;
	else if (m_pitch > 1.57f)
		m_pitch = 1.57f;

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

	auto pos = trans.m_Position + zdir * forward * speed * 100 * deltaTime;
	pos = pos + xdir * rightward * speed * 100 * deltaTime;

	pos = will_collide(pos);

	trans.setPosition(pos);

	if (shouldSend) m_client->sendMove(trans);

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
		m_client->sendShoot();
		projTrans.m_Position = pos;
		proj->setDirection(zdir);
		proj->setSpeed(500.0f);
		proj->setLifeTime(15.0f);
		proj->setDamage(10.0f);
		proj->setOwner(getID());
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



Vector3d findIntersectionPoints(const Vector3d& sphereCenter, double sphereRadius,
	const Vector3d& cubeCorner1, const Vector3d& cubeCorner2) {
	Vector3d pMax;
	for (int i = 0; i < 3; ++i) {
		pMax.x = max(sphereCenter.x + sphereRadius, max(cubeCorner1.x, cubeCorner2.x));
		pMax.y = max(sphereCenter.y + sphereRadius, max(cubeCorner1.y, cubeCorner2.y));
		pMax.z = max(sphereCenter.z + sphereRadius, max(cubeCorner1.z, cubeCorner2.z));
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
		float radius = t.m_Scale.x * 2.8;
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
