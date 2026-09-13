#pragma once

struct ChaosStep
{
    double A = 0.0;
    double B = 0.0;
    double C = 0.0;
    double D = 0.0;

    ChaosStep() = default;

    ChaosStep(double a, double b, double c, double d)
        : A(a),
          B(b),
          C(c),
          D(d)
    {
    }
};