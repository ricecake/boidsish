#ifndef SHADER_H
#define SHADER_H

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <filesystem>

#include <GL/glew.h>
#include <glm/glm.hpp>

class ShaderBase {
public:
	struct UniformValue {
		std::variant<std::monostate, bool, int, unsigned int, float, glm::vec2, glm::vec3, glm::vec4, glm::mat2, glm::mat3, glm::mat4, std::vector<int>> value;

		UniformValue() = default;
		template<typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, UniformValue>>>
		UniformValue(T&& v) :
			value(std::forward<T>(v)) {}

		void apply(int location) const {
			if (location == -1)
				return;
			std::visit(
				[location](auto&& arg) {
					using T = std::decay_t<decltype(arg)>;
					if constexpr (std::is_same_v<T, std::monostate>)
						return;
					else if constexpr (std::is_same_v<T, bool>)
						glUniform1i(location, (int)arg);
					else if constexpr (std::is_same_v<T, int>)
						glUniform1i(location, arg);
					else if constexpr (std::is_same_v<T, unsigned int>)
						glUniform1ui(location, arg);
					else if constexpr (std::is_same_v<T, float>)
						glUniform1f(location, arg);
					else if constexpr (std::is_same_v<T, glm::vec2>)
						glUniform2fv(location, 1, &arg[0]);
					else if constexpr (std::is_same_v<T, glm::vec3>)
						glUniform3fv(location, 1, &arg[0]);
					else if constexpr (std::is_same_v<T, glm::vec4>)
						glUniform4fv(location, 1, &arg[0]);
					else if constexpr (std::is_same_v<T, glm::mat2>)
						glUniformMatrix2fv(location, 1, GL_FALSE, &arg[0][0]);
					else if constexpr (std::is_same_v<T, glm::mat3>)
						glUniformMatrix3fv(location, 1, GL_FALSE, &arg[0][0]);
					else if constexpr (std::is_same_v<T, glm::mat4>)
						glUniformMatrix4fv(location, 1, GL_FALSE, &arg[0][0]);
					else if constexpr (std::is_same_v<T, std::vector<int>>)
						glUniform1iv(location, (int)arg.size(), arg.data());
				},
				value
			);
		}
	};

	class UniformGuard {
	public:
		UniformGuard(ShaderBase& s) :
			shader(s) {
			shader.use();
		}

