#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include "shader.h"

namespace fs = std::filesystem;

class TestPreprocessor : public ShaderBase {
public:
	std::string process(const std::string& path) {
		std::set<std::string> includedFiles;
		return loadShaderSource(path, includedFiles);
	}
};

class ShaderPreprocessorTest : public ::testing::Test {
protected:
	void SetUp() override {
		temp_dir_ = fs::current_path() / "temp_test_shaders";
		fs::create_directories(temp_dir_);
	}

	void TearDown() override {
		fs::remove_all(temp_dir_);
	}

	void CreateFile(const std::string& name, const std::string& content) {
		std::ofstream out(temp_dir_ / name);
		out << content;
	}

	fs::path temp_dir_;
};

TEST_F(ShaderPreprocessorTest, DeduplicateIncludes) {
	CreateFile("common.glsl", "void foo() {}");
	CreateFile("main.glsl", "#include \"common.glsl\"\n#include \"common.glsl\"\nvoid main() {}");

	TestPreprocessor p;
	std::string      result = p.process((temp_dir_ / "main.glsl").string());

	// Should contain "foo" once
	size_t first = result.find("void foo()");
	EXPECT_NE(first, std::string::npos);
	size_t second = result.find("void foo()", first + 1);
	EXPECT_EQ(second, std::string::npos);

	// Should only have one //START common.glsl
	size_t start_first = result.find("//START common.glsl");
	EXPECT_NE(start_first, std::string::npos);
	size_t start_second = result.find("//START common.glsl", start_first + 1);
	EXPECT_EQ(start_second, std::string::npos);
}

TEST_F(ShaderPreprocessorTest, CircularIncludes) {
	CreateFile("a.glsl", "#include \"b.glsl\"\nvoid a() {}");
	CreateFile("b.glsl", "#include \"a.glsl\"\nvoid b() {}");

	TestPreprocessor p;
	// This should not stack overflow
	std::string result = p.process((temp_dir_ / "a.glsl").string());

	EXPECT_NE(result.find("void a()"), std::string::npos);
	EXPECT_NE(result.find("void b()"), std::string::npos);
}

TEST_F(ShaderPreprocessorTest, VersionHandling) {
	CreateFile("common.glsl", "#version 430 core\nvoid foo() {}");
	CreateFile("main.glsl", "#include \"common.glsl\"\nvoid main() {}");

	TestPreprocessor p;
	std::string      result = p.process((temp_dir_ / "main.glsl").string());

	// #version should be at the very top
	EXPECT_EQ(result.substr(0, 17), "#version 430 core");

	// Should not contain another #version
	size_t second_version = result.find("#version", 1);
	EXPECT_EQ(second_version, std::string::npos);
}

TEST_F(ShaderPreprocessorTest, AutoGuards) {
	CreateFile("a.glsl", "int a = 1;");
	CreateFile("main.glsl", "#include \"a.glsl\"\nvoid main() {}");

	TestPreprocessor p;
	std::string      result = p.process((temp_dir_ / "main.glsl").string());

	EXPECT_NE(result.find("#ifndef AUTO_GUARD_"), std::string::npos);
	EXPECT_NE(result.find("#define AUTO_GUARD_"), std::string::npos);
	EXPECT_NE(result.find("#endif // AUTO_GUARD_"), std::string::npos);
}
