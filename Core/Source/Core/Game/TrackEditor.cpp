#include "TrackEditor.h"
#include "Game.h"
#include "World.h"
#include "IEntity.h"
#include "Component/TransformComponent.h"
#include "Component/CameraComponent.h"
#include "Component/MeshComponent.h"
#include "Component/ColliderComponent.h"
#include "Core/Input/InputManager.h"
#include "Core/Graphics/DebugRenderer.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Resource/AssetHelper.h"
#include "Core/Log/Log.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>

namespace Core
{
	TrackEditor::TrackEditor(Game* game)
		: m_game(game)
	{
	}

	void TrackEditor::setEditorActive(bool active)
	{
		m_isEditorMode = active;
		if (m_game && m_game->getInputManager())
		{
			// When editor is active, mouse cursor is free
			m_game->getInputManager()->enablePlayMode(!active);
		}
		darklog.info(L"TrackEditor: " + std::wstring(active ? L"ENABLED" : L"DISABLED"));
	}

	void TrackEditor::toggleEditor()
	{
		setEditorActive(!m_isEditorMode);
	}

	void TrackEditor::selectPalette(int index)
	{
		if (index >= 0 && index <= 5)
		{
			m_selectedPaletteIndex = index;
		}
	}

	Ray TrackEditor::computeMouseRay(const Vector2d& mousePos, const Mat4x4& view, const Mat4x4& proj, int screenWidth, int screenHeight)
	{
		Ray ray;
		if (screenWidth <= 0 || screenHeight <= 0)
			return ray;

		float ndcX = (2.0f * mousePos.x / static_cast<float>(screenWidth)) - 1.0f;
		float ndcY = 1.0f - (2.0f * mousePos.y / static_cast<float>(screenHeight));

		// Camera basis vectors from view matrix
		Vector3d camRight(view.mat[0][0], view.mat[1][0], view.mat[2][0]);
		Vector3d camUp(view.mat[0][1], view.mat[1][1], view.mat[2][1]);
		Vector3d camForward(view.mat[0][2], view.mat[1][2], view.mat[2][2]);

		// Extract camera position from inverse view translation
		Vector3d camPos(
			-(view.mat[3][0] * view.mat[0][0] + view.mat[3][1] * view.mat[0][1] + view.mat[3][2] * view.mat[0][2]),
			-(view.mat[3][0] * view.mat[1][0] + view.mat[3][1] * view.mat[1][1] + view.mat[3][2] * view.mat[1][2]),
			-(view.mat[3][0] * view.mat[2][0] + view.mat[3][1] * view.mat[2][1] + view.mat[3][2] * view.mat[2][2])
		);

		float aspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
		float fovTan = 0.76f; // tan(fov / 2) for fov ~ 1.3 rad

		Vector3d rayDir = camForward + (camRight * (ndcX * fovTan * aspect)) + (camUp * (ndcY * fovTan));
		ray.origin = camPos;
		ray.direction = Vector3d::normalize(rayDir);
		return ray;
	}

