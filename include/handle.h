#pragma once

#include <compare>
#include <cstdint>
#include <functional>

namespace Boidsish {

	/**
	 * @brief A generic type-safe and tagged handle for resources.
	 *
	 * @tparam T The type of the resource this handle refers to.
	 * @tparam Tag A unique tag to differentiate handle types for different resource categories.
	 */
	template <typename T, typename Tag = T>
	struct Handle {
		using ValueType = uint32_t;
		ValueType id = 0;

		constexpr Handle() = default;

		constexpr explicit Handle(ValueType id): id(id) {}

		constexpr bool IsValid() const { return id != 0; }

		constexpr explicit operator bool() const { return IsValid(); }

		auto operator<=>(const Handle&) const = default;
		bool operator==(const Handle&) const = default;
	};

} // namespace Boidsish

// Hash support for using Handle in unordered maps
namespace std {
	template <typename T, typename Tag>
	struct hash<Boidsish::Handle<T, Tag>> {
		size_t operator()(const Boidsish::Handle<T, Tag>& h) const { return hash<uint32_t>{}(h.id); }
	};
} // namespace std
