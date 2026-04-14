#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <tuple>
#include <typeindex>
#include <unordered_map>

class ServiceLocator {
private:
	std::unordered_map<std::type_index, std::shared_ptr<void>>                                 instances;
	std::unordered_map<std::type_index, std::function<std::shared_ptr<void>(ServiceLocator&)>> factories;

	// Read-write lock for thread safety
	mutable std::shared_mutex mtx;

public:
	template <typename T, typename... Args>
	void Register(Args... args) {
		// Exclusive lock for writing to factories
		std::unique_lock lock(mtx);

		factories[std::type_index(typeid(T))] =
			[captured_args = std::make_tuple(std::move(args)...)](ServiceLocator& loc) {
				return std::apply(
					[&loc](const auto&... unpacked) { return std::make_shared<T>(loc, unpacked...); },
					captured_args
				);
			};
	}

	template <typename T>
	std::shared_ptr<T> Get() {
		auto type_idx = std::type_index(typeid(T));

		// 1. Fast path: Read with a shared lock
		{
			std::shared_lock shared_lock(mtx);
			if (auto it = instances.find(type_idx); it != instances.end()) {
				return std::static_pointer_cast<T>(it->second);
			}
		}

		// 2. Slow path: We need to instantiate. Grab an exclusive lock.
		std::unique_lock exclusive_lock(mtx);

		// Double-check: another thread might have instantiated it while we were waiting for the lock
		if (auto it = instances.find(type_idx); it != instances.end()) {
			return std::static_pointer_cast<T>(it->second);
		}

		// Construct, cache, and return
		if (auto it = factories.find(type_idx); it != factories.end()) {
			std::shared_ptr<void> instance = it->second(*this);
			instances[type_idx] = instance;
			return std::static_pointer_cast<T>(instance);
		}

		throw std::runtime_error("Service not registered.");
	}

	struct Proxy {
		ServiceLocator& locator;

		template <typename T>
		operator std::shared_ptr<T>() const {
			return locator.Get<T>();
		}
	};

	Proxy operator()() { return Proxy{*this}; }
};