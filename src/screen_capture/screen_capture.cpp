#include "screen_capture.h"
#include <iostream>
#ifdef HAVE_TURBOJPEG
#include <turbojpeg.h>
#endif
#include <algorithm>
#include <chrono>
#include "../utils/base64.h"

// Add GDI+ support for fallback JPEG compression
#ifdef NO_TURBOJPEG
#include <Windows.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

// GDI+ initialization
static class GdiPlusInitializer {
public:
    GdiPlusInitializer() {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    }
    ~GdiPlusInitializer() {
        Gdiplus::GdiplusShutdown(gdiplusToken);
    }
private:
    ULONG_PTR gdiplusToken;
} gdiPlusInit;

// Helper function to get encoder CLSID
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;          // number of image encoders
    UINT size = 0;         // size of the image encoder array in bytes
    
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;
    
    Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL)
        return -1;
    
    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    
    free(pImageCodecInfo);
    return -1;
}
#endif

// Constructor
ScreenCapture::ScreenCapture(int captureIntervalMs, int quality)
    : _captureIntervalMs(captureIntervalMs), _quality(quality) {
}

// Destructor
ScreenCapture::~ScreenCapture() {
    stop();
}

// Start capturing
bool ScreenCapture::start() {
    if (_running) return true;
    
    _running = true;
    _captureThread = std::thread(&ScreenCapture::captureLoop, this);
    
    return true;
}

// Stop capturing
void ScreenCapture::stop() {
    if (!_running) return;
    
    _running = false;
    
    // Wake up capture thread if it's waiting
    {
        std::lock_guard<std::mutex> lock(_frameMutex);
        _frameReady = true;
        _frameCondition.notify_one();
    }
    
    if (_captureThread.joinable()) {
        _captureThread.join();
    }
}

// Convert frame data to JSON
nlohmann::json ScreenCapture::frameToJson(const FrameData& frame) {
    nlohmann::json jsonFrame;
    
    // Base64 encode the JPEG data
    std::string base64Data = base64_encode(frame.jpegData.data(), frame.jpegData.size());
    
    jsonFrame["data"] = base64Data;
    jsonFrame["width"] = frame.width;
    jsonFrame["height"] = frame.height;
    jsonFrame["quality"] = frame.quality;
    
    // Timestamp as milliseconds since epoch
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        frame.timestamp.time_since_epoch()).count();
    jsonFrame["timestamp"] = timestamp;
    
    return jsonFrame;
}

// Select monitor to capture
void ScreenCapture::selectMonitor(int monitorIndex) {
    _monitorIndex = monitorIndex;
    _captureWindow = false;
    _targetWindow = NULL;
}

// Get information about available monitors
std::vector<std::string> ScreenCapture::getMonitorInfo() {
    std::vector<std::string> monitors;
    
    // Callback for EnumDisplayMonitors
    auto monitorCallback = [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) -> BOOL {
        auto monitorsPtr = reinterpret_cast<std::vector<std::string>*>(dwData);
        
        MONITORINFOEX monitorInfo;
        monitorInfo.cbSize = sizeof(MONITORINFOEX);
        
        if (GetMonitorInfo(hMonitor, &monitorInfo)) {
            std::stringstream info;
            info << "Monitor " << monitorsPtr->size() << ": "
                 << monitorInfo.szDevice << " - "
                 << (monitorInfo.dwFlags & MONITORINFOF_PRIMARY ? "Primary" : "Secondary") << " - "
                 << "Resolution: " << (monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left) 
                 << "x" << (monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top);
            
            monitorsPtr->push_back(info.str());
        }
        
        return TRUE; // Continue enumeration
    };
    
    // Enumerate monitors
    EnumDisplayMonitors(NULL, NULL, monitorCallback, reinterpret_cast<LPARAM>(&monitors));
    
    return monitors;
}

// Select window to capture
bool ScreenCapture::selectWindow(HWND windowHandle) {
    if (!IsWindow(windowHandle)) {
        return false;
    }
    
    _targetWindow = windowHandle;
    _captureWindow = true;
    
    return true;
}

// Get list of windows
std::vector<std::pair<HWND, std::string>> ScreenCapture::getWindowList() {
    std::vector<std::pair<HWND, std::string>> windows;
    
    // Callback for EnumWindows
    auto windowCallback = [](HWND hwnd, LPARAM lParam) -> BOOL {
        auto windowsPtr = reinterpret_cast<std::vector<std::pair<HWND, std::string>>*>(lParam);
        
        // Skip invisible windows
        if (!IsWindowVisible(hwnd)) {
            return TRUE;
        }
        
        // Get window title
        char title[256];
        int length = GetWindowTextA(hwnd, title, sizeof(title));
        
        if (length > 0) {
            windowsPtr->push_back({hwnd, std::string(title)});
        }
        
        return TRUE; // Continue enumeration
    };
    
    // Enumerate windows
    EnumWindows(windowCallback, reinterpret_cast<LPARAM>(&windows));
    
    return windows;
}

