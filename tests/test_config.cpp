#include <gtest/gtest.h>
#include "Config.h"
#include <fstream>
#include <cstdio>
#include <algorithm>

using namespace Boidsish;

TEST(ConfigTest, SetGet) {
    Config cfg("test.ini");
    cfg.SetString("Section1", "Key1", "Value1");
    cfg.SetInt("Section1", "Key2", 42);
    cfg.SetFloat("Section2", "Key3", 3.14f);
    cfg.SetBool("Section2", "Key4", true);

    EXPECT_EQ(cfg.GetString("Section1", "Key1", ""), "Value1");
    EXPECT_EQ(cfg.GetInt("Section1", "Key2", 0), 42);
    EXPECT_NEAR(cfg.GetFloat("Section2", "Key3", 0.0f), 3.14f, 0.001f);
    EXPECT_TRUE(cfg.GetBool("Section2", "Key4", false));
}

TEST(ConfigTest, DefaultValues) {
    Config cfg("test.ini");
    EXPECT_EQ(cfg.GetString("NonExistent", "Key", "Default"), "Default");
    EXPECT_EQ(cfg.GetInt("NonExistent", "Key", 123), 123);
    EXPECT_FLOAT_EQ(cfg.GetFloat("NonExistent", "Key", 0.5f), 0.5f);
    EXPECT_FALSE(cfg.GetBool("NonExistent", "Key", false));
}

TEST(ConfigTest, SaveLoad) {
    const char* filename = "test_save_load.ini";
    {
        Config cfg(filename);
        cfg.SetString("General", "Name", "Boidsish");
        cfg.SetInt("General", "Version", 1);
        cfg.Save();
    }

    {
        Config cfg(filename);
        cfg.Load();
        EXPECT_EQ(cfg.GetString("General", "Name", ""), "Boidsish");
        EXPECT_EQ(cfg.GetInt("General", "Version", 0), 1);
    }
    std::remove(filename);
}

TEST(ConfigTest, Sections) {
    Config cfg("test.ini");
    cfg.SetString("S1", "K1", "V1");
    cfg.SetString("S2", "K2", "V2");

    auto sections = cfg.GetSections();
    EXPECT_EQ(sections.size(), 2);
    EXPECT_TRUE(std::find(sections.begin(), sections.end(), "S1") != sections.end());
    EXPECT_TRUE(std::find(sections.begin(), sections.end(), "S2") != sections.end());

    auto s1 = cfg.GetSection("S1");
    EXPECT_EQ(s1.size(), 1);
    EXPECT_EQ(s1["K1"], "V1");
}
