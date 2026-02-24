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
		model->SetScale(glm::vec3(0.1f));
		model->SetPosition(-10.0f, 5.0f, 0.0f); // Move model to the left

		// Create a separate model for the SDF visualization to avoid state conflicts
		auto sdf_viz_model = std::make_shared<Model>("assets/Mesh_Cat.obj", false, false);
		sdf_viz_model->SetScale(glm::vec3(0.1f));
		sdf_viz_model->SetPosition(10.0f, 5.0f, 0.0f); // Move SDF viz to the right

		// Load visualization shader
		auto viz_shader = std::make_shared<Shader>("shaders/sdf/sdf_viz.vert", "shaders/sdf/sdf_viz.frag");

		viz.AddShapeHandler([&](float /* time */) {
			auto shapes = std::vector<std::shared_ptr<Shape>>();

			// Both stationary for easier comparison

			// Render the real model normally (it will use its default shader unless hijacked)
			shapes.push_back(model);

			// Render the SDF visualization
			// This is a bit of a hack since we're using Shape::shader which is global
			// but it works for this isolated test.
			// We only want the hijacking to apply to sdf_viz_model.
			// However, in this simple framework, Shape::shader applies to all Shapes if set.
			// So we'll render them in separate groups or just handle it in the shader.

			// Actually, let's just use two shape handlers or something.
			// Or better: set Shape::shader only when needed.
			// But the renderer will call all of them.

			return shapes;
		});

		// Add a second handler for the SDF viz to demonstrate different rendering
		viz.AddShapeHandler([&](float /* time */) {
			auto shapes = std::vector<std::shared_ptr<Shape>>();

			// We'll use a local scope to set the shader and uniforms
			// Note: This relies on how Visualizer calls these, which might be risky.
			// A better way is to use the shader hijacking carefully.

			Shape::shader = viz_shader;

			viz_shader->use();
			glm::mat4 model_mat = sdf_viz_model->GetModelMatrix();
			viz_shader->setMat4("u_invModel", glm::inverse(model_mat));

			AABB local_aabb = sdf_viz_model->GetLocalAABB();
			viz_shader->setVec3("u_min_bounds", local_aabb.min);
			viz_shader->setVec3("u_max_bounds", local_aabb.max);
			viz_shader->setVec3("u_viewPos", viz.GetCamera().pos());
			viz_shader->setFloat("u_scale", sdf_viz_model->GetScale().x);

			GLuint sdf_tex = model->GetSdfTexture(); // Get from the precomputed one
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_3D, sdf_tex);
			viz_shader->setInt("u_sdf_texture", 0);

			shapes.push_back(sdf_viz_model);
			return shapes;
		});

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
