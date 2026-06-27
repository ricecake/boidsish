#include <iostream>
#include <vector>

#include "graphics.h"
#include "post_processing/PostProcessingManager.h"
#include "post_processing/effects/AtmosphereEffect.h"
#include <glm/glm.hpp>

int main() {
	try {
		Boidsish::Visualizer visualizer(1280, 720, "Cloud Dynamics Demo");

		// Set a custom camera position
		Boidsish::Camera camera;
		camera.x = 0.0f;
		camera.y = 50.0f;
		camera.z = 200.0f;
		camera.pitch = 20.0f;
		camera.yaw = 0.0f;
		visualizer.SetCamera(camera);

		// Add input callback for mouse interactions
		visualizer.AddInputCallback([&visualizer](const Boidsish::InputState& input) {
			// Left click: Push away clouds (Sink/Source effect)
			if (input.mouse_button_down[0]) {
                // For clouds, we want to target the cloud altitude
                // AtmosphereEffect default altitude is around 95.0f
                float cloudAltitude = 95.0f;

                // Get Atmosphere effect to access cloud altitude
                auto atmosphere = visualizer.GetPostProcessingManager().GetEffect<Boidsish::PostProcessing::AtmosphereEffect>("Atmosphere");
                if (atmosphere) {
                    cloudAltitude = atmosphere->GetCloudAltitude();
                }

                // Raycast to cloud plane
                auto camera = visualizer.GetCamera();
                glm::vec3 cameraPos(camera.x, camera.y, camera.z);

                // Calculate ray direction from screen coordinates
                glm::mat4 invProj = visualizer.GetProjectionMatrix();
                invProj = glm::inverse(invProj);
                glm::mat4 invView = visualizer.GetViewMatrix();
                invView = glm::inverse(invView);

                float x = (2.0f * (float)input.mouse_x) / 1280.0f - 1.0f;
                float y = 1.0f - (2.0f * (float)input.mouse_y) / 720.0f;
                glm::vec4 ray_clip = glm::vec4(x, y, -1.0, 1.0);
                glm::vec4 ray_eye = invProj * ray_clip;
                ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0, 0.0);
                glm::vec3 rayDir = glm::normalize(glm::vec3(invView * ray_eye));

                if (abs(rayDir.y) > 0.001f) {
                    float t = (cloudAltitude - cameraPos.y) / rayDir.y;
                    if (t > 0) {
                        glm::vec3 hitPos = cameraPos + rayDir * t;

                        std::cout << "Creating cloud push at " << hitPos.x << ", " << hitPos.y << ", " << hitPos.z << std::endl;

                        if (atmosphere) {
                            // advectionMagnitude = 5.0 (push), densityOffset = -0.5 (clear clouds)
                            atmosphere->GetCloudDynamicsManager().AddEffect(hitPos, 150.0f, 5.0f, -0.5f);
                        }
                    }
                }
			}
			// Right click: Add clouds (Source effect)
			else if (input.mouse_button_down[1]) {
                float cloudAltitude = 95.0f;
                auto atmosphere = visualizer.GetPostProcessingManager().GetEffect<Boidsish::PostProcessing::AtmosphereEffect>("Atmosphere");
                if (atmosphere) {
                    cloudAltitude = atmosphere->GetCloudAltitude();
                }

                auto camera = visualizer.GetCamera();
                glm::vec3 cameraPos(camera.x, camera.y, camera.z);

                // Calculate ray direction from screen coordinates
                glm::mat4 invProj = visualizer.GetProjectionMatrix();
                invProj = glm::inverse(invProj);
                glm::mat4 invView = visualizer.GetViewMatrix();
                invView = glm::inverse(invView);

                float x = (2.0f * (float)input.mouse_x) / 1280.0f - 1.0f;
                float y = 1.0f - (2.0f * (float)input.mouse_y) / 720.0f;
                glm::vec4 ray_clip = glm::vec4(x, y, -1.0, 1.0);
                glm::vec4 ray_eye = invProj * ray_clip;
                ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0, 0.0);
                glm::vec3 rayDir = glm::normalize(glm::vec3(invView * ray_eye));

                if (abs(rayDir.y) > 0.001f) {
                    float t = (cloudAltitude - cameraPos.y) / rayDir.y;
                    if (t > 0) {
                        glm::vec3 hitPos = cameraPos + rayDir * t;

                        std::cout << "Creating cloud source at " << hitPos.x << ", " << hitPos.y << ", " << hitPos.z << std::endl;

                        if (atmosphere) {
                            // advectionMagnitude = -2.0 (slight pull/flow), densityOffset = 1.0 (add clouds)
                            atmosphere->GetCloudDynamicsManager().AddEffect(hitPos, 100.0f, -2.0f, 1.0f);
                        }
                    }
                }
			}
		});

		std::cout << "Cloud Dynamics Demo" << std::endl;
		std::cout << "Left Click: Push away clouds and create a void" << std::endl;
		std::cout << "Right Click: Create a thick cloud source" << std::endl;

		visualizer.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
