//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <cstdint>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// Color - RGB color for pixel operations
//------------------------------------------------------------------------
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    Color() = default;
    Color(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}

    static Color Black() { return Color(0, 0, 0); }
    static Color White() { return Color(255, 255, 255); }
    static Color Red() { return Color(255, 0, 0); }
    static Color Green() { return Color(0, 255, 0); }
    static Color Blue() { return Color(0, 0, 255); }
};

//------------------------------------------------------------------------
// FlaschenTaschenClient - UDP client for sending frames to LED matrix
//------------------------------------------------------------------------
class FlaschenTaschenClient {
public:
    FlaschenTaschenClient();
    ~FlaschenTaschenClient();

    // Connect to server (initializes socket)
    bool connect(const std::string& ip, int port);

    // Disconnect
    void disconnect();

    // Check if connected
    bool isConnected() const { return isConnected_; }

    // Set display dimensions
    void setDisplaySize(int width, int height);

    // Set display offset (position on larger matrix)
    void setOffset(int x, int y);

    // Set Z-layer (0 = background, higher = overlay)
    void setLayer(int z);

    // Clear the frame buffer
    void clear(const Color& color = Color::Black());

    // Set a single pixel
    void setPixel(int x, int y, const Color& color);

    // Get frame buffer dimensions
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

    // Get pixel at position
    Color getPixel(int x, int y) const;

    // Send the current frame to the server
    bool send();

    // Get last error message
    const std::string& getLastError() const { return lastError_; }

private:
    int width_ = 45;
    int height_ = 35;
    int offsetX_ = 0;
    int offsetY_ = 0;
    int layer_ = 0;

    std::vector<uint8_t> frameBuffer_;  // RGB data

    bool isConnected_ = false;
    std::string lastError_;

#ifdef _WIN32
    SOCKET socket_ = INVALID_SOCKET;
    sockaddr_in serverAddr_;
    bool wsaInitialized_ = false;
#else
    int socket_ = -1;
    sockaddr_in serverAddr_;
#endif

    void resizeBuffer();
    std::string buildPPMPacket() const;
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
