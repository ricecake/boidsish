#include <iostream>
#include <vector>

#include "arrow.h"
#include "dot.h"
#include "field.h"
#include "graphics.h"
#include "model.h"
#include "spatial_entity_handler.h"
#include <glm/gtc/quaternion.hpp>

using namespace Boidsish;

class PaperPlane: public Entity<Model> {
public:
	PaperPlane(int id = 0): Entity<Model>(id, "assets/Paper Airplane.obj", true) {
		SetTrailLength(100);
		SetTrailIridescence(true);

		SetColor(1.0f, 0.5f, 0.0f);
		shape_->SetScale(glm::vec3(0.01f));
		SetPosition(0, 4, 0);
		SetVelocity(Vector3(2, 0, 2));
	}

	void UpdateEntity(const EntityHandler& handler, float time, float delta_time) {
		auto vel = velocity_;
		SetVelocity(Vector3(3 * sin(time / 5), sin(time / 10), 3 * cos(time / 5)));
	}
};

class PaperPlaneHandler: public SpatialEntityHandler {
public:
	PaperPlaneHandler(task_thread_pool::task_thread_pool& thread_pool): SpatialEntityHandler(thread_pool) {}
};

// void DefaultInputHandler(const InputState& state) {
// 	// Camera movement, orbit, and zoom controls
// 	switch (camera_mode) {
// 	case CameraMode::FREE: {
// 		float     camera_speed_val = camera.speed * state.delta_time;
// 		glm::vec3 front(
// 			cos(glm::radians(camera.pitch)) * sin(glm::radians(camera.yaw)),
// 			sin(glm::radians(camera.pitch)),
// 			-cos(glm::radians(camera.pitch)) * cos(glm::radians(camera.yaw))
// 		);
// 		glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

// 		if (state.keys[GLFW_KEY_W]) {
// 			camera.x += front.x * camera_speed_val;
// 			camera.y += front.y * camera_speed_val;
// 			camera.z += front.z * camera_speed_val;
// 		}
// 		if (state.keys[GLFW_KEY_S]) {
// 			camera.x -= front.x * camera_speed_val;
// 			camera.y -= front.y * camera_speed_val;
// 			camera.z -= front.z * camera_speed_val;
// 		}
// 		if (state.keys[GLFW_KEY_A]) {
// 			camera.x -= right.x * camera_speed_val;
// 			camera.z -= right.z * camera_speed_val;
// 		}
// 		if (state.keys[GLFW_KEY_D]) {
// 			camera.x += right.x * camera_speed_val;
// 			camera.z += right.z * camera_speed_val;
// 		}
// 		if (state.keys[GLFW_KEY_SPACE])
// 			camera.y += camera_speed_val;
// 		if (state.keys[GLFW_KEY_LEFT_SHIFT])
// 			camera.y -= camera_speed_val;
// 		if (state.keys[GLFW_KEY_Q])
// 			camera.roll -= kCameraRollSpeed * state.delta_time;
// 		if (state.keys[GLFW_KEY_E])
// 			camera.roll += kCameraRollSpeed * state.delta_time;
// 		if (camera.y < kMinCameraHeight)
// 			camera.y = kMinCameraHeight;

// 		float sensitivity = 1.f;
// 		float xoffset = state.mouse_delta_x * sensitivity;
// 		float yoffset = state.mouse_delta_y * sensitivity;

// 		camera.yaw += xoffset;
// 		camera.pitch += yoffset;

// 		if (camera.pitch > 89.0f)
// 			camera.pitch = 89.0f;
// 		if (camera.pitch < -89.0f)
// 			camera.pitch = -89.0f;
// 		break;
// 	}
// 	case CameraMode::TRACKING: {
// 		float sensitivity = 0.1f;
// 		float xoffset = state.mouse_delta_x * sensitivity;
// 		float yoffset = state.mouse_delta_y * sensitivity;

// 		single_track_orbit_yaw += xoffset;
// 		single_track_orbit_pitch += yoffset;

// 		if (single_track_orbit_pitch > 89.0f)
// 			single_track_orbit_pitch = 89.0f;
// 		if (single_track_orbit_pitch < -89.0f)
// 			single_track_orbit_pitch = -89.0f;
// 		break;
// 	}
// 	default:
// 		// No movement controls for other modes (AUTO, STATIONARY, CHASE)
// 		break;
// 	}

