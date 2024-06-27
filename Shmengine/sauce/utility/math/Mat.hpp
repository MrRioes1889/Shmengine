#pragma once

#include "Defines.hpp"
#include "Vec3.hpp"
#include "Geometry.hpp"

#define MAT4_IDENTITY \
{1.0f, 0.0f, 0.0f, 0.0f,\
 0.0f, 1.0f, 0.0f, 0.0f,\
 0.0f, 0.0f, 1.0f, 0.0f,\
 0.0f, 0.0f, 0.0f, 1.0f}					

namespace Math
{

    SHMAPI float32 sin(float32 x);
    SHMAPI float32 cos(float32 x);
    SHMAPI float32 tan(float32 x);
    SHMAPI float32 acos(float32 x);
    SHMAPI float32 abs(float32 x);
    SHMAPI float32 sqrt(float32 a);

//#pragma pack(push, 1)

	struct Mat4//alignas(16) Mat4
	{
		union
		{
		float32 data[16];

		Vec4f rows[4];
		};	
	};

//#pragma pack(pop)

	SHMINLINE Mat4 mat_mul(Mat4 m1, Mat4 m2)
	{
		Mat4 res = MAT4_IDENTITY;

		const float32* m1_ptr = m1.data;
		const float32* m2_ptr = m2.data;
		float32* dst_ptr = res.data;

		for (uint32 i = 0; i < 4; i++)
		{
			for (uint32 j = 0; j < 4; j++)
			{
				*dst_ptr =
					m1_ptr[0] * m2_ptr[0 + j] +
					m1_ptr[1] * m2_ptr[4 + j] +
					m1_ptr[2] * m2_ptr[8 + j] +
					m1_ptr[3] * m2_ptr[12 + j];
				dst_ptr++;
			}
			m1_ptr += 4;
		}
		return res;
	}

	/**
 * @brief Creates and returns an orthographic projection matrix. Typically used to
 * render flat or 2D scenes.
 *
 * @param left - The left side of the view frustum.
 * @param right - The right side of the view frustum.
 * @param bottom - The bottom side of the view frustum.
 * @param top - The top side of the view frustum.
 * @param near_clip - The near clipping plane distance.
 * @param far_clip - The far clipping plane distance.
 * @return A new orthographic projection matrix.
 */
	SHMINLINE Mat4 mat_orthographic(float32 left, float32 right, float32 bottom, float32 top, float32 near_clip, float32 far_clip) {
		Mat4 res = MAT4_IDENTITY;

		float32 lr = 1.0f / (left - right);
		float32 bt = 1.0f / (bottom - top);
		float32 nf = 1.0f / (near_clip - far_clip);

		res.data[0] = -2.0f * lr;
		res.data[5] = -2.0f * bt;
		res.data[10] = 2.0f * nf;

		res.data[12] = (left + right) * lr;
		res.data[13] = (top + bottom) * bt;
		res.data[14] = (far_clip + near_clip) * nf;
		return res;
	}

	/**
	 * @brief Creates and returns a perspective matrix. Typically used to render 3d scenes.
	 *
	 * @param fov_radians - The field of view in radians.
	 * @param aspect_ratio - The aspect ratio.
	 * @param near_clip - The near clipping plane distance.
	 * @param far_clip - The far clipping plane distance.
	 * @return A new perspective matrix.
	 */
	SHMINLINE Mat4 mat_perspective(float32 fov_radians, float32 aspect_ratio, float32 near_clip, float32 far_clip) {
		float32 half_tan_fov = tan(fov_radians * 0.5f);
		Mat4 res = {};
		res.data[0] = 1.0f / (aspect_ratio * half_tan_fov);
		res.data[5] = 1.0f / half_tan_fov;
		res.data[10] = -((far_clip + near_clip) / (far_clip - near_clip));
		res.data[11] = -1.0f;
		res.data[14] = -((2.0f * far_clip * near_clip) / (far_clip - near_clip));
		return res;
	}

