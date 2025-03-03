#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

class InputHandler {
public:
    // Input event types
    enum class EventType {
        MOUSE_MOVE,
        MOUSE_DOWN,
        MOUSE_UP,
        MOUSE_WHEEL,
        KEY_DOWN,
        KEY_UP,
        TEXT_INPUT
    };

    // Input event data
    struct InputEvent {
        EventType type;
        int x;              // Mouse X position
        int y;              // Mouse Y position
        int button;         // Mouse button (0=left, 1=middle, 2=right)
        int wheelDelta;     // Mouse wheel delta
        int keyCode;        // Virtual key code
        std::string text;   // Text input content
        bool altKey;        // Alt key pressed
        bool ctrlKey;       // Ctrl key pressed
        bool shiftKey;      // Shift key pressed
    };

    // Constructor
    InputHandler();
    
    // Destructor
    ~InputHandler();
    
    // Process input event from JSON
    bool processInputEvent(const nlohmann::json& eventJson);
    
    // Create input event from JSON
    static InputEvent parseInputEvent(const nlohmann::json& eventJson);
    
    // Helper methods for each event type
    void handleMouseMove(int x, int y, bool isRelative = false);
    void handleMouseDown(int x, int y, int button);
    void handleMouseUp(int x, int y, int button);
    void handleMouseWheel(int delta);
    void handleKeyDown(int keyCode, bool altKey, bool ctrlKey, bool shiftKey);
    void handleKeyUp(int keyCode, bool altKey, bool ctrlKey, bool shiftKey);
    void handleTextInput(const std::string& text);
    
    // Set scale factors to translate coordinates
    void setScaleFactor(float scaleX, float scaleY) {
        _scaleX = scaleX;
        _scaleY = scaleY;
    }
    
    // Set offset for coordinate translation
    void setOffset(int offsetX, int offsetY) {
        _offsetX = offsetX;
        _offsetY = offsetY;
    }
    
private:
    // Input state
    bool _mouseButtons[3] = {false, false, false};
    
    // Scale factors for coordinate translation
    float _scaleX = 1.0f;
    float _scaleY = 1.0f;
    
    // Offset for coordinate translation
    int _offsetX = 0;
    int _offsetY = 0;
    
    // Helper method to scale and offset coordinates
    void translateCoordinates(int& x, int& y);
    
    // Helper method to determine if a key is extended
    bool isExtendedKey(int keyCode);
};