	void TrackEditor::updateFlyCamera(float deltaTime)
	{
		InputManager* input = m_game->getInputManager();
		if (!input)
			return;

		// Mouse look when holding Right Mouse Button
		if (input->isMouseDown(MouseButton::Right))
		{
			m_cameraRot.y += input->getMouseXAxis() * 0.003f;
			m_cameraRot.x += input->getMouseYAxis() * 0.003f;

			// Clamp pitch
			m_cameraRot.x = (std::max)(-1.5f, (std::min)(1.5f, m_cameraRot.x));
		}

		float pitch = m_cameraRot.x;
		float yaw = m_cameraRot.y;

		Vector3d forward(
			std::sin(yaw) * std::cos(pitch),
			-std::sin(pitch),
			std::cos(yaw) * std::cos(pitch)
		);
		forward = Vector3d::normalize(forward);

		Vector3d right(std::cos(yaw), 0, -std::sin(yaw));
		right = Vector3d::normalize(right);

		Vector3d up(0, 1, 0);

		float speed = m_camSpeed;
		if (input->isKeyDown(Key::Shift))
			speed *= 3.0f;

		if (input->isKeyDown(Key::W)) m_cameraPos = m_cameraPos + (forward * (speed * deltaTime));
		if (input->isKeyDown(Key::S)) m_cameraPos = m_cameraPos - (forward * (speed * deltaTime));
		if (input->isKeyDown(Key::D)) m_cameraPos = m_cameraPos + (right * (speed * deltaTime));
		if (input->isKeyDown(Key::A)) m_cameraPos = m_cameraPos - (right * (speed * deltaTime));
		if (input->isKeyDown(Key::E)) m_cameraPos = m_cameraPos + (up * (speed * deltaTime));
		if (input->isKeyDown(Key::Q)) m_cameraPos = m_cameraPos - (up * (speed * deltaTime));

		// Update Main Camera Entity
		auto world = m_game->getActiveWorld();
		if (world)
		{
			auto camEntity = world->getEntity<Entity>(world->getMainCameraID());
			if (camEntity && camEntity->hasComponent<TransformComponent>())
			{
				camEntity->getComponent<TransformComponent>()
					.setPosition(m_cameraPos)
					.setRotation(m_cameraRot);
			}
		}
	}

	void TrackEditor::spawnTrackPieceEntity(PlacedTrackPiece& piece)
	{
		auto world = m_game->getActiveWorld();
		if (!world)
			return;

		auto entity = AssetHelper::SpawnModel(
			world.get(),
			piece.name,
			piece.meshPath,
			piece.texturePath,
			piece.position,
			piece.rotation,
			piece.scale
		);

		if (entity)
		{
			// Attach box collider for raycast selection and physics
			entity->addComponent<BoxColliderComponent>(BoxColliderComponent(piece.scale * 0.5f));
			piece.entityID = entity->getID();
		}
	}