    /**
 * @brief Creates and returns a look-at matrix, or a matrix looking
 * at target from the perspective of position.
 *
 * @param position - The position of the matrix.
 * @param target - The position to "look at".
 * @param up - The up vector.
 * @return A matrix looking at target from the perspective of position.
 */
    SHMINLINE Mat4 mat_look_at(Vec3f position, Vec3f target, Vec3f up) {
        Mat4 out_matrix;
        Vec3f z_axis;
        z_axis.x = target.x - position.x;
        z_axis.y = target.y - position.y;
        z_axis.z = target.z - position.z;

        z_axis = normalized(z_axis);
        Vec3f x_axis = normalized(cross_product(z_axis, up));
        Vec3f y_axis = cross_product(x_axis, z_axis);

        out_matrix.data[0] = x_axis.x;
        out_matrix.data[1] = y_axis.x;
        out_matrix.data[2] = -z_axis.x;
        out_matrix.data[3] = 0;
        out_matrix.data[4] = x_axis.y;
        out_matrix.data[5] = y_axis.y;
        out_matrix.data[6] = -z_axis.y;
        out_matrix.data[7] = 0;
        out_matrix.data[8] = x_axis.z;
        out_matrix.data[9] = y_axis.z;
        out_matrix.data[10] = -z_axis.z;
        out_matrix.data[11] = 0;
        out_matrix.data[12] = -inner_product(x_axis, position);
        out_matrix.data[13] = -inner_product(y_axis, position);
        out_matrix.data[14] = inner_product(z_axis, position);
        out_matrix.data[15] = 1.0f;

        return out_matrix;
    }

    /**
     * @brief Returns a transposed copy of the provided matrix (rows->colums)
     *
     * @param matrix - The matrix to be transposed.
     * @return A transposed copy of of the provided matrix.
     */
    SHMINLINE Mat4 mat_transposed(Mat4 matrix) {
        Mat4 out_matrix = MAT4_IDENTITY;
        out_matrix.data[0] = matrix.data[0];
        out_matrix.data[1] = matrix.data[4];
        out_matrix.data[2] = matrix.data[8];
        out_matrix.data[3] = matrix.data[12];
        out_matrix.data[4] = matrix.data[1];
        out_matrix.data[5] = matrix.data[5];
        out_matrix.data[6] = matrix.data[9];
        out_matrix.data[7] = matrix.data[13];
        out_matrix.data[8] = matrix.data[2];
        out_matrix.data[9] = matrix.data[6];
        out_matrix.data[10] = matrix.data[10];
        out_matrix.data[11] = matrix.data[14];
        out_matrix.data[12] = matrix.data[3];
        out_matrix.data[13] = matrix.data[7];
        out_matrix.data[14] = matrix.data[11];
        out_matrix.data[15] = matrix.data[15];
        return out_matrix;
    }

    /**
     * @brief Creates and returns an inverse of the provided matrix.
     *
     * @param matrix - The matrix to be inverted.
     * @return A inverted copy of the provided matrix.
     */
    SHMINLINE Mat4 mat_inverse(Mat4 matrix) {
        const float32* m = matrix.data;

        float32 t0 = m[10] * m[15];
        float32 t1 = m[14] * m[11];
        float32 t2 = m[6] * m[15];
        float32 t3 = m[14] * m[7];
        float32 t4 = m[6] * m[11];
        float32 t5 = m[10] * m[7];
        float32 t6 = m[2] * m[15];
        float32 t7 = m[14] * m[3];
        float32 t8 = m[2] * m[11];
        float32 t9 = m[10] * m[3];
        float32 t10 = m[2] * m[7];
        float32 t11 = m[6] * m[3];
        float32 t12 = m[8] * m[13];
        float32 t13 = m[12] * m[9];
        float32 t14 = m[4] * m[13];
        float32 t15 = m[12] * m[5];
        float32 t16 = m[4] * m[9];
        float32 t17 = m[8] * m[5];
        float32 t18 = m[0] * m[13];
        float32 t19 = m[12] * m[1];
        float32 t20 = m[0] * m[9];
        float32 t21 = m[8] * m[1];
        float32 t22 = m[0] * m[5];
        float32 t23 = m[4] * m[1];

        Mat4 out_matrix;
        float32* o = out_matrix.data;

        o[0] = (t0 * m[5] + t3 * m[9] + t4 * m[13]) - (t1 * m[5] + t2 * m[9] + t5 * m[13]);
        o[1] = (t1 * m[1] + t6 * m[9] + t9 * m[13]) - (t0 * m[1] + t7 * m[9] + t8 * m[13]);
        o[2] = (t2 * m[1] + t7 * m[5] + t10 * m[13]) - (t3 * m[1] + t6 * m[5] + t11 * m[13]);
        o[3] = (t5 * m[1] + t8 * m[5] + t11 * m[9]) - (t4 * m[1] + t9 * m[5] + t10 * m[9]);

        float32 d = 1.0f / (m[0] * o[0] + m[4] * o[1] + m[8] * o[2] + m[12] * o[3]);

        o[0] = d * o[0];
        o[1] = d * o[1];
        o[2] = d * o[2];
        o[3] = d * o[3];
        o[4] = d * ((t1 * m[4] + t2 * m[8] + t5 * m[12]) - (t0 * m[4] + t3 * m[8] + t4 * m[12]));
        o[5] = d * ((t0 * m[0] + t7 * m[8] + t8 * m[12]) - (t1 * m[0] + t6 * m[8] + t9 * m[12]));
        o[6] = d * ((t3 * m[0] + t6 * m[4] + t11 * m[12]) - (t2 * m[0] + t7 * m[4] + t10 * m[12]));
        o[7] = d * ((t4 * m[0] + t9 * m[4] + t10 * m[8]) - (t5 * m[0] + t8 * m[4] + t11 * m[8]));
        o[8] = d * ((t12 * m[7] + t15 * m[11] + t16 * m[15]) - (t13 * m[7] + t14 * m[11] + t17 * m[15]));
        o[9] = d * ((t13 * m[3] + t18 * m[11] + t21 * m[15]) - (t12 * m[3] + t19 * m[11] + t20 * m[15]));
        o[10] = d * ((t14 * m[3] + t19 * m[7] + t22 * m[15]) - (t15 * m[3] + t18 * m[7] + t23 * m[15]));
        o[11] = d * ((t17 * m[3] + t20 * m[7] + t23 * m[11]) - (t16 * m[3] + t21 * m[7] + t22 * m[11]));
        o[12] = d * ((t14 * m[10] + t17 * m[14] + t13 * m[6]) - (t16 * m[14] + t12 * m[6] + t15 * m[10]));
        o[13] = d * ((t20 * m[14] + t12 * m[2] + t19 * m[10]) - (t18 * m[10] + t21 * m[14] + t13 * m[2]));
        o[14] = d * ((t18 * m[6] + t23 * m[14] + t15 * m[2]) - (t22 * m[14] + t14 * m[2] + t19 * m[6]));
        o[15] = d * ((t22 * m[10] + t16 * m[2] + t21 * m[6]) - (t20 * m[6] + t23 * m[10] + t17 * m[2]));

        return out_matrix;
    }

