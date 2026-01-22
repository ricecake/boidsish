#ifndef SHADER_H
#define SHADER_H

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

#include <GL/glew.h>
#include <glm/glm.hpp>

class ShaderBase {
public:
	unsigned int ID;

	// activate the shader
	// ------------------------------------------------------------------------
	void use() { glUseProgram(ID); }

	// utility uniform functions
	// ------------------------------------------------------------------------
	void setBool(const std::string& name, bool value) const {
		glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
	}

	// ------------------------------------------------------------------------
	void setInt(const std::string& name, int value) const {
		glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
	}

	// ------------------------------------------------------------------------
	void setFloat(const std::string& name, float value) const {
		glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
	}

	// ------------------------------------------------------------------------
	void setVec2(const std::string& name, const glm::vec2& value) const {
		glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
	}

	void setVec2(const std::string& name, float x, float y) const {
		glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y);
	}

	// ------------------------------------------------------------------------
	void setVec3(const std::string& name, const glm::vec3& value) const {
		glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
	}

	void setVec3(const std::string& name, float x, float y, float z) const {
		glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
	}

	// ------------------------------------------------------------------------
	void setVec4(const std::string& name, const glm::vec4& value) const {
		glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
	}

	void setVec4(const std::string& name, float x, float y, float z, float w) {
		glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
	}

	// ------------------------------------------------------------------------
	void setMat2(const std::string& name, const glm::mat2& mat) const {
		glUniformMatrix2fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
	}

	// ------------------------------------------------------------------------
	void setMat3(const std::string& name, const glm::mat3& mat) const {
		glUniformMatrix3fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
	}

	// ------------------------------------------------------------------------
	void setMat4(const std::string& name, const glm::mat4& mat) const {
		glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
	}

	// ------------------------------------------------------------------------
	void setIntArray(const std::string& name, const int* values, int count) const {
		glUniform1iv(glGetUniformLocation(ID, name.c_str()), count, values);
	}

protected:
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
				std::string includePath = line.substr(line.find('"') + 1, line.rfind('"') - line.find('"') - 1);
				finalSource += loadShaderSource(directory + "/" + includePath, includedFiles);
			} else {
				finalSource += line + "\n";
			}
		}
		return finalSource;
	}

	// utility function for checking shader compilation/linking errors.
	// ------------------------------------------------------------------------
	void checkCompileErrors(GLuint shader, std::string type, std::string filePath) {
		GLint  success;
		GLchar infoLog[1024];
		if (type != "PROGRAM") {
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(shader, 1024, NULL, infoLog);
				std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
						  << filePath << std::endl
						  << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		} else {
			glGetProgramiv(shader, GL_LINK_STATUS, &success);
			if (!success) {
				glGetProgramInfoLog(shader, 1024, NULL, infoLog);
				std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
						  << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		}
	}
};

class Shader: public ShaderBase {
public:
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
		checkCompileErrors(vertex, "VERTEX", vertexPath);
		// fragment Shader
		fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment, 1, &fShaderCode, NULL);
		glCompileShader(fragment);
		checkCompileErrors(fragment, "FRAGMENT", fragmentPath);

		unsigned int tessControl, tessEvaluation;
		if (tessControlPath != nullptr && tessEvaluationPath != nullptr) {
			const char* tcShaderCode = tessControlCode.c_str();
			tessControl = glCreateShader(GL_TESS_CONTROL_SHADER);
			glShaderSource(tessControl, 1, &tcShaderCode, NULL);
			glCompileShader(tessControl);
			checkCompileErrors(tessControl, "TESS_CONTROL", tessControlPath);

			const char* teShaderCode = tessEvaluationCode.c_str();
			tessEvaluation = glCreateShader(GL_TESS_EVALUATION_SHADER);
			glShaderSource(tessEvaluation, 1, &teShaderCode, NULL);
			glCompileShader(tessEvaluation);
			checkCompileErrors(tessEvaluation, "TESS_EVALUATION", tessEvaluationPath);
		}

		// if geometry shader is given, compile geometry shader
		unsigned int geometry;
		if (geometryPath != nullptr) {
			const char* gShaderCode = geometryCode.c_str();
			geometry = glCreateShader(GL_GEOMETRY_SHADER);
			glShaderSource(geometry, 1, &gShaderCode, NULL);
			glCompileShader(geometry);
			checkCompileErrors(geometry, "GEOMETRY", geometryPath);
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
		checkCompileErrors(ID, "PROGRAM", vertexPath);

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
	}
};

class ComputeShader: public ShaderBase {
public:
	ComputeShader(const char* computePath) {
		// 1. retrieve the vertex/fragment source code from filePath
		std::string vertexCode;
		std::string fragmentCode;
		std::string tessControlCode;
		std::string tessEvaluationCode;
		std::string geometryCode;
		std::string computeCode;

		std::set<std::string> includedFiles;

		computeCode = loadShaderSource(computePath, includedFiles);

		// if compute shader is given, compile geometry shader
		unsigned int compute;
		const char*  gShaderCode = computeCode.c_str();
		compute = glCreateShader(GL_COMPUTE_SHADER);
		glShaderSource(compute, 1, &gShaderCode, NULL);
		glCompileShader(compute);
		checkCompileErrors(compute, "COMPUTE", computePath);

		// shader Program
		ID = glCreateProgram();
		glAttachShader(ID, compute);

		glLinkProgram(ID);
		checkCompileErrors(ID, "PROGRAM", computePath);

		// delete the shaders as they're linked into our program now and no longer necessary
		glDeleteShader(compute);
	}

	void dispatch(unsigned int x, unsigned int y, unsigned int z) {
		glDispatchCompute(x, y, z);
	}
};

#endif
