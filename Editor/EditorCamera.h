#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"

namespace Editor
{
	using Core::Vector2;
	using Core::Vector3;
	using Core::Matrix;
	using Core::Quaternion;

	class EditorCamera
	{
	public:
		EditorCamera();
		EditorCamera(float fov, float aspectRatio, float nearZ, float farZ);

		void onUpdate(float deltaTime, bool isViewportHovered, bool isViewportFocused);

		void setViewportSize(uint32_t width, uint32_t height);
		void focusOn(const Vector3& target, float radius = 5.0f);
		void setAngles(float pitch, float yaw);

		const Matrix& getViewMatrix() const noexcept { return m_viewMatrix; }
		const Matrix& getProjectionMatrix() const noexcept { return m_projectionMatrix; }
		const Matrix& getViewProjectionMatrix() const noexcept { return m_viewProjMatrix; }
		const Matrix& getInverseViewProjectionMatrix() const noexcept { return m_invViewProjMatrix; }

		Vector3 getPosition() const noexcept { return m_position; }
		Vector3 getForward() const noexcept { return m_forward; }
		Vector3 getUp() const noexcept { return m_up; }
		Vector3 getRight() const noexcept { return m_right; }

		float getNearZ() const noexcept { return m_nearZ; }
		float getFarZ() const noexcept { return m_farZ; }
		float getFov() const noexcept { return m_fov; }
		float getAspectRatio() const noexcept { return m_aspectRatio; }

		bool isFlyModeActive() const noexcept { return m_isFlyMode; }

	private:
		void updateCameraVectors();
		void updateMatrices();

		void mousePan(const Vector2& delta);
		void mouseRotate(const Vector2& delta);
		void mouseZoom(float delta);

		Vector3 calculatePosition() const;

		float m_fov = 60.0f; // degrees
		float m_aspectRatio = 16.0f / 9.0f;
		float m_nearZ = 0.1f;
		float m_farZ = 1000.0f;

		Matrix m_viewMatrix = Matrix{};
		Matrix m_projectionMatrix = Matrix{};
		Matrix m_viewProjMatrix = Matrix{};
		Matrix m_invViewProjMatrix = Matrix{};

		Vector3 m_position = Vector3(0.0f, 6.0f, -10.0f);
		Vector3 m_focalPoint = Vector3(0.0f, 1.0f, 0.0f);
		Vector3 m_forward = Vector3(0.0f, 0.0f, 1.0f);
		Vector3 m_up = Vector3(0.0f, 1.0f, 0.0f);
		Vector3 m_right = Vector3(1.0f, 0.0f, 0.0f);

		float m_distance = 12.0f;
		float m_pitch = -0.50f;
		float m_yaw = 0.0f;

		uint32_t m_viewportWidth = 1280;
		uint32_t m_viewportHeight = 720;

		bool m_isFlyMode = false;
		float m_flySpeed = 10.0f;
	};
}
