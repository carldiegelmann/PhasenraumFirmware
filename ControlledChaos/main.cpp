#include "daisy_seed.h"
#include "FirmwareVersion.h"
#include "ChaosEngine.h"
#include "DebugConsole.h"
#include "ChaosStep.h"
#include "PatternEngine.h"


using namespace daisy;

DaisySeed hardware;
ChaosEngine engine;
PatternEngine patternEngine;
DebugConsole console;

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

        const ChaosStep liveStep(
            output.A,
            output.B,
            output.C,
            output.D);

        const ChaosStep patternStep =
            patternEngine.Next(liveStep);

        hardware.SetLed(patternStep.A > 0.0);

        hardware.DelayMs(5);
    }
}