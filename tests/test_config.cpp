#include <gtest/gtest.h>
#include "Config.h"
#include <fstream>
#include <cstdio>
#include <algorithm>

using namespace Boidsish;

TEST(ConfigTest, LoadAndGet) {
    const std::string filename = "test_config.ini";
    {
        std::ofstream file(filename);
        file << "[Section1]\n";
        file << "key1=value1\n";
        file << "key2=123\n";
        file << "key3=1.23\n";
        file << "key4=true\n";
    }

    Config config(filename);
    config.Load();

    EXPECT_EQ(config.GetString("Section1", "key1", "default"), "value1");
    EXPECT_EQ(config.GetInt("Section1", "key2", 0), 123);
    EXPECT_FLOAT_EQ(config.GetFloat("Section1", "key3", 0.0f), 1.23f);
    EXPECT_TRUE(config.GetBool("Section1", "key4", false));

    EXPECT_EQ(config.GetString("Section1", "nonexistent", "default"), "default");

    std::remove(filename.c_str());
}

TEST(ConfigTest, SetAndSave) {
    const std::string filename = "test_config_save.ini";
    Config config(filename);

    config.SetString("Section1", "key1", "value1");
    config.SetInt("Section1", "key2", 456);
    config.SetFloat("Section2", "key3", 4.56f);
    config.SetBool("Section2", "key4", false);

    config.Save();

    Config config2(filename);
    config2.Load();

    EXPECT_EQ(config2.GetString("Section1", "key1", ""), "value1");
    EXPECT_EQ(config2.GetInt("Section1", "key2", 0), 456);
    EXPECT_FLOAT_EQ(config2.GetFloat("Section2", "key3", 0.0f), 4.56f);
    EXPECT_FALSE(config2.GetBool("Section2", "key4", true));

    std::remove(filename.c_str());
}

TEST(ConfigTest, GetSections) {
    Config config("dummy.ini");
    config.SetString("S1", "k", "v");
    config.SetString("S2", "k", "v");

    auto sections = config.GetSections();
    EXPECT_EQ(sections.size(), 2);
    EXPECT_TRUE(std::find(sections.begin(), sections.end(), "S1") != sections.end());
    EXPECT_TRUE(std::find(sections.begin(), sections.end(), "S2") != sections.end());
}
