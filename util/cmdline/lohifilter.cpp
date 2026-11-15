/*
    lohifilter.cpp  -  Don Cross <cosinekitty@gmail.com>

    Verify stable behavior of LoHiPassFilter class.
*/

#include "sapphire_engine.hpp"

#include <cstdio>

static int TestLoHi()
{
    using filter_t = Sapphire::LoHiPassFilter<double>;
    const int SAMPLE_RATE = 44100;
    const int NSAMPLES = 300;

    filter_t filter;
    filter.SetCutoffFrequency(1000);

    for (int i = 0; i < NSAMPLES; ++i)
    {
        double x = ((i/37) & 1) ? -1.0 : +1.0;
        filter.Update(x, SAMPLE_RATE);
        double lo = filter.LoPass();
        double hi = filter.HiPass();
        printf("i = %3d, x = %4.1lf, lo = %19.16lf, hi = %19.16lf\n", i, x, lo, hi);
    }

    return 0;
}


static int TestSnap()
{
    using filter_t = Sapphire::StagedFilter<double, 3>;

    // Confirm we can completely reject a step function by "snapping" to it.
    filter_t filter;

    const float sampleRate = 48000;
    const double xStep = 1.0;
    filter.SnapHiPass(xStep);
    double y = filter.UpdateHiPass(xStep, sampleRate);
    printf("TestSnap: y = %0.16lg\n", y);
    if (std::abs(y) > 1.0e-16)
    {
        printf("EXCESSIVE SNAP ERROR\n");
        return 1;
    }
    return 0;
}


int main()
{
    return TestLoHi() || TestSnap();
}
