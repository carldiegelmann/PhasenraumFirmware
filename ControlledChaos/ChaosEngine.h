#pragma once

#include <cmath>
#include <cstdint>

struct ChaosOutput
{
    double A;
    double B;
    double C;
    double D;
};

class ChaosEngine
{
  public:
    void Init(uint64_t seed)
    {
        seed_ = seed;
        Reset(seed);
    }

    void SetSpeed(double value)
    {
        speed_ = Clamp01(value);
    }

    void SetChaos(double value)
    {
        chaos_ = Clamp01(value);
    }

    double GetSpeed() const
    {
        return speed_;
    }

    double GetChaos() const
    {
        return chaos_;
    }

    uint64_t GetSeed() const
    {
        return seed_;
    }


    void Reset(uint64_t seed)
    {
        seed_ = seed;

        uint64_t state = seed_;

        x_ = RandomSigned(state) * 5.0;
        y_ = RandomSigned(state) * 5.0;
        z_ = 20.0 + RandomSigned(state) * 5.0;
    }

    void Mutate(double amount)
    {
        amount = Clamp01(amount);

        const double epsilon =
            std::pow(10.0, -3.0 + amount * 3.3);

        x_ += RandomSigned(mutationState_) * epsilon;
        y_ += RandomSigned(mutationState_) * epsilon;
        z_ += RandomSigned(mutationState_) * epsilon;
    }

    ChaosOutput Tick()
    {
        const double rho = ChaosToRho(chaos_);
        const double dt  = BaseDt * SpeedToMultiplier(speed_);

        StepRungeKutta4(dt, rho);

        return CurrentOutput();
    }

    ChaosOutput CurrentOutput() const
    {
        const double rho       = ChaosToRho(chaos_);
        const double zCentered = z_ - (rho - 1.0);

        const double nx = x_ / 18.0;
        const double ny = y_ / 22.0;
        const double nz = zCentered / 20.0;

        return {
            std::tanh(nx),
            std::tanh(0.65 * ny - 0.45 * nz),
            std::tanh(0.70 * nz + 0.30 * nx),
            std::tanh(0.55 * nx - 0.65 * ny + 0.55 * nz)
        };
    }

  private:
    static constexpr double Sigma  = 10.0;
    static constexpr double Beta   = 8.0 / 3.0;
    static constexpr double BaseDt = 0.001;

    double x_ = 0.0;
    double y_ = 0.0;
    double z_ = 0.0;

    uint64_t seed_          = 1;
    uint64_t mutationState_ = 0x123456789ABCDEF0ULL;

    double speed_ = 0.55;
    double chaos_ = 0.65;

    static double Clamp01(double value)
    {
        return value < 0.0
            ? 0.0
            : (value > 1.0 ? 1.0 : value);
    }

    static double ChaosToRho(double chaos)
    {
        return 20.0 + chaos * 15.0;
    }

    static double SpeedToMultiplier(double speed)
    {
        const double exponent = -4.0 + speed * 7.0;
        return std::pow(2.0, exponent);
    }

    static uint64_t SplitMix64(uint64_t& state)
    {
        uint64_t z =
            (state += 0x9E3779B97F4A7C15ULL);

        z = (z ^ (z >> 30))
            * 0xBF58476D1CE4E5B9ULL;

        z = (z ^ (z >> 27))
            * 0x94D049BB133111EBULL;

        return z ^ (z >> 31);
    }

    static double RandomSigned(uint64_t& state)
    {
        const uint64_t value =
            SplitMix64(state);

        const double normalized =
            static_cast<double>(value >> 11)
            * (1.0 / 9007199254740992.0);

        return normalized * 2.0 - 1.0;
    }

    static void Derivatives(
        double x,
        double y,
        double z,
        double rho,
        double& dx,
        double& dy,
        double& dz)
    {
        dx = Sigma * (y - x);
        dy = x * (rho - z) - y;
        dz = x * y - Beta * z;
    }

    void StepRungeKutta4(double dt, double rho)
    {
        double k1x, k1y, k1z;
        double k2x, k2y, k2z;
        double k3x, k3y, k3z;
        double k4x, k4y, k4z;

        Derivatives(
            x_,
            y_,
            z_,
            rho,
            k1x,
            k1y,
            k1z);

        Derivatives(
            x_ + 0.5 * dt * k1x,
            y_ + 0.5 * dt * k1y,
            z_ + 0.5 * dt * k1z,
            rho,
            k2x,
            k2y,
            k2z);

        Derivatives(
            x_ + 0.5 * dt * k2x,
            y_ + 0.5 * dt * k2y,
            z_ + 0.5 * dt * k2z,
            rho,
            k3x,
            k3y,
            k3z);

        Derivatives(
            x_ + dt * k3x,
            y_ + dt * k3y,
            z_ + dt * k3z,
            rho,
            k4x,
            k4y,
            k4z);

        x_ += dt / 6.0
              * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);

        y_ += dt / 6.0
              * (k1y + 2.0 * k2y + 2.0 * k3y + k4y);

        z_ += dt / 6.0
              * (k1z + 2.0 * k2z + 2.0 * k3z + k4z);
    }
};