#include <iostream>
#include <memory>
#include <vector>

#include "graphics.h"
#include "model.h"
#include "shader.h"
#include "asset_manager.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace Boidsish;

int main() {
	try {
		Visualizer viz(1280, 720, "SDF Approximation Test");

		// Add a directional light so models are not black
		Light sun = Light::CreateDirectional(45.0f, 45.0f, 1.5f, glm::vec3(1.0f, 0.9f, 0.8f));
		viz.GetLightManager().AddLight(sun);

		// Load model and precompute SDF
		auto cat_model = std::make_shared<Model>("assets/Mesh_Cat.obj", false, true);
		cat_model->SetScale(glm::vec3(0.1f));
		cat_model->SetPosition(-10.0f, 5.0f, 0.0f);
		cat_model->SetColor(1.0f, 1.0f, 1.0f);

		// Create a separate model for the SDF visualization using a Cube
		auto sdf_cube = std::make_shared<Model>("assets/cube.obj", false, false);
		AABB local_aabb = cat_model->GetLocalAABB();
		glm::vec3 size = local_aabb.max - local_aabb.min;
		// Cube.obj is 2x2x2 (-1 to 1). Scale it to cover the AABB.
		sdf_cube->SetScale(size * 0.1f * 0.5f * 1.05f);
		// Center the cube at the same relative position as the cat
		glm::vec3 center = (local_aabb.min + local_aabb.max) * 0.5f;
		sdf_cube->SetPosition(10.0f + center.x * 0.1f, 5.0f + center.y * 0.1f, 0.0f + center.z * 0.1f);

		// Load and register visualization shader
		auto viz_shader = std::make_shared<Shader>("shaders/sdf/sdf_viz.vert", "shaders/sdf/sdf_viz.frag");
		ShaderHandle viz_handle = viz.RegisterShader(viz_shader);

		// Set the custom shader on the cube instance
		sdf_cube->SetShader(viz_shader, viz_handle);

		viz.AddShapeHandler([&](float /* time */) {
			auto shapes = std::vector<std::shared_ptr<Shape>>();

			// Both stationary for easier comparison
			shapes.push_back(cat_model);
			shapes.push_back(sdf_cube);

			// Set uniforms for raymarching
			viz_shader->use();
			viz_shader->setVec3("u_viewPos", viz.GetCamera().pos());

			// Bind SDF texture to unit 10
			GLuint sdf_tex = cat_model->GetSdfTexture();
			glActiveTexture(GL_TEXTURE10);
			glBindTexture(GL_TEXTURE_3D, sdf_tex);
			viz_shader->setInt("u_sdf_texture", 10);

			return shapes;
		});

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
