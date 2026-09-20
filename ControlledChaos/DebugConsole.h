#pragma once

#include "daisy_seed.h"
#include "ChaosEngine.h"
#include "PatternEngine.h"
#include "ClockEngine.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

class DebugConsole
{
  public:
    // ================================================================
    // INIT
    // ================================================================

    void Init(
    daisy::DaisySeed* hardware,
    ChaosEngine* chaos,
    PatternEngine* patternEngine,
    ClockEngine* clockEngine)
    {
        hardware_ = hardware;
        chaos_ = chaos;
        patternEngine_ = patternEngine;
        clockEngine_ = clockEngine;
        hardware_ = hardware;
        chaos_ = chaos;
        patternEngine_ = patternEngine;

        Instance() = this;

        hardware_->usb_handle.SetReceiveCallback(
            UsbReceiveCallback,
            daisy::UsbHandle::FS_INTERNAL);

        hardware_->Print("cc> ");
    }

    // ================================================================
    // PROCESS
    // ================================================================

    void Process()
    {
        if(!lineReady_)
            return;

        char command[CommandBufferSize];

        std::strncpy(
            command,
            commandLine_,
            CommandBufferSize);

        command[CommandBufferSize - 1] = '\0';

        lineReady_ = false;

        ExecuteCommand(command);

        hardware_->Print("cc> ");
    }

    // ================================================================
    // STATE
    // ================================================================

    bool IsFrozen() const
    {
        return frozen_;
    }

    // ================================================================
    // STATUS
    // ================================================================

    void Status()
    {
        if(hardware_ == nullptr
           || chaos_ == nullptr
           || patternEngine_ == nullptr)
        {
            return;
        }

        const ChaosOutput output =
            chaos_->CurrentOutput();

        hardware_->PrintLine("");
        hardware_->PrintLine("PHASENRAUM");
        hardware_->PrintLine("-------------------------");

        hardware_->PrintLine(
            "Seed=%d",
            static_cast<int>(chaos_->GetSeed()));

        hardware_->PrintLine(
            "Chaos=" FLT_FMT(3),
            FLT_VAR(3, chaos_->GetChaos()));

        hardware_->PrintLine(
            "Speed=" FLT_FMT(3),
            FLT_VAR(3, chaos_->GetSpeed()));

        hardware_->PrintLine(
            frozen_
                ? "Frozen=YES"
                : "Frozen=NO");

        hardware_->PrintLine(
            "Pattern=%s",
            PatternModeName());

        hardware_->PrintLine(
            "Length=%d",
            patternEngine_->GetLength());

        hardware_->PrintLine(
            "Captured=%d",
            patternEngine_->GetCapturedSteps());

        hardware_->PrintLine(
            "Mutate=" FLT_FMT(3),
            FLT_VAR(
                3,
                patternEngine_->GetMutation()));

        hardware_->PrintLine(
            "BPM=" FLT_FMT(1),
            FLT_VAR(1, clockEngine_->GetBpm()));

        hardware_->PrintLine(
            "Rate=" FLT_FMT(1),
            FLT_VAR(1, clockEngine_->GetRate()));

        PrintOutput(output);

        hardware_->PrintLine("");
    }

  private:
    // ================================================================
    // CONSTANTS / STATE
    // ================================================================

    static constexpr size_t CommandBufferSize = 64;

    daisy::DaisySeed* hardware_ = nullptr;

    ChaosEngine* chaos_ = nullptr;

    PatternEngine* patternEngine_ = nullptr;

    bool frozen_ = false;

    char receiveBuffer_[CommandBufferSize] = {};

    volatile size_t receiveLength_ = 0;

    char commandLine_[CommandBufferSize] = {};

    volatile bool lineReady_ = false;

    ClockEngine* clockEngine_ = nullptr;


    // ================================================================
    // SINGLETON ACCESS FOR USB CALLBACK
    // ================================================================

    static DebugConsole*& Instance()
    {
        static DebugConsole* instance = nullptr;

        return instance;
    }

    // ================================================================
    // USB RECEIVE
    // ================================================================