	void TrackEditor::update(float deltaTime)
	{
		InputManager* input = m_game->getInputManager();
		if (!input)
			return;

		// Toggle editor mode with F2
		if (input->isKeyPressed(Key::F2))
		{
			toggleEditor();
		}

		// Toggle debug physics wireframes with F1
		if (input->isKeyPressed(Key::F1))
		{
			DebugRenderer::get().toggle();
		}

		if (!m_isEditorMode)
			return;

		// 1. Flight Camera
		updateFlyCamera(deltaTime);

		// 2. Palette Keys (1-6)
		if (input->isKeyPressed(Key::N1)) selectPalette(0);
		if (input->isKeyPressed(Key::N2)) selectPalette(1);
		if (input->isKeyPressed(Key::N3)) selectPalette(2);
		if (input->isKeyPressed(Key::N4)) selectPalette(3);
		if (input->isKeyPressed(Key::N5)) selectPalette(4);
		if (input->isKeyPressed(Key::N6)) selectPalette(5);

		// Toggle Waypoint Tool with W key (when not holding Right Mouse Button)
		if (!input->isMouseDown(MouseButton::Right) && input->isKeyPressed(Key::W))
		{
			toggleWaypointTool();
			darklog.info(L"TrackEditor: Waypoint Tool " + std::wstring(m_waypointToolActive ? L"ACTIVE" : L"INACTIVE"));
		}

		// Rotate piece with R key
		if (input->isKeyPressed(Key::R))
		{
			m_currentPlacementYaw += 3.14159265f / 4.0f; // 45 degrees
			if (m_currentPlacementYaw >= 6.2831853f)
				m_currentPlacementYaw -= 6.2831853f;
		}

		// Save track with F5, Load with F9
		if (input->isKeyPressed(Key::F5)) saveTrack("track_01");
		if (input->isKeyPressed(Key::F9)) loadTrack("track_01");

		// 3. Raycast cursor against terrain/colliders
		auto world = m_game->getActiveWorld();
		if (world)
		{
			auto camEntity = world->getEntity<Entity>(world->getMainCameraID());
			if (camEntity && camEntity->hasComponent<CameraComponent>())
			{
				auto& camComp = camEntity->getComponent<CameraComponent>();
				int screenWidth = camComp.m_Screen.width;
				int screenHeight = camComp.m_Screen.height;

				Ray ray = computeMouseRay(input->getMousePosition(), camComp.m_ViewMatrix, camComp.m_ProjectionMatrix, screenWidth, screenHeight);

				RaycastHit hit;
				if (m_game->getPhysicsEngine()->raycast(ray, hit, 2000.0f))
				{
					m_previewPosition = hit.point;
					m_hasGroundHit = true;

					// Left Click: Place piece or Waypoint
					if (!input->isMouseDown(MouseButton::Right) && input->isMousePressed(MouseButton::Left))
					{
						if (m_waypointToolActive)
						{
							TrackWaypoint wp;
							wp.position = hit.point + Vector3d(0, 1.0f, 0);
							m_waypoints.push_back(wp);
							darklog.info(L"Waypoint placed at: (" + std::to_wstring((int)wp.position.x) + L", " + std::to_wstring((int)wp.position.z) + L")");
						}
						else
						{
							PlacedTrackPiece piece;
							piece.position = hit.point;
							piece.rotation = Vector3d(0, m_currentPlacementYaw, 0);

							switch (m_selectedPaletteIndex)
							{
							case 0: // Straight Road
								piece.name = "RoadStraight_" + std::to_string(m_placedPieces.size());
								piece.pieceType = "RoadStraight";
								piece.meshPath = L"Assets/Meshes/box.obj";
								piece.texturePath = L"Assets/Textures/grass.jpg";
								piece.scale = Vector3d(14, 0.2f, 25);
								break;
							case 1: // Curved Road
								piece.name = "RoadCurve_" + std::to_string(m_placedPieces.size());
								piece.pieceType = "RoadCurve";
								piece.meshPath = L"Assets/Meshes/box.obj";
								piece.texturePath = L"Assets/Textures/grass.jpg";
								piece.scale = Vector3d(14, 0.2f, 18);
								break;
							case 2: // Boost Pad
								piece.name = "BoostPad_" + std::to_string(m_placedPieces.size());
								piece.pieceType = "BoostPad";
								piece.meshPath = L"Assets/Meshes/box.obj";
								piece.texturePath = L"Assets/Textures/asteroid.jpg";
								piece.scale = Vector3d(6, 0.25f, 4);
								break;
							case 3: // Item Box
								piece.name = "ItemBox_" + std::to_string(m_placedPieces.size());
								piece.pieceType = "ItemBox";
								piece.meshPath = L"Assets/Meshes/box.obj";
								piece.texturePath = L"Assets/Textures/asteroid.jpg";
								piece.scale = Vector3d(1.8f, 1.8f, 1.8f);
								piece.position.y += 1.5f;
								break;
							case 4: // Barrier Guardrail
								piece.name = "Barrier_" + std::to_string(m_placedPieces.size());
								piece.pieceType = "Barrier";
								piece.meshPath = L"Assets/Meshes/box.obj";
								piece.texturePath = L"Assets/Textures/spaceship.jpg";
								piece.scale = Vector3d(1, 1.5f, 25);
								break;
							case 5: // Tree Prop
								piece.name = "Tree_" + std::to_string(m_placedPieces.size());
								piece.pieceType = "Tree";
								piece.meshPath = L"Assets/Meshes/sphere.obj";
								piece.texturePath = L"Assets/Textures/grass.jpg";
								piece.scale = Vector3d(3, 8, 3);
								break;
							}

							spawnTrackPieceEntity(piece);
							m_placedPieces.push_back(piece);
						}
					}
				}
				else
				{
					m_hasGroundHit = false;
				}
			}
		}
	}

