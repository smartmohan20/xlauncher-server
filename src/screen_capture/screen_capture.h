#pragma once

#include <Windows.h>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <string>
#include <nlohmann/json.hpp>

class ScreenCapture {
public:
    struct FrameData {
        std::vector<uint8_t> jpegData;
        int width;
        int height;
        int quality;
        std::chrono::system_clock::time_point timestamp;
    };

    // Constructor
    explicit ScreenCapture(int captureIntervalMs = 100, int quality = 70);
    
    // Destructor
    ~ScreenCapture();
    
    // Start capture
    bool start();
    
    // Stop capture
    void stop();
    
    // Check if capture is running
    bool isRunning() const { return _running; }
    
    // Set frame callback
    void setFrameCallback(std::function<void(const FrameData&)> callback) { 
        _frameCallback = std::move(callback); 
    }
    
    // Set capture interval
    void setCaptureInterval(int intervalMs) { _captureIntervalMs = intervalMs; }
    
    // Set JPEG quality
    void setQuality(int quality) { _quality = quality; }
    
    // Get single frame immediately
    FrameData captureFrame();
    
    // Convert FrameData to JSON
    static nlohmann::json frameToJson(const FrameData& frame);
    
    // Capture specific monitor
    void selectMonitor(int monitorIndex);
    
    // Get available monitors
    std::vector<std::string> getMonitorInfo();
    
    // Capture specific window
    bool selectWindow(HWND windowHandle);
    
    // Get window list
    std::vector<std::pair<HWND, std::string>> getWindowList();
    
private:
    // Capture methods
    void captureLoop();
    FrameData captureScreen();
    
    // Compress frame to JPEG
    std::vector<uint8_t> compressToJpeg(BYTE* data, int width, int height, int stride, int quality);
    
    // Capture state
    std::atomic<bool> _running{false};
    std::thread _captureThread;
    std::function<void(const FrameData&)> _frameCallback;
    int _captureIntervalMs;
    int _quality;
    
    // Capture region
    int _monitorIndex{0};
    HWND _targetWindow{NULL};
    bool _captureWindow{false};
    
    // Synchronization
    std::mutex _frameMutex;
    std::condition_variable _frameCondition;
    bool _frameReady{false};
    FrameData _latestFrame;
};