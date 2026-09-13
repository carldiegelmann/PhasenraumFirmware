using System;

namespace ChaosReference;

internal static class Program
{
    private static void Main()
    {
        var chaos = new ChaosEngine
        {
            Speed = 0.55,
            Chaos = 0.65
        };

        chaos.Reset(303);

        for (var i = 0; i < 20000; i++)
            chaos.Tick();

        for (var i = 0; i < 10; i++)
        {
            var output = chaos.Tick();

            Console.WriteLine(
                $"{i}," +
                $"{output.A:F9}," +
                $"{output.B:F9}," +
                $"{output.C:F9}," +
                $"{output.D:F9}");
        }
    }
}

internal readonly record struct ChaosOutput(
    double A,
    double B,
    double C,
    double D);

internal sealed class ChaosEngine
{
    private const double Sigma = 10.0;
    private const double Beta = 8.0 / 3.0;
    private const double BaseDt = 0.001;

    private double _x;
    private double _y;
    private double _z;

    public double Speed { get; set; } = 0.5;
    public double Chaos { get; set; } = 0.5;

    public void Reset(ulong seed)
    {
        var state = seed;

        _x = RandomSigned(ref state) * 5.0;
        _y = RandomSigned(ref state) * 5.0;
        _z = 20.0 + RandomSigned(ref state) * 5.0;
    }

    public ChaosOutput Tick()
    {
        var rho = ChaosToRho(Chaos);
        var dt = BaseDt * SpeedToMultiplier(Speed);

        StepRungeKutta4(dt, rho);

        return CurrentOutput();
    }

    public ChaosOutput CurrentOutput()
    {
        var rho = ChaosToRho(Chaos);
        var zCentered = _z - (rho - 1.0);

        var nx = _x / 18.0;
        var ny = _y / 22.0;
        var nz = zCentered / 20.0;

        return new ChaosOutput(
            Math.Tanh(nx),
            Math.Tanh(0.65 * ny - 0.45 * nz),
            Math.Tanh(0.70 * nz + 0.30 * nx),
            Math.Tanh(0.55 * nx - 0.65 * ny + 0.55 * nz)
        );
    }

    private static double ChaosToRho(double chaos)
        => 20.0 + chaos * 15.0;

    private static double SpeedToMultiplier(double speed)
    {
        var exponent = -4.0 + speed * 7.0;
        return Math.Pow(2.0, exponent);
    }

    private static ulong SplitMix64(ref ulong state)
    {
        var z = state += 0x9E3779B97F4A7C15UL;

        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9UL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBUL;

        return z ^ (z >> 31);
    }

    private static double RandomSigned(ref ulong state)
    {
        var value = SplitMix64(ref state);

        var normalized =
            (value >> 11) *
            (1.0 / 9007199254740992.0);

        return normalized * 2.0 - 1.0;
    }

    private static void Derivatives(
        double x,
        double y,
        double z,
        double rho,
        out double dx,
        out double dy,
        out double dz)
    {
        dx = Sigma * (y - x);
        dy = x * (rho - z) - y;
        dz = x * y - Beta * z;
    }

    private void StepRungeKutta4(double dt, double rho)
    {
        Derivatives(
            _x,
            _y,
            _z,
            rho,
            out var k1x,
            out var k1y,
            out var k1z);

        Derivatives(
            _x + 0.5 * dt * k1x,
            _y + 0.5 * dt * k1y,
            _z + 0.5 * dt * k1z,
            rho,
            out var k2x,
            out var k2y,
            out var k2z);

        Derivatives(
            _x + 0.5 * dt * k2x,
            _y + 0.5 * dt * k2y,
            _z + 0.5 * dt * k2z,
            rho,
            out var k3x,
            out var k3y,
            out var k3z);

        Derivatives(
            _x + dt * k3x,
            _y + dt * k3y,
            _z + dt * k3z,
            rho,
            out var k4x,
            out var k4y,
            out var k4z);

        _x += dt / 6.0 *
              (k1x + 2.0 * k2x + 2.0 * k3x + k4x);

        _y += dt / 6.0 *
              (k1y + 2.0 * k2y + 2.0 * k3y + k4y);

        _z += dt / 6.0 *
              (k1z + 2.0 * k2z + 2.0 * k3z + k4z);
    }
}