	void TrackEditor::render(GraphicsEngine* gfx, const RenderContext& ctx)
	{
		if (!m_isEditorMode)
			return;

		// 1. Placement Preview Box
		if (m_hasGroundHit && !m_waypointToolActive)
		{
			Vector3d previewExtents(7, 0.5f, 12.5f);
			if (m_selectedPaletteIndex == 2) previewExtents = Vector3d(3, 0.25f, 2);
			if (m_selectedPaletteIndex == 3) previewExtents = Vector3d(1, 1, 1);
			if (m_selectedPaletteIndex == 4) previewExtents = Vector3d(0.5f, 0.8f, 12.5f);
			if (m_selectedPaletteIndex == 5) previewExtents = Vector3d(1.5f, 4.0f, 1.5f);

			DebugRenderer::get().drawWireBox(m_previewPosition + Vector3d(0, previewExtents.y, 0), previewExtents, Vector4d(0, 1, 0.3f, 1));
		}

		// 2. Waypoint Spline Ribbon and Waypoint Spheres
		if (!m_waypoints.empty())
		{
			std::vector<Vector3d> wpPoints;
			wpPoints.reserve(m_waypoints.size());
			for (size_t i = 0; i < m_waypoints.size(); ++i)
			{
				wpPoints.push_back(m_waypoints[i].position);
				DebugRenderer::get().drawWireSphere(m_waypoints[i].position, 1.5f, Vector4d(1, 0.9f, 0.1f, 1), 12);
			}

			// Draw connecting line ribbon
			DebugRenderer::get().drawSpline(wpPoints, Vector4d(1, 0.2f, 0.8f, 1), true);
		}
	}

	bool TrackEditor::saveTrack(const std::string& trackName)
	{
		std::string dir = "Assets/Tracks";
		if (!std::filesystem::exists(dir))
		{
			std::filesystem::create_directories(dir);
		}

		std::string filePath = dir + "/" + trackName + ".json";
		std::ofstream out(filePath);
		if (!out.is_open())
		{
			darklog.error(L"TrackEditor: Failed to open file for save: " + std::wstring(filePath.begin(), filePath.end()));
			return false;
		}

		out << "{\n";
		out << "  \"name\": \"" << trackName << "\",\n";

		// Waypoints
		out << "  \"waypoints\": [\n";
		for (size_t i = 0; i < m_waypoints.size(); ++i)
		{
			out << "    {\"x\": " << m_waypoints[i].position.x
			    << ", \"y\": " << m_waypoints[i].position.y
			    << ", \"z\": " << m_waypoints[i].position.z
			    << ", \"width\": " << m_waypoints[i].width << "}";
			if (i + 1 < m_waypoints.size()) out << ",";
			out << "\n";
		}
		out << "  ],\n";

		// Placed Pieces
		out << "  \"pieces\": [\n";
		for (size_t i = 0; i < m_placedPieces.size(); ++i)
		{
			const auto& p = m_placedPieces[i];
			std::string meshA(p.meshPath.begin(), p.meshPath.end());
			std::string texA(p.texturePath.begin(), p.texturePath.end());

			out << "    {\n"
			    << "      \"name\": \"" << p.name << "\",\n"
			    << "      \"type\": \"" << p.pieceType << "\",\n"
			    << "      \"mesh\": \"" << meshA << "\",\n"
			    << "      \"texture\": \"" << texA << "\",\n"
			    << "      \"posX\": " << p.position.x << ", \"posY\": " << p.position.y << ", \"posZ\": " << p.position.z << ",\n"
			    << "      \"rotX\": " << p.rotation.x << ", \"rotY\": " << p.rotation.y << ", \"rotZ\": " << p.rotation.z << ",\n"
			    << "      \"scaleX\": " << p.scale.x << ", \"scaleY\": " << p.scale.y << ", \"scaleZ\": " << p.scale.z << "\n"
			    << "    }";
			if (i + 1 < m_placedPieces.size()) out << ",";
			out << "\n";
		}
		out << "  ]\n";
		out << "}\n";

		darklog.info(L"TrackEditor: Saved " + std::to_wstring(m_placedPieces.size()) + L" pieces and " +
		             std::to_wstring(m_waypoints.size()) + L" waypoints to " + std::wstring(filePath.begin(), filePath.end()));
		return true;
	}

