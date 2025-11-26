//------------------------------------------------------------------------
// Copyright(c) 2024 Stratojets.
//------------------------------------------------------------------------

#pragma once

#include "FlaschenTaschenClient.h"
#include <string>
#include <cstdint>

namespace FlaschenTaschen {

//------------------------------------------------------------------------
// BitmapFont - Simple 5x7 bitmap font renderer for LED matrices
//------------------------------------------------------------------------
class BitmapFont {
public:
    // Character dimensions
    static constexpr int CHAR_WIDTH = 5;
    static constexpr int CHAR_HEIGHT = 7;
    static constexpr int CHAR_SPACING = 1;

    BitmapFont() = default;
    ~BitmapFont() = default;

    // Set scale factor (1 = normal, 2 = double size, etc.)
    void setScale(int scale) { scale_ = (scale > 0) ? scale : 1; }
    int getScale() const { return scale_; }

    // Get scaled dimensions
    int getScaledCharWidth() const { return CHAR_WIDTH * scale_; }
    int getScaledCharHeight() const { return CHAR_HEIGHT * scale_; }
    int getScaledSpacing() const { return CHAR_SPACING * scale_; }

    // Calculate text width in pixels
    int getTextWidth(const std::string& text) const;

    // Render a single character at position
    void renderChar(FlaschenTaschenClient& client, char c, int x, int y,
                    const Color& color, const Color& bgColor = Color::Black()) const;

    // Render text string at position
    void renderText(FlaschenTaschenClient& client, const std::string& text,
                    int x, int y, const Color& color, const Color& bgColor = Color::Black()) const;

    // Render text centered horizontally
    void renderTextCentered(FlaschenTaschenClient& client, const std::string& text,
                            int y, const Color& color, const Color& bgColor = Color::Black()) const;

    // Render text centered both horizontally and vertically
    void renderTextCenteredFull(FlaschenTaschenClient& client, const std::string& text,
                                const Color& color, const Color& bgColor = Color::Black()) const;

    // Get the bitmap data for a character (for custom rendering)
    static const uint8_t* getCharBitmap(char c);

private:
    int scale_ = 1;

    // Character bitmap lookup
    static int charToIndex(char c);
};

//------------------------------------------------------------------------
} // namespace FlaschenTaschen