    static void UsbReceiveCallback(
        uint8_t* buffer,
        uint32_t* length)
    {
        DebugConsole* console =
            Instance();

        if(console == nullptr
           || buffer == nullptr
           || length == nullptr)
        {
            return;
        }

        for(uint32_t i = 0; i < *length; ++i)
        {
            const char c =
                static_cast<char>(buffer[i]);

            // --------------------------------------------------------
            // ENTER
            // --------------------------------------------------------

            if(c == '\r' || c == '\n')
            {
                uint8_t newline[] =
                {
                    '\r',
                    '\n'
                };

                console->hardware_
                    ->usb_handle
                    .TransmitInternal(
                        newline,
                        2);

                if(console->receiveLength_ == 0)
                    continue;

                // Ein noch nicht verarbeitetes Kommando
                // nicht überschreiben.
                if(console->lineReady_)
                {
                    console->receiveLength_ = 0;
                    continue;
                }

                const size_t commandLength =
                    console->receiveLength_
                            < CommandBufferSize - 1
                        ? console->receiveLength_
                        : CommandBufferSize - 1;

                for(size_t j = 0;
                    j < commandLength;
                    ++j)
                {
                    console->commandLine_[j] =
                        console->receiveBuffer_[j];
                }

                console
                    ->commandLine_[commandLength]
                    = '\0';

                console->receiveLength_ = 0;

                console->lineReady_ = true;

                continue;
            }

            // --------------------------------------------------------
            // BACKSPACE
            // --------------------------------------------------------

            if(c == '\b' || c == 127)
            {
                if(console->receiveLength_ > 0)
                {
                    console->receiveLength_--;
                }

                continue;
            }

            // --------------------------------------------------------
            // CHARACTER
            // --------------------------------------------------------

            if(console->receiveLength_
               < CommandBufferSize - 1)
            {
                console->receiveBuffer_[
                    console->receiveLength_++] = c;

                // Echo
                uint8_t echo =
                    static_cast<uint8_t>(c);

                console->hardware_
                    ->usb_handle
                    .TransmitInternal(
                        &echo,
                        1);
            }
        }
    }

    // ================================================================
    // COMMAND EXECUTION
    // ================================================================

