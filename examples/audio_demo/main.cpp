#include <cmath>
#include <filesystem>
#include <functional> // For std::ref
#include <iostream>
#include <vector>

#include "audio_manager.h"
#include "dot.h"
#include "entity.h"
#include "graphics.h"
#include "sound_effect.h"

using namespace Boidsish;

// A simple entity that moves in a circle
class MovingSoundEntity: public Entity<Dot> {
public:
	MovingSoundEntity(float radius): Entity<Dot>(), m_radius(radius) {
		shape_ = std::make_shared<Dot>(0, 0, 0, 10.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0);
	}

	void UpdateEntity(const EntityHandler&, float time, float) override {
		float x = cos(time) * m_radius;
		float z = sin(time) * m_radius;
		SetPosition(Vector3(x, 0.0f, z));
	}

private:
	float m_radius;
};

// Custom handler for the audio demo
class AudioDemoHandler: public EntityHandler {
public:
	AudioDemoHandler(task_thread_pool::task_thread_pool& thread_pool, Visualizer& visualizer):
		EntityHandler(thread_pool), m_visualizer(visualizer) {
		AddEntity<MovingSoundEntity>(5.0f);
		// Create a looping sound that we can move
		m_moving_sound = m_visualizer.AddSoundEffect(
			"assets/test_sound.wav",
			glm::vec3(0.0f), // Initial position
			glm::vec3(0.0f), // No velocity, we'll set position directly
			1.0f,            // Volume
			true             // Loop = true
		);
	}

protected:
	void PostTimestep(float, float) override {
		if (m_moving_sound) {
			// There's only one entity, so this is safe for the demo
			const auto& entity = GetAllEntities().begin()->second;
			const auto& pos = entity->GetPosition();
			m_moving_sound->SetPosition(glm::vec3(pos.x, pos.y, pos.z));
		}
	}

private:
	Visualizer&                  m_visualizer;
	std::shared_ptr<SoundEffect> m_moving_sound;
};

int main() {
	try {
		auto path = std::filesystem::current_path();
		std::cout << "CWD: " << path.string() << std::endl;

		Visualizer viz(1024, 768, "Audio Demo");
		viz.SetCameraMode(CameraMode::FREE);

		// Create the handler
		AudioDemoHandler handler(viz.GetThreadPool(), viz);

		// Play background music
		viz.GetAudioManager().PlayMusic("assets/background_music.ogg", true);

		viz.AddShapeHandler(std::ref(handler));

		viz.Run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
