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
		Visualizer viz(1024, 768, "SDF Approximation Test");

		// Load model and precompute SDF
		// We'll use the cat model as it's a good complex shape
		auto model = std::make_shared<Model>("assets/Mesh_Cat.obj", false, true);
		model->SetScale(glm::vec3(0.1f)); // Scale it down as the cat model is usually large
		model->SetPosition(0, 5, 0);

		// Load visualization shader
		auto viz_shader = std::make_shared<Shader>("shaders/sdf/sdf_viz.vert", "shaders/sdf/sdf_viz.frag");

		viz.AddShapeHandler([&](float time) {
			auto shapes = std::vector<std::shared_ptr<Shape>>();

			// Update rotation to see it from all sides
			model->SetRotation(glm::angleAxis(time, glm::vec3(0, 1, 0)));

			// We'll use the standard Shape::shader hijacking for this test
			// In a real application, you'd use a proper material/shader system
			Shape::shader = viz_shader;

			viz_shader->use();
			glm::mat4 model_mat = model->GetModelMatrix();
			viz_shader->setMat4("u_invModel", glm::inverse(model_mat));

			AABB local_aabb = model->GetLocalAABB();
			viz_shader->setVec3("u_min_bounds", local_aabb.min);
			viz_shader->setVec3("u_max_bounds", local_aabb.max);
			viz_shader->setVec3("u_viewPos", viz.GetCamera().pos());

			GLuint sdf_tex = model->GetSdfTexture();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_3D, sdf_tex);
			viz_shader->setInt("u_sdf_texture", 0);

			shapes.push_back(model);
			return shapes;
		});

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