    SHMINLINE Mat4 mat_translation(Vec3f position) {
        Mat4 out_matrix = MAT4_IDENTITY;
        out_matrix.data[12] = position.x;
        out_matrix.data[13] = position.y;
        out_matrix.data[14] = position.z;
        return out_matrix;
    }

    /**
     * @brief Returns a scale matrix using the provided scale.
     *
     * @param scale - The 3-component scale.
     * @return A scale matrix.
     */
    SHMINLINE Mat4 mat_scale(Vec3f scale) {
        Mat4 out_matrix = MAT4_IDENTITY;
        out_matrix.data[0] = scale.x;
        out_matrix.data[5] = scale.y;
        out_matrix.data[10] = scale.z;
        return out_matrix;
    }

    SHMINLINE Mat4 mat_euler_x(float32 angle_radians) {
        Mat4 out_matrix = MAT4_IDENTITY;
        float32 c = cos(angle_radians);
        float32 s = sin(angle_radians);

        out_matrix.data[5] = c;
        out_matrix.data[6] = s;
        out_matrix.data[9] = -s;
        out_matrix.data[10] = c;
        return out_matrix;
    }
    SHMINLINE Mat4 mat_euler_y(float32 angle_radians) {
        Mat4 out_matrix = MAT4_IDENTITY;
        float32 c = cos(angle_radians);
        float32 s = sin(angle_radians);

        out_matrix.data[0] = c;
        out_matrix.data[2] = -s;
        out_matrix.data[8] = s;
        out_matrix.data[10] = c;
        return out_matrix;
    }
    SHMINLINE Mat4 mat_euler_z(float32 angle_radians) {
        Mat4 out_matrix = MAT4_IDENTITY;

        float32 c = cos(angle_radians);
        float32 s = sin(angle_radians);

        out_matrix.data[0] = c;
        out_matrix.data[1] = s;
        out_matrix.data[4] = -s;
        out_matrix.data[5] = c;
        return out_matrix;
    }
    SHMINLINE Mat4 mat_euler_xyz(float32 x_radians, float32 y_radians, float32 z_radians) {
        Mat4 rx = mat_euler_x(x_radians);
        Mat4 ry = mat_euler_y(y_radians);
        Mat4 rz = mat_euler_z(z_radians);
        Mat4 out_matrix = mat_mul(rx, ry);
        out_matrix = mat_mul(out_matrix, rz);
        return out_matrix;
    }

    /**
     * @brief Returns a forward vector relative to the provided matrix.
     *
     * @param matrix - The matrix from which to base the vector.
     * @return A 3-component directional vector.
     */
    SHMINLINE Vec3f mat_forward(Mat4 matrix) {
        Vec3f forward;
        forward.x = -matrix.data[2];
        forward.y = -matrix.data[6];
        forward.z = -matrix.data[10];
        normalize(forward);
        return forward;
    }

