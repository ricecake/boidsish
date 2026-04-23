#include "ScriptManager.h"
#include "graphics.h"
#include "shape.h"
#include <iostream>

namespace Boidsish {

	ScriptManager::ScriptManager(Visualizer& visualizer): m_visualizer(visualizer), m_context(m_runtime) {
		auto& module = m_context.addModule("boidsish");

		// Bind Shape class
		module.class_<Shape>("Shape")
			.fun<&Shape::GetId>("GetId")
			.fun<&Shape::GetX>("GetX")
			.fun<&Shape::GetY>("GetY")
			.fun<&Shape::GetZ>("GetZ")
			.fun<&Shape::GetPosition>("GetPosition")
			.fun<&Shape::SetPosition>("SetPosition")
			.fun<&Shape::GetR>("GetR")
			.fun<&Shape::GetG>("GetG")
			.fun<&Shape::GetB>("GetB")
			.fun<&Shape::GetA>("GetA")
			.fun<&Shape::SetColor>("SetColor")
			.fun<&Shape::GetScale>("GetScale")
			.fun<&Shape::SetScale>("SetScale");

		// Global functions
		m_context.global().add("getShapes", [this]() -> std::vector<std::shared_ptr<Shape>> {
			return m_visualizer.GetPersistentShapes();
		});

		m_context.eval("import * as boidsish from 'boidsish'; globalThis.boidsish = boidsish;", "<init>", JS_EVAL_TYPE_MODULE);
	}

	ScriptManager::~ScriptManager() {}

	std::string ScriptManager::Execute(const std::string& code) {
		try {
			qjs::Value result = m_context.eval(code);
			if (JS_IsException(result.v)) {
				qjs::Value exception = m_context.getException();
				return "Error: " + (std::string)exception;
			}
			if (JS_IsUndefined(result.v)) {
				return "undefined";
			}
			return (std::string)result;
		} catch (const qjs::exception& e) {
			return "Script Error";
		} catch (const std::exception& e) {
			return "Error: " + std::string(e.what());
		}
	}

} // namespace Boidsish
