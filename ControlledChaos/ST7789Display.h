#pragma once

#include "daisy_seed.h"

using namespace daisy;

class ST7789Display
{
  public:
    void Init(DaisySeed* hardware)
{
    hardware_ = hardware;

    // ------------------------------------------------------------
    // CONTROL PINS
    // ------------------------------------------------------------

    dc_.Init(seed::D5, GPIO::Mode::OUTPUT);
    rst_.Init(seed::D6, GPIO::Mode::OUTPUT);
    cs_.Init(seed::D7, GPIO::Mode::OUTPUT);

    cs_.Write(1);
    dc_.Write(1);
    rst_.Write(1);

    // ------------------------------------------------------------
    // BACKLIGHT PWM
    //
    // Daisy D4 = TIM3 Channel 3
    // ------------------------------------------------------------

    PWMHandle::Config pwmConfig(
        PWMHandle::Config::Peripheral::TIM_3,
        0,
        9999);

    backlightPwm_.Init(pwmConfig);

    PWMHandle::Channel::Config backlightConfig(
        seed::D4,
        PWMHandle::Channel::Config::Polarity::HIGH);

    backlightPwm_.Channel3().Init(
        backlightConfig);

    // Während der kompletten Display-Initialisierung dunkel
    backlightPwm_.Channel3().Set(0.0f);

    // ------------------------------------------------------------
    // SPI1
    // ------------------------------------------------------------

    SpiHandle::Config spiConfig;

    spiConfig.periph =
        SpiHandle::Config::Peripheral::SPI_1;

    spiConfig.mode =
        SpiHandle::Config::Mode::MASTER;

    spiConfig.direction =
        SpiHandle::Config::Direction::TWO_LINES_TX_ONLY;

    spiConfig.datasize = 8;

    spiConfig.clock_polarity =
        SpiHandle::Config::ClockPolarity::LOW;

    spiConfig.clock_phase =
        SpiHandle::Config::ClockPhase::ONE_EDGE;

    spiConfig.nss =
        SpiHandle::Config::NSS::SOFT;

    spiConfig.baud_prescaler = SpiHandle::Config::BaudPrescaler::PS_8;

    spiConfig.pin_config.sclk =
        seed::D8;

    spiConfig.pin_config.mosi =
        seed::D10;

    spiConfig.pin_config.miso =
        Pin();

    spiConfig.pin_config.nss =
        Pin();

    spi_.Init(spiConfig);

    // ------------------------------------------------------------
    // HARDWARE RESET
    // ------------------------------------------------------------

    hardware_->DelayMs(20);

    rst_.Write(0);
    hardware_->DelayMs(20);

    rst_.Write(1);
    hardware_->DelayMs(150);

    // ------------------------------------------------------------
    // ST7789 INITIALISIERUNG
    // ------------------------------------------------------------

    // Software reset
    Command(0x01);
    hardware_->DelayMs(150);

    // Sleep out
    Command(0x11);
    hardware_->DelayMs(150);

    // Inversion ON
    Command(0x21);

    // RGB565
    Command(0x3A);
    Data(0x55);

    // Memory Access Control
    Command(0x36);
    Data(0x00);

    // Normal display mode
    Command(0x13);
    hardware_->DelayMs(10);

    // Display ON
    // Backlight bleibt trotzdem noch AUS.
    Command(0x29);
    hardware_->DelayMs(20);

    // Display-RAM unsichtbar schwarz löschen
    FillBlack();

    // ------------------------------------------------------------
    // BACKLIGHT EIN
    // ------------------------------------------------------------

    SetBrightness(0.65f);
}

    void SetBrightness(float brightness)
    {
        backlightPwm_.Channel3().Set(brightness);
    }