    void ExecuteCommand(char* line)
    {
        Trim(line);

        if(line == nullptr || line[0] == '\0')
            return;

        // ------------------------------------------------------------
        // HELP
        // ------------------------------------------------------------

        if(Equals(line, "help")
           || Equals(line, "?"))
        {
            PrintHelp();
            return;
        }

        // ------------------------------------------------------------
        // STATUS
        // ------------------------------------------------------------

        if(Equals(line, "status")
           || Equals(line, "s"))
        {
            Status();
            return;
        }

        // ------------------------------------------------------------
        // FREEZE
        // ------------------------------------------------------------

        if(Equals(line, "freeze")
           || Equals(line, "f"))
        {
            frozen_ = true;

            hardware_->PrintLine(
                "FREEZE ON");

            PrintOutput(
                chaos_->CurrentOutput());

            return;
        }

        // ------------------------------------------------------------
        // RELEASE
        // ------------------------------------------------------------

        if(Equals(line, "release"))
        {
            frozen_ = false;

            hardware_->PrintLine(
                "FREEZE OFF");

            PrintOutput(
                chaos_->CurrentOutput());

            return;
        }

        // ------------------------------------------------------------
        // MUTATE
        //
        // mutate
        //     -> einmaliger Chaos-State-Impuls
        //
        // mutate 0.1
        //     -> Loop-Mutation auf 10 %
        // ------------------------------------------------------------

        if(Equals(line, "mutate")
           || Equals(line, "m"))
        {
            MutateChaosState(0.75);

            return;
        }

        if(StartsWith(line, "mutate "))
        {
            double value;

            if(ParseDouble(
                   line + 7,
                   value))
            {
                value =
                    Clamp01(value);

                patternEngine_
                    ->SetMutation(value);

                hardware_->PrintLine(
                    "Mutate=" FLT_FMT(3),
                    FLT_VAR(
                        3,
                        patternEngine_
                            ->GetMutation()));
            }
            else
            {
                hardware_->PrintLine(
                    "ERR invalid mutate value");
            }

            return;
        }

        // ------------------------------------------------------------
        // CAPTURE
        // ------------------------------------------------------------

        if(Equals(line, "capture"))
        {
            patternEngine_->Capture();

            hardware_->PrintLine(
                "Pattern CAPTURE");

            return;
        }

        // ------------------------------------------------------------
        // LOOP
        // ------------------------------------------------------------

        if(Equals(line, "loop"))
        {
            patternEngine_->GoLoop();

            hardware_->PrintLine(
                "Pattern LOOP");

            return;
        }

        // ------------------------------------------------------------
        // LIVE
        // ------------------------------------------------------------

        if(Equals(line, "live"))
        {
            patternEngine_->GoLive();

            hardware_->PrintLine(
                "Pattern LIVE");

            return;
        }

        // ------------------------------------------------------------
        // LENGTH
        // ------------------------------------------------------------

        if(StartsWith(line, "length "))
        {
            int value;

            if(ParseInt(
                   line + 7,
                   value))
            {
                patternEngine_
                    ->SetLength(value);

                hardware_->PrintLine(
                    "Length=%d",
                    patternEngine_
                        ->GetLength());
            }
            else
            {
                hardware_->PrintLine(
                    "ERR invalid length");
            }

            return;
        }

        // ------------------------------------------------------------
        // RESET
        // ------------------------------------------------------------

        if(Equals(line, "reset")
           || Equals(line, "r"))
        {
            chaos_->Reset(303);

            hardware_->PrintLine(
                "RESET Seed=303");

            PrintOutput(
                chaos_->CurrentOutput());

            return;
        }

        // ------------------------------------------------------------
        // SEED
        // ------------------------------------------------------------

        if(StartsWith(line, "seed "))
        {
            uint64_t seed;

            if(ParseUInt64(
                   line + 5,
                   seed))
            {
                chaos_->Reset(seed);

                hardware_->PrintLine(
                    "Seed=%d",
                    static_cast<int>(seed));

                PrintOutput(
                    chaos_->CurrentOutput());
            }
            else
            {
                hardware_->PrintLine(
                    "ERR invalid seed");
            }

            return;
        }

        // ------------------------------------------------------------
        // CHAOS
        // ------------------------------------------------------------

        if(StartsWith(line, "chaos "))
        {
            double value;

            if(ParseDouble(
                   line + 6,
                   value))
            {
                value =
                    Clamp01(value);

                chaos_->SetChaos(value);

                hardware_->PrintLine(
                    "Chaos=" FLT_FMT(3),
                    FLT_VAR(3, value));
            }
            else
            {
                hardware_->PrintLine(
                    "ERR invalid chaos value");
            }

            return;
        }

        // ------------------------------------------------------------
        // SPEED
        // ------------------------------------------------------------

        if(StartsWith(line, "speed "))
        {
            double value;

            if(ParseDouble(
                   line + 6,
                   value))
            {
                value =
                    Clamp01(value);

                chaos_->SetSpeed(value);

                hardware_->PrintLine(
                    "Speed=" FLT_FMT(3),
                    FLT_VAR(3, value));
            }
            else
            {
                hardware_->PrintLine(
                    "ERR invalid speed value");
            }

            return;
        }

        // ------------------------------------------------------------
        // BPM
        // ------------------------------------------------------------

        if(StartsWith(line, "bpm "))
        {
            double value;

            if(ParseDouble(
                line + 4,
                value))
            {
                if(value < 30.0)
                    value = 30.0;

                if(value > 300.0)
                    value = 300.0;

                clockEngine_->SetBpm(value);

                hardware_->PrintLine(
                    "BPM=" FLT_FMT(1),
                    FLT_VAR(
                        1,
                        clockEngine_->GetBpm()));
            }
            else
            {
                hardware_->PrintLine(
                    "ERR invalid bpm value");
            }

            return;
        }

        // ------------------------------------------------------------
        // RATE
        // ------------------------------------------------------------

        if(StartsWith(line, "rate "))
        {
            double value;

            if(ParseDouble(line + 5, value))
            {
                if(value == 0.5 ||
                value == 1.0 ||
                value == 2.0 ||
                value == 4.0)
                {
                    clockEngine_->SetRate(value);

                    hardware_->PrintLine(
                        "Rate=" FLT_FMT(1),
                        FLT_VAR(1, clockEngine_->GetRate()));
                }
                else
                {
                    hardware_->PrintLine(
                        "ERR rate must be 0.5, 1, 2 or 4");
                }
            }
            else
            {
                hardware_->PrintLine(
                    "ERR invalid rate value");
            }

            return;
        }

        // ------------------------------------------------------------
        // UNKNOWN
        // ------------------------------------------------------------

        hardware_->PrintLine(
            "ERR unknown command: %s",
            line);
    }

    // ================================================================
    // CHAOS MUTATE
    // ================================================================

    void MutateChaosState(double amount)
    {
        chaos_->Mutate(amount);

        hardware_->PrintLine(
            "MUTATE amount=" FLT_FMT(3),
            FLT_VAR(3, amount));

        PrintOutput(
            chaos_->CurrentOutput());
    }

