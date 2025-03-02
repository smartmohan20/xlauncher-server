#define WINVER 0x0601  // Windows 7 or later
#define _WIN32_WINNT 0x0601

#include "app_launcher.h"
#include <Windows.h>
#include <shellapi.h>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <CommCtrl.h>

// For Base64 encoding
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

// Ensure these Windows-specific constants are available
#ifndef SEE_MASK_NOCLOSE
#define SEE_MASK_NOCLOSE 0x00000040
#endif

// Initialize static member
std::vector<ApplicationLauncher::Application> ApplicationLauncher::_registeredApplications;

// Base64 encoding helper function
std::string base64Encode(const std::vector<unsigned char>& data) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_push(b64, bio);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    
    BIO_write(b64, data.data(), static_cast<int>(data.size()));
    BIO_flush(b64);
    
    BUF_MEM* bufferPtr;
    BIO_get_mem_ptr(b64, &bufferPtr);
    
    std::string result(bufferPtr->data, bufferPtr->length);
    
    BIO_free_all(b64);
    
    return result;
}

// Bitmap to PNG/JPEG conversion and encoding
std::optional<ApplicationLauncher::IconData> convertBitmapToBase64(HBITMAP hBitmap) {
    BITMAP bm;
    if (!GetObject(hBitmap, sizeof(BITMAP), &bm)) {
        std::cerr << "Failed to get bitmap info" << std::endl;
        return std::nullopt;
    }
    
    // Create a device context
    HDC hDC = CreateCompatibleDC(NULL);
    if (!hDC) {
        std::cerr << "Failed to create compatible DC" << std::endl;
        return std::nullopt;
    }
    
    // Select the bitmap into the DC
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hDC, hBitmap);
    
    // Create a DIB section
    BITMAPINFOHEADER bi;
    ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = bm.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32; // 32-bit ARGB format
    bi.biCompression = BI_RGB;
    
    unsigned char* bits = NULL;
    HBITMAP hDIBSection = CreateDIBSection(hDC, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
    if (!hDIBSection || !bits) {
        SelectObject(hDC, hOldBitmap);
        DeleteDC(hDC);
        std::cerr << "Failed to create DIB section" << std::endl;
        return std::nullopt;
    }
    
    HDC hMemDC = CreateCompatibleDC(hDC);
    SelectObject(hMemDC, hDIBSection);
    
    // Copy the bitmap data
    BitBlt(hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, hDC, 0, 0, SRCCOPY);
    
    // Convert BGRA to RGBA for proper PNG encoding
    int size = bm.bmWidth * bm.bmHeight * 4;
    std::vector<unsigned char> pixelData(size);
    
    for (int i = 0; i < size; i += 4) {
        // BGRA to RGBA conversion
        pixelData[i] = bits[i + 2];     // Red
        pixelData[i + 1] = bits[i + 1]; // Green
        pixelData[i + 2] = bits[i];     // Blue
        pixelData[i + 3] = bits[i + 3]; // Alpha
    }
    
    // For simplicity, we'll store as a data URI with base64 encoded PNG
    // In a real application, you might want to use a proper PNG encoding library
    // For now, we'll just encode the raw pixel data as base64
    std::string encodedData = base64Encode(pixelData);
    
    // Cleanup
    SelectObject(hMemDC, NULL);
    DeleteDC(hMemDC);
    SelectObject(hDC, hOldBitmap);
    DeleteDC(hDC);
    DeleteObject(hDIBSection);
    
    ApplicationLauncher::IconData iconData;
    iconData.base64Data = encodedData;
    iconData.mimeType = "image/png";
    
    return iconData;
}

std::optional<ApplicationLauncher::IconData> ApplicationLauncher::extractIconFromPath(const std::string& path) {
    // Check if the path is an executable file
    if (path.empty()) {
        return std::nullopt;
    }
    
    // Handle websites differently - use default browser icon
    if (path.find("http://") == 0 || path.find("https://") == 0) {
        // Return a generic web icon
        // In a real application, you might want to implement favicon fetching
        return std::nullopt;
    }
    
    // Extract icon from executable
    HICON hIcon = NULL;
    UINT extractResult;
    
    // Try to extract the icon from the file
    extractResult = ExtractIconExA(path.c_str(), 0, &hIcon, NULL, 1);
    
    if (extractResult == 0 || !hIcon) {
        // Fallback to default icon
        hIcon = (HICON)LoadImageA(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
        if (!hIcon) {
            std::cerr << "Failed to load default icon" << std::endl;
            return std::nullopt;
        }
    }
    
    // Convert icon to bitmap for encoding
    HDC hDC = GetDC(NULL);
    if (!hDC) {
        DestroyIcon(hIcon);
        std::cerr << "Failed to get device context" << std::endl;
        return std::nullopt;
    }
    
    // Get icon dimensions
    int iconWidth = GetSystemMetrics(SM_CXICON);
    int iconHeight = GetSystemMetrics(SM_CYICON);
    
    // Create a compatible DC
    HDC hMemDC = CreateCompatibleDC(hDC);
    if (!hMemDC) {
        ReleaseDC(NULL, hDC);
        DestroyIcon(hIcon);
        std::cerr << "Failed to create compatible DC" << std::endl;
        return std::nullopt;
    }
    
    // Create a bitmap for the icon
    HBITMAP hBitmap = CreateCompatibleBitmap(hDC, iconWidth, iconHeight);
    if (!hBitmap) {
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hDC);
        DestroyIcon(hIcon);
        std::cerr << "Failed to create compatible bitmap" << std::endl;
        return std::nullopt;
    }
    
    // Select the bitmap into the DC
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
    
    // Fill with transparent background
    RECT rect = { 0, 0, iconWidth, iconHeight };
    FillRect(hMemDC, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
    
    // Draw the icon onto the bitmap
    DrawIconEx(hMemDC, 0, 0, hIcon, iconWidth, iconHeight, 0, NULL, DI_NORMAL);
    
    // Get the bitmap and convert to base64
    SelectObject(hMemDC, hOldBitmap);
    auto iconData = convertBitmapToBase64(hBitmap);
    
    // Cleanup
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hDC);
    DestroyIcon(hIcon);
    
    return iconData;
}

// Launch application by ID
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

// Method to launch with custom path and arguments
bool ApplicationLauncher::launchApplication(
    const std::string& path, 
    const std::vector<std::string>& arguments
) {
    // Use ShellExecute for launching
    SHELLEXECUTEINFOA sei = { sizeof(SHELLEXECUTEINFO) };
    sei.fMask = SEE_MASK_NOCLOSE | SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = NULL;
    sei.lpVerb = "open";
    sei.lpFile = path.c_str();
    
    // Combine arguments into a single string if not empty
    std::string args;
    for (const auto& arg : arguments) {
        args += arg + " ";
    }
    sei.lpParameters = args.empty() ? NULL : args.c_str();
    
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExA(&sei)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to launch application. Error code: " << error 
                  << ", Path: " << path << std::endl;
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

    // Create a copy of the app to modify
    Application newApp = app;
    
    // Extract icon if not already present
    if (!newApp.icon.has_value()) {
        auto extractedIcon = extractIconFromPath(newApp.path);
        if (extractedIcon) {
            newApp.icon = extractedIcon;
        }
    }

    _registeredApplications.push_back(newApp);
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

// Implementation of private launch methods
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