		~UniformGuard() {
			for (auto const& [loc, val] : originalValues) {
				val.apply(loc);
				shader.m_UniformValues[loc] = val;
			}
			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			for (int i = 0; i < 4; ++i) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, 0);
			}
			glActiveTexture(GL_TEXTURE0);
			glUseProgram(0);
		}

		UniformGuard(const UniformGuard&) = delete;
		UniformGuard& operator=(const UniformGuard&) = delete;
		UniformGuard(UniformGuard&&) = default;
		UniformGuard& operator=(UniformGuard&&) = delete;

		void setBool(const std::string& name, bool value) {
			capture(name);
			shader.setBool(name, value);
		}
		void setInt(const std::string& name, int value) {
			capture(name);
			shader.setInt(name, value);
		}
		void setUint(const std::string& name, unsigned int value) {
			capture(name);
			shader.setUint(name, value);
		}
		void setFloat(const std::string& name, float value) {
			capture(name);
			shader.setFloat(name, value);
		}
		void setVec2(const std::string& name, const glm::vec2& value) {
			capture(name);
			shader.setVec2(name, value);
		}
		void setVec2(const std::string& name, float x, float y) {
			capture(name);
			shader.setVec2(name, x, y);
		}
		void setVec3(const std::string& name, const glm::vec3& value) {
			capture(name);
			shader.setVec3(name, value);
		}
		void setVec3(const std::string& name, float x, float y, float z) {
			capture(name);
			shader.setVec3(name, x, y, z);
		}
		void setVec4(const std::string& name, const glm::vec4& value) {
			capture(name);
			shader.setVec4(name, value);
		}
		void setVec4(const std::string& name, float x, float y, float z, float w) {
			capture(name);
			shader.setVec4(name, x, y, z, w);
		}
		void setMat2(const std::string& name, const glm::mat2& mat) {
			capture(name);
			shader.setMat2(name, mat);
		}
		void setMat3(const std::string& name, const glm::mat3& mat) {
			capture(name);
			shader.setMat3(name, mat);
		}
		void setMat4(const std::string& name, const glm::mat4& mat) {
			capture(name);
			shader.setMat4(name, mat);
		}
		void setIntArray(const std::string& name, const int* values, int count) {
			capture(name);
			shader.setIntArray(name, values, count);
		}

	private:
		void capture(const std::string& name) {
			int loc = shader.getUniformLocation(name);
			if (loc != -1 && originalValues.find(loc) == originalValues.end()) {
				originalValues[loc] = shader.getUniformValue(name);
			}
		}
		ShaderBase&                           shader;
		std::unordered_map<int, UniformValue> originalValues;
	};

	unsigned int ID = 0;

	// Rule of Five: Disallow copying due to manual OpenGL resource management.
	// Moving is supported to allow transferring ownership of shader programs.
	ShaderBase() = default;
	ShaderBase(const ShaderBase&) = delete;
	ShaderBase& operator=(const ShaderBase&) = delete;

	ShaderBase(ShaderBase&& other) noexcept :
		ID(other.ID), m_UniformLocationCache(std::move(other.m_UniformLocationCache)) {
		other.ID = 0;
	}

	ShaderBase& operator=(ShaderBase&& other) noexcept {
		if (this != &other) {
			if (ID != 0)
				glDeleteProgram(ID);
			ID = other.ID;
			m_UniformLocationCache = std::move(other.m_UniformLocationCache);
			other.ID = 0;
		}
		return *this;
	}

	virtual ~ShaderBase() {
		if (ID != 0) {
			glDeleteProgram(ID);
		}
	}

	/**
	 * @brief Static registry for shader variable replacements.
	 * Allows synchronizing C++ constants with shader code using [[VAR_NAME]] syntax.
	 */
	static std::map<std::string, std::string>& GetReplacements() {
		static std::map<std::string, std::string> replacements;
		return replacements;
	}

	/**
	 * @brief Register a constant for use in shaders.
	 * In the shader, use [[name]] to reference this constant.
	 */
	static void RegisterConstant(const std::string& name, const std::string& value) {
		GetReplacements()["[[" + name + "]]"] = value;
	}

	/**
	 * @brief Register a numeric constant for use in shaders.
	 */
	template<typename T>
	static void RegisterConstant(const std::string& name, T value) {
		RegisterConstant(name, std::to_string(value));
	}

	// activate the shader
	// ------------------------------------------------------------------------
	void use() { glUseProgram(ID); }

	UniformGuard createGuard() { return UniformGuard(*this); }

	// utility uniform functions
	// ------------------------------------------------------------------------
	void setBool(const std::string& name, bool value) const {
		int loc = getUniformLocation(name);
		glUniform1i(loc, (int)value);
		m_UniformValues[loc] = UniformValue{value};
	}

	// ------------------------------------------------------------------------
	void setUint(const std::string& name, unsigned int value) const {
		int loc = getUniformLocation(name);
		glUniform1ui(loc, value);
		m_UniformValues[loc] = UniformValue{value};
	}

	// ------------------------------------------------------------------------
	void setInt(const std::string& name, int value) const {
		int loc = getUniformLocation(name);
		glUniform1i(loc, value);
		m_UniformValues[loc] = UniformValue{value};
	}

	void trySetInt(const std::string& name, int value) const {
		int loc = getUniformLocation(name);
		if (loc != -1) {
			glUniform1i(loc, value);
			m_UniformValues[loc] = UniformValue{value};
		}
	}

	// ------------------------------------------------------------------------
	void setFloat(const std::string& name, float value) const {
		int loc = getUniformLocation(name);
		glUniform1f(loc, value);
		m_UniformValues[loc] = UniformValue{value};
	}

	// ------------------------------------------------------------------------
	void setVec2(const std::string& name, const glm::vec2& value) const {
		int loc = getUniformLocation(name);
		glUniform2fv(loc, 1, &value[0]);
		m_UniformValues[loc] = UniformValue{value};
	}

	void setVec2(const std::string& name, float x, float y) const {
		int loc = getUniformLocation(name);
		glUniform2f(loc, x, y);
		m_UniformValues[loc] = UniformValue{glm::vec2(x, y)};
	}

	// ------------------------------------------------------------------------
	void setVec3(const std::string& name, const glm::vec3& value) const {
		int loc = getUniformLocation(name);
		glUniform3fv(loc, 1, &value[0]);
		m_UniformValues[loc] = UniformValue{value};
	}

	void setVec3(const std::string& name, float x, float y, float z) const {
		int loc = getUniformLocation(name);
		glUniform3f(loc, x, y, z);
		m_UniformValues[loc] = UniformValue{glm::vec3(x, y, z)};
	}

	// ------------------------------------------------------------------------
	void setVec4(const std::string& name, const glm::vec4& value) const {
		int loc = getUniformLocation(name);
		glUniform4fv(loc, 1, &value[0]);
		m_UniformValues[loc] = UniformValue{value};
	}

	void setVec4(const std::string& name, float x, float y, float z, float w) const {
		int loc = getUniformLocation(name);
		glUniform4f(loc, x, y, z, w);
		m_UniformValues[loc] = UniformValue{glm::vec4(x, y, z, w)};
	}

	// ------------------------------------------------------------------------
	void setMat2(const std::string& name, const glm::mat2& mat) const {
		int loc = getUniformLocation(name);
		glUniformMatrix2fv(loc, 1, GL_FALSE, &mat[0][0]);
		m_UniformValues[loc] = UniformValue{mat};
	}

	// ------------------------------------------------------------------------
	void setMat3(const std::string& name, const glm::mat3& mat) const {
		int loc = getUniformLocation(name);
		glUniformMatrix3fv(loc, 1, GL_FALSE, &mat[0][0]);
		m_UniformValues[loc] = UniformValue{mat};
	}

	// ------------------------------------------------------------------------
	void setMat4(const std::string& name, const glm::mat4& mat) const {
		int loc = getUniformLocation(name);
		glUniformMatrix4fv(loc, 1, GL_FALSE, &mat[0][0]);
		m_UniformValues[loc] = UniformValue{mat};
	}

	// ------------------------------------------------------------------------
	void setIntArray(const std::string& name, const int* values, int count) const {
		int loc = getUniformLocation(name);
		glUniform1iv(loc, count, values);
		m_UniformValues[loc] = UniformValue{std::vector<int>(values, values + count)};
	}

