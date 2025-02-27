#pragma once

#define WINVER 0x0601  // Windows 7 or later
#define _WIN32_WINNT 0x0601

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include <Windows.h>

class ApplicationLauncher {
public:
    enum class ApplicationType {
        EXECUTABLE,
        WEBSITE,
        SYSTEM_COMMAND
    };

    struct Application {
        std::string id;
        std::string name;
        std::string path;
        ApplicationType type;
        std::vector<std::string> arguments;
    };

    // Launch an application by its ID
    static bool launchApplication(const std::string& appId);

    // Register a new application
    static bool registerApplication(const Application& app);

    // Get list of registered applications
    static std::vector<Application> getRegisteredApplications();

    // Find application by ID
    static std::optional<Application> findApplicationById(const std::string& appId);

private:
    // Internal method to launch different types of applications
    static bool launchExecutable(const Application& app);
    static bool launchWebsite(const Application& app);
    static bool launchSystemCommand(const Application& app);

    // Store registered applications
    static std::vector<Application> _registeredApplications;
};