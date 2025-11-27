//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#include "FlaschenTaschenClient.h"
#include <sstream>
#include <cstring>
#include <iostream>

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// FlaschenTaschenClient Implementation
//------------------------------------------------------------------------
FlaschenTaschenClient::FlaschenTaschenClient() {
    resizeBuffer();
}

FlaschenTaschenClient::~FlaschenTaschenClient() {
    disconnect();
}

bool FlaschenTaschenClient::connect(const std::string& ip, int port) {
    disconnect();  // Clean up any existing connection

#ifdef _WIN32
    // Initialize Winsock
    if (!wsaInitialized_) {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            lastError_ = "WSAStartup failed: " + std::to_string(result);
            return false;
        }
        wsaInitialized_ = true;
    }

    // Create UDP socket
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) {
        lastError_ = "Failed to create socket: " + std::to_string(WSAGetLastError());
        return false;
    }
#else
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) {
        lastError_ = "Failed to create socket";
        return false;
    }
#endif

    // Set up server address
    memset(&serverAddr_, 0, sizeof(serverAddr_));
    serverAddr_.sin_family = AF_INET;
    serverAddr_.sin_port = htons(static_cast<u_short>(port));

#ifdef _WIN32
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr_.sin_addr) != 1) {
        lastError_ = "Invalid IP address: " + ip;
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        return false;
    }
#else
    if (inet_pton(AF_INET, ip.c_str(), &serverAddr_.sin_addr) != 1) {
        lastError_ = "Invalid IP address: " + ip;
        close(socket_);
        socket_ = -1;
        return false;
    }
#endif

    isConnected_ = true;
    return true;
}

void FlaschenTaschenClient::disconnect() {
#ifdef _WIN32
    if (socket_ != INVALID_SOCKET) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
    }
#else
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
#endif
    isConnected_ = false;
}

void FlaschenTaschenClient::setDisplaySize(int width, int height) {
    if (width > 0 && height > 0) {
        width_ = width;
        height_ = height;
        resizeBuffer();
    }
}

void FlaschenTaschenClient::setOffset(int x, int y) {
    offsetX_ = x;
    offsetY_ = y;
}

void FlaschenTaschenClient::setLayer(int z) {
    layer_ = z;
}

void FlaschenTaschenClient::resizeBuffer() {
    frameBuffer_.resize(width_ * height_ * 3, 0);
}

void FlaschenTaschenClient::clear(const Color& color) {
    for (int i = 0; i < width_ * height_; ++i) {
        frameBuffer_[i * 3] = color.r;
        frameBuffer_[i * 3 + 1] = color.g;
        frameBuffer_[i * 3 + 2] = color.b;
    }
}

void FlaschenTaschenClient::setPixel(int x, int y, const Color& color) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return;
    }

    int index = (y * width_ + x) * 3;
    frameBuffer_[index] = color.r;
    frameBuffer_[index + 1] = color.g;
    frameBuffer_[index + 2] = color.b;
}

Color FlaschenTaschenClient::getPixel(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return Color::Black();
    }

    int index = (y * width_ + x) * 3;
    return Color(frameBuffer_[index], frameBuffer_[index + 1], frameBuffer_[index + 2]);
}

std::string FlaschenTaschenClient::buildPPMPacket() const {
    std::ostringstream packet;

    // Debug: confirm horizontal flip is active
    static bool firstCall = true;
    if (firstCall) {
        std::cout << "    [DEBUG] Horizontal flip ENABLED" << std::endl;
        firstCall = false;
    }

    // PPM P6 header
    packet << "P6\n";
    packet << width_ << " " << height_ << "\n";

    // FlaschenTaschen offset header (optional but useful)
    if (offsetX_ != 0 || offsetY_ != 0 || layer_ != 0) {
        packet << "#FT: " << offsetX_ << " " << offsetY_ << " " << layer_ << "\n";
    }

    // Max color value
    packet << "255\n";

    // Get header as string
    std::string header = packet.str();

    // Build flipped buffer (horizontal flip - mirror left/right)
    std::vector<uint8_t> flippedBuffer(frameBuffer_.size());
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            int srcIndex = (y * width_ + x) * 3;
            int dstIndex = (y * width_ + (width_ - 1 - x)) * 3;
            flippedBuffer[dstIndex] = frameBuffer_[srcIndex];
            flippedBuffer[dstIndex + 1] = frameBuffer_[srcIndex + 1];
            flippedBuffer[dstIndex + 2] = frameBuffer_[srcIndex + 2];
        }
    }

    // Build complete packet: header + binary pixel data
    std::string result;
    result.reserve(header.size() + flippedBuffer.size());
    result = header;
    result.append(reinterpret_cast<const char*>(flippedBuffer.data()), flippedBuffer.size());

    return result;
}

bool FlaschenTaschenClient::send() {
    if (!isConnected_) {
        lastError_ = "Not connected";
        return false;
    }

    std::string packet = buildPPMPacket();

#ifdef _WIN32
    int result = sendto(socket_,
                        packet.c_str(),
                        static_cast<int>(packet.size()),
                        0,
                        reinterpret_cast<sockaddr*>(&serverAddr_),
                        sizeof(serverAddr_));

    if (result == SOCKET_ERROR) {
        lastError_ = "Failed to send packet: " + std::to_string(WSAGetLastError());
        return false;
    }
#else
    ssize_t result = sendto(socket_,
                            packet.c_str(),
                            packet.size(),
                            0,
                            reinterpret_cast<sockaddr*>(&serverAddr_),
                            sizeof(serverAddr_));

    if (result < 0) {
        lastError_ = "Failed to send packet";
        return false;
    }
#endif

    return true;
}

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