    // ================================================================
    // HELP
    // ================================================================

    void PrintHelp()
    {
        hardware_->PrintLine("");
        hardware_->PrintLine(
            "PHASENRAUM COMMANDS");

        hardware_->PrintLine(
            "-------------------------");

        hardware_->PrintLine(
            "help");

        hardware_->PrintLine(
            "status");

        hardware_->PrintLine(
            "chaos <0..1>");

        hardware_->PrintLine(
            "speed <0..1>");

        hardware_->PrintLine(
            "seed <number>");

        hardware_->PrintLine(
            "freeze");

        hardware_->PrintLine(
            "release");

        hardware_->PrintLine(
            "mutate");

        hardware_->PrintLine(
            "mutate <0..1>");

        hardware_->PrintLine(
            "length <1..16>");

        hardware_->PrintLine(
            "capture");

        hardware_->PrintLine(
            "loop");

        hardware_->PrintLine(
            "live");

        hardware_->PrintLine(
            "bpm <30..300>");

        hardware_->PrintLine(
            "rate <0.5|1|2|4>");

        hardware_->PrintLine(
            "reset");

        hardware_->PrintLine("");
    }

    // ================================================================
    // OUTPUT
    // ================================================================

    void PrintOutput(
        const ChaosOutput& output)
    {
        hardware_->PrintLine(
            "A=" FLT_FMT(6)
            " B=" FLT_FMT(6)
            " C=" FLT_FMT(6)
            " D=" FLT_FMT(6),
            FLT_VAR(6, output.A),
            FLT_VAR(6, output.B),
            FLT_VAR(6, output.C),
            FLT_VAR(6, output.D));
    }

    // ================================================================
    // STRING HELPERS
    // ================================================================

    static bool Equals(
        const char* a,
        const char* b)
    {
        return std::strcmp(a, b) == 0;
    }

    static bool StartsWith(
        const char* text,
        const char* prefix)
    {
        return std::strncmp(
                   text,
                   prefix,
                   std::strlen(prefix))
               == 0;
    }

    // ================================================================
    // PARSING
    // ================================================================

    static bool ParseDouble(
        const char* text,
        double& value)
    {
        if(text == nullptr
           || *text == '\0')
        {
            return false;
        }

        char* end = nullptr;

        value =
            std::strtod(
                text,
                &end);

        if(end == text)
            return false;

        while(*end == ' ')
            ++end;

        return *end == '\0';
    }

    static bool ParseInt(
        const char* text,
        int& value)
    {
        if(text == nullptr
           || *text == '\0')
        {
            return false;
        }

        char* end = nullptr;

        const long parsed =
            std::strtol(
                text,
                &end,
                10);

        if(end == text)
            return false;

        while(*end == ' ')
            ++end;

        if(*end != '\0')
            return false;

        value =
            static_cast<int>(parsed);

        return true;
    }

    static bool ParseUInt64(
        const char* text,
        uint64_t& value)
    {
        if(text == nullptr
           || *text == '\0')
        {
            return false;
        }

        char* end = nullptr;

        const unsigned long long parsed =
            std::strtoull(
                text,
                &end,
                10);

        if(end == text)
            return false;

        while(*end == ' ')
            ++end;

        if(*end != '\0')
            return false;

        value =
            static_cast<uint64_t>(
                parsed);

        return true;
    }

    // ================================================================
    // CLAMP
    // ================================================================

    static double Clamp01(double value)
    {
        if(value < 0.0)
            return 0.0;

        if(value > 1.0)
            return 1.0;

        return value;
    }

    // ================================================================
    // TRIM
    // ================================================================

    static void Trim(char* text)
    {
        if(text == nullptr)
            return;

        char* start = text;

        while(*start == ' '
              || *start == '\t')
        {
            ++start;
        }

        if(start != text)
        {
            std::memmove(
                text,
                start,
                std::strlen(start) + 1);
        }

        size_t length =
            std::strlen(text);

        while(
            length > 0
            && (text[length - 1] == ' '
                || text[length - 1] == '\t'))
        {
            text[length - 1] = '\0';

            --length;
        }
    }

    // ================================================================
    // PATTERN MODE
    // ================================================================

    const char* PatternModeName() const
    {
        switch(patternEngine_->GetMode())
        {
            case PatternMode::Live:
                return "LIVE";

            case PatternMode::Capturing:
                return "CAPTURE";

            case PatternMode::Loop:
                return "LOOP";

            default:
                return "?";
        }
    }
};