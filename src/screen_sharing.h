#pragma once

#include "screen_capture/screen_capture.h"
#include "input/input_handler.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>
#include <vector>
#include <map>
#include <memory>

class ScreenSharing {
public:
    // Constructor
    ScreenSharing();
    
    // Destructor
    ~ScreenSharing();
    
    // Initialize screen sharing
    bool initialize();
    
    // Start screen sharing session
    bool startSharing(int width, int height, int quality, int fps);
    
    // Stop screen sharing session
    void stopSharing();
    
    // Check if screen sharing is active
    bool isActive() const { return _isSharing; }
    
    // Get available monitors
    std::vector<std::string> getMonitors();
    
    // Select monitor to capture
    bool selectMonitor(int monitorIndex);
    
    // Get available windows
    std::vector<std::pair<std::string, std::string>> getWindows();
    
    // Select window to capture
    bool selectWindow(const std::string& windowId);
    
    // Process input event from client
    bool processInputEvent(const nlohmann::json& eventJson);
    
    // Set frame callback
    void setFrameCallback(std::function<void(const std::vector<uint8_t>&, int, int)> callback) {
        _frameCallback = std::move(callback);
    }
    
    // Get current resolution
    std::pair<int, int> getResolution() const {
        return {_width, _height};
    }
    
    // Get current quality setting
    int getQuality() const {
        return _quality;
    }
    
    // Get current FPS setting
    int getFps() const {
        return _fps;
    }
    
    // Handle client message
    nlohmann::json handleMessage(const nlohmann::json& message);
    
private:
    // Screen capture
    std::unique_ptr<ScreenCapture> _screenCapture;
    
    // Input handler
    std::unique_ptr<InputHandler> _inputHandler;
    
    // Window handles map (id -> HWND)
    std::map<std::string, HWND> _windowHandles;
    
    // Sharing state
    std::atomic<bool> _isSharing{false};
    int _width{1280};
    int _height{720};
    int _quality{70};
    int _fps{10};
    
    // Thread for frame processing
    std::thread _processingThread;
    
    // Frame callback
    std::function<void(const std::vector<uint8_t>&, int, int)> _frameCallback;
    
    // Mutex for thread safety
    std::mutex _mutex;
    
    // Processing thread function
    void processingLoop();
    
    // Helper method to generate window ID
    std::string generateWindowId(HWND hwnd);
};