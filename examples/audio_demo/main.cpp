#include <cmath>
#include <filesystem>
#include <functional> // For std::ref
#include <iostream>
#include <vector>

#include "audio_manager.h"
#include "dot.h"
#include "entity.h"
#include "graphics.h"

using namespace Boidsish;

// A simple entity that moves in a circle
class MovingSoundEntity: public Entity<Dot> {
public:
	MovingSoundEntity(int id, float radius): Entity<Dot>(id), m_radius(radius) {
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
	AudioDemoHandler(task_thread_pool::task_thread_pool& thread_pool, AudioManager& audio_manager):
		EntityHandler(thread_pool), m_audio_manager(audio_manager), m_sound_timer(0.0f) {
		AddEntity<MovingSoundEntity>(5.0f);
	}

protected:
	void PostTimestep(float, float delta_time) override {
		m_sound_timer += delta_time;
		if (m_sound_timer > 1.0f) {
			m_sound_timer = 0.0f;
			// There's only one entity, so this is safe for the demo
			const auto& entity = GetAllEntities().begin()->second;
			const auto& pos = entity->GetPosition();
			m_audio_manager.PlaySound("assets/test_sound.wav", glm::vec3(pos.x, pos.y, pos.z));
		}
	}

private:
	AudioManager& m_audio_manager;
	float         m_sound_timer;
};

int main() {
	try {
		auto path = std::filesystem::current_path();
		std::cout << "CWD: " << path.string() << std::endl;

		Visualizer viz(1024, 768, "Audio Demo");
		viz.SetCameraMode(CameraMode::FREE);

		// Create the handler
		AudioDemoHandler handler(viz.GetThreadPool(), viz.GetAudioManager());

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