protected:
	int getUniformLocation(const std::string& name) const {
		auto it = m_UniformLocationCache.find(name);
		if (it != m_UniformLocationCache.end()) {
			return it->second;
		}

		int location = glGetUniformLocation(ID, name.c_str());
		m_UniformLocationCache[name] = location;
		return location;
	}

	mutable std::unordered_map<std::string, int>   m_UniformLocationCache;
	mutable std::unordered_map<int, UniformValue>  m_UniformValues;
	mutable std::unordered_map<int, GLenum>        m_UniformTypeCache;

	UniformValue getUniformValue(const std::string& name) const {
		int  loc = getUniformLocation(name);
		auto it = m_UniformValues.find(loc);
		if (it != m_UniformValues.end()) {
			return it->second;
		}
		// Fetch from GL
		UniformValue val = fetchFromGL(loc);
		m_UniformValues[loc] = val;
		return val;
	}

	UniformValue fetchFromGL(int loc) const {
		if (ID == 0 || loc == -1)
			return {};

		GLenum type = getUniformType(loc);

		switch (type) {
		case GL_FLOAT: {
			float v;
			glGetUniformfv(ID, loc, &v);
			return {v};
		}
		case GL_FLOAT_VEC2: {
			glm::vec2 v;
			glGetUniformfv(ID, loc, &v[0]);
			return {v};
		}
		case GL_FLOAT_VEC3: {
			glm::vec3 v;
			glGetUniformfv(ID, loc, &v[0]);
			return {v};
		}
		case GL_FLOAT_VEC4: {
			glm::vec4 v;
			glGetUniformfv(ID, loc, &v[0]);
			return {v};
		}
		case GL_INT:
		case GL_BOOL:
		case GL_SAMPLER_2D:
		case GL_SAMPLER_CUBE:
		case GL_SAMPLER_2D_ARRAY:
		case GL_SAMPLER_3D: {
			int v;
			glGetUniformiv(ID, loc, &v);
			if (type == GL_BOOL)
				return {(bool)v};
			return {v};
		}
		case GL_UNSIGNED_INT: {
			unsigned int v;
			glGetUniformuiv(ID, loc, &v);
			return {v};
		}
		case GL_FLOAT_MAT2: {
			glm::mat2 v;
			glGetUniformfv(ID, loc, &v[0][0]);
			return {v};
		}
		case GL_FLOAT_MAT3: {
			glm::mat3 v;
			glGetUniformfv(ID, loc, &v[0][0]);
			return {v};
		}
		case GL_FLOAT_MAT4: {
			glm::mat4 v;
			glGetUniformfv(ID, loc, &v[0][0]);
			return {v};
		}
		default:
			return {};
		}
	}

	GLenum getUniformType(int loc) const {
		if (m_UniformTypeCache.empty()) {
			GLint count;
			glGetProgramiv(ID, GL_ACTIVE_UNIFORMS, &count);
			for (GLint i = 0; i < count; i++) {
				GLchar  name[256];
				GLsizei length;
				GLint   size;
				GLenum  type;
				glGetActiveUniform(ID, i, 256, &length, &size, &type, name);
				int l = glGetUniformLocation(ID, name);
				if (l != -1) {
					m_UniformTypeCache[l] = type;
				}
			}
		}
		auto it = m_UniformTypeCache.find(loc);
		if (it != m_UniformTypeCache.end())
			return it->second;
		return 0;
	}

	std::string generateIncludeGuard(const std::string& path) {
		std::string guard = "G";
		for (char c : path) {
			if (std::isalnum(c)) {
				guard += (char)std::toupper(c);
			} else if (!guard.empty() && guard.back() != '_') {
				guard += '_';
			}
		}
		while (!guard.empty() && guard.back() == '_') {
			guard.pop_back();
		}
		return guard;
	}

	std::string loadShaderSource(const std::string& path, std::set<std::string>& includedFiles) {
		namespace fs = std::filesystem;
		fs::path p = fs::absolute(fs::path(path));
		try {
			p = fs::weakly_canonical(p);
			fs::path root = fs::current_path();
			// Use relative path if possible for cleaner guards
			if (p.string().find(root.string()) == 0) {
				p = fs::relative(p, root);
			}
		} catch (...) {
		}

		std::string processedPath = p.string();
		bool        isTopLevel = includedFiles.empty();

		// 1. If we are explicitly loading a unified shader, just load it from disk.
		if (processedPath.find(".unified.") != std::string::npos) {
			if (isTopLevel) {
				return loadFromFile(processedPath);
			}
		}

		std::string sourceCode = loadFromFile(processedPath);

		std::string unifiedPath;
		if (isTopLevel) {
			std::string unifiedName = p.stem().string() + ".unified" + p.extension().string();

			// 2. Determine where we'd expect the unified file
#ifdef BOIDSISH_BUILD_DIR
			fs::path buildDir(BOIDSISH_BUILD_DIR);
			fs::path p_abs = fs::absolute(p);
			fs::path build_abs = fs::absolute(buildDir);

			// Check if already in build dir
			auto it_p = p_abs.begin();
			auto it_b = build_abs.begin();
			bool alreadyInBuild = true;
			while (it_b != build_abs.end()) {
				if (it_p == p_abs.end() || *it_p != *it_b) {
					alreadyInBuild = false;
					break;
				}
				++it_p;
				++it_b;
			}

			if (alreadyInBuild) {
				unifiedPath = "";
			} else {
				// Reconstruct relative to 'shaders/'
				std::string p_str = p.string();
				size_t      s_idx = p_str.find("shaders/");
				if (s_idx != std::string::npos) {
					unifiedPath = (buildDir / p_str.substr(s_idx)).replace_filename(unifiedName).string();
				} else {
					unifiedPath = (buildDir / "shaders" / p.filename()).replace_filename(unifiedName).string();
				}
			}
#else
			unifiedPath = (p.parent_path() / unifiedName).string();
#endif

			if (sourceCode.empty()) {
				// 3. Try loading from unifiedPath
				if (!unifiedPath.empty()) {
					sourceCode = loadFromFile(unifiedPath);
					if (!sourceCode.empty())
						return sourceCode;
				}

				// 4. Try loading from next to original
				fs::path nextToOriginal = p.parent_path() / unifiedName;
				if (nextToOriginal.string() != unifiedPath) {
					sourceCode = loadFromFile(nextToOriginal.string());
					if (!sourceCode.empty())
						return sourceCode;
				}
			}
		}

		if (sourceCode.empty()) {
			if (!isTopLevel) {
				std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << processedPath << std::endl;
			}
			return "";
		}

		if (includedFiles.count(processedPath)) {
			// Prevent circular inclusion
			return "";
		}
		includedFiles.insert(processedPath);

		std::string        guard = generateIncludeGuard(processedPath);
		std::string        finalSource;
		std::string        versionLine;
		std::istringstream iss(sourceCode);
		std::string        line;

		std::string preVersionContent;
		std::string postVersionContent;
		bool        foundVersion = false;

		while (std::getline(iss, line)) {
			// Trim leading whitespace for checks
			std::string trimmed = line;
			size_t      firstNonWhitespace = trimmed.find_first_not_of(" \t\r\n");
			if (firstNonWhitespace != std::string::npos) {
				trimmed.erase(0, firstNonWhitespace);
			}

			// Always look for #include, even before #version (though unusual)
			if (trimmed.substr(0, 8) == "#include") {
				size_t firstQuote = line.find('"');
				size_t lastQuote = line.rfind('"');
				if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote < lastQuote) {
					std::string includePath = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);

					// Search order: 1. Relative to current file, 2. relative to 'shaders/', 3. relative to 'external/'
					std::vector<fs::path> searchPaths;
					searchPaths.push_back(p.parent_path() / includePath);
					searchPaths.push_back(fs::path("shaders") / includePath);
					searchPaths.push_back(fs::path("external") / includePath);

					std::string fullPathStr = "";
					for (const auto& candidate : searchPaths) {
						if (fs::exists(candidate) && !fs::is_directory(candidate)) {
							fullPathStr = candidate.string();
							break;
						}
					}

					if (!fullPathStr.empty()) {
						std::string includedSource = loadShaderSource(fullPathStr, includedFiles);
						std::string commentStart = "//START " + includePath + "\n";
						std::string commentEnd = "//END " + includePath + " (returning to " + processedPath + ")\n";

						if (foundVersion) {
							postVersionContent += commentStart + includedSource;
							if (!includedSource.empty() && includedSource.back() != '\n')
								postVersionContent += "\n";
							postVersionContent += commentEnd;
						} else {
							preVersionContent += commentStart + includedSource;
							if (!includedSource.empty() && includedSource.back() != '\n')
								preVersionContent += "\n";
							preVersionContent += commentEnd;
						}
					} else {
						std::cerr << "ERROR::SHADER::INCLUDE_NOT_FOUND: " << includePath
								  << " (searched in relative, shaders/, and external/)" << std::endl;
					}
					continue;
				}
			}

			if (!foundVersion) {
				if (trimmed.substr(0, 8) == "#version") {
					versionLine = line + "\n";
					foundVersion = true;
					continue;
				}
				preVersionContent += line + "\n";
			} else {
				postVersionContent += line + "\n";
			}
		}

		// If no #version was found, all content is effectively post-version for the purpose of guards
		if (!foundVersion) {
			postVersionContent = preVersionContent;
			preVersionContent = "";
		}

		finalSource = versionLine;
		finalSource += "#ifndef " + guard + "\n";
		finalSource += "#define " + guard + "\n";
		finalSource += preVersionContent;
		finalSource += postVersionContent;
		finalSource += "#endif // " + guard + "\n";

		// Apply variable replacements (e.g., [[MAX_LIGHTS]])
		for (auto const& [placeholder, value] : GetReplacements()) {
			size_t pos = 0;
			while ((pos = finalSource.find(placeholder, pos)) != std::string::npos) {
				finalSource.replace(pos, placeholder.length(), value);
				pos += value.length();
			}
		}

		if (isTopLevel && !finalSource.empty() && !unifiedPath.empty()) {
			try {
				fs::path up(unifiedPath);
				std::filesystem::create_directories(up.parent_path());
				std::ofstream out(unifiedPath);
				if (out.is_open()) {
					out << finalSource;
				}
			} catch (...) {
			}
		}

		return finalSource;
	}

	// utility function for checking shader compilation/linking errors.
	// Returns true if successful, false if error occurred.
	// ------------------------------------------------------------------------
	std::string loadFromFile(const std::string& path) {
		namespace fs = std::filesystem;
		if (fs::exists(path) && !fs::is_directory(path)) {
			std::ifstream f(path);
			if (f.is_open()) {
				std::stringstream ss;
				ss << f.rdbuf();
				return ss.str();
			}
		}
		return "";
	}

	bool checkCompileErrors(GLuint shader, std::string type, std::string filePath) {
		GLint  success;
		GLchar infoLog[1024];
		if (type != "PROGRAM") {
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(shader, 1024, NULL, infoLog);
				std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
						  << filePath << std::endl
						  << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
				return false;
			}
		} else {
			glGetProgramiv(shader, GL_LINK_STATUS, &success);
			if (!success) {
				glGetProgramInfoLog(shader, 1024, NULL, infoLog);
				std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
						  << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
				return false;
			}
		}
		return true;
	}
};

