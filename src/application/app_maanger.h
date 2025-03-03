#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

/**
 * Manages application-related operations for XLauncher
 * Handles application registration, discovery, and lifecycle management
 */
class AppManager {
public:
    // Singleton pattern to ensure global access
    static AppManager& getInstance();

    // Register a new application
    bool registerApplication(const nlohmann::json& appConfig);

    // Unregister an existing application
    bool unregisterApplication(const std::string& appId);

    // Get list of registered applications
    std::vector<nlohmann::json> getRegisteredApplications() const;

    // Find application by ID
    std::optional<nlohmann::json> findApplicationById(const std::string& appId) const;

private:
    // Private constructor to enforce singleton
    AppManager() = default;
    
    // Prevent copying
    AppManager(const AppManager&) = delete;
    AppManager& operator=(const AppManager&) = delete;

    // Internal storage for applications
    std::vector<nlohmann::json> _registeredApplications;
};