#pragma once

#include "Vector3d.h"
#include "Vector4d.h"
#include <memory>

namespace Core
{
	class Mat4x4
	{
	public:
		Mat4x4(){}
		Mat4x4(float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33)
		{
			m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
			m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
			m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
			m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
		}

		void setIdentity()
		{
			memset(m, 0, sizeof(float)*16);
			m[0][0] = 1.0f;
			m[1][1] = 1.0f;
			m[2][2] = 1.0f;
			m[3][3] = 1.0f;
		}

		void setTranslation(const Vector3d& v)
		{
			m[3][0] = v.x;
			m[3][1] = v.y;
			m[3][2] = v.z;
		}

		void setScale(const Vector3d& v)
		{
			m[0][0] = v.x;
			m[1][1] = v.y;
			m[2][2] = v.z;
		}

		void setRotationX(float angle)
		{
			setIdentity();
			m[1][1] = cos(angle);
			m[1][2] = sin(angle);
			m[2][1] = -sin(angle);
			m[2][2] = cos(angle);
		}

		void setRotationY(float angle)
		{
			setIdentity();
			m[0][0] = cos(angle);
			m[0][2] = -sin(angle);
			m[2][0] = sin(angle);
			m[2][2] = cos(angle);
		}

		void setRotationZ(float angle)
		{
			setIdentity();
			m[0][0] = cos(angle);
			m[0][1] = sin(angle);
			m[1][0] = -sin(angle);
			m[1][1] = cos(angle);
		}

		void setPerspectiveFovLH(float fov, float aspect, float znear, float zfar)
		{
			setIdentity();
			float yscale = 1.0f / tan(fov / 2.0f);
			float xscale = yscale / aspect;
			m[0][0] = xscale;
			m[1][1] = yscale;
			m[2][2] = zfar / (zfar - znear);
			m[2][3] = 1.0f;
			m[3][2] = (-znear * zfar) / (zfar - znear);
		}

		void setOrthoLH(float width, float height, float near_plane, float far_plane)
		{
			setIdentity();
			m[0][0] = 2.0f / width;
			m[1][1] = 2.0f / height;
			m[2][2] = 1.0f / (far_plane - near_plane);
			m[3][2] = -(near_plane / (far_plane - near_plane));
		}

		Mat4x4 operator*(const Mat4x4& matrix) const
		{
			Mat4x4 temp;
			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					temp.m[i][j] = m[i][0] * matrix.m[0][j] +
						m[i][1] * matrix.m[1][j] +
						m[i][2] * matrix.m[2][j] +
						m[i][3] * matrix.m[3][j];
				}
			}
			return temp;
		}

        void setMatrix(const Mat4x4& matrix)
        {
            memcpy(m, matrix.m, sizeof(float) * 16);
        }

		Vector3d getRightwardDirection()
		{
			return Vector3d(m[0][0], m[0][1], m[0][2]);
		}
		Vector3d getUpDirection()
		{
			return Vector3d(m[1][0], m[1][1], m[1][2]);
		}
		Vector3d getForwardDirection()
		{
			return Vector3d(m[2][0], m[2][1], m[2][2]);
		}

		Vector3d getTranslation()
		{
			return Vector3d(m[3][0], m[3][1], m[3][2]);
		}


        void operator*=(const Mat4x4& matrix)
        {
            Mat4x4 out;
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    out.m[i][j] =
                        m[i][0] * matrix.m[0][j] + m[i][1] * matrix.m[1][j] +
                        m[i][2] * matrix.m[2][j] + m[i][3] * matrix.m[3][j];
                }
            }
            setMatrix(out);
        }

		float getDeterminant()
		{
			Vector4d minor, v1, v2, v3;
			float det;

			v1 = Vector4d(this->m[0][0], this->m[1][0], this->m[2][0], this->m[3][0]);
			v2 = Vector4d(this->m[0][1], this->m[1][1], this->m[2][1], this->m[3][1]);
			v3 = Vector4d(this->m[0][2], this->m[1][2], this->m[2][2], this->m[3][2]);


			minor.cross(v1, v2, v3);
			det = -(this->m[0][3] * minor.x + this->m[1][3] * minor.y + this->m[2][3] * minor.z +
				this->m[3][3] * minor.w);
			return det;
		}

		void inverse()
		{
			int a, i, j;
			Mat4x4 out;
			Vector4d v, vec[3];
			float det = 0.0f;

			det = this->getDeterminant();
			if (!det) return;
			for (i = 0; i < 4; i++)
			{
				for (j = 0; j < 4; j++)
				{
					if (j != i)
					{
						a = j;
						if (j > i) a = a - 1;
						vec[a].x = (this->m[j][0]);
						vec[a].y = (this->m[j][1]);
						vec[a].z = (this->m[j][2]);
						vec[a].w = (this->m[j][3]);
					}
				}
				v.cross(vec[0], vec[1], vec[2]);

				out.m[0][i] = (float) pow(-1.0f, i) * v.x / det;
				out.m[1][i] = (float) pow(-1.0f, i) * v.y / det;
				out.m[2][i] = (float) pow(-1.0f, i) * v.z / det;
				out.m[3][i] = (float) pow(-1.0f, i) * v.w / det;
			}

			this->setMatrix(out);
		}

		~Mat4x4() {}
	public:
		float m[4][4];
	};
}