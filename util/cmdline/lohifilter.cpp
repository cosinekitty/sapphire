/*
    lohifilter.cpp  -  Don Cross <cosinekitty@gmail.com>

    Verify stable behavior of LoHiPassFilter class.
*/

#include "sapphire_engine.hpp"

#include <cstdio>

int main()
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
