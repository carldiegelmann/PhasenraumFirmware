#include "daisy_seed.h"
#include "FirmwareVersion.h"
#include "ChaosEngine.h"
#include "DebugConsole.h"
#include "ChaosStep.h"
#include "PatternEngine.h"
#include "ClockEngine.h"
#include "ST7789Display.h"
#include <cstdio>

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
    for (int i = 0; i < 20000; ++i)
        chaos.Tick();

    display.FillBlack();

    display.DrawText(
        4,
        4,
        "PHASENRAUM",
        0xFFFF);

    display.DrawVersion(200,4,CONTROLLED_CHAOS_VERSION,0xFFFF);

    // Ein einzelner weißer Pixel ziemlich genau in der Mitte
    // Erster echter PHASENRAUM-Plot
    int previousX = -1;
    int previousY = -1;

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

    display.DrawText(
        4,
        305,
        "LIVE",
        0xFFFF);

    ClockEngine clockEngine;
    clockEngine.Init(120.0);

    DebugConsole console;
    console.Init(
        &hardware,
        &chaos,
        &patternEngine);

    hardware.PrintLine("Controlled Chaos online");

    console.Status();

    while (1)
    {
        console.Process();

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

        const int y = 160 - static_cast<int>(output.C * 145.0);

        if (x >= 0 && x < 240 && y >= 0 && y < 320)
        {
            if (previousX >= 0 && previousY >= 0)
            {
                display.DrawLine(
                    previousX,
                    previousY,
                    x,
                    y,
                    0xFFFF);
            }

            previousX = x;
            previousY = y;
        }

        if (clockEngine.Tick(5.0))
        {
            const ChaosStep liveStep(
                output.A,
                output.B,
                output.C,
                output.D);

            const ChaosStep patternStep = patternEngine.Next(liveStep);

            // Alte Statuszeile löschen
            display.FillRect(
                0,
                302,
                120,
                12,
                0x0000);

            
            display.DrawText(
                4,
                305,
                "LIVE",
                0xFFFF);
            
            if(patternEngine.GetMode() == PatternMode::Loop)
            {
                char status[32];

                snprintf(
                    status,
                    sizeof(status),
                    "LOOP %d/%d",
                    patternEngine.GetLastPlayedPosition(),
                    patternEngine.GetLength());

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
            }

            hardware.PrintLine(
                "STEP %d/%d  A=" FLT_FMT(3) " B=" FLT_FMT(3) " C=" FLT_FMT(3) " D=" FLT_FMT(3),
                patternEngine.GetLastPlayedPosition(),
                patternEngine.GetLength(),
                FLT_VAR(3, patternStep.A),
                FLT_VAR(3, patternStep.B),
                FLT_VAR(3, patternStep.C),
                FLT_VAR(3, patternStep.D));
        }

        hardware.DelayMs(5);
    }
}