class Shader: public ShaderBase {
public:
	bool valid_{false}; // Track if shader compiled successfully

	// constructor generates the shader on the fly
	// ------------------------------------------------------------------------
	Shader(
		const char* vertexPath,
		const char* fragmentPath,
		const char* tessControlPath = nullptr,
		const char* tessEvaluationPath = nullptr,
		const char* geometryPath = nullptr
	) {
		// 1. retrieve the vertex/fragment source code from filePath
		std::string vertexCode;
		std::string fragmentCode;
		std::string tessControlCode;
		std::string tessEvaluationCode;
		std::string geometryCode;

		std::set<std::string> includedFiles;
		vertexCode = loadShaderSource(vertexPath, includedFiles);

		includedFiles.clear();
		fragmentCode = loadShaderSource(fragmentPath, includedFiles);

		if (tessControlPath != nullptr && tessEvaluationPath != nullptr) {
			includedFiles.clear();
			tessControlCode = loadShaderSource(tessControlPath, includedFiles);
			includedFiles.clear();
			tessEvaluationCode = loadShaderSource(tessEvaluationPath, includedFiles);
		}

		if (geometryPath != nullptr) {
			includedFiles.clear();
			geometryCode = loadShaderSource(geometryPath, includedFiles);
		}

		const char* vShaderCode = vertexCode.c_str();
		const char* fShaderCode = fragmentCode.c_str();
		// 2. compile shaders
		unsigned int vertex, fragment;
		// vertex shader
		vertex = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex, 1, &vShaderCode, NULL);
		glCompileShader(vertex);
		if (!checkCompileErrors(vertex, "VERTEX", vertexPath)) {
			glDeleteShader(vertex);
			ID = 0;
			return;
		}
		// fragment Shader
		fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment, 1, &fShaderCode, NULL);
		glCompileShader(fragment);
		if (!checkCompileErrors(fragment, "FRAGMENT", fragmentPath)) {
			glDeleteShader(vertex);
			glDeleteShader(fragment);
			ID = 0;
			return;
		}

		unsigned int tessControl, tessEvaluation;
		if (tessControlPath != nullptr && tessEvaluationPath != nullptr) {
			const char* tcShaderCode = tessControlCode.c_str();
			tessControl = glCreateShader(GL_TESS_CONTROL_SHADER);
			glShaderSource(tessControl, 1, &tcShaderCode, NULL);
			glCompileShader(tessControl);
			if (!checkCompileErrors(tessControl, "TESS_CONTROL", tessControlPath)) {
				glDeleteShader(vertex);
				glDeleteShader(fragment);
				glDeleteShader(tessControl);
				ID = 0;
				return;
			}

			const char* teShaderCode = tessEvaluationCode.c_str();
			tessEvaluation = glCreateShader(GL_TESS_EVALUATION_SHADER);
			glShaderSource(tessEvaluation, 1, &teShaderCode, NULL);
			glCompileShader(tessEvaluation);
			if (!checkCompileErrors(tessEvaluation, "TESS_EVALUATION", tessEvaluationPath)) {
				glDeleteShader(vertex);
				glDeleteShader(fragment);
				glDeleteShader(tessControl);
				glDeleteShader(tessEvaluation);
				ID = 0;
				return;
			}
		}

		// if geometry shader is given, compile geometry shader
		unsigned int geometry;
		if (geometryPath != nullptr) {
			const char* gShaderCode = geometryCode.c_str();
			geometry = glCreateShader(GL_GEOMETRY_SHADER);
			glShaderSource(geometry, 1, &gShaderCode, NULL);
			glCompileShader(geometry);
			if (!checkCompileErrors(geometry, "GEOMETRY", geometryPath)) {
				glDeleteShader(vertex);
				glDeleteShader(fragment);
				if (tessControlPath != nullptr && tessEvaluationPath != nullptr) {
					glDeleteShader(tessControl);
					glDeleteShader(tessEvaluation);
				}
				glDeleteShader(geometry);
				ID = 0;
				return;
			}
		}

		// shader Program
		ID = glCreateProgram();
		glAttachShader(ID, vertex);
		glAttachShader(ID, fragment);
		if (tessControlPath != nullptr && tessEvaluationPath != nullptr) {
			glAttachShader(ID, tessControl);
			glAttachShader(ID, tessEvaluation);
		}
		if (geometryPath != nullptr) {
			glAttachShader(ID, geometry);
		}

		glLinkProgram(ID);
		if (!checkCompileErrors(ID, "PROGRAM", vertexPath)) {
			glDeleteShader(vertex);
			glDeleteShader(fragment);
			if (tessControlPath != nullptr && tessEvaluationPath != nullptr) {
				glDeleteShader(tessControl);
				glDeleteShader(tessEvaluation);
			}
			if (geometryPath != nullptr) {
				glDeleteShader(geometry);
			}
			glDeleteProgram(ID);
			ID = 0;
			return;
		}

		// delete the shaders as they're linked into our program now and no longer necessary
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		if (tessControlPath != nullptr && tessEvaluationPath != nullptr) {
			glDeleteShader(tessControl);
			glDeleteShader(tessEvaluation);
		}
		if (geometryPath != nullptr) {
			glDeleteShader(geometry);
		}

		valid_ = true;
	}

	bool isValid() const { return valid_ && ID != 0; }
};

