#pragma once

namespace Boidsish {

	/**
	 * @brief Base interface for all manager classes in the engine.
	 *
	 * Managers are responsible for handling specific resources or systems
	 * and typically require an explicit initialization step.
	 */
	class IManager {
	public:
		virtual ~IManager() = default;

		/**
		 * @brief Initialize the manager.
		 */
		virtual void Initialize() = 0;
	};

} // namespace Boidsish
