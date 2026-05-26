#include "lightning_manager.h"
#include "service_locator.h"
#include "light_manager.h"
#include "profiler.h"
#include "render_shader.h"
#include "shader.h"
#include <algorithm>
#include <random>
#include <GL/glew.h>

namespace Boidsish {

	 LightningManager::LightningManager(ServiceLocator& loc) : _loc(loc) {}

	 LightningManager::~LightningManager() {
		 if (_vao) glDeleteVertexArrays(1, &_vao);
		 if (_vbo) glDeleteBuffers(1, &_vbo);
	 }

	 void LightningManager::Initialize() {
		 _shader = std::make_unique<Shader>("shaders/lightning.vert", "shaders/lightning.frag");

		 glGenVertexArrays(1, &_vao);
		 glGenBuffers(1, &_vbo);

		 glBindVertexArray(_vao);
		 glBindBuffer(GL_ARRAY_BUFFER, _vbo);

		 glEnableVertexAttribArray(0);
		 glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LightningVertex), (void*)offsetof(LightningVertex, pos));

		 glEnableVertexAttribArray(1);
		 glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LightningVertex), (void*)offsetof(LightningVertex, color));

		 glEnableVertexAttribArray(2);
		 glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(LightningVertex), (void*)offsetof(LightningVertex, intensity));

		 glBindVertexArray(0);
	 }

	 void LightningManager::Update(float deltaTime, float totalTime) {
		 PROJECT_PROFILE_SCOPE("LightningManager::Update");

		 _globalPulse = 0.0f;
		 _globalColor = glm::vec3(0.0f);

		 auto light_manager = _loc.Get<LightManager>();

		 for (auto it = _activeStrikes.begin(); it != _activeStrikes.end(); ) {
			 it->lifetime += deltaTime;

			 if (it->lifetime >= it->max_lifetime) {
				 it = _activeStrikes.erase(it);
				 continue;
			 }

			 // Intensity curve: quick peak then flickering decay
			 float t = it->lifetime / it->max_lifetime;
			 if (t < 0.1f) {
				 it->intensity = (t / 0.1f) * _intensityMultiplier;
			 } else {
				 float flicker = (sin(it->lifetime * 100.0f) * 0.5f + 0.5f) * 0.4f + 0.6f;
				 it->intensity = (1.0f - (t - 0.1f) / 0.9f) * flicker * _intensityMultiplier;
			 }

			 // Contribute to global pulse (mostly for cloud lightning and volumetrics)
			 float contribution = it->intensity;
			 if (it->type == LightningType::CLOUD_TO_CLOUD) contribution *= 0.5f;

			 if (contribution > _globalPulse) {
				 _globalPulse = contribution;
				 _globalColor = it->color;
			 }

			 // Spawn physical light for terrain illumination
			 if (!it->has_spawned_flash && it->type != LightningType::CLOUD_TO_CLOUD && t > 0.05f) {
				 if (light_manager && !it->segments.empty()) {
					 glm::vec3 strikePos = it->segments.back().end;
					 Light flash = Light::CreateFlash(strikePos, 150.0f * it->intensity, it->color, 300.0f);
					 flash.auto_remove = true;
					 flash.SetEaseOut(it->max_lifetime * 0.8f);
					 light_manager->AddLight(flash);
					 it->has_spawned_flash = true;
				 }
			 }

			 ++it;
		 }
	 }

	 void LightningManager::Render(const glm::mat4& view, const glm::mat4& projection) {
		 if (_activeStrikes.empty()) return;

		 PROJECT_PROFILE_SCOPE("LightningManager::Render");

		 std::vector<LightningVertex> vertices;
		 for (const auto& strike : _activeStrikes) {
			 for (const auto& seg : strike.segments) {
				 vertices.push_back({seg.start, strike.color, strike.intensity});
				 vertices.push_back({seg.end, strike.color, strike.intensity});
			 }
		 }

		 if (vertices.empty()) return;

		 _shader->use();
		 _shader->setMat4("view", view);
		 _shader->setMat4("projection", projection);

		 glBindBuffer(GL_ARRAY_BUFFER, _vbo);
		 glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(LightningVertex), vertices.data(), GL_STREAM_DRAW);

		 glBindVertexArray(_vao);
		 glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));
		 glBindVertexArray(0);
	 }

	 void LightningManager::TriggerStrike(LightningType type, const glm::vec3& startPos, const glm::vec3& endPos, const glm::vec3& color) {
		 LightningStrike strike;
		 strike.id = _nextStrikeId++;
		 strike.type = type;
		 strike.color = color;
		 strike.lifetime = 0.0f;
		 strike.max_lifetime = (0.2f + (static_cast<float>(rand()) / RAND_MAX) * 0.3f) * _lifetimeMultiplier;
		 strike.intensity = 0.0f;
		 strike.has_spawned_flash = false;

		 if (type == LightningType::BOLT || type == LightningType::FORK) {
			 GenerateBolt(strike, startPos, endPos, (type == LightningType::FORK) ? 5 : 0);
		 } else {
			 // Cloud to cloud - simpler segments
			 glm::vec3 current = startPos;
			 int segments = 5 + rand() % 5;
			 for (int i = 0; i < segments; ++i) {
				 float t = static_cast<float>(i + 1) / segments;
				 glm::vec3 next = glm::mix(startPos, endPos, t);
				 next += glm::vec3(
					 (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 50.0f,
					 (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 20.0f,
					 (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 50.0f
				 );
				 strike.segments.push_back({current, next});
				 current = next;
			 }
		 }

		 _activeStrikes.push_back(std::move(strike));
	 }

	 void LightningManager::GenerateBolt(LightningStrike& strike, const glm::vec3& start, const glm::vec3& end, int depth) {
		 float dist = glm::distance(start, end);
		 if (dist < 10.0f || depth > 8) {
			 strike.segments.push_back({start, end});
			 return;
		 }

		 glm::vec3 mid = (start + end) * 0.5f;
		 glm::vec3 offset = glm::vec3(
			 (static_cast<float>(rand()) / RAND_MAX - 0.5f),
			 (static_cast<float>(rand()) / RAND_MAX - 0.5f),
			 (static_cast<float>(rand()) / RAND_MAX - 0.5f)
		 ) * dist * 0.15f;

		 mid += offset;

		 // Potential fork
		 if (depth > 0 && (static_cast<float>(rand()) / RAND_MAX) < _branchProbability) {
			 glm::vec3 direction = glm::normalize(mid - start);
			 glm::vec3 forkEnd = mid + direction * dist * 0.5f;
			 forkEnd += glm::vec3(
				 (static_cast<float>(rand()) / RAND_MAX - 0.5f),
				 -0.5f, // Mostly downwards
				 (static_cast<float>(rand()) / RAND_MAX - 0.5f)
			 ) * dist * 0.3f;

			 GenerateBolt(strike, mid, forkEnd, depth - 1);
		 }

		 GenerateBolt(strike, start, mid, depth + 1);
		 GenerateBolt(strike, mid, end, depth + 1);
	 }

} // namespace Boidsish
