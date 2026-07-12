#include <cmath>
#include <memory>
#include <vector>
#include <iostream>

#include "graphics.h"
#include "model.h"
#include "shader.h"
#include "render_shader.h"
#include "shader_table.h"
#include "ConfigManager.h"
#include "IWidget.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Boidsish;

class TessellationControlWidget : public UI::IWidget {
public:
	TessellationControlWidget(std::shared_ptr<Shader> tessShader) : m_tessShader(tessShader) {
		m_tessFactorMin = 1.0f;
		m_tessFactorMax = 12.0f;
		m_tessDistMin = 5.0f;
		m_tessDistMax = 100.0f;
		m_tessDetailStrength = 0.05f;
		m_tessDetailFrequency = 10.00f;
	}

	void Draw() override {
		if (ImGui::Begin("Tessellation Controls")) {
			ImGui::Text("Tessellation Density / LOD:");
			ImGui::SliderFloat("Min Tess Factor", &m_tessFactorMin, 1.0f, 16.0f);
			ImGui::SliderFloat("Max Tess Factor", &m_tessFactorMax, 1.0f, 64.0f);
			ImGui::SliderFloat("Min Tess Distance", &m_tessDistMin, 1.0f, 100.0f);
			ImGui::SliderFloat("Max Tess Distance", &m_tessDistMax, 5.0f, 500.0f);

			ImGui::Separator();
			ImGui::Text("Procedural Noise Displacement:");
			ImGui::SliderFloat("Detail Strength", &m_tessDetailStrength, 0.0f, 0.5f);
			ImGui::SliderFloat("Detail Frequency", &m_tessDetailFrequency, 1.0f, 50.0f);

			ImGui::Separator();
			ImGui::Text("Interactive Guide:");
			ImGui::BulletText("Tessellation subdivides faces on the fly.");
			ImGui::BulletText("LOD dynamically scales down with distance.");
			ImGui::BulletText("Noise displacement adds high-frequency details.");
		}
		ImGui::End();

		// Apply uniforms to shader
		if (m_tessShader && m_tessShader->isValid()) {
			m_tessShader->use();
			m_tessShader->setFloat("u_tessFactorMin", m_tessFactorMin);
			m_tessShader->setFloat("u_tessFactorMax", m_tessFactorMax);
			m_tessShader->setFloat("u_tessDistMin", m_tessDistMin);
			m_tessShader->setFloat("u_tessDistMax", m_tessDistMax);
			m_tessShader->setFloat("u_tessDetailStrength", m_tessDetailStrength);
			m_tessShader->setFloat("u_tessDetailFrequency", m_tessDetailFrequency);
		}
	}

private:
	std::shared_ptr<Shader> m_tessShader;
	float m_tessFactorMin;
	float m_tessFactorMax;
	float m_tessDistMin;
	float m_tessDistMax;
	float m_tessDetailStrength;
	float m_tessDetailFrequency;
};

int main(int argc, char** argv) {
	Visualizer vis(1280, 960, "Tessellated Model Demo");

	// Disable complex features that are not required for showing tessellation
	auto& config = ConfigManager::GetInstance();
	config.SetBool("render_skybox", false);
	config.SetBool("render_terrain", false);
	config.SetBool("render_decor", false);
	config.SetBool("day_night_cycle", false);
	config.SetBool("enable_floor", true);
	config.SetFloat("ambient_particle_density", 0.0f);
	config.SetBool("enable_shadows", false);

	// Load custom tessellated shader
	auto tessShader = std::make_shared<Shader>(
		"shaders/vis_tess.vert",
		"shaders/vis_tess.frag",
		"shaders/vis_tess.tcs",
		"shaders/vis_tess.tes"
	);

	if (!tessShader->isValid()) {
		std::cerr << "Failed to compile/link tessellation shader!" << std::endl;
		return -1;
	}

	auto widget = std::make_shared<TessellationControlWidget>(tessShader);
	vis.AddWidget(widget);

	// Create model with no_cull=true to prevent winding order/backface culling issues
	auto model = std::make_shared<Model>("assets/utah_teapot.obj", true);
	// Place it slightly above the floor and scale it up for maximum visibility
	model->SetPosition(0.0f, 1.0f, 0.0f);
	model->SetScale(glm::vec3(2.5f));

	vis.AddPrepareCallback([tessShader, model](Visualizer& v) {
		// Register custom tessellated shader and get handle
		ShaderHandle handle = v.RegisterShader(tessShader);

		// Configure model to use custom tessellation shader and PATCH draw mode
		model->SetCustomShaderHandle(handle);
		model->SetCustomDrawMode(GL_PATCHES);

		// Add model to the visualizer
		v.AddShape(model);
	});

	// Position camera nicely facing the teapot model
	Camera cam = vis.GetCamera();
	cam.x = 0;
	cam.y = 5;
	cam.z = 12;
	cam.pitch = -18;
	cam.yaw = 0;
	vis.SetCamera(cam);

	vis.Run();

	return 0;
}
