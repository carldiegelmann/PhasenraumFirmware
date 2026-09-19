#pragma once

#include "ChaosStep.h"

#include <cstdint>
#include <cstddef>

enum class PatternMode
{
    Live,
    Capturing,
    Loop
};

class PatternEngine
{
  public:
    static constexpr int MaxPatternLength = 16;

    PatternEngine()
    {
        Reset();
    }

    void Reset()
    {
        mode_ = PatternMode::Live;

        length_ = MaxPatternLength;

        captureIndex_ = 0;
        playIndex_ = 0;

        captureIndex_ = 0;
        playIndex_ = 0;
        lastPlayedIndex_ = -1;    

        mutation_ = 0.0;

        // Deterministischer Startwert für Loop-Mutation.
        mutationState_ = 0x123456789ABCDEF0ULL;

        for(int i = 0; i < MaxPatternLength; i++)
        {
            pattern_[i] = ChaosStep();
        }
    }

    // ------------------------------------------------------------
    // MODES
    // ------------------------------------------------------------

    void Capture()
    {
        captureIndex_ = 0;
        playIndex_ = 0;

        mode_ = PatternMode::Capturing;
    }

    void GoLive()
    {
        mode_ = PatternMode::Live;
        playIndex_ = 0;
    }

    void GoLoop()
    {
        // Nur sinnvoll, wenn mindestens ein Step vorhanden ist.
        if(captureIndex_ > 0)
        {
            mode_ = PatternMode::Loop;
            playIndex_ = 0;
        }
    }

    PatternMode GetMode() const
    {
        return mode_;
    }

    // ------------------------------------------------------------
    // MAIN STEP FUNCTION
    // ------------------------------------------------------------

    ChaosStep Next(const ChaosStep& liveStep)
{
    lastStepMutated_ = false;

    switch(mode_)
    {
        case PatternMode::Live:
            return liveStep;

        case PatternMode::Capturing:
            return CaptureNext(liveStep);

        case PatternMode::Loop:
            return LoopNext(liveStep);

        default:
            return liveStep;
    }
}

    // ------------------------------------------------------------
    // LENGTH
    // ------------------------------------------------------------

    void SetLength(int length)
    {
        if(length < 1)
            length = 1;

        if(length > MaxPatternLength)
            length = MaxPatternLength;

        length_ = length;

        if(playIndex_ >= length_)
            playIndex_ = 0;

        if(captureIndex_ > length_)
            captureIndex_ = length_;
    }

    int GetLength() const
    {
        return length_;
    }

    // ------------------------------------------------------------
    // CAPTURE STATUS
    // ------------------------------------------------------------

    int GetCapturedSteps() const
    {
        return captureIndex_;
    }

    bool IsCaptureComplete() const
    {
        return captureIndex_ >= length_;
    }

    // ------------------------------------------------------------
    // LOOP MUTATION
    // ------------------------------------------------------------

    void SetMutation(double value)
    {
        mutation_ = Clamp01(value);
    }

    double GetMutation() const
    {
        return mutation_;
    }

    // Optional: reproduzierbare Mutation-Sequenz einstellen.
    void SetMutationSeed(uint64_t seed)
    {
        mutationState_ = seed;

        if(mutationState_ == 0)
            mutationState_ = 0x123456789ABCDEF0ULL;
    }

    // ------------------------------------------------------------
    // DEBUG / INSPECTION
    // ------------------------------------------------------------

    int GetPlayIndex() const
    {
        return playIndex_;
    }

    const ChaosStep& GetStep(int index) const
    {
        if(index < 0)
            index = 0;

        if(index >= MaxPatternLength)
            index = MaxPatternLength - 1;

        return pattern_[index];
    }

    int GetLastPlayedIndex() const
    {
        return lastPlayedIndex_;
    }

    int GetLastPlayedPosition() const
    {
        return lastPlayedIndex_ + 1;
    }

    bool WasLastStepMutated() const
    {
        return lastStepMutated_;
    }

  private:
    ChaosStep pattern_[MaxPatternLength];

    PatternMode mode_ = PatternMode::Live;

    int length_ = MaxPatternLength;

    int captureIndex_ = 0;
    int playIndex_ = 0;
    int lastPlayedIndex_ = -1;

    double mutation_ = 0.0;

    bool lastStepMutated_ = false;

    uint64_t mutationState_ = 0x123456789ABCDEF0ULL;



    // ------------------------------------------------------------
    // CAPTURE
    // ------------------------------------------------------------

    ChaosStep CaptureNext(const ChaosStep& liveStep)
    {
        if(captureIndex_ < length_)
        {
            pattern_[captureIndex_] = liveStep;
            captureIndex_++;
        }

        // Sobald alle Steps aufgenommen wurden,
        // automatisch in LOOP wechseln.
        if(captureIndex_ >= length_)
        {
            mode_ = PatternMode::Loop;
            playIndex_ = 0;
        }

        // Während Capture hören/sehen wir weiterhin
        // den aktuellen Live-Step.
        return liveStep;
    }

    // ------------------------------------------------------------
    // LOOP
    // ------------------------------------------------------------

    ChaosStep LoopNext(const ChaosStep& liveStep)
    {
        // Falls aus irgendeinem Grund nichts aufgenommen wurde.
        if(captureIndex_ <= 0)
        {
            return liveStep;
        }

        // Falls z.B. nach Capture die Length kleiner gemacht wurde.
        int effectiveLength = length_;

        if(effectiveLength > captureIndex_)
            effectiveLength = captureIndex_;

        if(effectiveLength < 1)
            return liveStep;

        if(playIndex_ >= effectiveLength)
            playIndex_ = 0;

        // --------------------------------------------------------
        // EVOLVE:
        //
        // Mit Wahrscheinlichkeit "mutation_" wird der gespeicherte
        // Step durch den aktuellen Zustand des weiterlaufenden
        // Lorenz-Systems ersetzt.
        // --------------------------------------------------------

        if(mutation_ > 0.0)
        {
            const double randomValue = NextRandom01();

            if(randomValue < mutation_)
            {
                pattern_[playIndex_] = liveStep;
                lastStepMutated_ = true;
            }
        }

        lastPlayedIndex_ = playIndex_;

        const ChaosStep result = pattern_[playIndex_];

        playIndex_++;

        if(playIndex_ >= effectiveLength)
            playIndex_ = 0;

        return result;
    }

    // ------------------------------------------------------------
    // RANDOM
    // ------------------------------------------------------------

    double NextRandom01()
    {
        const uint64_t value = SplitMix64(mutationState_);

        // obere 53 Bit -> double 0..1
        const uint64_t mantissa = value >> 11;

        return static_cast<double>(mantissa)
               * (1.0 / 9007199254740992.0);
    }

    static uint64_t SplitMix64(uint64_t& state)
    {
        uint64_t z = (state += 0x9E3779B97F4A7C15ULL);

        z = (z ^ (z >> 30))
            * 0xBF58476D1CE4E5B9ULL;

        z = (z ^ (z >> 27))
            * 0x94D049BB133111EBULL;

        return z ^ (z >> 31);
    }

    static double Clamp01(double value)
    {
        if(value < 0.0)
            return 0.0;

        if(value > 1.0)
            return 1.0;

        return value;
    }
};