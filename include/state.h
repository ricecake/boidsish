#pragma once
#include <functional>
#include <optional>
#include <variant>
#include <vector>

template <class... Ts>
struct overload: Ts... {
	using Ts::operator()...;
};

namespace boidish {
	namespace state {
		namespace actions {
			namespace grass {
				struct ToggleGrassSystem {
					bool status;
				};
			}; // namespace grass
		}; // namespace actions

		class SystemConfiguration {
		public:
			struct {
				bool enabled;

				struct {
					float length;
					float width;
				} settings;
			} grass;
		};

		class SystemState {
		public:
			SystemConfiguration target;
			SystemConfiguration actual;
		};

		// template<auto... Params>
		// class Action {};

		// class Action {};

		using Action = std::variant<actions::grass::ToggleGrassSystem>;

		// Reducer must remain deterministic and completely side-effect free
		SystemState AppReducer(const SystemState& previousState, const Action& action) {
			SystemState newState = previousState; // Deep copy state snapshot

			std::visit(
				[&](auto&& arg) {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<T, actions::grass::ToggleGrassSystem>) {
						newState.target.grass.enabled += arg.status;
					}
				},
				action);

			// std::visit(overload {
			// 	[](actions::grass::ToggleGrassSystem action) {newState.target.grass.enabled += arg.status;}
			// }, action);

			return newState;
		}

		class Store {
		public:
			using Reducer = std::function<SystemState(const SystemState&, const Action&)>;
			using Listener = std::function<void(const SystemState&)>;

			Store(Reducer reducer, SystemState initialState): m_reducer(reducer), m_state(initialState) {}

			// Dispatch an action to trigger state mutations
			void Dispatch(const Action& action) {
				m_state = m_reducer(m_state, action);
				for (const auto& listener : m_listeners) {
					listener(m_state);
				}
			}

			// Read-only access to state
			const SystemState& GetState() const { return m_state; }

			// Register UI reactive hooks
			void Subscribe(Listener listener) { m_listeners.push_back(listener); }

		private:
			Reducer               m_reducer;
			SystemState           m_state;
			std::vector<Listener> m_listeners;
		};
	}; // namespace state
}; // namespace boidish