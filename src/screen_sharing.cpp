#include "screen_sharing.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <sstream>

// Constructor
ScreenSharing::ScreenSharing() {
    _screenCapture = std::make_unique<ScreenCapture>();
    _inputHandler = std::make_unique<InputHandler>();
}

// Destructor
ScreenSharing::~ScreenSharing() {
    stopSharing();
}

// Initialize screen sharing
bool ScreenSharing::initialize() {
    return true;
}

// Start screen sharing session
bool ScreenSharing::startSharing(int width, int height, int quality, int fps) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    if (_isSharing) {
        stopSharing();
    }
    
    _width = width;
    _height = height;
    _quality = quality;
    _fps = fps;
    
    // Configure screen capture
    _screenCapture->setQuality(quality);
    _screenCapture->setCaptureInterval(1000 / fps);
    
    // Set frame callback
    _screenCapture->setFrameCallback([this](const ScreenCapture::FrameData& frame) {
        if (_frameCallback) {
            // Call the user-provided callback with the JPEG data
            _frameCallback(frame.jpegData, frame.width, frame.height);
        }
    });
    
    // Start capture
    if (!_screenCapture->start()) {
        std::cerr << "Failed to start screen capture" << std::endl;
        return false;
    }
    
    _isSharing = true;
    
    // Start processing thread
    _processingThread = std::thread(&ScreenSharing::processingLoop, this);
    
    return true;
}

// Stop screen sharing session
void ScreenSharing::stopSharing() {
    if (!_isSharing) {
        return;
    }
    
    _isSharing = false;
    
    // Stop screen capture
    _screenCapture->stop();
    
    // Join processing thread
    if (_processingThread.joinable()) {
        _processingThread.join();
    }
}

// Get available monitors
std::vector<std::string> ScreenSharing::getMonitors() {
    return _screenCapture->getMonitorInfo();
}

// Select monitor to capture
bool ScreenSharing::selectMonitor(int monitorIndex) {
    std::vector<std::string> monitors = getMonitors();
    
    if (monitorIndex < 0 || monitorIndex >= static_cast<int>(monitors.size())) {
        std::cerr << "Invalid monitor index: " << monitorIndex << std::endl;
        return false;
    }
    
    _screenCapture->selectMonitor(monitorIndex);
    return true;
}

// Get available windows
std::vector<std::pair<std::string, std::string>> ScreenSharing::getWindows() {
    auto windowList = _screenCapture->getWindowList();
    std::vector<std::pair<std::string, std::string>> result;
    
    // Clear window handles map
    _windowHandles.clear();
    
    for (const auto& [hwnd, title] : windowList) {
        if (!title.empty()) {
            std::string windowId = generateWindowId(hwnd);
            _windowHandles[windowId] = hwnd;
            result.push_back({windowId, title});
        }
    }
    
    return result;
}

// Select window to capture
bool ScreenSharing::selectWindow(const std::string& windowId) {
    auto it = _windowHandles.find(windowId);
    if (it == _windowHandles.end()) {
        std::cerr << "Window ID not found: " << windowId << std::endl;
        return false;
    }
    
    HWND hwnd = it->second;
    return _screenCapture->selectWindow(hwnd);
}

// Generate window ID
std::string ScreenSharing::generateWindowId(HWND hwnd) {
    std::stringstream ss;
    ss << "window_" << reinterpret_cast<intptr_t>(hwnd);
    return ss.str();
}

// Process input event from client
bool ScreenSharing::processInputEvent(const nlohmann::json& eventJson) {
    return _inputHandler->processInputEvent(eventJson);
}

// Main processing loop
void ScreenSharing::processingLoop() {
    while (_isSharing) {
        // Sleep to maintain the desired frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / _fps));
    }
}

// Handle client message
nlohmann::json ScreenSharing::handleMessage(const nlohmann::json& message) {
    nlohmann::json response;
    response["type"] = "error";
    response["message"] = "Unknown message type";
    
    try {
        if (!message.contains("type")) {
            response["message"] = "Missing message type";
            return response;
        }
        
        std::string type = message["type"];
        
        if (type == "start_sharing") {
            int width = message.value("width", 1280);
            int height = message.value("height", 720);
            int quality = message.value("quality", 70);
            int fps = message.value("fps", 10);
            
            bool success = startSharing(width, height, quality, fps);
            
            response["type"] = "sharing_status";
            response["success"] = success;
            if (success) {
                response["message"] = "Screen sharing started";
                response["width"] = _width;
                response["height"] = _height;
                response["quality"] = _quality;
                response["fps"] = _fps;
            } else {
                response["message"] = "Failed to start screen sharing";
            }
        }
        else if (type == "stop_sharing") {
            stopSharing();
            
            response["type"] = "sharing_status";
            response["success"] = true;
            response["message"] = "Screen sharing stopped";
        }
        else if (type == "get_monitors") {
            std::vector<std::string> monitors = getMonitors();
            
            response["type"] = "monitors_list";
            response["monitors"] = monitors;
        }
        else if (type == "select_monitor") {
            int monitorIndex = message.value("index", 0);
            bool success = selectMonitor(monitorIndex);
            
            response["type"] = "monitor_selected";
            response["success"] = success;
            response["index"] = monitorIndex;
        }
        else if (type == "get_windows") {
            auto windows = getWindows();
            
            response["type"] = "windows_list";
            response["windows"] = nlohmann::json::array();
            
            for (const auto& [id, title] : windows) {
                response["windows"].push_back({{"id", id}, {"title", title}});
            }
        }
        else if (type == "select_window") {
            std::string windowId = message.value("id", "");
            bool success = selectWindow(windowId);
            
            response["type"] = "window_selected";
            response["success"] = success;
            response["id"] = windowId;
        }
        else if (type == "input_event") {
            bool success = processInputEvent(message);
            
            response["type"] = "input_event_result";
            response["success"] = success;
        }
        else if (type == "get_status") {
            response["type"] = "sharing_status";
            response["active"] = static_cast<bool>(_isSharing);
            
            if (_isSharing) {
                response["width"] = _width;
                response["height"] = _height;
                response["quality"] = _quality;
                response["fps"] = _fps;
            }
        }
        else if (type == "update_settings") {
            if (!_isSharing) {
                response["type"] = "error";
                response["message"] = "Screen sharing is not active";
                return response;
            }
            
            // Update quality if provided
            if (message.contains("quality")) {
                _quality = message["quality"];
                _screenCapture->setQuality(_quality);
            }
            
            // Update fps if provided
            if (message.contains("fps")) {
                _fps = message["fps"];
                _screenCapture->setCaptureInterval(1000 / _fps);
            }
            
            response["type"] = "settings_updated";
            response["quality"] = _quality;
            response["fps"] = _fps;
        }
    } catch (const std::exception& e) {
        response["type"] = "error";
        response["message"] = std::string("Error processing message: ") + e.what();
    }
    
    return response;
}