// Main capture loop
void ScreenCapture::captureLoop() {
    while (_running) {
        auto startTime = std::chrono::steady_clock::now();
        
        // Capture frame
        FrameData frame = captureScreen();
        
        // Notify callback
        if (_frameCallback && !frame.jpegData.empty()) {
            _frameCallback(frame);
        }
        
        // Store latest frame
        {
            std::lock_guard<std::mutex> lock(_frameMutex);
            _latestFrame = std::move(frame);
            _frameReady = true;
            _frameCondition.notify_one();
        }
        
        // Calculate time to wait
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        
        int waitTime = _captureIntervalMs - static_cast<int>(elapsedTime);
        
        if (waitTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
        }
    }
}

// Capture a single frame now
ScreenCapture::FrameData ScreenCapture::captureFrame() {
    if (_running) {
        // Wait for next frame from capture thread
        std::unique_lock<std::mutex> lock(_frameMutex);
        _frameReady = false;
        _frameCondition.wait(lock, [this] { return _frameReady || !_running; });
        
        return _latestFrame;
    } else {
        // Capture a frame directly
        return captureScreen();
    }
}

// Internal screen capture implementation
ScreenCapture::FrameData ScreenCapture::captureScreen() {
    FrameData frame;
    frame.quality = _quality;
    frame.timestamp = std::chrono::system_clock::now();
    
    HDC hdcScreen;
    HDC hdcMemDC = NULL;
    HBITMAP hbmScreen = NULL;
    RECT rcClient;
    
    try {
        // Get device context based on capture mode
        if (_captureWindow && _targetWindow != NULL) {
            // Capture specific window
            hdcScreen = GetDC(_targetWindow);
            GetClientRect(_targetWindow, &rcClient);
        } else {
            // Capture monitor or primary display
            hdcScreen = CreateDC(TEXT("DISPLAY"), NULL, NULL, NULL);
            
            // Get monitor bounds
            int monitorCount = GetSystemMetrics(SM_CMONITORS);
            if (_monitorIndex >= monitorCount) {
                _monitorIndex = 0; // Default to primary monitor
            }
            
            if (_monitorIndex == 0) {
                // Primary monitor
                rcClient.left = 0;
                rcClient.top = 0;
                rcClient.right = GetSystemMetrics(SM_CXSCREEN);
                rcClient.bottom = GetSystemMetrics(SM_CYSCREEN);
            } else {
                // Specific monitor by index
                // We need to enumerate monitors to find the correct one
                struct MonitorData {
                    int targetIndex;
                    int currentIndex;
                    RECT bounds;
                    bool found;
                };
                
                MonitorData data = {_monitorIndex, 0, {0}, false};
                
                auto monitorCallback = [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) -> BOOL {
                    auto dataPtr = reinterpret_cast<MonitorData*>(dwData);
                    
                    if (dataPtr->currentIndex == dataPtr->targetIndex) {
                        dataPtr->bounds = *lprcMonitor;
                        dataPtr->found = true;
                        return FALSE; // Stop enumeration
                    }
                    
                    dataPtr->currentIndex++;
                    return TRUE; // Continue enumeration
                };
                
                EnumDisplayMonitors(NULL, NULL, monitorCallback, reinterpret_cast<LPARAM>(&data));
                
                if (data.found) {
                    rcClient = data.bounds;
                } else {
                    // Fallback to primary monitor
                    rcClient.left = 0;
                    rcClient.top = 0;
                    rcClient.right = GetSystemMetrics(SM_CXSCREEN);
                    rcClient.bottom = GetSystemMetrics(SM_CYSCREEN);
                }
            }
        }
        
        // Create a compatible DC
        hdcMemDC = CreateCompatibleDC(hdcScreen);
        if (!hdcMemDC) {
            throw std::runtime_error("Failed to create compatible DC");
        }
        
        // Create a compatible bitmap
        frame.width = rcClient.right - rcClient.left;
        frame.height = rcClient.bottom - rcClient.top;
        
        hbmScreen = CreateCompatibleBitmap(hdcScreen, frame.width, frame.height);
        if (!hbmScreen) {
            throw std::runtime_error("Failed to create compatible bitmap");
        }
        
        // Select the bitmap into the compatible DC
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMemDC, hbmScreen);
        
        // Copy from the screen DC to the memory DC
        if (!BitBlt(hdcMemDC, 0, 0, frame.width, frame.height,
                    hdcScreen, rcClient.left, rcClient.top, SRCCOPY | CAPTUREBLT)) {
            throw std::runtime_error("BitBlt failed");
        }
        
        // Get the bitmap info
        BITMAPINFOHEADER bi;
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = frame.width;
        bi.biHeight = -frame.height;  // Negative height for top-down DIB
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        bi.biSizeImage = 0;
        bi.biXPelsPerMeter = 0;
        bi.biYPelsPerMeter = 0;
        bi.biClrUsed = 0;
        bi.biClrImportant = 0;
        
        // Get the DIB bits
        BYTE* lpBits = new BYTE[frame.width * frame.height * 4];
        GetDIBits(hdcMemDC, hbmScreen, 0, frame.height, lpBits,
                 (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        
        // Compress to JPEG
        frame.jpegData = compressToJpeg(lpBits, frame.width, frame.height, 
                                        frame.width * 4, frame.quality);
        
        // Cleanup
        delete[] lpBits;
        SelectObject(hdcMemDC, hbmOld);
        DeleteObject(hbmScreen);
        DeleteDC(hdcMemDC);
        
        if (_captureWindow) {
            ReleaseDC(_targetWindow, hdcScreen);
        } else {
            DeleteDC(hdcScreen);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in screen capture: " << e.what() << std::endl;
        
        // Cleanup on error
        if (hbmScreen) DeleteObject(hbmScreen);
        if (hdcMemDC) DeleteDC(hdcMemDC);
        
        if (_captureWindow && hdcScreen) {
            ReleaseDC(_targetWindow, hdcScreen);
        } else if (hdcScreen) {
            DeleteDC(hdcScreen);
        }
    }
    
    return frame;
}

// Compress raw pixels to JPEG
std::vector<uint8_t> ScreenCapture::compressToJpeg(BYTE* data, int width, int height, 
                                                 int stride, int quality) {
    std::vector<uint8_t> jpegData;
    
#ifdef HAVE_TURBOJPEG
    tjhandle jpegCompressor = tjInitCompress();
    
    if (!jpegCompressor) {
        std::cerr << "TurboJPEG initialization failed" << std::endl;
        return jpegData;
    }
    
    unsigned char* jpegBuf = NULL;
    unsigned long jpegSize = 0;
    
    // Convert BGRA to RGB for libjpeg
    std::vector<uint8_t> rgbData(width * height * 3);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            BYTE* pixel = data + y * stride + x * 4;
            int rgbIndex = (y * width + x) * 3;
            
            // BGRA to RGB conversion
            rgbData[rgbIndex] = pixel[2];     // R
            rgbData[rgbIndex + 1] = pixel[1]; // G
            rgbData[rgbIndex + 2] = pixel[0]; // B
        }
    }
    
    // Compress to JPEG
    int result = tjCompress2(jpegCompressor, rgbData.data(), width, 0, height,
                            TJPF_RGB, &jpegBuf, &jpegSize, TJSAMP_420, quality,
                            TJFLAG_FASTDCT);
    
    if (result == 0 && jpegBuf && jpegSize > 0) {
        jpegData.resize(jpegSize);
        std::memcpy(jpegData.data(), jpegBuf, jpegSize);
    } else {
        std::cerr << "JPEG compression failed: " << tjGetErrorStr2(jpegCompressor) << std::endl;
    }
    
    if (jpegBuf) {
        tjFree(jpegBuf);
    }
    
    tjDestroy(jpegCompressor);
#else
    // GDI+ fallback implementation
    // Create a GDI+ bitmap from raw data
    Gdiplus::Bitmap bitmap(width, height, stride, PixelFormat32bppARGB, data);
    
    // Setup encoder parameters for JPEG quality
    Gdiplus::EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
    encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    ULONG qualityValue = quality;
    encoderParams.Parameter[0].Value = &qualityValue;
    
    // Get JPEG encoder CLSID
    CLSID jpegClsid;
    GetEncoderClsid(L"image/jpeg", &jpegClsid);
    
    // Create a memory stream to save the JPEG
    IStream* stream = NULL;
    if (CreateStreamOnHGlobal(NULL, TRUE, &stream) == S_OK) {
        // Save bitmap to stream as JPEG
        if (bitmap.Save(stream, &jpegClsid, &encoderParams) == Gdiplus::Ok) {
            // Get stream size
            STATSTG stats;
            if (stream->Stat(&stats, STATFLAG_NONAME) == S_OK) {
                // Copy stream data to vector
                LARGE_INTEGER seekPos = {0};
                stream->Seek(seekPos, STREAM_SEEK_SET, NULL);
                
                jpegData.resize(stats.cbSize.LowPart);
                ULONG bytesRead;
                stream->Read(jpegData.data(), stats.cbSize.LowPart, &bytesRead);
            }
        }
        
        // Release stream
        stream->Release();
    }
#endif
    
    return jpegData;
}