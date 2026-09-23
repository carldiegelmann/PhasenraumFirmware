#include "daisy_seed.h"
#include "FirmwareVersion.h"
#include "ChaosEngine.h"
#include "DebugConsole.h"
#include "ChaosStep.h"
#include "PatternEngine.h"
#include "ClockEngine.h"
#include "ST7789Display.h"

#include <cstdio>
#include <cstring>
#include <cmath>

using namespace daisy;

DaisySeed hardware;

int main(void)
{
    hardware.Init();

    // ------------------------------------------------------------
    // DISPLAY
    // ------------------------------------------------------------

    ST7789Display display;
    display.Init(&hardware);
    display.SetBrightness(0.10f);

    // ------------------------------------------------------------
    // BOOT SPLASH
    // ------------------------------------------------------------

    display.DrawTextScaled(
        30,
        125,
        "PHASENRAUM",
        0xFFFF,
        3);

    display.DrawVersion(
        96,
        160,
        CONTROLLED_CHAOS_VERSION,
        0x7BEF);

    // ------------------------------------------------------------
    // CHAOS ENGINE
    // ------------------------------------------------------------

    ChaosEngine chaos;

    chaos.Init(303);
    chaos.SetSpeed(0.55);
    chaos.SetChaos(0.65);

    // Attraktor einschwingen lassen
    for(int i = 0; i < 20000; ++i)
    {
        chaos.Tick();
    }

    hardware.DelayMs(2000);

    // Splash löschen
    display.FillBlack();

    // ------------------------------------------------------------
    // 3D TRAIL
    // ------------------------------------------------------------

    static constexpr int TrailLength = 80;

    // Wir speichern jetzt die Chaos-Werte selbst,
    // nicht mehr nur die Bildschirmkoordinaten.
    float trailA[TrailLength] = {};
    float trailB[TrailLength] = {};
    float trailC[TrailLength] = {};

    // Letzte Projektion auf das Display.
    // Diese brauchen wir, um das alte Bild wieder zu löschen.
    int trailX[TrailLength] = {};
    int trailY[TrailLength] = {};

    int dirtyMinX[320] = {};
    int dirtyMaxX[320] = {};

    int trailCount = 0;
    int displayDivider = 0;
    int markerDisplayDivider = 0;

    // ------------------------------------------------------------
    // 3D ROTATION
    // ------------------------------------------------------------

    float rotationAngle = 0.0f;
    float pitchAngle = 0.0f;

    float rotationCos = 1.0f;
    float rotationSin = 0.0f;

    float pitchCos = 1.0f;
    float pitchSin = 0.0f;

    static constexpr float TwoPi =
        6.283185307179586f;

    // Eine volle Hauptrotation in 20 Sekunden
    static constexpr float RotationPeriodMs = 15000.0f;

    // ------------------------------------------------------------
    // LOG
    // ------------------------------------------------------------

    hardware.StartLog(false);

    hardware.PrintLine("");
    hardware.PrintLine("===============================");
    hardware.PrintLine("CONTROLLED CHAOS");
    hardware.PrintLine(
        "Firmware v%s",
        CONTROLLED_CHAOS_VERSION);
    hardware.PrintLine("===============================");

    // ------------------------------------------------------------
    // PATTERN ENGINE
    // ------------------------------------------------------------

    PatternEngine patternEngine;

    display.DrawText(
        4,
        305,
        "LIVE",
        0xFFFF);

    // ------------------------------------------------------------
    // CLOCK
    // ------------------------------------------------------------

    ClockEngine clockEngine;
    clockEngine.Init(120.0);

    // ------------------------------------------------------------
    // DEBUG CONSOLE
    // ------------------------------------------------------------

    DebugConsole console;

    console.Init(
        &hardware,
        &chaos,
        &patternEngine,
        &clockEngine);

    hardware.PrintLine("Controlled Chaos online");

    console.Status();

    // ------------------------------------------------------------
    // UI STATE
    // ------------------------------------------------------------

    int statusRefreshCounter = 0;

    char lastStatus[32] =
        "LIVE";

    bool lastFrozenState =
        false;

    constexpr uint16_t FrozenColor =
        0x7DFF;

    int previousMarkerX[
        PatternEngine::MaxPatternLength] = {};

    int previousMarkerY[
        PatternEngine::MaxPatternLength] = {};

    bool previousMarkerValid[
        PatternEngine::MaxPatternLength] = {};

    int lastDisplayedBpm =
        -1;

    double lastDisplayedRate =
        -1.0;

    uint32_t lastClockTime =
        System::GetNow();

    uint32_t lastRotationTime =
        System::GetNow();

    static constexpr int DisplayGridSize = 2;

    // ------------------------------------------------------------
    // 3D -> 2D PROJECTION
    // ------------------------------------------------------------

    auto Project3D =
        [&](float a,
            float b,
            float c,
            int& screenX,
            int& screenY)
    {
        // --------------------------------------------------------
        // Erste Rotation
        //
        // A und B drehen sich umeinander.
        // C ist zunächst die vertikale Achse.
        // --------------------------------------------------------

        const float x1 =
            a * rotationCos +
            b * rotationSin;

        const float z1 =
           -a * rotationSin +
            b * rotationCos;

        // --------------------------------------------------------
        // Zweite Rotation / Kippung
        //
        // Dadurch sehen wir nicht nur einen flachen,
        // rotierenden Schmetterling.
        // --------------------------------------------------------

        const float y1 =
            c * pitchCos -
            z1 * pitchSin;

        // --------------------------------------------------------
        // Bildschirmprojektion
        // --------------------------------------------------------

        screenX =
            120 +
            static_cast<int>(
            x1 * 115.0f);

        screenY =
            155 -
            static_cast<int>(
                y1 * 72.0f);

        // --------------------------------------------------------
        // Künstlich gröbere Display-Auflösung
        // --------------------------------------------------------

        screenX =
            (screenX / DisplayGridSize)
            * DisplayGridSize;

        screenY =
            (screenY / DisplayGridSize)
            * DisplayGridSize;
    };

    // ------------------------------------------------------------
    // PATTERN STEP -> DISPLAY
    // ------------------------------------------------------------

    auto StepToX =
        [&](const ChaosStep& step)
    {
        int x;
        int y;

        Project3D(
            static_cast<float>(step.A),
            static_cast<float>(step.B),
            static_cast<float>(step.C),
            x,
            y);

        return x;
    };

    auto StepToY =
        [&](const ChaosStep& step)
    {
        int x;
        int y;

        Project3D(
            static_cast<float>(step.A),
            static_cast<float>(step.B),
            static_cast<float>(step.C),
            x,
            y);

        return y;
    };

    // ------------------------------------------------------------
    // ORIENTATION CUBE
    // ------------------------------------------------------------

    static constexpr float cubeVertices[8][3] =
    {
        {-1.0f, -1.0f, -1.0f},
        { 1.0f, -1.0f, -1.0f},
        { 1.0f,  1.0f, -1.0f},
        {-1.0f,  1.0f, -1.0f},

        {-1.0f, -1.0f,  1.0f},
        { 1.0f, -1.0f,  1.0f},
        { 1.0f,  1.0f,  1.0f},
        {-1.0f,  1.0f,  1.0f}
    };

    static constexpr int cubeEdges[12][2] =
    {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},

        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4},

        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7}
    };

    int cubeX[8] = {};
    int cubeY[8] = {};

    auto DrawLineToBuffer =
        [&](int targetY,
            int x0,
            int y0,
            int x1,
            int y1,
            uint16_t color)
    {
        const int dx =
            x1 > x0
                ? x1 - x0
                : x0 - x1;

        const int sx =
            x0 < x1
                ? 1
                : -1;

        const int dy =
            -(y1 > y0
                ? y1 - y0
                : y0 - y1);

        const int sy =
            y0 < y1
                ? 1
                : -1;

        int err = dx + dy;

        while(true)
        {
            if(y0 == targetY)
            {
                display.SetLinePixel(
                    x0,
                    color);
            }

            if(x0 == x1 &&
            y0 == y1)
            {
                break;
            }

            const int e2 =
                2 * err;

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
    };

    // ============================================================
    // MAIN LOOP
    // ============================================================

    while(1)
    {
        // --------------------------------------------------------
        // CONSOLE
        // --------------------------------------------------------

        console.Process();

        // --------------------------------------------------------
        // BPM DISPLAY
        // --------------------------------------------------------

        const int currentBpm =
            static_cast<int>(
                clockEngine.GetBpm());

        if(currentBpm != lastDisplayedBpm)
        {
            char bpmText[16];

            snprintf(
                bpmText,
                sizeof(bpmText),
                "%d BPM",
                currentBpm);

            display.FillRect(
                188,
                4,
                52,
                10,
                0x0000);

            display.DrawText(
                194,
                5,
                bpmText,
                0xFFFF);

            lastDisplayedBpm =
                currentBpm;
        }

        // --------------------------------------------------------
        // RATE DISPLAY
        // --------------------------------------------------------

        const double currentRate =
            clockEngine.GetRate();

        if(currentRate != lastDisplayedRate)
        {
            char rateText[8];

            if(currentRate == 0.5)
            {
                snprintf(
                    rateText,
                    sizeof(rateText),
                    "/2");
            }
            else
            {
                snprintf(
                    rateText,
                    sizeof(rateText),
                    "X%d",
                    static_cast<int>(
                        currentRate));
            }

            display.FillRect(
                214,
                15,
                26,
                9,
                0x0000);

            display.DrawText(
                228,
                16,
                rateText,
                0xFFFF);

            lastDisplayedRate =
                currentRate;
        }

        // --------------------------------------------------------
        // FREEZE
        // --------------------------------------------------------

        const bool frozen =
            console.IsFrozen();

        if(frozen != lastFrozenState)
        {
            display.FillRect(
                150,
                302,
                90,
                12,
                0x0000);

            if(frozen)
            {
                display.DrawText(
                    194,
                    305,
                    "FROZEN",
                    FrozenColor);

                // Bestehenden Trail blau einfärben
                for(int i = 1;
                    i < trailCount;
                    ++i)
                {
                    display.DrawLine(
                        trailX[i - 1],
                        trailY[i - 1],
                        trailX[i],
                        trailY[i],
                        FrozenColor);
                }
            }
            else
            {
                // Beim Auftauen wieder normale
                // Helligkeitsabstufung herstellen.
                for(int i = 1;
                    i < trailCount;
                    ++i)
                {
                    const float age =
                        static_cast<float>(i)
                        /
                        static_cast<float>(
                            trailCount);

                    uint16_t color;

                    if(age < 0.25f)
                    {
                        color = 0x39E7;
                    }
                    else if(age < 0.50f)
                    {
                        color = 0x7BEF;
                    }
                    else if(age < 0.75f)
                    {
                        color = 0xC618;
                    }
                    else
                    {
                        color = 0xFFFF;
                    }

                    display.DrawLine(
                        trailX[i - 1],
                        trailY[i - 1],
                        trailX[i],
                        trailY[i],
                        color);
                }
            }

            lastFrozenState =
                frozen;
        }

        // --------------------------------------------------------
        // CHAOS OUTPUT
        // --------------------------------------------------------

        ChaosOutput output;

        if(frozen)
        {
            output =
                chaos.CurrentOutput();
        }
        else
        {
            output =
                chaos.Tick();
        }

        // Wenn FREEZE aktiv ist, soll sich auch die
        // Kamera/Rotation nicht weiterbewegen.
        if(frozen)
        {
            lastRotationTime =
                System::GetNow();
        }

        // ========================================================
        // 3D DISPLAY UPDATE
        // ========================================================

        if(!frozen)
        {
            displayDivider++;

            // 8 * ungefähr 5 ms = etwa 40 ms
            // -> ca. 25 Display-Frames pro Sekunde
            if(displayDivider >= 8)
            {
                displayDivider = 0;

                // ------------------------------------------------
                // DIRTY-ZEILEN VORBEREITEN
                // ------------------------------------------------

                for(int y = 25; y < 285; ++y)
                {
                    dirtyMinX[y] = 240;
                    dirtyMaxX[y] = -1;
                }

                // ------------------------------------------------
                // ALTE 3D-PUNKTE ALS DIRTY MARKIEREN
                // ------------------------------------------------

                for(int i = 0; i < trailCount; ++i)
                {
                    const int x = trailX[i];
                    const int y = trailY[i];

                    if(x < 0 ||
                    x >= 240 ||
                    y < 25 ||
                    y >= 285)
                    {
                        continue;
                    }

                    if(x < dirtyMinX[y])
                        dirtyMinX[y] = x;

                    if(x > dirtyMaxX[y])
                        dirtyMaxX[y] = x;
                }

                // ------------------------------------------------
                // TRAIL RING / SHIFT
                // ------------------------------------------------

                if(trailCount >= TrailLength)
                {
                    for(int i = 1;
                        i < TrailLength;
                        ++i)
                    {
                        trailA[i - 1] =
                            trailA[i];

                        trailB[i - 1] =
                            trailB[i];

                        trailC[i - 1] =
                            trailC[i];
                    }

                    trailCount =
                        TrailLength - 1;
                }

                // ------------------------------------------------
                // NEUEN 3D-PUNKT SPEICHERN
                // ------------------------------------------------

                trailA[trailCount] =
                    static_cast<float>(
                        output.A);

                trailB[trailCount] =
                    static_cast<float>(
                        output.B);

                trailC[trailCount] =
                    static_cast<float>(
                        output.C);

                trailCount++;

                // ------------------------------------------------
                // ROTATION
                // ------------------------------------------------

                const uint32_t rotationNow =
                    System::GetNow();

                const uint32_t rotationElapsed =
                    rotationNow -
                    lastRotationTime;

                lastRotationTime =
                    rotationNow;

                if(console.IsRotationEnabled())
                {
                    rotationAngle +=
                        TwoPi *
                        static_cast<float>(rotationElapsed)
                        /
                        RotationPeriodMs;

                    pitchAngle +=
                        TwoPi *
                        static_cast<float>(rotationElapsed)
                        /
                        (RotationPeriodMs * 2.0f);
                }

                while(rotationAngle >= TwoPi)
                {
                    rotationAngle -= TwoPi;
                }

                while(pitchAngle >= TwoPi)
                {
                    pitchAngle -= TwoPi;
                }

                rotationCos =
                    std::cos(rotationAngle);

                rotationSin =
                    std::sin(rotationAngle);

                pitchCos =
                    std::cos(pitchAngle);

                pitchSin =
                    std::sin(pitchAngle);


                // ------------------------------------------------
                // KLEINEN ORIENTATION CUBE PROJIZIEREN
                // ------------------------------------------------

                static constexpr float CubeScale =
                    6.5f;

                static constexpr int CubeCenterX =
                    18;

                static constexpr int CubeCenterY =
                    13;

                for(int i = 0; i < 8; ++i)
                {
                    const float a =
                        cubeVertices[i][0];

                    const float b =
                        cubeVertices[i][1];

                    const float c =
                        cubeVertices[i][2];

                    // Gleiche Rotation wie beim Phasenraum
                    const float x1 =
                        a * rotationCos +
                        b * rotationSin;

                    const float z1 =
                    -a * rotationSin +
                        b * rotationCos;

                    const float y1 =
                        c * pitchCos -
                        z1 * pitchSin;

                    cubeX[i] =
                        CubeCenterX +
                        static_cast<int>(
                            x1 * CubeScale);

                    cubeY[i] =
                        CubeCenterY -
                        static_cast<int>(
                            y1 * CubeScale);
                }

                // ------------------------------------------------
                // ORIENTATION CUBE RENDERN
                // ------------------------------------------------

                constexpr uint16_t CubeColor =
                    0x39E7;

                // Der Würfel bleibt vollständig oberhalb des
                // eigentlichen Phasenraum-Bereichs ab Y=25.

                for(int y = 1; y <= 24; ++y)
                {
                    display.ClearLineBuffer(
                        0x0000);

                    for(int edge = 0;
                        edge < 12;
                        ++edge)
                    {
                        const int a =
                            cubeEdges[edge][0];

                        const int b =
                            cubeEdges[edge][1];

                        DrawLineToBuffer(
                            y,
                            cubeX[a],
                            cubeY[a],
                            cubeX[b],
                            cubeY[b],
                            CubeColor);
                    }

                    display.FlushLineBuffer(
                        y,
                        4,
                        32);
                }

                // ------------------------------------------------
                // ALLE 3D-PUNKTE NEU PROJIZIEREN
                // ------------------------------------------------

                for(int i = 0;
                    i < trailCount;
                    ++i)
                {
                    Project3D(
                        trailA[i],
                        trailB[i],
                        trailC[i],
                        trailX[i],
                        trailY[i]);
                }

                // ------------------------------------------------
                // NEUE 3D-PUNKTE EBENFALLS ALS DIRTY MARKIEREN
                // ------------------------------------------------

                for(int i = 0; i < trailCount; ++i)
                {
                    const int x = trailX[i];
                    const int y = trailY[i];

                    if(x < 0 ||
                    x >= 240 ||
                    y < 25 ||
                    y >= 285)
                    {
                        continue;
                    }

                    if(x < dirtyMinX[y])
                        dirtyMinX[y] = x;

                    if(x > dirtyMaxX[y])
                        dirtyMaxX[y] = x;
                }

                // ------------------------------------------------
                // TRAIL ZEILENWEISE NEU ZEICHNEN
                // ------------------------------------------------

                for(int y = 25; y < 285; ++y)
                {
                    const int x0 = dirtyMinX[y];
                    const int x1 = dirtyMaxX[y];

                    // In dieser Zeile hat sich nichts geändert.
                    if(x1 < x0)
                        continue;

                    // Zeilenbuffer zunächst schwarz.
                    display.ClearLineBuffer(0x0000);

                    // Alle aktuell sichtbaren Trailpunkte dieser
                    // Bildschirmzeile in den RAM-Buffer zeichnen.
                    for(int i = 0; i < trailCount; ++i)
                    {
                        if(trailY[i] != y)
                            continue;

                        const int x = trailX[i];

                        if(x < 0 || x >= 240)
                            continue;

                        const float age =
                            trailCount > 1
                                ? static_cast<float>(i)
                                    / static_cast<float>(
                                        trailCount - 1)
                                : 1.0f;

                        uint16_t color;

                        if(age < 0.25f)
                        {
                            color = 0x2104;
                        }
                        else if(age < 0.50f)
                        {
                            color = 0x39E7;
                        }
                        else if(age < 0.75f)
                        {
                            color = 0x7BEF;
                        }
                        else
                        {
                            color = 0xFFFF;
                        }

                        display.SetLinePixel(
                            x,
                            color);
                    }

                    // Nur den tatsächlich veränderten Teil
                    // dieser Zeile zum ST7789 schicken.
                    display.FlushLineBuffer(
                        y,
                        x0,
                        x1);
                }

                // =================================================
                // PATTERN MARKER
                // =================================================

                markerDisplayDivider++;

                const bool updateMarkerPositions =
                    markerDisplayDivider >= 3;

                if(updateMarkerPositions)
                {
                    markerDisplayDivider = 0;
                }

                const int markerCount = patternEngine.GetMode() == PatternMode::Live ? 0 : patternEngine.GetCapturedSteps();

                const int activeStep =
                    patternEngine.GetMode()
                        == PatternMode::Loop
                        ? patternEngine
                              .GetLastPlayedPosition()
                              - 1
                        : -1;

                // ------------------------------------------------
                // MARKER-POSITIONEN NUR REDUZIERT AKTUALISIEREN
                // ------------------------------------------------
                //
                // Die 3D-Projektion der Capture-Marker muss nicht
                // in jedem Display-Frame neu berechnet werden.
                // Alte Positionen werden nur gelöscht, wenn der
                // Marker wirklich auf ein anderes Pixel wandert.
                // ------------------------------------------------

                if(updateMarkerPositions)
                {
                    for(int i = 0;
                        i < PatternEngine::MaxPatternLength;
                        ++i)
                    {
                        const bool currentValid =
                            i < markerCount;

                        if(!currentValid)
                        {
                            if(previousMarkerValid[i])
                            {
                                display.FillRect(
                                    previousMarkerX[i]-1,
                                    previousMarkerY[i]-1,
                                    3,
                                    3,
                                    0x0000);
                            }

                            previousMarkerValid[i] = false;
                            continue;
                        }

                        const ChaosStep& step =
                            patternEngine.GetStep(i);

                        const int markerX =
                            StepToX(step);

                        const int markerY =
                            StepToY(step);

                        const bool onScreen =
                            markerX >= 1 &&
                            markerX < 239 &&
                            markerY >= 26 &&
                            markerY < 284;

                        if(!onScreen)
                        {
                            if(previousMarkerValid[i])
                            {
                                display.FillRect(
                                    previousMarkerX[i]-1,
                                    previousMarkerY[i]-1,
                                    3,
                                    3,
                                    0x0000);
                            }

                            previousMarkerValid[i] = false;
                            continue;
                        }

                        if(previousMarkerValid[i] &&
                           (previousMarkerX[i] != markerX ||
                            previousMarkerY[i] != markerY))
                        {
                            display.FillRect(
                                previousMarkerX[i]-1,
                                previousMarkerY[i]-1,
                                3,
                                3,
                                0x0000);
                        }

                        previousMarkerX[i] = markerX;
                        previousMarkerY[i] = markerY;
                        previousMarkerValid[i] = true;
                    }
                }

                // ------------------------------------------------
                // MARKER IN JEDEM 3D-FRAME ZULETZT ZEICHNEN
                // ------------------------------------------------
                //
                // Der Trail kann die Marker beim Löschen/Neuzeichnen
                // übermalen. Deshalb werden die gespeicherten Marker
                // in jedem Display-Frame wieder ganz zum Schluss
                // darübergelegt. So bleiben sie sichtbar, obwohl ihre
                // Position nur jedes dritte Display-Frame aktualisiert
                // wird.
                // ------------------------------------------------

                for(int i = 0;
                    i < PatternEngine::MaxPatternLength;
                    ++i)
                {
                    if(!previousMarkerValid[i])
                    {
                        continue;
                    }

                    const uint16_t color =
                        i == activeStep
                            ? 0xF800   // rot: aktuell gespielter Step
                            : 0xFFE0;  // Capture-Step: gelb

                    display.FillRect(
                        previousMarkerX[i]-1,
                        previousMarkerY[i]-1,
                        3,
                        3,
                        color);
                }
            }
        }

        // ========================================================
        // CLOCK
        // ========================================================

        const uint32_t now =
            System::GetNow();

        const uint32_t elapsedMs =
            now -
            lastClockTime;

        lastClockTime =
            now;

        if(clockEngine.Tick(
            static_cast<double>(
                elapsedMs)))
        {
            const ChaosStep liveStep(
                output.A,
                output.B,
                output.C,
                output.D);

            // Modus merken, bevor Next() eventuell beim
            // letzten Capture-Step automatisch zu LOOP wechselt.
            const PatternMode modeBeforeStep =
                patternEngine.GetMode();

            const ChaosStep patternStep =
                patternEngine.Next(
                    liveStep);

            char status[32];

            // ----------------------------------------------------
            // CAPTURE
            // ----------------------------------------------------

            if(modeBeforeStep ==
               PatternMode::Capturing)
            {
                const int captured =
                    patternEngine
                        .GetCapturedSteps();

                const int length =
                    patternEngine
                        .GetLength();

                snprintf(
                    status,
                    sizeof(status),
                    "CAP %d/%d",
                    captured,
                    length);

                display.DrawStepProgress(
                    4,
                    291,
                    232,
                    5,
                    captured,
                    length);
            }

            // ----------------------------------------------------
            // LOOP
            // ----------------------------------------------------

            else if(modeBeforeStep ==
                    PatternMode::Loop)
            {
                const int currentStep =
                    patternEngine
                        .GetLastPlayedPosition();

                const int length =
                    patternEngine
                        .GetLength();

                display.DrawLoopPosition(
                    4,
                    291,
                    232,
                    5,
                    currentStep,
                    length,
                    patternEngine
                        .WasLastStepMutated());

                snprintf(
                    status,
                    sizeof(status),
                    "LOOP %d/%d",
                    currentStep,
                    length);
            }

            // ----------------------------------------------------
            // LIVE
            // ----------------------------------------------------

            else
            {
                // Step-Bar nur beim Wechsel nach LIVE löschen
    if(strcmp(lastStatus, "LIVE") != 0)
    {
        display.FillRect(
            4,
            291,
            232,
            5,
            0x0000);
    }

    snprintf(
        status,
        sizeof(status),
        "LIVE");
            }

            // ----------------------------------------------------
            // STATUS
            // ----------------------------------------------------

            if(strcmp(
                status,
                lastStatus) != 0)
            {
                display.FillRect(
                    0,
                    302,
                    120,
                    12,
                    0x0000);

                display.DrawText(
                    4,
                    305,
                    status,
                    0xFFFF);

                snprintf(
                    lastStatus,
                    sizeof(lastStatus),
                    "%s",
                    status);
            }

            // ----------------------------------------------------
            // SERIAL DEBUG
            // ----------------------------------------------------

            hardware.PrintLine(
                "STEP %d/%d  A=" FLT_FMT(3)
                " B=" FLT_FMT(3)
                " C=" FLT_FMT(3)
                " D=" FLT_FMT(3),
                patternEngine
                    .GetLastPlayedPosition(),
                patternEngine
                    .GetLength(),
                FLT_VAR(
                    3,
                    patternStep.A),
                FLT_VAR(
                    3,
                    patternStep.B),
                FLT_VAR(
                    3,
                    patternStep.C),
                FLT_VAR(
                    3,
                    patternStep.D));
        }

        // --------------------------------------------------------
        // LIVE STATUS REFRESH
        // --------------------------------------------------------

        statusRefreshCounter++;

        if(statusRefreshCounter >= 100)
        {
            statusRefreshCounter = 0;

            if(patternEngine.GetMode() ==
               PatternMode::Live)
            {
                display.DrawText(
                    4,
                    305,
                    "LIVE",
                    0xFFFF);
            }
        }

        // --------------------------------------------------------
        // MAIN LOOP DELAY
        // --------------------------------------------------------

        hardware.DelayMs(5);
    }
}