    void DrawLine(int x0, int y0, int x1,int y1, uint16_t color)
    {
        const int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
        const int sx = (x0 < x1) ? 1 : -1;

        const int dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
        const int sy = (y0 < y1) ? 1 : -1;

        int err = dx + dy;

        while(true)
        {
            if(x0 >= 0 && x0 < 240 &&
            y0 >= 0 && y0 < 320)
            {
                DrawPixel(
                    static_cast<uint16_t>(x0),
                    static_cast<uint16_t>(y0),
                    color);
            }

            if(x0 == x1 && y0 == y1)
                break;

            const int e2 = 2 * err;

            if(e2 >= dy)
            {
                err += dy;
                x0 += sx;
            }

            if(e2 <= dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    }

    void FillBlack()
    {
        SetAddressWindow(0, 0, 239, 319);

        Command(0x2C);

        cs_.Write(0);
        dc_.Write(1);

        uint8_t buffer[128];

        for(size_t i = 0; i < sizeof(buffer); ++i)
        {
            buffer[i] = 0x00;
        }

        const int pixels = 240 * 320;
        const int pixelsPerBuffer = sizeof(buffer) / 2;

        for(int i = 0; i < pixels; i += pixelsPerBuffer)
        {
            spi_.BlockingTransmit(buffer, sizeof(buffer));
        }

        cs_.Write(1);
    }

    void DrawPixel(
    uint16_t x,
    uint16_t y,
    uint16_t color)
    {
        if(x >= 240 || y >= 320)
            return;

        uint8_t command;
        uint8_t data[4];

        // Eine einzige CS-Transaktion für den kompletten Pixel
        cs_.Write(0);

        // ------------------------------------------------------------
        // COLUMN ADDRESS
        // ------------------------------------------------------------

        command = 0x2A;

        dc_.Write(0);
        spi_.BlockingTransmit(
            &command,
            1);

        data[0] =
            static_cast<uint8_t>(x >> 8);

        data[1] =
            static_cast<uint8_t>(x & 0xFF);

        // Start und Ende sind beim Einzelpixel identisch
        data[2] = data[0];
        data[3] = data[1];

        dc_.Write(1);

        spi_.BlockingTransmit(
            data,
            4);

        // ------------------------------------------------------------
        // ROW ADDRESS
        // ------------------------------------------------------------

        command = 0x2B;

        dc_.Write(0);

        spi_.BlockingTransmit(
            &command,
            1);

        data[0] = static_cast<uint8_t>(y >> 8);

        data[1] = static_cast<uint8_t>(y & 0xFF);

        data[2] = data[0];
        data[3] = data[1];

        dc_.Write(1);

        spi_.BlockingTransmit(
            data,
            4);

        // ------------------------------------------------------------
        // MEMORY WRITE
        // ------------------------------------------------------------

        command = 0x2C;

        dc_.Write(0);

        spi_.BlockingTransmit(
            &command,
            1);

        uint8_t pixel[2];

        pixel[0] =
            static_cast<uint8_t>(
                color >> 8);

        pixel[1] =
            static_cast<uint8_t>(
                color & 0xFF);

        dc_.Write(1);

        spi_.BlockingTransmit(
            pixel,
            2);

        // komplette Transaktion fertig
        cs_.Write(1);
    }

    void FillRed()
    {
        SetAddressWindow(0, 0, 239, 319);

        Command(0x2C);

        cs_.Write(0);
        dc_.Write(1);

        // RGB565: Rot = 0xF800
        uint8_t buffer[128];

        for(size_t i = 0; i < sizeof(buffer); i += 2)
        {
            buffer[i]     = 0xF8;
            buffer[i + 1] = 0x00;
        }

        const int pixels = 240 * 320;
        const int pixelsPerBuffer = sizeof(buffer) / 2;

        for(int i = 0; i < pixels; i += pixelsPerBuffer)
        {
            spi_.BlockingTransmit(buffer, sizeof(buffer));
        }

        cs_.Write(1);
    }

    void DrawVersion(int x,int y,const char* version,
    uint16_t color)
    {
        DrawChar(x, y, 'v', color);
        x += 6;

        while(*version)
        {
            DrawChar(x, y, *version, color);
            x += 6;
            ++version;
        }
    }

    void DrawText(int x,int y,const char* text,uint16_t color)
    {
        while(*text)
        {
            DrawChar(x, y, *text, color);
            x += 6;
            ++text;
        }
    }

    void DrawStepProgress(
    int x,
    int y,
    int width,
    int height,
    int current,
    int total)
    {
        if(total <= 0)
            return;

        if(current < 0)
            current = 0;

        if(current > total)
            current = total;

        const int gap = 2;

        const int availableWidth =
            width - ((total - 1) * gap);

        for(int i = 0; i < total; ++i)
        {
            const int start =
                (i * availableWidth) / total;

            const int end =
                ((i + 1) * availableWidth) / total;

            const int segmentWidth =
                end - start;

            const int segmentX =
                x + start + i * gap;

            const uint16_t color =
                (i < current)
                    ? 0xFFFF
                    : 0x2104;

            FillRect(
                segmentX,
                y,
                segmentWidth,
                height,
                color);
        }
    }

    void DrawLoopPosition(
    int x,
    int y,
    int width,
    int height,
    int current,
    int total,
    bool mutated)
{
    if(total <= 0)
        return;

    if(current < 1)
        current = 1;

    if(current > total)
        current = total;

    const int gap = 2;

    const int availableWidth =
        width - ((total - 1) * gap);

    for(int i = 0; i < total; ++i)
    {
        const int start =
            (i * availableWidth) / total;

        const int end =
            ((i + 1) * availableWidth) / total;

        const int segmentWidth =
            end - start;

        const int segmentX =
            x + start + i * gap;

        uint16_t color = 0x2104;

        if(i == current - 1)
        {
            color = mutated
                ? 0xFD20
                : 0xFFFF;
        }

        FillRect(
            segmentX,
            y,
            segmentWidth,
            height,
            color);
    }
}

    void FillRect(
    int x,
    int y,
    int width,
    int height,
    uint16_t color)
    {
        if(width <= 0 || height <= 0)
            return;

        if(x < 0 || y < 0)
            return;

        if(x + width > 240)
            width = 240 - x;

        if(y + height > 320)
            height = 320 - y;

        SetAddressWindow(
            static_cast<uint16_t>(x),
            static_cast<uint16_t>(y),
            static_cast<uint16_t>(x + width - 1),
            static_cast<uint16_t>(y + height - 1));

        Command(0x2C);

        cs_.Write(0);
        dc_.Write(1);

        const uint8_t high =
            static_cast<uint8_t>(color >> 8);

        const uint8_t low =
            static_cast<uint8_t>(color & 0xFF);

        uint8_t buffer[128];

        for(size_t i = 0; i < sizeof(buffer); i += 2)
        {
            buffer[i]     = high;
            buffer[i + 1] = low;
        }

        int pixelsRemaining = width * height;

        while(pixelsRemaining > 0)
        {
            int pixelsThisTime = 64;

            if(pixelsThisTime > pixelsRemaining)
                pixelsThisTime = pixelsRemaining;

            spi_.BlockingTransmit(
                buffer,
                pixelsThisTime * 2);

            pixelsRemaining -= pixelsThisTime;
        }

        cs_.Write(1);
    }

    void DrawTextScaled(
        int x,
        int y,
        const char* text,
        uint16_t color,
        int scale)
    {
        while(*text)
        {
            DrawCharScaled(
                x,
                y,
                *text,
                color,
                scale);

            x += 6 * scale;
            ++text;
        }
    }

    void DrawMarker(
    int x,
    int y,
    uint16_t color)
    {
        static const uint8_t star[7] =
        {
            0b1001001,
            0b0101010,
            0b0011100,
            0b1111111,
            0b0011100,
            0b0101010,
            0b1001001
        };

        for(int row = 0; row < 7; ++row)
        {
            for(int col = 0; col < 7; ++col)
            {
                if(star[row] & (1 << (6 - col)))
                {
                    const int px = x + col - 3;
                    const int py = y + row - 3;

                    if(px >= 0 && px < 240 &&
                    py >= 0 && py < 320)
                    {
                        DrawPixel(
                            static_cast<uint16_t>(px),
                            static_cast<uint16_t>(py),
                            color);
                    }
                }
            }
        }
    }

    void DrawActiveMarker(
    int x,
    int y,
    uint16_t color)
    {
        // Zentrum
        DrawMarker(x, y, color);

        // großer vertikaler Strahl
        for(int d = -5; d <= 5; ++d)
        {
            DrawPixel(x, y + d, color);
        }

        // großer horizontaler Strahl
        for(int d = -5; d <= 5; ++d)
        {
            DrawPixel(x + d, y, color);
        }

        // diagonale Strahlen
        for(int d = -3; d <= 3; ++d)
        {
            DrawPixel(x + d, y + d, color);
            DrawPixel(x + d, y - d, color);
        }
    }

    void DrawHorizontalSpan(
    int x,
    int y,
    const uint16_t* colors,
    int count)
    {
        if(colors == nullptr || count <= 0)
            return;

        if(y < 0 || y >= 320)
            return;

        if(x < 0)
        {
            const int skip = -x;

            if(skip >= count)
                return;

            colors += skip;
            count -= skip;
            x = 0;
        }

        if(x >= 240)
            return;

        if(x + count > 240)
            count = 240 - x;

        // ------------------------------------------------------------
        // Eine CS-Transaktion für den kompletten Span
        // ------------------------------------------------------------

        cs_.Write(0);

        uint8_t command;
        uint8_t address[4];

        // X-Bereich
        command = 0x2A;

        dc_.Write(0);
        spi_.BlockingTransmit(&command, 1);

        const uint16_t x1 =
            static_cast<uint16_t>(x + count - 1);

        address[0] =
            static_cast<uint8_t>(x >> 8);

        address[1] =
            static_cast<uint8_t>(x & 0xFF);

        address[2] =
            static_cast<uint8_t>(x1 >> 8);

        address[3] =
            static_cast<uint8_t>(x1 & 0xFF);

        dc_.Write(1);
        spi_.BlockingTransmit(address, 4);

        // Y = genau eine Zeile
        command = 0x2B;

        dc_.Write(0);
        spi_.BlockingTransmit(&command, 1);

        address[0] =
            static_cast<uint8_t>(y >> 8);

        address[1] =
            static_cast<uint8_t>(y & 0xFF);

        address[2] = address[0];
        address[3] = address[1];

        dc_.Write(1);
        spi_.BlockingTransmit(address, 4);

        // RAM Write
        command = 0x2C;

        dc_.Write(0);
        spi_.BlockingTransmit(&command, 1);

        // ------------------------------------------------------------
        // Farben in kleine RGB565-Blöcke packen
        // ------------------------------------------------------------

        dc_.Write(1);

        uint8_t buffer[128];

        int remaining = count;
        int sourceIndex = 0;

        while(remaining > 0)
        {
            const int pixelsThisTime =
                remaining > 64
                    ? 64
                    : remaining;

            for(int i = 0;
                i < pixelsThisTime;
                ++i)
            {
                const uint16_t color =
                    colors[sourceIndex + i];

                buffer[i * 2] =
                    static_cast<uint8_t>(
                        color >> 8);

                buffer[i * 2 + 1] =
                    static_cast<uint8_t>(
                        color & 0xFF);
            }

            spi_.BlockingTransmit(
                buffer,
                pixelsThisTime * 2);

            sourceIndex +=
                pixelsThisTime;

            remaining -=
                pixelsThisTime;
        }

        cs_.Write(1);
    }

    void ClearLineBuffer(uint16_t color = 0x0000)
{
    for(int x = 0; x < 240; ++x)
    {
        lineBuffer_[x] = color;
    }
}

void SetLinePixel(
    int x,
    uint16_t color)
{
    if(x < 0 || x >= 240)
        return;

    lineBuffer_[x] = color;
}

void FillLineSpan(
    int x,
    int width,
    uint16_t color)
{
    if(width <= 0)
        return;

    int x0 = x;
    int x1 = x + width - 1;

    if(x0 < 0)
        x0 = 0;

    if(x1 >= 240)
        x1 = 239;

    if(x0 > x1)
        return;

    for(int px = x0; px <= x1; ++px)
    {
        lineBuffer_[px] = color;
    }
}

void FlushLineBuffer(
    int y,
    int x0 = 0,
    int x1 = 239)
{
    if(y < 0 || y >= 320)
        return;

    if(x0 < 0)
        x0 = 0;

    if(x1 >= 240)
        x1 = 239;

    if(x0 > x1)
        return;

    DrawHorizontalSpan(
        x0,
        y,
        &lineBuffer_[x0],
        x1 - x0 + 1);
}

  private:
    DaisySeed* hardware_ = nullptr;

    SpiHandle spi_;

        GPIO dc_;
        GPIO rst_;
        GPIO cs_;
        uint16_t lineBuffer_[240] = {};

    PWMHandle backlightPwm_;

    void Command(uint8_t command)
    {
        cs_.Write(0);
        dc_.Write(0);

        spi_.BlockingTransmit(&command, 1);

        cs_.Write(1);
    }

    void Data(uint8_t data)
    {
        cs_.Write(0);
        dc_.Write(1);

        spi_.BlockingTransmit(&data, 1);

        cs_.Write(1);
    }

    void SetAddressWindow(
    uint16_t x0,
    uint16_t y0,
    uint16_t x1,
    uint16_t y1)
{
    // ------------------------------------------------------------
    // COLUMN ADDRESS
    // ------------------------------------------------------------

    Command(0x2A);

    uint8_t columnData[4] =
    {
        static_cast<uint8_t>(x0 >> 8),
        static_cast<uint8_t>(x0 & 0xFF),
        static_cast<uint8_t>(x1 >> 8),
        static_cast<uint8_t>(x1 & 0xFF)
    };

    cs_.Write(0);
    dc_.Write(1);

    spi_.BlockingTransmit(
        columnData,
        sizeof(columnData));

    cs_.Write(1);

    // ------------------------------------------------------------
    // ROW ADDRESS
    // ------------------------------------------------------------

    Command(0x2B);

    uint8_t rowData[4] =
    {
        static_cast<uint8_t>(y0 >> 8),
        static_cast<uint8_t>(y0 & 0xFF),
        static_cast<uint8_t>(y1 >> 8),
        static_cast<uint8_t>(y1 & 0xFF)
    };

    cs_.Write(0);
    dc_.Write(1);

    spi_.BlockingTransmit(
        rowData,
        sizeof(rowData));

    cs_.Write(1);
}

        void DrawCharScaled(
        int x,
        int y,
        char c,
        uint16_t color,
        int scale)
    {
        uint8_t glyph[5] = {0, 0, 0, 0, 0};

        switch(c)
        {
            case 'A':
                glyph[0] = 0x7E;
                glyph[1] = 0x11;
                glyph[2] = 0x11;
                glyph[3] = 0x11;
                glyph[4] = 0x7E;
                break;

            case 'E':
                glyph[0] = 0x7F;
                glyph[1] = 0x49;
                glyph[2] = 0x49;
                glyph[3] = 0x49;
                glyph[4] = 0x41;
                break;

            case 'H':
                glyph[0] = 0x7F;
                glyph[1] = 0x08;
                glyph[2] = 0x08;
                glyph[3] = 0x08;
                glyph[4] = 0x7F;
                break;

            case 'M':
                glyph[0] = 0x7F;
                glyph[1] = 0x02;
                glyph[2] = 0x0C;
                glyph[3] = 0x02;
                glyph[4] = 0x7F;
                break;

            case 'N':
                glyph[0] = 0x7F;
                glyph[1] = 0x04;
                glyph[2] = 0x08;
                glyph[3] = 0x10;
                glyph[4] = 0x7F;
                break;

            case 'P':
                glyph[0] = 0x7F;
                glyph[1] = 0x09;
                glyph[2] = 0x09;
                glyph[3] = 0x09;
                glyph[4] = 0x06;
                break;

            case 'R':
                glyph[0] = 0x7F;
                glyph[1] = 0x09;
                glyph[2] = 0x19;
                glyph[3] = 0x29;
                glyph[4] = 0x46;
                break;

            case 'S':
                glyph[0] = 0x46;
                glyph[1] = 0x49;
                glyph[2] = 0x49;
                glyph[3] = 0x49;
                glyph[4] = 0x31;
                break;

            case 'U':
                glyph[0] = 0x3F;
                glyph[1] = 0x40;
                glyph[2] = 0x40;
                glyph[3] = 0x40;
                glyph[4] = 0x3F;
                break;

            case 'X':
                glyph[0] = 0x63;
                glyph[1] = 0x14;
                glyph[2] = 0x08;
                glyph[3] = 0x14;
                glyph[4] = 0x63;
                break;

            case '/':
                glyph[0] = 0x20;
                glyph[1] = 0x10;
                glyph[2] = 0x08;
                glyph[3] = 0x04;
                glyph[4] = 0x02;
                break;

            default:
                return;
        }

        for(int col = 0; col < 5; ++col)
        {
            for(int row = 0; row < 7; ++row)
            {
                if(glyph[col] & (1 << row))
                {
                    FillRect(
                        x + col * scale,
                        y + row * scale,
                        scale,
                        scale,
                        color);
                }
            }
        }
    }

    void DrawChar(
    int x,
    int y,
    char c,
    uint16_t color)
{
    uint8_t glyph[5] = {0, 0, 0, 0, 0};

    switch(c)
    {
        case 'v':
            glyph[0] = 0x18;
            glyph[1] = 0x20;
            glyph[2] = 0x40;
            glyph[3] = 0x20;
            glyph[4] = 0x18;
            break;

        case '0':
            glyph[0] = 0x3E;
            glyph[1] = 0x51;
            glyph[2] = 0x49;
            glyph[3] = 0x45;
            glyph[4] = 0x3E;
            break;

        case '1':
            glyph[0] = 0x00;
            glyph[1] = 0x42;
            glyph[2] = 0x7F;
            glyph[3] = 0x40;
            glyph[4] = 0x00;
            break;

        case '2':
            glyph[0] = 0x42;
            glyph[1] = 0x61;
            glyph[2] = 0x51;
            glyph[3] = 0x49;
            glyph[4] = 0x46;
            break;

        case '3':
            glyph[0] = 0x21;
            glyph[1] = 0x41;
            glyph[2] = 0x45;
            glyph[3] = 0x4B;
            glyph[4] = 0x31;
            break;

        case '4':
            glyph[0] = 0x18;
            glyph[1] = 0x14;
            glyph[2] = 0x12;
            glyph[3] = 0x7F;
            glyph[4] = 0x10;
            break;

        case '5':
            glyph[0] = 0x27;
            glyph[1] = 0x45;
            glyph[2] = 0x45;
            glyph[3] = 0x45;
            glyph[4] = 0x39;
            break;

        case '6':
            glyph[0] = 0x3C;
            glyph[1] = 0x4A;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x30;
            break;

        case '7':
            glyph[0] = 0x01;
            glyph[1] = 0x71;
            glyph[2] = 0x09;
            glyph[3] = 0x05;
            glyph[4] = 0x03;
            break;

        case '8':
            glyph[0] = 0x36;
            glyph[1] = 0x49;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x36;
            break;

        case '9':
            glyph[0] = 0x06;
            glyph[1] = 0x49;
            glyph[2] = 0x49;
            glyph[3] = 0x29;
            glyph[4] = 0x1E;
            break;

        case '.':
            glyph[0] = 0x00;
            glyph[1] = 0x60;
            glyph[2] = 0x60;
            glyph[3] = 0x00;
            glyph[4] = 0x00;
            break;

        case 'A':
            glyph[0] = 0x7E;
            glyph[1] = 0x11;
            glyph[2] = 0x11;
            glyph[3] = 0x11;
            glyph[4] = 0x7E;
            break;
        
        case 'B':
            glyph[0] = 0x7F;
            glyph[1] = 0x49;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x36;
            break;

        case 'E':
            glyph[0] = 0x7F;
            glyph[1] = 0x49;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x41;
            break;

        case 'F':
            glyph[0] = 0x7F;
            glyph[1] = 0x09;
            glyph[2] = 0x09;
            glyph[3] = 0x09;
            glyph[4] = 0x01;
            break;

        case 'H':
            glyph[0] = 0x7F;
            glyph[1] = 0x08;
            glyph[2] = 0x08;
            glyph[3] = 0x08;
            glyph[4] = 0x7F;
            break;

        case 'I':
            glyph[0] = 0x00;
            glyph[1] = 0x41;
            glyph[2] = 0x7F;
            glyph[3] = 0x41;
            glyph[4] = 0x00;
            break;

        case 'L':
            glyph[0] = 0x7F;
            glyph[1] = 0x40;
            glyph[2] = 0x40;
            glyph[3] = 0x40;
            glyph[4] = 0x40;
            break;

        case 'M':
            glyph[0] = 0x7F;
            glyph[1] = 0x02;
            glyph[2] = 0x0C;
            glyph[3] = 0x02;
            glyph[4] = 0x7F;
            break;

        case 'N':
            glyph[0] = 0x7F;
            glyph[1] = 0x04;
            glyph[2] = 0x08;
            glyph[3] = 0x10;
            glyph[4] = 0x7F;
            break;

        case 'O':
            glyph[0] = 0x3E;
            glyph[1] = 0x41;
            glyph[2] = 0x41;
            glyph[3] = 0x41;
            glyph[4] = 0x3E;
            break;

        case 'P':
            glyph[0] = 0x7F;
            glyph[1] = 0x09;
            glyph[2] = 0x09;
            glyph[3] = 0x09;
            glyph[4] = 0x06;
            break;

        case 'R':
            glyph[0] = 0x7F;
            glyph[1] = 0x09;
            glyph[2] = 0x19;
            glyph[3] = 0x29;
            glyph[4] = 0x46;
            break;

        case 'S':
            glyph[0] = 0x46;
            glyph[1] = 0x49;
            glyph[2] = 0x49;
            glyph[3] = 0x49;
            glyph[4] = 0x31;
            break;

        case 'U':
            glyph[0] = 0x3F;
            glyph[1] = 0x40;
            glyph[2] = 0x40;
            glyph[3] = 0x40;
            glyph[4] = 0x3F;
            break;

        case 'V':
            glyph[0] = 0x1F;
            glyph[1] = 0x20;
            glyph[2] = 0x40;
            glyph[3] = 0x20;
            glyph[4] = 0x1F;
            break;

        case 'X':
            glyph[0] = 0x63;
            glyph[1] = 0x14;
            glyph[2] = 0x08;
            glyph[3] = 0x14;
            glyph[4] = 0x63;
            break;

        case 'Z':
            glyph[0] = 0x61;
            glyph[1] = 0x51;
            glyph[2] = 0x49;
            glyph[3] = 0x45;
            glyph[4] = 0x43;
            break;

        case '/':
            glyph[0] = 0x20;
            glyph[1] = 0x10;
            glyph[2] = 0x08;
            glyph[3] = 0x04;
            glyph[4] = 0x02;
            break;

        case ' ':
            break;

        default:
            return;
    }

    for(int col = 0; col < 5; ++col)
    {
        for(int row = 0; row < 7; ++row)
        {
            if(glyph[col] & (1 << row))
            {
                DrawPixel(
                    static_cast<uint16_t>(x + col),
                    static_cast<uint16_t>(y + row),
                    color);
            }
        }
    }
}
};