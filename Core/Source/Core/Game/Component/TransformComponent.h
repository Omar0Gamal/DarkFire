#pragma once
#include "Core/Math/Vector3d.h"
#include "Core/Math/Mat4x4.h"
#include "Core/Game/IComponent.h"
namespace Core
{
	class TransformComponent : public IComponent
	{
	public:
		TransformComponent(Vector3d position, Vector3d rotation, Vector3d scale)
			: m_Position(position), m_Rotation(rotation), m_Scale(scale), m_worldMatrix()
		{
			proccessMatrix();
		}
	public:
		Mat4x4 m_worldMatrix;
		Vector3d m_Position;
		Vector3d m_Rotation;
		Vector3d m_Scale;
	public:
		TransformComponent& setPosition(Vector3d position)
		{
			m_Position = position;
			proccessMatrix();
			return *this;
		}
		TransformComponent& setRotation(Vector3d rotation)
		{
			m_Rotation = rotation;
			proccessMatrix();
			return *this;
		}
		TransformComponent& setScale(Vector3d scale)
		{
			m_Scale = scale;
			proccessMatrix();
			return *this;
		}
	public: 
		void proccessMatrix()
		{
			Mat4x4 temp;

			m_worldMatrix.setIdentity();

			temp.setIdentity();
			temp.setScale(m_Scale);
			m_worldMatrix *= temp;


			temp.setIdentity();
			temp.setRotationX(m_Rotation.x);
			m_worldMatrix *= temp;

			temp.setIdentity();
			temp.setRotationY(m_Rotation.y);
			m_worldMatrix *= temp;

			temp.setIdentity();
			temp.setRotationZ(m_Rotation.z);
			m_worldMatrix *= temp;


			temp.setIdentity();
			temp.setTranslation(m_Position);
			m_worldMatrix *= temp;
		}
	};
}