    /**
     * @brief Returns a backward vector relative to the provided matrix.
     *
     * @param matrix - The matrix from which to base the vector.
     * @return A 3-component directional vector.
     */
    SHMINLINE Vec3f mat_backward(Mat4 matrix) {
        Vec3f backward;
        backward.x = matrix.data[2];
        backward.y = matrix.data[6];
        backward.z = matrix.data[10];
        normalize(backward);
        return backward;
    }

    /**
     * @brief Returns a upward vector relative to the provided matrix.
     *
     * @param matrix - The matrix from which to base the vector.
     * @return A 3-component directional vector.
     */
    SHMINLINE Vec3f mat_up(Mat4 matrix) {
        Vec3f up;
        up.x = matrix.data[1];
        up.y = matrix.data[5];
        up.z = matrix.data[9];
        normalize(up);
        return up;
    }

    /**
     * @brief Returns a downward vector relative to the provided matrix.
     *
     * @param matrix - The matrix from which to base the vector.
     * @return A 3-component directional vector.
     */
    SHMINLINE Vec3f mat_down(Mat4 matrix) {
        Vec3f down;
        down.x = -matrix.data[1];
        down.y = -matrix.data[5];
        down.z = -matrix.data[9];
        normalize(down);
        return down;
    }

    /**
     * @brief Returns a left vector relative to the provided matrix.
     *
     * @param matrix - The matrix from which to base the vector.
     * @return A 3-component directional vector.
     */
    SHMINLINE Vec3f mat_left(Mat4 matrix) {
        Vec3f left;
        left.x = -matrix.data[0];
        left.y = -matrix.data[4];
        left.z = -matrix.data[8];
        normalize(left);
        return left;
    }

    /**
     * @brief Returns a right vector relative to the provided matrix.
     *
     * @param matrix - The matrix from which to base the vector.
     * @return A 3-component directional vector.
     */
    SHMINLINE Vec3f mat_right(Mat4 matrix) {
        Vec3f right;
        right.x = matrix.data[0];
        right.y = matrix.data[4];
        right.z = matrix.data[8];
        normalize(right);
        return right;
    }

    SHMINLINE Mat4 quat_to_mat(Quat q) {
        Mat4 res = MAT4_IDENTITY;

        // https://stackoverflow.com/questions/1556260/convert-quaternion-rotation-to-rotation-matrix

        Quat n = quat_normalize(q);

        res.data[0] = 1.0f - 2.0f * n.y * n.y - 2.0f * n.z * n.z;
        res.data[1] = 2.0f * n.x * n.y - 2.0f * n.z * n.w;
        res.data[2] = 2.0f * n.x * n.z + 2.0f * n.y * n.w;

        res.data[4] = 2.0f * n.x * n.y + 2.0f * n.z * n.w;
        res.data[5] = 1.0f - 2.0f * n.x * n.x - 2.0f * n.z * n.z;
        res.data[6] = 2.0f * n.y * n.z - 2.0f * n.x * n.w;

        res.data[8] = 2.0f * n.x * n.z - 2.0f * n.y * n.w;
        res.data[9] = 2.0f * n.y * n.z + 2.0f * n.x * n.w;
        res.data[10] = 1.0f - 2.0f * n.x * n.x - 2.0f * n.y * n.y;

        return res;
    }

    // Calculates a rotation matrix based on the quaternion and the passed in center point.
    SHMINLINE Mat4 quat_to_rotation_matrix(Quat q, Vec3f center) {
        Mat4 res;

        float32* o = res.data;
        o[0] = (q.x * q.x) - (q.y * q.y) - (q.z * q.z) + (q.w * q.w);
        o[1] = 2.0f * ((q.x * q.y) + (q.z * q.w));
        o[2] = 2.0f * ((q.x * q.z) - (q.y * q.w));
        o[3] = center.x - center.x * o[0] - center.y * o[1] - center.z * o[2];

        o[4] = 2.0f * ((q.x * q.y) - (q.z * q.w));
        o[5] = -(q.x * q.x) + (q.y * q.y) - (q.z * q.z) + (q.w * q.w);
        o[6] = 2.0f * ((q.y * q.z) + (q.x * q.w));
        o[7] = center.y - center.x * o[4] - center.y * o[5] - center.z * o[6];

        o[8] = 2.0f * ((q.x * q.z) + (q.y * q.w));
        o[9] = 2.0f * ((q.y * q.z) - (q.x * q.w));
        o[10] = -(q.x * q.x) - (q.y * q.y) + (q.z * q.z) + (q.w * q.w);
        o[11] = center.z - center.x * o[8] - center.y * o[9] - center.z * o[10];

        o[12] = 0.0f;
        o[13] = 0.0f;
        o[14] = 0.0f;
        o[15] = 1.0f;
        return res;
    }

}
