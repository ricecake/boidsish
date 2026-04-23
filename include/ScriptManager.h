#pragma once

#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "quickjspp.hpp"

namespace Boidsish {

	class Visualizer;
	class Shape;

	class ScriptManager {
	public:
		ScriptManager(Visualizer& visualizer);
		~ScriptManager();

		std::string Execute(const std::string& code);

	private:
		Visualizer& m_visualizer;
		qjs::Runtime m_runtime;
		qjs::Context m_context;
	};

} // namespace Boidsish

// QuickJS++ traits for extra types
namespace qjs {
	template <>
	struct js_traits<float> {
		static float unwrap(JSContext* ctx, JSValueConst v) {
			double r;
			if (JS_ToFloat64(ctx, &r, v))
				throw exception{ctx};
			return (float)r;
		}

		static JSValue wrap(JSContext* ctx, float i) noexcept { return JS_NewFloat64(ctx, (double)i); }
	};

	template <>
	struct js_traits<glm::vec3> {
		static JSValue wrap(JSContext* ctx, const glm::vec3& v) noexcept {
			JSValue obj = JS_NewObject(ctx);
			JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, v.x));
			JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, v.y));
			JS_SetPropertyStr(ctx, obj, "z", JS_NewFloat64(ctx, v.z));
			return obj;
		}
		static glm::vec3 unwrap(JSContext* ctx, JSValueConst v) {
			glm::vec3 res;
			JSValue  x = JS_GetPropertyStr(ctx, v, "x");
			JSValue  y = JS_GetPropertyStr(ctx, v, "y");
			JSValue  z = JS_GetPropertyStr(ctx, v, "z");
			double   dv;
			JS_ToFloat64(ctx, &dv, x);
			res.x = (float)dv;
			JS_ToFloat64(ctx, &dv, y);
			res.y = (float)dv;
			JS_ToFloat64(ctx, &dv, z);
			res.z = (float)dv;
			JS_FreeValue(ctx, x);
			JS_FreeValue(ctx, y);
			JS_FreeValue(ctx, z);
			return res;
		}
	};
} // namespace qjs
