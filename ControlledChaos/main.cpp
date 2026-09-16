#include "daisy_seed.h"
#include "FirmwareVersion.h"
#include "ChaosEngine.h"
#include "DebugConsole.h"
#include "ChaosStep.h"
#include "PatternEngine.h"
#include "ClockEngine.h"

using namespace daisy;

DaisySeed hardware;

int main(void)
{
    hardware.Init();

    hardware.StartLog(true);

    hardware.PrintLine("");
    hardware.PrintLine("===============================");
    hardware.PrintLine("CONTROLLED CHAOS");
    hardware.PrintLine("Firmware v%s", CONTROLLED_CHAOS_VERSION);
    hardware.PrintLine("===============================");

    ChaosEngine chaos;
    chaos.Init(303);
    chaos.SetSpeed(0.55);
    chaos.SetChaos(0.65);

    for(int i = 0; i < 20000; ++i)
        chaos.Tick();

    PatternEngine patternEngine;

    ClockEngine clockEngine;
    clockEngine.Init(120.0);

    DebugConsole console;
    console.Init(
        &hardware,
        &chaos,
        &patternEngine);

    hardware.PrintLine("Controlled Chaos online");

    console.Status();

    while(1)
    {
        console.Process();

        ChaosOutput output;

        if(console.IsFrozen())
        {
            output = chaos.CurrentOutput();
        }
        else
        {
            output = chaos.Tick();
        }

        if(clockEngine.Tick(5.0))
{
    const ChaosStep liveStep(
        output.A,
        output.B,
        output.C,
        output.D);

    const ChaosStep patternStep =
        patternEngine.Next(liveStep);

    hardware.PrintLine(
        "STEP %d  A=" FLT_FMT(3)
        " B=" FLT_FMT(3)
        " C=" FLT_FMT(3)
        " D=" FLT_FMT(3),
        static_cast<int>(clockEngine.GetStepCount()),
        FLT_VAR(3, patternStep.A),
        FLT_VAR(3, patternStep.B),
        FLT_VAR(3, patternStep.C),
        FLT_VAR(3, patternStep.D));
}

        hardware.DelayMs(5);
    }
}