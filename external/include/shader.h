#ifndef SHADER_H
#define SHADER_H

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <set>
#include <filesystem>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>

class Shader {
public:
	unsigned int ID;

	// constructor generates the shader on the fly
	// ------------------------------------------------------------------------
	Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath = nullptr, const char* tessControlPath = nullptr, const char* tessEvalPath = nullptr) {
		std::string vertexCode = loadShader(vertexPath);
		std::string fragmentCode = loadShader(fragmentPath);
		std::string geometryCode = loadShader(geometryPath);
		std::string tessControlCode = loadShader(tessControlPath);
		std::string tessEvalCode = loadShader(tessEvalPath);

		const char* vShaderCode = vertexCode.c_str();
		const char* fShaderCode = fragmentCode.c_str();

		unsigned int vertex = compileShader(GL_VERTEX_SHADER, vShaderCode, "VERTEX");
		unsigned int fragment = compileShader(GL_FRAGMENT_SHADER, fShaderCode, "FRAGMENT");

        unsigned int geometry = 0;
		if (geometryPath != nullptr && !geometryCode.empty()) {
			const char* gShaderCode = geometryCode.c_str();
			geometry = compileShader(GL_GEOMETRY_SHADER, gShaderCode, "GEOMETRY");
		}

        unsigned int tessControl = 0;
        if (tessControlPath != nullptr && !tessControlCode.empty()) {
            const char* tcShaderCode = tessControlCode.c_str();
            tessControl = compileShader(GL_TESS_CONTROL_SHADER, tcShaderCode, "TESS_CONTROL");
        }

        unsigned int tessEval = 0;
        if (tessEvalPath != nullptr && !tessEvalCode.empty()) {
            const char* teShaderCode = tessEvalCode.c_str();
            tessEval = compileShader(GL_TESS_EVALUATION_SHADER, teShaderCode, "TESS_EVALUATION");
        }

		// shader Program
		ID = glCreateProgram();
		glAttachShader(ID, vertex);
		glAttachShader(ID, fragment);
		if (geometry != 0)
			glAttachShader(ID, geometry);
        if (tessControl != 0)
            glAttachShader(ID, tessControl);
        if (tessEval != 0)
            glAttachShader(ID, tessEval);
		glLinkProgram(ID);
		checkCompileErrors(ID, "PROGRAM");
		// delete the shaders as they're linked into our program now and no longer necessary
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		if (geometry != 0)
			glDeleteShader(geometry);
        if (tessControl != 0)
            glDeleteShader(tessControl);
        if (tessEval != 0)
            glDeleteShader(tessEval);
	}

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

private:
	// utility function for checking shader compilation/linking errors.
	// ------------------------------------------------------------------------
	void checkCompileErrors(GLuint shader, std::string type) {
		GLint  success;
		GLchar infoLog[1024];
		if (type != "PROGRAM") {
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(shader, 1024, NULL, infoLog);
				std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
						  << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		} else {
			glGetProgramiv(shader, GL_LINK_STATUS, &success);
			if (!success) {
				glGetProgramInfoLog(shader, 1024, NULL, infoLog);
				std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
						  << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		}
	}

    std::string loadShaderSourceWithIncludes(const std::string& filepath, std::set<std::string>& includedFiles) {
        if (includedFiles.count(filepath)) {
            // Circular dependency detected
            std::cerr << "ERROR::SHADER::CIRCULAR_INCLUDE: " << filepath << std::endl;
            return "";
        }
        includedFiles.insert(filepath);

        std::ifstream shaderFile;
        shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        std::string shaderCode;
        try {
            shaderFile.open(filepath);
            std::stringstream shaderStream;
            shaderStream << shaderFile.rdbuf();
            shaderFile.close();
            shaderCode = shaderStream.str();
        } catch (std::ifstream::failure& e) {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << " at " << filepath << std::endl;
            return "";
        }

        std::stringstream processedCode;
        std::istringstream sourceStream(shaderCode);
        std::string line;

        std::filesystem::path currentPath(filepath);
        std::filesystem::path currentDir = currentPath.parent_path();

        while (std::getline(sourceStream, line)) {
            if (line.rfind("#include \"", 0) == 0) {
                size_t start = line.find('"') + 1;
                size_t end = line.find('"', start);
                if (end != std::string::npos) {
                    std::string includeFilename = line.substr(start, end - start);
                    std::filesystem::path includePath = currentDir / includeFilename;
                    try {
                        std::string absoluteIncludePath = std::filesystem::canonical(includePath).string();
                        processedCode << loadShaderSourceWithIncludes(absoluteIncludePath, includedFiles) << "\n";
                    } catch (const std::filesystem::filesystem_error& e) {
                        std::cerr << "ERROR::SHADER::INCLUDE_FILE_NOT_FOUND: " << e.what() << std::endl;
                        processedCode << line << "\n"; // Keep the include line to see it in potential shader compile errors
                    }
                } else {
                    processedCode << line << "\n";
                }
            } else {
                processedCode << line << "\n";
            }
        }

        return processedCode.str();
    }

    std::string loadShader(const char* path) {
        if (!path) return "";
        std::set<std::string> includedFiles;
        try {
            return loadShaderSourceWithIncludes(std::filesystem::canonical(path).string(), includedFiles);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cout << "ERROR::SHADER::FILE_NOT_FOUND: " << e.what() << std::endl;
            return "";
        }
    }

    unsigned int compileShader(GLenum type, const char* source, const std::string& typeName) {
        if (!source || std::string(source).empty()) return 0;
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, NULL);
        glCompileShader(shader);
        checkCompileErrors(shader, typeName);
        return shader;
    }
};
#endif
