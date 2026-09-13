#pragma once

#include "daisy_seed.h"
#include "ChaosEngine.h"
#include "PatternEngine.h"

#include <cstdlib>
#include <cstring>
#include <cstdint>


class DebugConsole
{
  public:
void Init(
    daisy::DaisySeed* hardware,
    ChaosEngine* engine,
    PatternEngine* patternEngine)
{
    hardware_ = hardware;
    chaos_ = engine;
    patternEngine_ = patternEngine;

    Instance() = this;

    hardware_->usb_handle.SetReceiveCallback(
        UsbReceiveCallback,
        daisy::UsbHandle::FS_INTERNAL);

    hardware_->Print("cc> ");
}

    void Process()
    {
        if(!lineReady_)
            return;

        char command[CommandBufferSize];

        // Kommando aus dem Empfangspuffer kopieren.
        std::strncpy(command, commandLine_, CommandBufferSize);
        command[CommandBufferSize - 1] = '\0';

        lineReady_ = false;

        ExecuteCommand(command);
        hardware_->Print("cc> ");
    }

    bool IsFrozen() const
    {
        return frozen_;
    }

    void Status()
    {
        if(hardware_ == nullptr || chaos_ == nullptr)
            return;

        const ChaosOutput output = chaos_->CurrentOutput();

        hardware_->PrintLine("");
        hardware_->PrintLine("CONTROLLED CHAOS");

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
            frozen_ ? "Frozen=YES" : "Frozen=NO");

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
            "Mutation=" FLT_FMT(3),
            FLT_VAR(3, patternEngine_->GetMutation()));

        PrintOutput(output);

        hardware_->PrintLine("");
    }

  private:
    static constexpr size_t CommandBufferSize = 64;

    PatternEngine* patternEngine_ = nullptr;

    daisy::DaisySeed* hardware_ = nullptr;
    ChaosEngine* chaos_ = nullptr;

    bool frozen_ = false;

    char receiveBuffer_[CommandBufferSize] = {};
    volatile size_t receiveLength_ = 0;

    char commandLine_[CommandBufferSize] = {};
    volatile bool lineReady_ = false;

    static DebugConsole*& Instance()
    {
        static DebugConsole* instance = nullptr;
        return instance;
    }

    static void UsbReceiveCallback(uint8_t* buffer, uint32_t* length)
    {
        DebugConsole* console = Instance();

        if(console == nullptr || buffer == nullptr || length == nullptr)
            return;

        for(uint32_t i = 0; i < *length; i++)
        {
            const char c = static_cast<char>(buffer[i]);

            // CR oder LF = Kommando abschließen.
            if(c == '\r' || c == '\n')
            {
                uint8_t newline[] = {'\r', '\n'};

                console->hardware_->usb_handle.TransmitInternal(newline, 2);

                if(console->receiveLength_ == 0)
                    continue;

                // Falls das vorherige Kommando noch nicht verarbeitet wurde,
                // werfen wir kein zweites darüber.
                if(console->lineReady_)
                {
                    console->receiveLength_ = 0;
                    continue;
                }

                const size_t length =
                    console->receiveLength_ < CommandBufferSize - 1
                        ? console->receiveLength_
                        : CommandBufferSize - 1;

                for(size_t j = 0; j < length; j++)
                {
                    console->commandLine_[j]
                        = console->receiveBuffer_[j];
                }

                console->commandLine_[length] = '\0';

                console->receiveLength_ = 0;
                console->lineReady_ = true;

                continue;
            }

            // Backspace unterstützen.
            if(c == '\b' || c == 127)
            {
                if(console->receiveLength_ > 0)
                    console->receiveLength_--;

                continue;
            }

            // Nur solange Platz im Puffer ist.
            if(console->receiveLength_ < CommandBufferSize - 1)
{
    console->receiveBuffer_[
        console->receiveLength_++] = c;

    // Zeichen zurück an das Terminal senden
    uint8_t echo = static_cast<uint8_t>(c);

    console->hardware_->usb_handle.TransmitInternal(
        &echo,
        1);
}
        }
    }

    void ExecuteCommand(char* line)
    {
        Trim(line);

        if(line[0] == '\0')
            return;

        // ---- HELP -------------------------------------------------

        if(Equals(line, "help") || Equals(line, "?"))
        {
            PrintHelp();
            return;
        }

        // ---- STATUS -----------------------------------------------

        if(Equals(line, "status") || Equals(line, "s"))
        {
            Status();
            return;
        }

        // ---- FREEZE -----------------------------------------------

        if(Equals(line, "freeze") || Equals(line, "f"))
        {
            frozen_ = true;

            hardware_->PrintLine("FREEZE ON");
            PrintOutput(chaos_->CurrentOutput());
            return;
        }

        // ---- RELEASE ----------------------------------------------

        if(Equals(line, "release"))
        {
            frozen_ = false;

            hardware_->PrintLine("FREEZE OFF");
            PrintOutput(chaos_->CurrentOutput());
            return;
        }

        // ---- MUTATE -----------------------------------------------

        if(Equals(line, "mutate") || Equals(line, "m"))
        {
            Mutate(0.75);
            return;
        }

        if(Equals(line, "capture"))
        {
            patternEngine_->Capture();

            hardware_->PrintLine("Pattern CAPTURE");
            return;
        }

        if(Equals(line, "loop"))
        {
            patternEngine_->GoLoop();

            hardware_->PrintLine("Pattern LOOP");
            return;
        }

        if(Equals(line, "live"))
        {
            patternEngine_->GoLive();

            hardware_->PrintLine("Pattern LIVE");
            return;
        }


        if(StartsWith(line, "mutate "))
        {
            double value;

            if(ParseDouble(line + 7, value))
            {
                value = Clamp01(value);
                Mutate(value);
            }
            else
            {
                hardware_->PrintLine("ERR invalid mutation value");
            }

            return;
        }

        // ---- RESET ------------------------------------------------

        if(Equals(line, "reset") || Equals(line, "r"))
        {
            chaos_->Reset(303);

            hardware_->PrintLine("RESET Seed=303");
            PrintOutput(chaos_->CurrentOutput());
            return;
        }

        // ---- SEED -------------------------------------------------

        if(StartsWith(line, "seed "))
        {
            uint64_t seed;

            if(ParseUInt64(line + 5, seed))
            {
                chaos_->Reset(seed);

                hardware_->PrintLine(
                    "Seed=%d",
                    static_cast<int>(seed));

                PrintOutput(chaos_->CurrentOutput());
            }
            else
            {
                hardware_->PrintLine("ERR invalid seed");
            }

            return;
        }

        // ---- CHAOS ------------------------------------------------

        if(StartsWith(line, "chaos "))
        {
            double value;

            if(ParseDouble(line + 6, value))
            {
                value = Clamp01(value);

                chaos_->SetChaos(value);

                hardware_->PrintLine(
                    "Chaos=" FLT_FMT(3),
                    FLT_VAR(3, value));
            }
            else
            {
                hardware_->PrintLine("ERR invalid chaos value");
            }

            return;
        }

        // ---- SPEED ------------------------------------------------

        if(StartsWith(line, "speed "))
        {
            double value;

            if(ParseDouble(line + 6, value))
            {
                value = Clamp01(value);

                chaos_->SetSpeed(value);

                hardware_->PrintLine(
                    "Speed=" FLT_FMT(3),
                    FLT_VAR(3, value));
            }
            else
            {
                hardware_->PrintLine("ERR invalid speed value");
            }

            return;
        }

        // ---- PATTERN ----------------------------------------------
            
        if(strncmp(line, "length ", 7) == 0)
        {
            const int value = static_cast<int>(
                std::strtol(line + 7, nullptr, 10));

            patternEngine_->SetLength(value);

            hardware_->PrintLine(
                "Length=%d",
                patternEngine_->GetLength());

            return;
        }

        if(strncmp(line, "mutation ", 9) == 0)
        {
            const double value = std::strtod(
                line + 9,
                nullptr);

            patternEngine_->SetMutation(value);

            hardware_->PrintLine(
                "Mutation=" FLT_FMT(3),
                FLT_VAR(3, patternEngine_->GetMutation()));

            return;
        }



        // ---- UNKNOWN ----------------------------------------------

        hardware_->PrintLine(
            "ERR unknown command: %s",
            line);
    }

    void Mutate(double amount)
    {
        chaos_->Mutate(amount);

        hardware_->PrintLine(
            "MUTATE amount=" FLT_FMT(3),
            FLT_VAR(3, amount));

        PrintOutput(chaos_->CurrentOutput());
    }

    void PrintHelp()
    {
        hardware_->PrintLine("");
        hardware_->PrintLine("CONTROLLED CHAOS COMMANDS");
        hardware_->PrintLine("-------------------------");
        hardware_->PrintLine("help");
        hardware_->PrintLine("status");
        hardware_->PrintLine("chaos <0..1>");
        hardware_->PrintLine("speed <0..1>");
        hardware_->PrintLine("seed <number>");
        hardware_->PrintLine("freeze");
        hardware_->PrintLine("release");
        hardware_->PrintLine("mutate");
        hardware_->PrintLine("mutate <0..1>");
        hardware_->PrintLine("reset");
        hardware_->PrintLine("");
    }

    void PrintOutput(const ChaosOutput& output)
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

    static bool Equals(const char* a, const char* b)
    {
        return std::strcmp(a, b) == 0;
    }

    static bool StartsWith(const char* text, const char* prefix)
    {
        return std::strncmp(
                   text,
                   prefix,
                   std::strlen(prefix))
               == 0;
    }

    static bool ParseDouble(const char* text, double& value)
    {
        if(text == nullptr || *text == '\0')
            return false;

        char* end = nullptr;

        value = std::strtod(text, &end);

        if(end == text)
            return false;

        while(*end == ' ')
            end++;

        return *end == '\0';
    }

    static bool ParseUInt64(const char* text, uint64_t& value)
    {
        if(text == nullptr || *text == '\0')
            return false;

        char* end = nullptr;

        const unsigned long long parsed =
            std::strtoull(text, &end, 10);

        if(end == text)
            return false;

        while(*end == ' ')
            end++;

        if(*end != '\0')
            return false;

        value = static_cast<uint64_t>(parsed);

        return true;
    }

    static double Clamp01(double value)
    {
        if(value < 0.0)
            return 0.0;

        if(value > 1.0)
            return 1.0;

        return value;
    }

    static void Trim(char* text)
    {
        if(text == nullptr)
            return;

        // Leerzeichen vorne entfernen.
        char* start = text;

        while(*start == ' ' || *start == '\t')
            start++;

        if(start != text)
        {
            std::memmove(
                text,
                start,
                std::strlen(start) + 1);
        }

        // Leerzeichen hinten entfernen.
        size_t length = std::strlen(text);

        while(length > 0
              && (text[length - 1] == ' '
                  || text[length - 1] == '\t'))
        {
            text[length - 1] = '\0';
            length--;
        }
    }

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