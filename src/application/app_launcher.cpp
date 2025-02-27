#define WINVER 0x0601  // Windows 7 or later
#define _WIN32_WINNT 0x0601

#include "app_launcher.h"
#include <Windows.h>
#include <shellapi.h>
#include <iostream>
#include <algorithm>

// Ensure these Windows-specific constants are available
#ifndef SEE_MASK_NOCLOSE
#define SEE_MASK_NOCLOSE 0x00000040
#endif

// Initialize static member
std::vector<ApplicationLauncher::Application> ApplicationLauncher::_registeredApplications;

bool ApplicationLauncher::launchApplication(const std::string& appId) {
    auto app = findApplicationById(appId);
    if (!app) {
        std::cerr << "Application not found: " << appId << std::endl;
        return false;
    }

    switch (app->type) {
        case ApplicationType::EXECUTABLE:
            return launchExecutable(*app);
        case ApplicationType::WEBSITE:
            return launchWebsite(*app);
        case ApplicationType::SYSTEM_COMMAND:
            return launchSystemCommand(*app);
        default:
            std::cerr << "Unsupported application type" << std::endl;
            return false;
    }
}

// Implemented below: launchExecutable, launchWebsite, launchSystemCommand methods...

bool ApplicationLauncher::launchExecutable(const Application& app) {
    // Construct full command with arguments
    std::string fullCommand = app.path;
    for (const auto& arg : app.arguments) {
        fullCommand += " " + arg;
    }

    // Use ShellExecute for launching
    SHELLEXECUTEINFOA sei = { sizeof(SHELLEXECUTEINFO) };
    sei.fMask = SEE_MASK_NOCLOSE | SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = "open";
    sei.lpFile = app.path.c_str();
    
    // Combine arguments into a single string if not empty
    std::string args;
    for (const auto& arg : app.arguments) {
        args += arg + " ";
    }
    sei.lpParameters = args.empty() ? NULL : args.c_str();
    
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExA(&sei)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to launch application. Error code: " << error << std::endl;
        return false;
    }

    return true;
}

bool ApplicationLauncher::launchWebsite(const Application& app) {
    // Use ShellExecute to open website in default browser
    HINSTANCE result = ShellExecuteA(NULL, "open", app.path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    
    // Check if launch was successful
    if ((intptr_t)result <= 32) {
        std::cerr << "Failed to open website. Error code: " << (intptr_t)result << std::endl;
        return false;
    }

    return true;
}

bool ApplicationLauncher::launchSystemCommand(const Application& app) {
    // Use system to execute command
    int result = system(app.path.c_str());
    
    if (result != 0) {
        std::cerr << "Failed to execute system command. Return code: " << result << std::endl;
        return false;
    }

    return true;
}

bool ApplicationLauncher::registerApplication(const Application& app) {
    // Check for duplicate
    if (findApplicationById(app.id)) {
        std::cerr << "Application already registered: " << app.id << std::endl;
        return false;
    }

    _registeredApplications.push_back(app);
    return true;
}

std::vector<ApplicationLauncher::Application> ApplicationLauncher::getRegisteredApplications() {
    return _registeredApplications;
}

std::optional<ApplicationLauncher::Application> ApplicationLauncher::findApplicationById(const std::string& appId) {
    auto it = std::find_if(_registeredApplications.begin(), _registeredApplications.end(),
        [&appId](const Application& app) { return app.id == appId; });
    
    if (it != _registeredApplications.end()) {
        return *it;
    }
    return std::nullopt;
}