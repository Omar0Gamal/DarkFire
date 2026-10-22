#include "Editor/EditorCamera.h"
#include "Engine/Platform/Input.h"
#include "Engine/Platform/KeyCodes.h"
#include <algorithm>
#include <cmath>

namespace Editor
{
	EditorCamera::EditorCamera()
		: EditorCamera(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f)
	{
	}

	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearZ, float farZ)
		: m_fov(fov)
		, m_aspectRatio(aspectRatio)
		, m_nearZ(nearZ)
		, m_farZ(farZ)
		, m_distance(12.0f)
		, m_pitch(-0.50f)
		, m_yaw(0.0f)
	{
		m_position = calculatePosition();
		updateCameraVectors();
		updateMatrices();
	}

	void EditorCamera::setViewportSize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
		{
			return;
		}

		m_viewportWidth = width;
		m_viewportHeight = height;
		m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		updateMatrices();
	}

	void EditorCamera::focusOn(const Vector3& target, float radius)
	{
		m_focalPoint = target;
		m_distance = std::max(radius * 2.0f, 2.0f);
		m_position = calculatePosition();
		updateCameraVectors();
		updateMatrices();
	}

	void EditorCamera::setAngles(float pitch, float yaw)
	{
		m_pitch = std::clamp(pitch, -Core::Math::HALF_PI + 0.01f, Core::Math::HALF_PI - 0.01f);
		m_yaw = yaw;
		m_position = calculatePosition();
		updateCameraVectors();
		updateMatrices();
	}

	void EditorCamera::onUpdate(float deltaTime, bool isViewportHovered, bool isViewportFocused)
	{
		using namespace Platform;

		Vector2 mouseDelta = Input::getMouseDelta();
		float scrollDelta = Input::getMouseScrollDelta();

		bool altHeld = Input::isKeyPressed(KeyCode::LeftAlt) || Input::isKeyPressed(KeyCode::RightAlt);
		bool rmbHeld = Input::isMouseButtonPressed(MouseCode::Right);
		bool lmbHeld = Input::isMouseButtonPressed(MouseCode::Left);
		bool mmbHeld = Input::isMouseButtonPressed(MouseCode::Middle);
		bool shiftHeld = Input::isKeyPressed(KeyCode::LeftShift) || Input::isKeyPressed(KeyCode::RightShift);

		if (isViewportHovered || isViewportFocused)
		{
			// Zoom via scroll wheel
			if (std::abs(scrollDelta) > 0.001f)
			{
				mouseZoom(scrollDelta * 2.0f);
			}

			// Fly camera mode (holding RMB)
			if (rmbHeld && !altHeld)
			{
				m_isFlyMode = true;

				// Mouse look (standard inverted delta so mouse UP looks UP)
				float lookSpeed = 0.003f;
				m_yaw += mouseDelta.x * lookSpeed;
				m_pitch -= mouseDelta.y * lookSpeed;
				m_pitch = std::clamp(m_pitch, -Core::Math::HALF_PI + 0.01f, Core::Math::HALF_PI - 0.01f);
				updateCameraVectors();
				m_focalPoint = m_position + m_forward * m_distance;

				// Movement
				float speed = m_flySpeed * (shiftHeld ? 3.0f : 1.0f);
				Vector3 moveDir(0.0f, 0.0f, 0.0f);

				if (Input::isKeyPressed(KeyCode::W)) moveDir += m_forward;
				if (Input::isKeyPressed(KeyCode::S)) moveDir -= m_forward;
				if (Input::isKeyPressed(KeyCode::D)) moveDir += m_right;
				if (Input::isKeyPressed(KeyCode::A)) moveDir -= m_right;
				if (Input::isKeyPressed(KeyCode::E)) moveDir += m_up;
				if (Input::isKeyPressed(KeyCode::Q)) moveDir -= m_up;

				if (moveDir.LengthSquared() > 0.0001f)
				{
					moveDir.Normalize();
					m_position += moveDir * (speed * deltaTime);
					m_focalPoint = m_position + m_forward * m_distance;
				}
			}
			else
			{
				m_isFlyMode = false;

				if (altHeld)
				{
					if (lmbHeld)
					{
						mouseRotate(mouseDelta);
					}
					else if (mmbHeld)
					{
						mousePan(mouseDelta);
					}
					else if (rmbHeld)
					{
						mouseZoom(mouseDelta.y * 0.05f);
					}
				}
				else if (mmbHeld)
				{
					mousePan(mouseDelta);
				}
			}
		}
		else
		{
			m_isFlyMode = false;
		}

		if (!m_isFlyMode)
		{
			m_position = calculatePosition();
			updateCameraVectors();
		}

		updateMatrices();
	}

	void EditorCamera::mousePan(const Vector2& delta)
	{
		float panSpeed = 0.002f * m_distance;
		Vector3 panOffset = -m_right * (delta.x * panSpeed) - m_up * (delta.y * panSpeed);
		m_focalPoint += panOffset;
		m_position += panOffset;
	}

	void EditorCamera::mouseRotate(const Vector2& delta)
	{
		float rotSpeed = 0.005f;
		m_yaw += delta.x * rotSpeed;
		m_pitch += delta.y * rotSpeed;
		m_pitch = std::clamp(m_pitch, -Core::Math::HALF_PI + 0.01f, Core::Math::HALF_PI - 0.01f);
	}

	void EditorCamera::mouseZoom(float delta)
	{
		float zoomSpeed = 0.15f * m_distance;
		m_distance -= delta * zoomSpeed;
		if (m_distance < 0.2f)
		{
			m_focalPoint += m_forward * (delta * 0.5f);
			m_distance = 0.2f;
		}
	}

	Vector3 EditorCamera::calculatePosition() const
	{
		float cosPitch = std::cos(m_pitch);
		float sinPitch = std::sin(m_pitch);
		float cosYaw = std::cos(m_yaw);
		float sinYaw = std::sin(m_yaw);

		// Left-handed: +Z forward, +Y up, +X right
		Vector3 dir(cosPitch * sinYaw, sinPitch, cosPitch * cosYaw);
		return m_focalPoint - dir * m_distance;
	}

	void EditorCamera::updateCameraVectors()
	{
		if (m_isFlyMode)
		{
			float cosPitch = std::cos(m_pitch);
			float sinPitch = std::sin(m_pitch);
			float cosYaw = std::cos(m_yaw);
			float sinYaw = std::sin(m_yaw);

			m_forward = Vector3(cosPitch * sinYaw, sinPitch, cosPitch * cosYaw);
			m_forward.Normalize();
		}
		else
		{
			m_forward = m_focalPoint - m_position;
			m_forward.Normalize();
		}

		m_right = Vector3(0.0f, 1.0f, 0.0f).Cross(m_forward);
		m_right.Normalize();
		m_up = m_forward.Cross(m_right);
		m_up.Normalize();
	}

	void EditorCamera::updateMatrices()
	{
		m_viewMatrix = Core::Math::lookAtLH(m_position, m_position + m_forward, m_up);
		m_projectionMatrix = Core::Math::perspectiveFovLH_ReversedZ(
			Core::Math::toRadians(m_fov),
			m_aspectRatio,
			m_nearZ,
			m_farZ
		);
		m_viewProjMatrix = m_viewMatrix * m_projectionMatrix;
		m_invViewProjMatrix = m_viewProjMatrix.Invert();
	}
}
