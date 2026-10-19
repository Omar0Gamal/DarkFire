#pragma once
#include "Core/Graphics/Prerequisites.h"
#include "Core/Math/Vector3d.h"
#include "Core/Math/Vector2d.h"
#include "Core/Math/Mat4x4.h"
#include "Core/Graphics/RenderPass.h"
#include "PhysicsEngine.h"
#include <string>
#include <vector>

namespace Core
{
	struct PlacedTrackPiece
	{
		std::string name;
		std::string pieceType;
		std::wstring meshPath;
		std::wstring texturePath;
		Vector3d position = { 0, 0, 0 };
		Vector3d rotation = { 0, 0, 0 };
		Vector3d scale = { 1, 1, 1 };
		EntityID entityID = NULL_ENTITY;
	};

	struct TrackWaypoint
	{
		Vector3d position;
		float width = 14.0f;
	};

	class TrackEditor
	{
	public:
		TrackEditor(Game* game);
		~TrackEditor() = default;

		void update(float deltaTime);
		void render(GraphicsEngine* gfx, const RenderContext& ctx);

		bool isEditorActive() const { return m_isEditorMode; }
		void setEditorActive(bool active);
		void toggleEditor();

		// Palette item selection (0: Straight Road, 1: Curved Road, 2: Boost Pad, 3: Item Box, 4: Barrier, 5: Tree)
		int getSelectedPaletteIndex() const { return m_selectedPaletteIndex; }
		void selectPalette(int index);

		// Waypoint mode
		bool isWaypointToolActive() const { return m_waypointToolActive; }
		void toggleWaypointTool() { m_waypointToolActive = !m_waypointToolActive; }

		// Track save/load to JSON
		bool saveTrack(const std::string& trackName = "track_01");
		bool loadTrack(const std::string& trackName = "track_01");
		void clearTrack();

		const std::vector<TrackWaypoint>& getWaypoints() const { return m_waypoints; }
		const std::vector<PlacedTrackPiece>& getPlacedPieces() const { return m_placedPieces; }

		Vector3d getCameraPosition() const { return m_cameraPos; }
		Vector3d getCameraRotation() const { return m_cameraRot; }

	public:
		static Ray computeMouseRay(const Vector2d& mousePos, const Mat4x4& view, const Mat4x4& proj, int screenWidth, int screenHeight);

	private:
		void updateFlyCamera(float deltaTime);
		void spawnTrackPieceEntity(PlacedTrackPiece& piece);

	private:
		Game* m_game = nullptr;
		bool m_isEditorMode = false;
		bool m_waypointToolActive = false;

		// Free/Fly camera controls
		Vector3d m_cameraPos = { 0, 15, -30 };
		Vector3d m_cameraRot = { 0.35f, 0, 0 }; // Pitch, Yaw, Roll
		float m_camSpeed = 35.0f;

		// Placement settings
		int m_selectedPaletteIndex = 0;
		float m_currentPlacementYaw = 0.0f;
		Vector3d m_previewPosition = { 0, 0, 0 };
		bool m_hasGroundHit = false;

		std::vector<PlacedTrackPiece> m_placedPieces;
		std::vector<TrackWaypoint> m_waypoints;
	};
}
