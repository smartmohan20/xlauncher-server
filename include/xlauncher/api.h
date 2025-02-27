#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace xlauncher {
    /**
     * Public API namespace for XLauncher server functionalities
     */
    namespace api {
        /**
         * Retrieves all registered applications
         * @return JSON array of application configurations
         */
        std::vector<nlohmann::json> getRegisteredApplications();

        /**
         * Registers a new application
         * @param appConfig Application configuration in JSON format
         * @return Success status of registration
         */
        bool registerApplication(const nlohmann::json& appConfig);

        /**
         * Unregisters an application
         * @param appId Unique identifier of the application
         * @return Success status of unregistration
         */
        bool unregisterApplication(const std::string& appId);

        /**
         * Launches an application
         * @param appId Unique identifier of the application
         * @return Launch status
         */
        bool launchApplication(const std::string& appId);
    }
}