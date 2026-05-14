#include <iostream>
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "asset_manager.h"
#include "bindless_texture.h"
#include "graphics.h"
#include "shader.h"

using namespace Boidsish;

// Simple quad geometry
float quadVertices[] = {
	// positions        // texture Coords
	-0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
	-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
	 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
	 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
};

unsigned int quadVAO = 0;
unsigned int quadVBO = 0;

void RenderQuad(Shader& shader, uint64_t handle) {
	if (quadVAO == 0) {
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	shader.use();
	shader.setHandle64("u_Texture", handle);
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

int main() {
	try {
		Visualizer viz(1024, 768, "Bindless Texture Demo");

		// We need an OpenGL context to be active before we can load textures and create bindless handles.
		// Visualizer creates the context in its constructor.

		std::unique_ptr<Shader> bindlessShader;
		BindlessTextureHandle   btHandle;
		uint64_t                gpuHandle = 0;

		viz.AddPrepareCallback([&](Visualizer& v) {
			bindlessShader = std::make_unique<Shader>("shaders/bindless_demo.vert", "shaders/bindless_demo.frag");

			// Load a texture
			GLuint texID = AssetManager::GetInstance().GetTexture("assets/container.jpg");
			if (texID == 0) {
				// Fallback if container.jpg is missing
				texID = AssetManager::GetInstance().GetTexture("assets/awesomeface.png");
			}

			if (texID != 0) {
				btHandle = BindlessTextureManager::GetInstance().GetBindlessHandle(texID);
				BindlessTextureManager::GetInstance().MakeResident(btHandle);
				gpuHandle = BindlessTextureManager::GetInstance().GetGpuHandle(btHandle);
				std::cout << "Bindless handle created: 0x" << std::hex << gpuHandle << std::dec << std::endl;
			} else {
				std::cerr << "Failed to load test texture!" << std::endl;
			}
		});

		viz.AddInputCallback([&](const InputState& input) {
			if (bindlessShader && gpuHandle != 0) {
				RenderQuad(*bindlessShader, gpuHandle);
			}
		});

		viz.Run();

	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
