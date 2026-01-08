#pragma once

#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace Boidsish {

	// Generic 3D vector class with basic operations
	class Vector3: public glm::vec3 {
	public:
		// float x, y, z;

		// Constructors
		constexpr Vector3(): glm::vec3(0) {} // x(0.0f), y(0.0f), z(0.0f) {}

		constexpr Vector3(const glm::vec3& ve): glm::vec3(ve) {}

		constexpr Vector3(float x, float y, float z): glm::vec3(x, y, z) {} //: x(x), y(y), z(z) {}

		// operator glm::vec3() const { return glm::vec3(x, y, z); }
		glm::vec3 Toglm() {
			return glm::vec3(x, y, z);
		}

		// Copy constructor and assignment
		Vector3(const Vector3& other) = default;
		Vector3& operator=(const Vector3& other) = default;

		// Vector addition
		constexpr Vector3 operator+(const Vector3& other) const {
			return Vector3(x + other.x, y + other.y, z + other.z);
		}

		Vector3& operator+=(const Vector3& other) {
			x += other.x;
			y += other.y;
			z += other.z;
			return *this;
		}

		// Vector subtraction
		constexpr Vector3 operator-(const Vector3& other) const {
			return Vector3(x - other.x, y - other.y, z - other.z);
		}

		Vector3& operator-=(const Vector3& other) {
			x -= other.x;
			y -= other.y;
			z -= other.z;
			return *this;
		}

		// Scalar multiplication
		constexpr Vector3 operator*(float scalar) const { return Vector3(x * scalar, y * scalar, z * scalar); }

		Vector3& operator*=(float scalar) {
			x *= scalar;
			y *= scalar;
			z *= scalar;
			return *this;
		}

		// Scalar division
		constexpr Vector3 operator/(float scalar) const {
			float inv = 1.0f / scalar;
			return Vector3(x * inv, y * inv, z * inv);
		}

		Vector3& operator/=(float scalar) {
			float inv = 1.0f / scalar;
			x *= inv;
			y *= inv;
			z *= inv;
			return *this;
		}

		// Unary minus
		constexpr Vector3 operator-() const { return Vector3(-x, -y, -z); } // Magnitude (length)

		float Magnitude() const { return std::sqrt(x * x + y * y + z * z); }

		// Squared magnitude (for efficiency when comparing lengths)
		constexpr float MagnitudeSquared() const { return x * x + y * y + z * z; }

		// Normalize to unit length
		Vector3 Normalized() const {
			float mag = Magnitude();
			if (mag > 0.0f) {
				return *this / mag;
			}
			return Vector3(0, 0, 0);
		}

		// Normalize in place
		void Normalize() {
			float mag = Magnitude();
			if (mag > 0.0f) {
				*this /= mag;
			}
		}

		// Dot product
		constexpr float Dot(const Vector3& other) const { return x * other.x + y * other.y + z * other.z; }

		// Cross product
		constexpr Vector3 Cross(const Vector3& other) const {
			return Vector3(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x);
		} // Spherical angle difference (angle between two vectors in radians)

		float AngleTo(const Vector3& other) const {
			float dot_product = Dot(other);
			float magnitudes = Magnitude() * other.Magnitude();
			if (magnitudes > 0.0f) {
				// Clamp to avoid floating point errors in acos
				float cos_angle = std::max(-1.0f, std::min(1.0f, dot_product / magnitudes));
				return std::acos(cos_angle);
			}
			return 0.0f;
		}

		// Distance to another vector
		float DistanceTo(const Vector3& other) const { return (*this - other).Magnitude(); }

		// Set components
		constexpr void Set(float x, float y, float z) {
			this->x = x;
			this->y = y;
			this->z = z;
		}

		// Zero vector
		static constexpr Vector3 Zero() { return Vector3(0, 0, 0); }

		static constexpr Vector3 One() { return Vector3(1, 1, 1); }

		static constexpr Vector3 Up() { return Vector3(0, 1, 0); }

		static constexpr Vector3 Right() { return Vector3(1, 0, 0); }

		static constexpr Vector3 Forward() { return Vector3(0, 0, 1); }
	};

	// Scalar multiplication (scalar * vector)
	constexpr inline Vector3 operator*(float scalar, const Vector3& vec) {
		return vec * scalar;
	}
} // namespace Boidsish