// 	// Camera mode switching (only if not in CHASE mode)
// 	if (camera_mode != CameraMode::CHASE) {
// 		if (state.key_down[GLFW_KEY_0]) {
// 			parent->SetCameraMode(CameraMode::FREE);
// 		} else if (state.key_down[GLFW_KEY_9]) {
// 			if (camera_mode == CameraMode::TRACKING) {
// 				tracked_dot_index++;
// 			} else {
// 				parent->SetCameraMode(CameraMode::TRACKING);
// 			}
// 		} else if (state.key_down[GLFW_KEY_8]) {
// 			parent->SetCameraMode(CameraMode::AUTO);
// 		} else if (state.key_down[GLFW_KEY_7]) {
// 			parent->SetCameraMode(CameraMode::STATIONARY);
// 		}
// 	}

// 	// Single track camera zoom
// 	if (state.keys[GLFW_KEY_EQUAL]) {
// 		single_track_distance -= 0.5f;
// 		if (single_track_distance < 1.0f)
// 			single_track_distance = 1.0f;
// 	}
// 	if (state.keys[GLFW_KEY_MINUS]) {
// 		single_track_distance += 0.5f;
// 	}

// 	// Camera speed adjustment
// 	if (state.key_down[GLFW_KEY_LEFT_BRACKET]) {
// 		camera.speed -= kCameraSpeedStep;
// 		if (camera.speed < kMinCameraSpeed)
// 			camera.speed = kMinCameraSpeed;
// 	}
// 	if (state.key_down[GLFW_KEY_RIGHT_BRACKET]) {
// 		camera.speed += kCameraSpeedStep;
// 	}

// 	// Toggles
// 	if (state.key_down[GLFW_KEY_P])
// 		paused = !paused;
// 	if (effects_enabled_) {
// 		if (state.key_down[GLFW_KEY_R])
// 			ripple_strength = (ripple_strength > 0.0f) ? 0.0f : 0.05f;
// 		if (state.key_down[GLFW_KEY_C])
// 			color_shift_effect = !color_shift_effect;
// 		if (state.key_down[GLFW_KEY_1])
// 			black_and_white_effect = !black_and_white_effect;
// 		if (state.key_down[GLFW_KEY_2])
// 			negative_effect = !negative_effect;
// 		if (state.key_down[GLFW_KEY_3])
// 			shimmery_effect = !shimmery_effect;
// 		if (state.key_down[GLFW_KEY_4])
// 			glitched_effect = !glitched_effect;
// 		if (state.key_down[GLFW_KEY_5])
// 			wireframe_effect = !wireframe_effect;
// 	}

// 	if (state.key_down[GLFW_KEY_F11]) {
// 		ToggleFullscreen();
// 	}

// 	if (state.key_down[GLFW_KEY_F1]) {
// 		for (auto& effect : post_processing_manager_->GetEffects()) {
// 			if (effect->GetName() == "OpticalFlow") {
// 				effect->SetEnabled(!effect->IsEnabled());
// 			}
// 		}
// 	}
// }

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Terrain Demo");

		Boidsish::Camera camera;
		visualizer.SetCamera(camera);
		// auto model = std::make_shared<Boidsish::Model>("assets/smolbird.fbx");
		// auto shapes = std::vector<std::shared_ptr<Boidsish::Shape>>();
		// shapes.push_back(model);
		// model->SetTrailLength(0); // Enable trails
		// model->SetColor(1.0f, 0.5f, 0.0f);
		// model->SetScale(glm::vec3(0.01f));
		// model->SetPosition(0, 4, 0);

		// visualizer.SetChaseCamera(std::static_pointer_cast<EntityBase>(model));

		auto handler = PaperPlaneHandler(visualizer.GetThreadPool());
		auto id = handler.AddEntity<PaperPlane>();
		auto plane = handler.GetEntity(id);

		visualizer.AddShapeHandler(std::ref(handler));
		visualizer.SetChaseCamera(plane);

		// // No shapes, just terrain
		// visualizer.AddShapeHandler([&](float) {
		// 	// auto cam = visualizer.GetCamera();
		// 	// auto front = cam.front();
		// 	// auto modelPost = cam.pos() + (6.0f * cam.front()) + (-1.0f * cam.up());

		// 	// glm::vec3 cameraPos(cam.x, cam.y, cam.z);
		// 	// glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cam.front(), cam.up());
		// 	// auto inv = glm::inverse(view);

		// 	// glm::quat rotation(glm::vec3(glm::radians(cam.pitch+15), glm::radians(-cam.yaw),
		// 	// glm::radians(-cam.roll)));

		// 	// model->SetRotation(rotation);

		// 	// model->SetPosition(modelPost.x, modelPost.y, modelPost.z);

		// 	return shapes;
		// });

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
