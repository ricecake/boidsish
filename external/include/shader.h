#ifndef SHADER_H
#define SHADER_H

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

#include <GL/glew.h>
#include <glm/glm.hpp>

class ShaderBase {
public:
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

	// utility uniform functions
	// ------------------------------------------------------------------------
	void setBool(const std::string& name, bool value) const {
		glUniform1i(getUniformLocation(name), (int)value);
	}

	// ------------------------------------------------------------------------
	void setInt(const std::string& name, int value) const {
		glUniform1i(getUniformLocation(name), value);
	}

	// ------------------------------------------------------------------------
	void setFloat(const std::string& name, float value) const {
		glUniform1f(getUniformLocation(name), value);
	}

	// ------------------------------------------------------------------------
	void setVec2(const std::string& name, const glm::vec2& value) const {
		glUniform2fv(getUniformLocation(name), 1, &value[0]);
	}

	void setVec2(const std::string& name, float x, float y) const {
		glUniform2f(getUniformLocation(name), x, y);
	}

	// ------------------------------------------------------------------------
	void setVec3(const std::string& name, const glm::vec3& value) const {
		glUniform3fv(getUniformLocation(name), 1, &value[0]);
	}

	void setVec3(const std::string& name, float x, float y, float z) const {
		glUniform3f(getUniformLocation(name), x, y, z);
	}

	// ------------------------------------------------------------------------
	void setVec4(const std::string& name, const glm::vec4& value) const {
		glUniform4fv(getUniformLocation(name), 1, &value[0]);
	}

	void setVec4(const std::string& name, float x, float y, float z, float w) {
		glUniform4f(getUniformLocation(name), x, y, z, w);
	}

	// ------------------------------------------------------------------------
	void setMat2(const std::string& name, const glm::mat2& mat) const {
		glUniformMatrix2fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
	}

	// ------------------------------------------------------------------------
	void setMat3(const std::string& name, const glm::mat3& mat) const {
		glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
	}

	// ------------------------------------------------------------------------
	void setMat4(const std::string& name, const glm::mat4& mat) const {
		glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &mat[0][0]);
	}

	// ------------------------------------------------------------------------
	void setIntArray(const std::string& name, const int* values, int count) const {
		glUniform1iv(getUniformLocation(name), count, values);
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

	mutable std::unordered_map<std::string, int> m_UniformLocationCache;
	std::string loadShaderSource(const std::string& path, std::set<std::string>& includedFiles) {
		if (includedFiles.count(path)) {
			// Prevent circular inclusion
			return "";
		}
		includedFiles.insert(path);

		std::string   sourceCode;
		std::ifstream shaderFile;
		shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

		try {
			shaderFile.open(path);
			std::stringstream shaderStream;
			shaderStream << shaderFile.rdbuf();
			shaderFile.close();
			sourceCode = shaderStream.str();
		} catch (std::ifstream::failure& e) {
			std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << path << " " << e.what() << std::endl;
			return "";
		}

		std::string        finalSource;
		std::istringstream iss(sourceCode);
		std::string        line;

		size_t      last_slash_idx = path.rfind('/');
		std::string directory = "";
		if (std::string::npos != last_slash_idx) {
			directory = path.substr(0, last_slash_idx);
		}

		while (std::getline(iss, line)) {
			if (line.substr(0, 8) == "#include") {
				size_t firstQuote = line.find('"');
				size_t lastQuote = line.rfind('"');
				if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote < lastQuote) {
					std::string includePath = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);

					// Search order: 1. Relative to current file, 2. relative to 'shaders/', 3. relative to 'external/'
					std::vector<std::string> searchPaths;
					if (!directory.empty()) {
						searchPaths.push_back(directory + "/" + includePath);
					} else {
						searchPaths.push_back(includePath);
					}
					searchPaths.push_back("shaders/" + includePath);
					searchPaths.push_back("external/" + includePath);

					std::string fullPath = "";
					for (const auto& candidate : searchPaths) {
						std::ifstream checkFile(candidate);
						if (checkFile.is_open()) {
							fullPath = candidate;
							checkFile.close();
							break;
						}
					}

					if (!fullPath.empty()) {
						finalSource += loadShaderSource(fullPath, includedFiles);
					} else {
						std::cerr << "ERROR::SHADER::INCLUDE_NOT_FOUND: " << includePath
								  << " (searched in relative, shaders/, and external/)" << std::endl;
					}
				}
			} else {
				finalSource += line + "\n";
			}
		}

		// Apply variable replacements (e.g., [[MAX_LIGHTS]])
		for (auto const& [placeholder, value] : GetReplacements()) {
			size_t pos = 0;
			while ((pos = finalSource.find(placeholder, pos)) != std::string::npos) {
				finalSource.replace(pos, placeholder.length(), value);
				pos += value.length();
			}
		}

		return finalSource;
	}

	// utility function for checking shader compilation/linking errors.
	// Returns true if successful, false if error occurred.
	// ------------------------------------------------------------------------
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
