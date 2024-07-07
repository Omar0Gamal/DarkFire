#pragma once
#include "Core/Game/IComponent.h"
#include "Core/Math/Mat4x4.h"
#include "Core/Math/Box.h"

namespace Core
{
	class CameraComponent : public IComponent
	{
	public:
		CameraComponent(float farClip, const Box& screen)
			: m_FarClip(farClip), m_Screen(screen)
		{
			m_ProjectionMatrix.setPerspectiveFovLH(1.3f, (float)m_Screen.width / (float)m_Screen.height, 0.1f, m_FarClip);
		}
	public:
		Mat4x4 m_ProjectionMatrix;
		Mat4x4 m_ViewMatrix;
		float m_FOV;
		float m_AspectRatio;
		float m_NearClip;
		float m_FarClip;
		Box m_Screen;
	public:
		void proccessMatrix(Mat4x4& mat)
		{
			m_ViewMatrix.setIdentity();
			m_ViewMatrix = mat;
			m_ViewMatrix.inverse();
		}
	};
}