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

using namespace daisy;

DaisySeed hardware;

int main(void)
{
    hardware.Init();

    ST7789Display display;
    display.Init(&hardware);

    ChaosEngine chaos;
    chaos.Init(303);
    chaos.SetSpeed(0.55);
    chaos.SetChaos(0.65);

    // Attraktor erst einschwingen lassen
    for(int i = 0; i < 20000; ++i)
        chaos.Tick();

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

    hardware.DelayMs(2000);

    // Splash löschen und Betriebsansicht vorbereiten
    display.FillBlack();

    static constexpr int TrailLength = 80;

    int trailX[TrailLength];
    int trailY[TrailLength];

    int trailCount = 0;
    int displayDivider = 0;

    hardware.StartLog(false);

    hardware.PrintLine("");
    hardware.PrintLine("===============================");
    hardware.PrintLine("CONTROLLED CHAOS");
    hardware.PrintLine("Firmware v%s", CONTROLLED_CHAOS_VERSION);
    hardware.PrintLine("===============================");

    chaos.Init(303);
    chaos.SetSpeed(0.55);
    chaos.SetChaos(0.65);

    for (int i = 0; i < 20000; ++i)
        chaos.Tick();

    PatternEngine patternEngine;

    display.DrawText(4,305,"LIVE",0xFFFF);

    ClockEngine clockEngine;
    clockEngine.Init(120.0);

    DebugConsole console;
    console.Init(
        &hardware,
        &chaos,
        &patternEngine,
        &clockEngine
    );

    hardware.PrintLine("Controlled Chaos online");

    console.Status();

    int statusRefreshCounter = 0;
    char lastStatus[32] = "LIVE";
    bool lastFrozenState = false;

    auto StepToX = [](const ChaosStep& step)
    {
        return 120 + static_cast<int>(step.A * 105.0);
    };

    auto StepToY = [](const ChaosStep& step)
    {
        return 155 - static_cast<int>(step.C * 120.0);
    };

    int previousMarkerX[PatternEngine::MaxPatternLength] = {};
    int previousMarkerY[PatternEngine::MaxPatternLength] = {};
    bool previousMarkerValid[PatternEngine::MaxPatternLength] = {};
    constexpr uint16_t FrozenColor = 0x7DFF;
    uint32_t lastClockTime = System::GetNow();
    int lastDisplayedBpm = -1;
    double lastDisplayedRate = -1.0;

    while (1)
    {
        console.Process();

        const int currentBpm =
        static_cast<int>(clockEngine.GetBpm());

        if(currentBpm != lastDisplayedBpm)
        {
            char bpmText[16];

            snprintf(
                bpmText,
                sizeof(bpmText),
                "%d BPM",
                currentBpm);

            // Bereich oben rechts löschen
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

            lastDisplayedBpm = currentBpm;
        }

        const double currentRate = clockEngine.GetRate();

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
                    static_cast<int>(currentRate));
            }

            // Bereich unter der BPM-Anzeige löschen
            display.FillRect(
                214,
                15,
                26,
                9,
                0x0000);

            // Rechtsbündig unter BPM
            display.DrawText(
                228,
                16,
                rateText,
                0xFFFF);

            lastDisplayedRate = currentRate;
        }

        const bool frozen = console.IsFrozen();

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

                for(int i = 1; i < trailCount; ++i)
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
                // Trail wieder nach Alter einfärben
                for(int i = 1; i < trailCount; ++i)
                {
                    uint16_t color;

                    const double age =
                        static_cast<double>(i)
                        / static_cast<double>(trailCount);

                    if(age < 0.25)
                        color = 0x39E7;      // dunkelgrau
                    else if(age < 0.50)
                        color = 0x7BEF;      // grau
                    else if(age < 0.75)
                        color = 0xC618;      // hellgrau
                    else
                        color = 0xFFFF;      // weiß

                    display.DrawLine(
                        trailX[i - 1],
                        trailY[i - 1],
                        trailX[i],
                        trailY[i],
                        color);
                }
            }

            lastFrozenState = frozen;
        }

        ChaosOutput output;

        if (console.IsFrozen())
        {
            output = chaos.CurrentOutput();
        }
        else
        {
            output = chaos.Tick();
        }

        const int x = 120 + static_cast<int>(output.A * 105.0);

        const int y = 155 - static_cast<int>(output.C * 120.0);

        // Display nicht bei jedem Chaos-Tick aktualisieren.
        // 4 * 5 ms ≈ 20 ms -> ungefähr 50 Updates/s.
        if(!frozen)
        {
        
            displayDivider++;

            if(displayDivider >= 4)
            {
                displayDivider = 0;

                if(x >= 0 && x < 240 &&
                y >= 25 && y < 285)
                {
                    // Trail voll?
                    if(trailCount >= TrailLength)
                    {
                        display.DrawLine(
                            trailX[0],
                            trailY[0],
                            trailX[1],
                            trailY[1],
                            0x0000);

                        for(int i = 1; i < TrailLength; ++i)
                        {
                            trailX[i - 1] = trailX[i];
                            trailY[i - 1] = trailY[i];
                        }

                        trailCount = TrailLength - 1;
                    }

                    trailX[trailCount] = x;
                    trailY[trailCount] = y;
                    trailCount++;

                    if(trailCount >= 2)
                    {
                        display.DrawLine(
                            trailX[trailCount - 2],
                            trailY[trailCount - 2],
                            trailX[trailCount - 1],
                            trailY[trailCount - 1],
                            0xFFFF);
                    }

                    // Fade-Zonen
                    if(trailCount == TrailLength)
                    {
                        const int darkIndex = 20;

                        display.DrawLine(
                            trailX[darkIndex],
                            trailY[darkIndex],
                            trailX[darkIndex + 1],
                            trailY[darkIndex + 1],
                            0x39E7);

                        const int midIndex = 40;

                        display.DrawLine(
                            trailX[midIndex],
                            trailY[midIndex],
                            trailX[midIndex + 1],
                            trailY[midIndex + 1],
                            0x7BEF);

                        const int brightIndex = 60;

                        display.DrawLine(
                            trailX[brightIndex],
                            trailY[brightIndex],
                            trailX[brightIndex + 1],
                            trailY[brightIndex + 1],
                            0xC618);
                    }
                }

                // --------------------------------------------------------
                // PATTERN-MARKER IMMER ZULETZT ZEICHNEN
                // Dadurch liegen sie über dem Trail.
                // --------------------------------------------------------

                const int markerCount = patternEngine.GetCapturedSteps();

                // --------------------------------------------------------
                // 1. Alte Marker entfernen, wenn sie nicht mehr existieren
                //    oder durch Mutation an eine andere Position gewandert sind.
                // --------------------------------------------------------

                for(int i = 0; i < PatternEngine::MaxPatternLength; ++i)
                {
                    const bool currentValid =
                        i < markerCount;

                    int currentX = 0;
                    int currentY = 0;

                    if(currentValid)
                    {
                        const ChaosStep& step =
                            patternEngine.GetStep(i);

                        currentX = StepToX(step);
                        currentY = StepToY(step);
                    }

                    if(previousMarkerValid[i])
                    {
                        const bool moved =
                            !currentValid ||
                            currentX != previousMarkerX[i] ||
                            currentY != previousMarkerY[i];

                        if(moved)
                        {
                            // alten gelben Marker löschen
                            display.DrawMarker(
                                previousMarkerX[i],
                                previousMarkerY[i],
                                0x0000);
                        }
                    }
                }

                // --------------------------------------------------------
                // 2. Aktuelle Pattern-Marker zeichnen
                //
                // Gelb  = gespeicherter Step
                // Weiss = aktuell gespielter Step
                // --------------------------------------------------------

                const int activeStep =
                    patternEngine.GetMode() == PatternMode::Loop
                        ? patternEngine.GetLastPlayedPosition() - 1
                        : -1;

                for(int i = 0; i < PatternEngine::MaxPatternLength; ++i)
                {
                    if(i < markerCount)
                    {
                        const ChaosStep& step =
                            patternEngine.GetStep(i);

                        const int markerX =
                            StepToX(step);

                        const int markerY =
                            StepToY(step);

                        if(markerX >= 1 && markerX < 239 &&
                           markerY >= 26 && markerY < 284)
                        {
                            const uint16_t markerColor =
                                i == activeStep
                                    ? 0xFFFF   // Weiss: wird gerade gespielt
                                    : 0xFFE0;  // Gelb: gespeicherter Step

                            if(i == activeStep)
                            {
                                display.DrawMarker(
                                    markerX,
                                    markerY,
                                    0x001F); // Blau
                            }
                            else
                            {
                                display.DrawMarker(
                                    markerX,
                                    markerY,
                                    0xFFE0); // gelber gespeicherter Step
                            }

                            previousMarkerX[i] = markerX;
                            previousMarkerY[i] = markerY;
                            previousMarkerValid[i] = true;
                        }
                        else
                        {
                            previousMarkerValid[i] = false;
                        }
                    }
                    else
                    {
                        previousMarkerValid[i] = false;
                    }
                }
            }
        }

        const uint32_t now = System::GetNow();

        const uint32_t elapsedMs = now - lastClockTime;

        lastClockTime = now;

        if(clockEngine.Tick(static_cast<double>(elapsedMs)))
        {
            const ChaosStep liveStep(
                output.A,
                output.B,
                output.C,
                output.D);

            // Modus merken, bevor Next() beim letzten Capture-Step
            // automatisch auf LOOP umschaltet.
            const PatternMode modeBeforeStep =
                patternEngine.GetMode();

            const ChaosStep patternStep =
                patternEngine.Next(liveStep);

            if(modeBeforeStep == PatternMode::Loop &&
            patternEngine.WasLastStepMutated())
            {
                const int markerX = StepToX(patternStep);
                const int markerY = StepToY(patternStep);

                if(markerX >= 0 && markerX < 240 &&
                markerY >= 25 && markerY < 285)
                {
                    display.DrawMarker(
                        markerX,
                        markerY,
                        0xFFE0); // gelb
                }
            }

            char status[32];

            if(modeBeforeStep == PatternMode::Capturing)
            {
                const int markerX = StepToX(liveStep);
                const int markerY = StepToY(liveStep);

                if(markerX >= 0 && markerX < 240 &&
                markerY >= 25 && markerY < 285)
                {
                    display.DrawMarker(
                        markerX,
                        markerY,
                        0xFFE0);  // gelb
                }

                const int captured =
                    patternEngine.GetCapturedSteps();

                const int length =
                    patternEngine.GetLength();

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
            else if(modeBeforeStep == PatternMode::Loop)
            {
                const int currentStep =
                    patternEngine.GetLastPlayedPosition();

                const int length =
                    patternEngine.GetLength();

                display.DrawLoopPosition(
                    4,
                    291,
                    232,
                    5,
                    currentStep,
                    length,
                    patternEngine.WasLastStepMutated());

                snprintf(
                    status,
                    sizeof(status),
                    "LOOP %d/%d",
                    currentStep,
                    length);
            }
            else
            {
                snprintf(
                    status,
                    sizeof(status),
                    "LIVE");
            }

            // Status nur neu zeichnen, wenn er sich geändert hat
            if(strcmp(status, lastStatus) != 0)
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

            hardware.PrintLine(
                "STEP %d/%d  A=" FLT_FMT(3)
                " B=" FLT_FMT(3)
                " C=" FLT_FMT(3)
                " D=" FLT_FMT(3),
                patternEngine.GetLastPlayedPosition(),
                patternEngine.GetLength(),
                FLT_VAR(3, patternStep.A),
                FLT_VAR(3, patternStep.B),
                FLT_VAR(3, patternStep.C),
                FLT_VAR(3, patternStep.D));
        }

        statusRefreshCounter++;

        if(statusRefreshCounter >= 100)
        {
            statusRefreshCounter = 0;

            if(patternEngine.GetMode() == PatternMode::Live)
            {
                // Kein FillRect!
                // Nur LIVE erneut weiß darüberzeichnen.
                display.DrawText(
                    4,
                    305,
                    "LIVE",
                    0xFFFF);
            }
        }

        hardware.DelayMs(5);
    }
}