class ComputeShader: public ShaderBase {
public:
	bool valid_{false}; // Track if shader compiled successfully

	ComputeShader(const char* computePath) {
		std::string           computeCode;
		std::set<std::string> includedFiles;

		computeCode = loadShaderSource(computePath, includedFiles);

		if (computeCode.empty()) {
			std::cerr << "ERROR::COMPUTE_SHADER::FILE_NOT_FOUND: " << computePath << std::endl;
			ID = 0;
			return;
		}

		// Check if compute shaders are supported
		// Use glGetString which is more reliable on some drivers (especially Mesa)
		int majorVersion = 0, minorVersion = 0;
		const char* versionStr = reinterpret_cast<const char*>(glGetString(GL_VERSION));
		if (versionStr && versionStr[0] != '\0') {
			if (sscanf(versionStr, "%d.%d", &majorVersion, &minorVersion) != 2) {
				std::cerr << "ERROR::COMPUTE_SHADER::VERSION_PARSE_FAILED: Could not parse GL_VERSION: '"
				          << versionStr << "'" << std::endl;
			}
		} else {
			std::cerr << "ERROR::COMPUTE_SHADER::NO_CONTEXT: glGetString(GL_VERSION) returned "
			          << (versionStr ? "empty string" : "NULL")
			          << " - is there an active OpenGL context?" << std::endl;
		}
		if (majorVersion < 4 || (majorVersion == 4 && minorVersion < 3)) {
			std::cerr << "ERROR::COMPUTE_SHADER::UNSUPPORTED: OpenGL " << majorVersion << "." << minorVersion
			          << " does not support compute shaders (requires 4.3+)\n"
			          << "  File: " << computePath << std::endl;
			ID = 0;
			return;
		}

		// Compile compute shader
		unsigned int compute;
		const char*  gShaderCode = computeCode.c_str();
		compute = glCreateShader(GL_COMPUTE_SHADER);
		if (compute == 0) {
			std::cerr << "ERROR::COMPUTE_SHADER::CREATE_FAILED: glCreateShader returned 0\n"
			          << "  File: " << computePath << "\n"
			          << "  GL Error: " << glGetError() << std::endl;
			ID = 0;
			return;
		}

		glShaderSource(compute, 1, &gShaderCode, NULL);
		glCompileShader(compute);

		if (!checkCompileErrors(compute, "COMPUTE", computePath)) {
			glDeleteShader(compute);
			ID = 0;
			return;
		}

		// shader Program
		ID = glCreateProgram();
		glAttachShader(ID, compute);

		glLinkProgram(ID);
		if (!checkCompileErrors(ID, "PROGRAM", computePath)) {
			glDeleteShader(compute);
			glDeleteProgram(ID);
			ID = 0;
			return;
		}

		// delete the shaders as they're linked into our program now and no longer necessary
		glDeleteShader(compute);
		valid_ = true;
	}

	bool isValid() const { return valid_ && ID != 0; }

	void dispatch(unsigned int x, unsigned int y, unsigned int z) {
		if (valid_) {
			glDispatchCompute(x, y, z);
		}
	}
};

#endif
