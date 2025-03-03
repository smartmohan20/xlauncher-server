#include "input_handler.h"
#include <iostream>

// Constructor
InputHandler::InputHandler() {
    // Initialize state
    for (int i = 0; i < 3; i++) {
        _mouseButtons[i] = false;
    }
}

// Destructor
InputHandler::~InputHandler() {
    // No cleanup needed
}

// Process input event from JSON
bool InputHandler::processInputEvent(const nlohmann::json& eventJson) {
    try {
        InputEvent event = parseInputEvent(eventJson);
        
        switch (event.type) {
            case EventType::MOUSE_MOVE:
                handleMouseMove(event.x, event.y);
                break;
            case EventType::MOUSE_DOWN:
                handleMouseDown(event.x, event.y, event.button);
                break;
            case EventType::MOUSE_UP:
                handleMouseUp(event.x, event.y, event.button);
                break;
            case EventType::MOUSE_WHEEL:
                handleMouseWheel(event.wheelDelta);
                break;
            case EventType::KEY_DOWN:
                handleKeyDown(event.keyCode, event.altKey, event.ctrlKey, event.shiftKey);
                break;
            case EventType::KEY_UP:
                handleKeyUp(event.keyCode, event.altKey, event.ctrlKey, event.shiftKey);
                break;
            case EventType::TEXT_INPUT:
                handleTextInput(event.text);
                break;
            default:
                std::cerr << "Unknown event type" << std::endl;
                return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error processing input event: " << e.what() << std::endl;
        return false;
    }
}

// Parse input event from JSON
InputHandler::InputEvent InputHandler::parseInputEvent(const nlohmann::json& eventJson) {
    InputEvent event;
    
    // Extract event type
    std::string eventType = eventJson["eventType"];
    
    if (eventType == "mousemove") {
        event.type = EventType::MOUSE_MOVE;
    } else if (eventType == "mousedown") {
        event.type = EventType::MOUSE_DOWN;
    } else if (eventType == "mouseup") {
        event.type = EventType::MOUSE_UP;
    } else if (eventType == "wheel") {
        event.type = EventType::MOUSE_WHEEL;
    } else if (eventType == "keydown") {
        event.type = EventType::KEY_DOWN;
    } else if (eventType == "keyup") {
        event.type = EventType::KEY_UP;
    } else if (eventType == "textinput") {
        event.type = EventType::TEXT_INPUT;
    } else {
        throw std::runtime_error("Unknown event type: " + eventType);
    }
    
    // Extract common fields
    if (eventJson.contains("x")) {
        event.x = eventJson["x"];
    } else {
        event.x = 0;
    }
    
    if (eventJson.contains("y")) {
        event.y = eventJson["y"];
    } else {
        event.y = 0;
    }
    
    // Extract event-specific fields
    if (event.type == EventType::MOUSE_DOWN || event.type == EventType::MOUSE_UP) {
        if (eventJson.contains("button")) {
            event.button = eventJson["button"];
        } else {
            event.button = 0; // Default to left button
        }
    }
    
    if (event.type == EventType::MOUSE_WHEEL) {
        if (eventJson.contains("delta")) {
            event.wheelDelta = eventJson["delta"];
        } else {
            event.wheelDelta = 0;
        }
    }
    
    if (event.type == EventType::KEY_DOWN || event.type == EventType::KEY_UP) {
        if (eventJson.contains("keyCode")) {
            event.keyCode = eventJson["keyCode"];
        } else {
            event.keyCode = 0;
        }
        
        // Extract modifier keys
        event.altKey = eventJson.value("altKey", false);
        event.ctrlKey = eventJson.value("ctrlKey", false);
        event.shiftKey = eventJson.value("shiftKey", false);
    }
    
    if (event.type == EventType::TEXT_INPUT) {
        if (eventJson.contains("text")) {
            event.text = eventJson["text"];
        } else {
            event.text = "";
        }
    }
    
    return event;
}

// Translate coordinates based on scale and offset
void InputHandler::translateCoordinates(int& x, int& y) {
    x = static_cast<int>((x - _offsetX) / _scaleX);
    y = static_cast<int>((y - _offsetY) / _scaleY);
}

// Handle mouse move events
void InputHandler::handleMouseMove(int x, int y, bool isRelative) {
    if (!isRelative) {
        translateCoordinates(x, y);
    }
    
    // Get screen dimensions
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Clamp coordinates to screen dimensions
    x = std::max(0, std::min(x, screenWidth - 1));
    y = std::max(0, std::min(y, screenHeight - 1));
    
    // Set cursor position
    SetCursorPos(x, y);
}

// Handle mouse down events
void InputHandler::handleMouseDown(int x, int y, int button) {
    translateCoordinates(x, y);
    
    // Set cursor position first
    SetCursorPos(x, y);
    
    // Track button state
    if (button >= 0 && button < 3) {
        _mouseButtons[button] = true;
    }
    
    // Simulate mouse button press
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    
    if (button == 0) {
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    } else if (button == 1) {
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
    } else if (button == 2) {
        input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    }
    
    SendInput(1, &input, sizeof(INPUT));
}

// Handle mouse up events
void InputHandler::handleMouseUp(int x, int y, int button) {
    translateCoordinates(x, y);
    
    // Set cursor position first
    SetCursorPos(x, y);
    
    // Track button state
    if (button >= 0 && button < 3) {
        _mouseButtons[button] = false;
    }
    
    // Simulate mouse button release
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    
    if (button == 0) {
        input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    } else if (button == 1) {
        input.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
    } else if (button == 2) {
        input.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    }
    
    SendInput(1, &input, sizeof(INPUT));
}

// Handle mouse wheel events
void InputHandler::handleMouseWheel(int delta) {
    // Simulate mouse wheel
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = delta;
    
    SendInput(1, &input, sizeof(INPUT));
}

// Determine if a key is an extended key
bool InputHandler::isExtendedKey(int keyCode) {
    // List of extended keys
    static const int extendedKeys[] = {
        VK_RMENU, VK_RCONTROL, VK_INSERT, VK_DELETE, VK_HOME, VK_END,
        VK_PRIOR, VK_NEXT, VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,
        VK_NUMLOCK, VK_CANCEL, VK_SNAPSHOT, VK_DIVIDE
    };
    
    for (int i = 0; i < sizeof(extendedKeys) / sizeof(extendedKeys[0]); i++) {
        if (keyCode == extendedKeys[i]) {
            return true;
        }
    }
    
    return false;
}

// Handle key down events
void InputHandler::handleKeyDown(int keyCode, bool altKey, bool ctrlKey, bool shiftKey) {
    // Simulate key press
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = 0;
    
    if (isExtendedKey(keyCode)) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    
    SendInput(1, &input, sizeof(INPUT));
}

// Handle key up events
void InputHandler::handleKeyUp(int keyCode, bool altKey, bool ctrlKey, bool shiftKey) {
    // Simulate key release
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = keyCode;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    
    if (isExtendedKey(keyCode)) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    
    SendInput(1, &input, sizeof(INPUT));
}

// Handle text input events
void InputHandler::handleTextInput(const std::string& text) {
    // For each character in the text
    for (char c : text) {
        // Prepare a unicode character input
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = c;
        input.ki.dwFlags = KEYEVENTF_UNICODE;
        
        // Send key down
        SendInput(1, &input, sizeof(INPUT));
        
        // Send key up
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    }
}