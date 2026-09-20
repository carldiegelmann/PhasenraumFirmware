#pragma once

#include <cstdint>

class ClockEngine
{
public:
    void Init(double bpm = 120.0)
    {
        bpm_ = ClampBpm(bpm);
        elapsedMs_ = 0.0;
        stepCount_ = 0;
    }

    void SetBpm(double bpm)
    {
        if(bpm < 30.0)
            bpm = 30.0;

        if(bpm > 300.0)
            bpm = 300.0;

        bpm_ = bpm;
    }

    double GetBpm() const
    {
        return bpm_;
    }

    double GetStepIntervalMs() const
    {
        return (60000.0 / bpm_) / rate_;
    }

    void SetRate(double rate)
    {
        if(rate == 0.5 ||
        rate == 1.0 ||
        rate == 2.0 ||
        rate == 4.0)
        {
            rate_ = rate;
        }
    }

    double GetRate() const
    {
        return rate_;
    }

    // Call continuously with elapsed real time.
    // Returns true exactly when a new sequencer step is due.
    bool Tick(double deltaMs)
    {
        elapsedMs_ += deltaMs;

        const double interval = GetStepIntervalMs();

        if(elapsedMs_ < interval)
            return false;

        // Keep remainder instead of resetting to zero.
        // This prevents accumulated timing drift.
        elapsedMs_ -= interval;
        ++stepCount_;

        return true;
    }

    uint64_t GetStepCount() const
    {
        return stepCount_;
    }

    void Reset()
    {
        elapsedMs_ = 0.0;
        stepCount_ = 0;
    }

private:
    static double ClampBpm(double bpm)
    {
        if(bpm < 30.0)
            return 30.0;

        if(bpm > 300.0)
            return 300.0;

        return bpm;
    }

    double bpm_ = 120.0;
    double rate_ = 1.0;
    double elapsedMs_ = 0.0;
    uint64_t stepCount_ = 0;

};