	bool TrackEditor::loadTrack(const std::string& trackName)
	{
		std::string filePath = "Assets/Tracks/" + trackName + ".json";
		std::ifstream in(filePath);
		if (!in.is_open())
		{
			darklog.warn(L"TrackEditor: Track file not found: " + std::wstring(filePath.begin(), filePath.end()));
			return false;
		}

		clearTrack();

		std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

		// Lightweight parse of waypoints
		size_t wpPos = content.find("\"waypoints\"");
		if (wpPos != std::string::npos)
		{
			size_t openBracket = content.find('[', wpPos);
			size_t closeBracket = content.find(']', openBracket);
			if (openBracket != std::string::npos && closeBracket != std::string::npos)
			{
				std::string wpSection = content.substr(openBracket, closeBracket - openBracket);
				std::stringstream ss(wpSection);
				std::string line;
				while (std::getline(ss, line))
				{
					size_t xPos = line.find("\"x\":");
					if (xPos != std::string::npos)
					{
						float x = 0, y = 0, z = 0, w = 14;
						sscanf_s(line.c_str() + xPos, "\"x\": %f, \"y\": %f, \"z\": %f, \"width\": %f", &x, &y, &z, &w);
						TrackWaypoint wp;
						wp.position = Vector3d(x, y, z);
						wp.width = w;
						m_waypoints.push_back(wp);
					}
				}
			}
		}

		// Lightweight parse of pieces
		size_t pcPos = content.find("\"pieces\"");
		if (pcPos != std::string::npos)
		{
			size_t cur = pcPos;
			while ((cur = content.find('{', cur + 1)) != std::string::npos)
			{
				size_t endObj = content.find('}', cur);
				if (endObj == std::string::npos) break;

				std::string block = content.substr(cur, endObj - cur);
				PlacedTrackPiece piece;

				auto extractStr = [&](const std::string& key) -> std::string {
					size_t k = block.find("\"" + key + "\":");
					if (k == std::string::npos) return "";
					size_t q1 = block.find('"', k + key.size() + 3);
					size_t q2 = block.find('"', q1 + 1);
					return block.substr(q1 + 1, q2 - q1 - 1);
				};

				auto extractFloat = [&](const std::string& key) -> float {
					size_t k = block.find("\"" + key + "\":");
					if (k == std::string::npos) return 0.0f;
					float val = 0.0f;
					sscanf_s(block.c_str() + k + key.size() + 3, "%f", &val);
					return val;
				};

				piece.name = extractStr("name");
				piece.pieceType = extractStr("type");
				std::string meshA = extractStr("mesh");
				std::string texA = extractStr("texture");
				piece.meshPath = std::wstring(meshA.begin(), meshA.end());
				piece.texturePath = std::wstring(texA.begin(), texA.end());

				piece.position = Vector3d(extractFloat("posX"), extractFloat("posY"), extractFloat("posZ"));
				piece.rotation = Vector3d(extractFloat("rotX"), extractFloat("rotY"), extractFloat("rotZ"));
				piece.scale = Vector3d(extractFloat("scaleX"), extractFloat("scaleY"), extractFloat("scaleZ"));

				if (!piece.meshPath.empty())
				{
					spawnTrackPieceEntity(piece);
					m_placedPieces.push_back(piece);
				}
			}
		}

		darklog.info(L"TrackEditor: Loaded track with " + std::to_wstring(m_placedPieces.size()) + L" pieces and " +
		             std::to_wstring(m_waypoints.size()) + L" waypoints");
		return true;
	}

	void TrackEditor::clearTrack()
	{
		auto world = m_game->getActiveWorld();
		if (world)
		{
			for (const auto& piece : m_placedPieces)
			{
				if (piece.entityID != NULL_ENTITY)
				{
					world->destroyEntity<ModelEntity>(piece.entityID);
				}
			}
		}
		m_placedPieces.clear();
		m_waypoints.clear();
	}
}
