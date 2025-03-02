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

// Initialize static members
std::vector<ApplicationLauncher::Application> ApplicationLauncher::_registeredApplications;
std::map<std::string, HANDLE> ApplicationLauncher::_applicationProcesses;

// Base64 encoding helper function - make it static to limit its scope to this file
static std::string base64Encode(const unsigned char* data, size_t length) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_push(b64, bio);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    
    BIO_write(b64, data, static_cast<int>(length));
    BIO_flush(b64);
    
    BUF_MEM* bufferPtr;
    BIO_get_mem_ptr(b64, &bufferPtr);
    
    std::string result(bufferPtr->data, bufferPtr->length);
    
    BIO_free_all(b64);
    
    return result;
}

// Create a BMP file in memory and encode it to base64
static std::optional<ApplicationLauncher::IconData> convertBitmapToBase64(HBITMAP hBitmap) {
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
    
    // Create a DIB section that contains the image with header and pixel data
    BITMAPINFOHEADER bi;
    ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bm.bmWidth;
    bi.biHeight = bm.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 24; // 24-bit RGB format for better compatibility
    bi.biCompression = BI_RGB;
    
    // Calculate row size and pad to 4-byte boundary
    int rowSize = ((bi.biWidth * bi.biBitCount + 31) / 32) * 4;
    int dataSize = rowSize * abs(bi.biHeight);
    
    // Create BMP file headers
    BITMAPFILEHEADER bmfh;
    bmfh.bfType = 0x4D42; // "BM"
    bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dataSize;
    bmfh.bfReserved1 = 0;
    bmfh.bfReserved2 = 0;
    bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    
    // Allocate memory for BMP file
    std::vector<unsigned char> bmpData(bmfh.bfSize);
    
    // Copy file header
    memcpy(bmpData.data(), &bmfh, sizeof(BITMAPFILEHEADER));
    
    // Copy info header
    memcpy(bmpData.data() + sizeof(BITMAPFILEHEADER), &bi, sizeof(BITMAPINFOHEADER));
    
    // Get the DIB bits
    unsigned char* pixelData = bmpData.data() + bmfh.bfOffBits;
    if (!GetDIBits(hDC, hBitmap, 0, abs(bi.biHeight), pixelData, (BITMAPINFO*)&bi, DIB_RGB_COLORS)) {
        SelectObject(hDC, hOldBitmap);
        DeleteDC(hDC);
        std::cerr << "Failed to get DIB bits" << std::endl;
        return std::nullopt;
    }
    
    // Encode the entire BMP file to base64
    std::string encodedData = base64Encode(bmpData.data(), bmpData.size());
    
    // Cleanup
    SelectObject(hDC, hOldBitmap);
    DeleteDC(hDC);
    
    ApplicationLauncher::IconData iconData;
    iconData.base64Data = encodedData;
    iconData.mimeType = "image/bmp";
    
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
    
    // Try to extract the icon from the file - use smaller icon for better display in list
    extractResult = ExtractIconExA(path.c_str(), 0, NULL, &hIcon, 1);
    
    // If small icon failed, try to get large icon
    if (extractResult == 0 || !hIcon) {
        extractResult = ExtractIconExA(path.c_str(), 0, &hIcon, NULL, 1);
    }
    
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
    
    // Get icon dimensions - use smaller icon for list views
    int iconWidth = GetSystemMetrics(SM_CXSMICON);
    int iconHeight = GetSystemMetrics(SM_CYSMICON);
    
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

    // Close any existing instance of this app
    auto it = _applicationProcesses.find(appId);
    if (it != _applicationProcesses.end()) {
        // There's already a process handle for this app
        if (it->second != NULL) {
            // Check if process is still running
            DWORD exitCode = 0;
            if (GetExitCodeProcess(it->second, &exitCode) && exitCode == STILL_ACTIVE) {
                std::cout << "Application " << appId << " is already running. Closing existing instance..." << std::endl;
                // Terminate the existing process
                TerminateProcess(it->second, 0);
                CloseHandle(it->second);
            } else {
                // Process has already exited, just close the handle
                CloseHandle(it->second);
            }
        }
        // Remove the old handle
        _applicationProcesses.erase(it);
    }

    bool result = false;
    HANDLE processHandle = NULL;

    switch (app->type) {
        case ApplicationType::EXECUTABLE:
            result = launchExecutable(*app);
            // Get the process handle
            if (result && _applicationProcesses.find(appId) != _applicationProcesses.end()) {
                processHandle = _applicationProcesses[appId];
            }
            break;
        case ApplicationType::WEBSITE:
            result = launchWebsite(*app);
            break;
        case ApplicationType::SYSTEM_COMMAND:
            result = launchSystemCommand(*app);
            break;
        default:
            std::cerr << "Unsupported application type" << std::endl;
            return false;
    }

    return result;
}

