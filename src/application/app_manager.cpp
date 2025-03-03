#include "app_manager.h"
#include <algorithm>
#include <stdexcept>

/**
 * Singleton instance getter with thread-safe initialization
 */
AppManager& AppManager::getInstance() {
    static AppManager instance;
    return instance;
}

/**
 * Register a new application with validation
 * @param appConfig Application configuration in JSON format
 * @return bool Indicates successful registration
 */
bool AppManager::registerApplication(const nlohmann::json& appConfig) {
    // Validate required fields
    if (!appConfig.contains("id") || !appConfig.contains("name")) {
        return false;
    }

    std::string appId = appConfig["id"];
    
    // Check for duplicate applications
    auto existingApp = findApplicationById(appId);
    if (existingApp) {
        return false;
    }

    _registeredApplications.push_back(appConfig);
    return true;
}

/**
 * Unregister an application by its ID
 * @param appId Unique identifier of the application
 * @return bool Indicates successful unregistration
 */
bool AppManager::unregisterApplication(const std::string& appId) {
    auto it = std::remove_if(_registeredApplications.begin(), 
                              _registeredApplications.end(),
                              [&appId](const nlohmann::json& app) {
        return app["id"] == appId;
    });

    if (it != _registeredApplications.end()) {
        _registeredApplications.erase(it, _registeredApplications.end());
        return true;
    }

    return false;
}

/**
 * Retrieve all registered applications
 * @return vector of application configurations
 */
std::vector<nlohmann::json> AppManager::getRegisteredApplications() const {
    return _registeredApplications;
}

/**
 * Find an application by its ID
 * @param appId Unique identifier of the application
 * @return Optional JSON configuration of the application
 */
std::optional<nlohmann::json> AppManager::findApplicationById(const std::string& appId) const {
    auto it = std::find_if(_registeredApplications.begin(), 
                            _registeredApplications.end(),
                            [&appId](const nlohmann::json& app) {
        return app["id"] == appId;
    });

    if (it != _registeredApplications.end()) {
        return *it;
    }

    return std::nullopt;
}