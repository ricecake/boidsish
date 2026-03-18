#include <gtest/gtest.h>
#include "logger.h"
#include "Config.h"
#include <fstream>
#include <sstream>

class TestBackend : public logger::BaseBackend {
public:
    std::vector<std::string> messages;
    bool render(const logger::LogLevel level, const std::string_view& str) override {
        if (!isEnabled(level)) return false;
        messages.push_back(std::string(str));
        return true;
    }
};

TEST(LoggerTest, TupleFormatting) {
    auto test_backend = std::make_unique<TestBackend>();
    auto* backend_ptr = test_backend.get();
    logger::defaultLogger.backend.clearBackends();
    logger::defaultLogger.backend.addBackend(std::move(test_backend));

    logger::INFO("Test tuple: {}", std::make_tuple("key", "value"));
    ASSERT_FALSE(backend_ptr->messages.empty());
    EXPECT_TRUE(backend_ptr->messages.back().find("key => [value]") != std::string::npos);

    logger::INFO("Test pair: {}", std::make_pair("foo", 42));
    EXPECT_TRUE(backend_ptr->messages.back().find("foo => [42]") != std::string::npos);
}

TEST(LoggerTest, LevelFiltering) {
    auto test_backend = std::make_unique<TestBackend>();
    auto* backend_ptr = test_backend.get();
    logger::defaultLogger.backend.clearBackends();
    logger::defaultLogger.backend.addBackend(std::move(test_backend));

    backend_ptr->setLogLevel(logger::LogLevel::DEBUG, false);

    logger::INFO("This should be logged");
    size_t count = backend_ptr->messages.size();
    EXPECT_EQ(count, 1);

    logger::DEBUG("This should NOT be logged");
    EXPECT_EQ(backend_ptr->messages.size(), count);
}

TEST(LoggerTest, FileLogging) {
    const std::string test_log = "test_file_logging.log";
    std::remove(test_log.c_str());

    {
        Boidsish::Config cfg("test_logger_cfg_file.ini");
        cfg.SetString("logging", "output_file", test_log);
        cfg.SetBool("logging", "console_enabled", false);
        cfg.Save();
        logger::Configure(cfg);

        // Verify config was populated with defaults
        EXPECT_TRUE(cfg.HasKey("logging", "info_console"));
        EXPECT_TRUE(cfg.HasKey("logging", "debug_console"));
        EXPECT_EQ(cfg.GetBool("logging", "debug_console", true), false);

        logger::INFO("Log to file");
    }

    std::ifstream file(test_log);
    ASSERT_TRUE(file.is_open());
    std::string line;
    std::getline(file, line);
    EXPECT_TRUE(line.find("Log to file") != std::string::npos);

    file.close();
    std::remove(test_log.c_str());
}

struct SideEffectGuard {
    bool* called;
    friend std::ostream& operator<<(std::ostream& os, const SideEffectGuard& g) {
        *g.called = true;
        return os << "side_effect";
    }
};

TEST(LoggerTest, EarlyExit) {
    auto test_backend = std::make_unique<TestBackend>();
    auto* backend_ptr = test_backend.get();

    // We need to clear and re-add because defaultLogger is static and shared
    logger::defaultLogger.backend.clearBackends();
    logger::defaultLogger.backend.addBackend(std::move(test_backend));

    backend_ptr->setLogLevel(logger::LogLevel::INFO, false);

    bool formatted = false;
    logger::INFO("Check formatting: {}", SideEffectGuard{&formatted});
    EXPECT_FALSE(formatted);
}
