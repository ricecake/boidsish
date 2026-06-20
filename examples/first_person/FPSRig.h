#pragma once
#include <memory>
#include <vector>

#include "model.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Boidsish {

	/**
	 * @brief A system for FPS rigging (view models) that follow the camera
	 * with inertia, sway, and bobbing.
	 */
	class FPSRig {
	public:
		FPSRig(const std::string& modelPath) {
			model_ = std::make_shared<Model>(modelPath);
			// Default settings for a typical view model
			model_->SetScale(glm::vec3(0.02f)); // Teapot is quite large, scale it down
			model_->SetUsePBR(true);
			model_->SetRoughness(0.3f);
			model_->SetMetallic(0.7f);

			// Adjust base rotation if needed (teapot might need to be rotated to face forward)
			// For utah_teapot.obj, we might need a 90 degree rotation
			model_->SetBaseRotation(glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
		}

		/**
		 * @brief Update the rig's position and orientation based on camera state.
		 *
		 * @param camPos Current camera position
		 * @param camFront Current camera front vector
		 * @param camUp Current camera up vector
		 * @param deltaTime Time since last frame
		 * @param bobAmount Strength of the bobbing effect (proportional to speed)
		 * @param bobCycle Current phase of the bobbing cycle
		 * @param mouseDeltaX Horizontal mouse movement for sway
		 * @param mouseDeltaY Vertical mouse movement for sway
		 */
		void Update(
			const glm::vec3& camPos,
			const glm::vec3& camFront,
			const glm::vec3& camUp,
			float            deltaTime,
			float            bobAmount,
			float            bobCycle,
			float            mouseDeltaX,
			float            mouseDeltaY
		) {
			glm::vec3 right = glm::normalize(glm::cross(camFront, camUp));
			glm::vec3 actualUp = glm::normalize(glm::cross(right, camFront));

			// 1. Base Offset: Position the model relative to the camera
			// Offset slightly to the right, down, and in front of the camera
			glm::vec3 baseOffset = right * 0.075f - actualUp * 0.1f + camFront * 0.25f;

			// 2. Bobbing: Add movement based on the walking cycle
			// Vertical bobbing (up/down) and horizontal "sway" (left/right)
			float     verticalBob = sin(bobCycle * 2.0f) * bobAmount * 0.01f;
			float     horizontalBob = cos(bobCycle) * bobAmount * 0.005f;
			glm::vec3 bobOffset = actualUp * verticalBob + right * horizontalBob;

			// 3. Sway: Delayed response to camera rotation (mouse movement)
			// This gives the view model a sense of weight/inertia
			float targetSwayX = -mouseDeltaX * 0.002f;
			float targetSwayY = mouseDeltaY * 0.002f;

			// Smoothly interpolate current sway toward target
			currentSwayX_ = glm::mix(currentSwayX_, targetSwayX, deltaTime * 10.0f);
			currentSwayY_ = glm::mix(currentSwayY_, targetSwayY, deltaTime * 10.0f);

			// Return sway toward center
			currentSwayX_ = glm::mix(currentSwayX_, 0.0f, deltaTime * 5.0f);
			currentSwayY_ = glm::mix(currentSwayY_, 0.0f, deltaTime * 5.0f);

			glm::vec3 swayOffset = right * currentSwayX_ + actualUp * currentSwayY_;

			// 4. Combine all offsets
			glm::vec3 finalPos = camPos + baseOffset + bobOffset + swayOffset;
			model_->SetPosition(finalPos.x, finalPos.y, finalPos.z);

			// 5. Rotation: Follow camera but add slight tilt from sway
			glm::quat camRot = glm::quatLookAt(camFront, actualUp);

			// Tilt based on sway
			glm::quat tiltX = glm::angleAxis(currentSwayX_ * 0.5f, actualUp);
			glm::quat tiltY = glm::angleAxis(-currentSwayY_ * 0.5f, right);

			model_->SetRotation(camRot * tiltX * tiltY);
		}

		std::shared_ptr<Model> GetModel() const { return model_; }

		/**
		 * @brief Get the world-space position of the teapot's spout (muzzle).
		 * @return glm::vec3 world-space position
		 */
		glm::vec3 GetMuzzlePosition() const {
			if (!model_)
				return glm::vec3(0.0f);
			// The Utah teapot spout is roughly at (3.43, 1.2, 0) in its original model space.
			return glm::vec3(model_->GetModelMatrix() * glm::vec4(3.43f, 1.2f, 0.0f, 1.0f));
		}

	private:
		std::shared_ptr<Model> model_;
		float                  currentSwayX_ = 0.0f;
		float                  currentSwayY_ = 0.0f;
	};

} // namespace Boidsish