// Close application by ID
bool ApplicationLauncher::closeApplication(const std::string& appId) {
    try {
        // Find the application info
        auto app = findApplicationById(appId);
        if (!app) {
            std::cerr << "Application not registered: " << appId << std::endl;
            return false;
        }

        // Check if we have a handle in our process map
        auto it = _applicationProcesses.find(appId);
        HANDLE processHandle = NULL;
        bool ownedHandle = false;
        
        if (it != _applicationProcesses.end() && it->second != NULL) {
            processHandle = it->second;
            ownedHandle = true;
            
            // Check if the process is still running
            DWORD exitCode = 0;
            if (!GetExitCodeProcess(processHandle, &exitCode) || exitCode != STILL_ACTIVE) {
                // Process has already exited, clean up and look for new instances
                CloseHandle(processHandle);
                _applicationProcesses.erase(it);
                processHandle = NULL;
                ownedHandle = false;
            }
        }
        
        // If we don't have a valid process handle, try to find the process by name
        if (processHandle == NULL) {
            // Find all windows with matching title or process name
            std::vector<HWND> matchingWindows;
            std::string execName = app->path.substr(app->path.find_last_of("\\/") + 1);
            
            // Remove extension if present
            size_t dotPos = execName.find_last_of(".");
            if (dotPos != std::string::npos) {
                execName = execName.substr(0, dotPos);
            }
            
            // Convert app name to lowercase for case-insensitive comparison
            std::string lowerAppName = app->name;
            std::transform(lowerAppName.begin(), lowerAppName.end(), lowerAppName.begin(), ::tolower);
            std::string lowerExecName = execName;
            std::transform(lowerExecName.begin(), lowerExecName.end(), lowerExecName.begin(), ::tolower);
            
            // Find all windows that might match our application
            EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
                std::vector<HWND>* windows = reinterpret_cast<std::vector<HWND>*>(lParam);
                if (IsWindowVisible(hwnd) || IsIconic(hwnd)) {  // Include minimized windows
                    windows->push_back(hwnd);
                }
                return TRUE; // Continue enumeration
            }, reinterpret_cast<LPARAM>(&matchingWindows));
            
            // Find window that matches our application name or executable name
            for (HWND hwnd : matchingWindows) {
                // Get window title
                char title[256] = {0};
                GetWindowTextA(hwnd, title, sizeof(title));
                std::string windowTitle = title;
                std::transform(windowTitle.begin(), windowTitle.end(), windowTitle.begin(), ::tolower);
                
                // Get process name
                DWORD pid = 0;
                GetWindowThreadProcessId(hwnd, &pid);
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                char procName[MAX_PATH] = {0};
                
                if (h) {
                    DWORD size = sizeof(procName);
                    if (QueryFullProcessImageNameA(h, 0, procName, &size)) {
                        std::string processPath = procName;
                        std::string processName = processPath.substr(processPath.find_last_of("\\/") + 1);
                        
                        // Remove extension if present
                        size_t dotPos = processName.find_last_of(".");
                        if (dotPos != std::string::npos) {
                            processName = processName.substr(0, dotPos);
                        }
                        
                        std::transform(processName.begin(), processName.end(), processName.begin(), ::tolower);
                        
                        // If the window title or process name matches our application
                        if (windowTitle.find(lowerAppName) != std::string::npos || 
                            windowTitle.find(lowerExecName) != std::string::npos ||
                            processName.find(lowerExecName) != std::string::npos) {
                            
                            // Get the process handle with PROCESS_TERMINATE rights to actually be able to close it
                            processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                            if (processHandle) {
                                // Store handle in our map for future use
                                _applicationProcesses[appId] = processHandle;
                                ownedHandle = true;
                                break;
                            }
                        }
                    }
                    CloseHandle(h);
                }
            }
        }
        
        // If we still don't have a valid process handle, try one more approach: by executable path
        if (processHandle == NULL && app->type == ApplicationType::EXECUTABLE) {
            // Create a snapshot of all processes
            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnapshot != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32 pe32;
                pe32.dwSize = sizeof(PROCESSENTRY32);
                
                // Get the process name from the app path
                std::string execName = app->path.substr(app->path.find_last_of("\\/") + 1);
                std::transform(execName.begin(), execName.end(), execName.begin(), ::tolower);
                
                if (Process32First(hSnapshot, &pe32)) {
                    do {
                        std::string procName = pe32.szExeFile;
                        std::transform(procName.begin(), procName.end(), procName.begin(), ::tolower);
                        
                        if (procName == execName) {
                            // Found a process with matching executable name
                            processHandle = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
                            if (processHandle) {
                                _applicationProcesses[appId] = processHandle;
                                ownedHandle = true;
                                break;
                            }
                        }
                    } while (Process32Next(hSnapshot, &pe32));
                }
                CloseHandle(hSnapshot);
            }
        }
        
        // If we still don't have a handle, we can't find the process
        if (processHandle == NULL) {
            std::cerr << "Could not find running instance of application: " << appId << std::endl;
            return false;
        }
        
        // Try to close the application gracefully first
        bool closed = false;
        
        // Find all windows belonging to the process
        DWORD processId = GetProcessId(processHandle);
        std::vector<HWND> processWindows;
        
        // Create a stable pair that we can safely take the address of
        std::pair<DWORD, std::vector<HWND>*> enumData(processId, &processWindows);
        
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto params = reinterpret_cast<std::pair<DWORD, std::vector<HWND>*>*>(lParam);
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            
            if (pid == params->first) {
                params->second->push_back(hwnd);
            }
            return TRUE; // Continue enumeration
        }, reinterpret_cast<LPARAM>(&enumData));
        
        // Try to close each window with WM_CLOSE
        for (HWND hwnd : processWindows) {
            if (IsWindow(hwnd)) {
                // Post the message instead of sending it to avoid deadlocks
                PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
        }
        
        // Wait for process to exit
        if (WaitForSingleObject(processHandle, 2000) == WAIT_OBJECT_0) {
            closed = true;
        }
        
        // If graceful close failed, force terminate
        if (!closed) {
            if (!TerminateProcess(processHandle, 0)) {
                DWORD error = GetLastError();
                std::cerr << "Failed to terminate process for application: " << appId;
                std::cerr << " (Error: " << error << ")" << std::endl;
                if (ownedHandle) {
                    CloseHandle(processHandle);
                    _applicationProcesses.erase(appId);
                }
                return false;
            }
            
            // Wait to ensure the process is really terminated
            WaitForSingleObject(processHandle, 1000);
        }
        
        // Clean up the handle if we own it
        if (ownedHandle) {
            CloseHandle(processHandle);
            _applicationProcesses.erase(appId);
        }
        
        std::cout << "Successfully closed application: " << appId << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in closeApplication: " << e.what() << std::endl;
        return false;
    }
    catch (...) {
        std::cerr << "Unknown exception in closeApplication" << std::endl;
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

    // Custom launches don't have an ID to track them by
    if (sei.hProcess) {
        CloseHandle(sei.hProcess); // We can't track this process, so close the handle
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

    // Store the process handle
    if (sei.hProcess) {
        _applicationProcesses[app.id] = sei.hProcess;
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

    // Note: We can't reliably track browser processes, so we don't store any process handle

    return true;
}

bool ApplicationLauncher::launchSystemCommand(const Application& app) {
    // Use system to execute command
    int result = system(app.path.c_str());
    
    if (result != 0) {
        std::cerr << "Failed to execute system command. Return code: " << result << std::endl;
        return false;
    }

    // Note: We can't reliably track system command processes, so we don't store any process handle

    return true;
}