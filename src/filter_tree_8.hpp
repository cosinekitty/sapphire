#pragma once
namespace Sapphire
{
    // filter[0]: corner=632.4555320336759
    // filter[1]: corner=112.46826503806982, input=0, select=0
    // filter[2]: corner=47.4274741132331, input=1, select=0
    // filter[3]: corner=266.70428643266484, input=1, select=1
    // filter[4]: corner=3556.5588200778457, input=0, select=1
    // filter[5]: corner=1499.7884186649117, input=4, select=0
    // filter[6]: corner=8433.930068571644, input=4, select=1
    // band[0]: findex=2, fsel=0, lo=20.0, hi=47.4274741132331
    // band[1]: findex=2, fsel=1, lo=47.4274741132331, hi=112.46826503806982
    // band[2]: findex=3, fsel=0, lo=112.46826503806982, hi=266.70428643266484
    // band[3]: findex=3, fsel=1, lo=266.70428643266484, hi=632.4555320336759
    // band[4]: findex=5, fsel=0, lo=632.4555320336759, hi=1499.7884186649117
    // band[5]: findex=5, fsel=1, lo=1499.7884186649117, hi=3556.5588200778457
    // band[6]: findex=6, fsel=0, lo=3556.5588200778457, hi=8433.930068571644
    // band[7]: findex=6, fsel=1, lo=8433.930068571644, hi=20000.0

    template <typename value_t>
    class FilterTree_8
    {
    public:
        static const int NBANDS = 8;

    private:
        static const int NFILTERS = 7;
        value_t cnum[NFILTERS] {};
        value_t cden[NFILTERS] {};
        value_t xprev[NFILTERS] {};
        value_t yprev[NFILTERS] {};

    public:
        void reset()
        {
            for (int i = 0; i < NFILTERS; ++i)
                xprev[i] = yprev[i] = 0;
        }

        void setSampleRate(value_t sampleRateHz)
        {
            value_t c;
            c = sampleRateHz / 1986.91765315922;
            cnum[0] = 1 - c;
            cden[0] = 1 + c;
            c = sampleRateHz / 353.3294752055899;
            cnum[1] = 1 - c;
            cden[1] = 1 + c;
            c = sampleRateHz / 148.9978042524532;
            cnum[2] = 1 - c;
            cden[2] = 1 + c;
            c = sampleRateHz / 837.8762269377678;
            cnum[3] = 1 - c;
            cden[3] = 1 + c;
            c = sampleRateHz / 11173.25906121654;
            cnum[4] = 1 - c;
            cden[4] = 1 + c;
            c = sampleRateHz / 4711.72427801674;
            cnum[5] = 1 - c;
            cden[5] = 1 + c;
            c = sampleRateHz / 26495.97274431474;
            cnum[6] = 1 - c;
            cden[6] = 1 + c;
        }

        void update(value_t input, value_t output[NBANDS])
        {
            // Update the filters.
            value_t x;
            x = input;
            yprev[0] = (x + xprev[0] - yprev[0]*cnum[0]) / cden[0];
            xprev[0] = x;
            x = yprev[0];
            yprev[1] = (x + xprev[1] - yprev[1]*cnum[1]) / cden[1];
            xprev[1] = x;
            x = yprev[1];
            yprev[2] = (x + xprev[2] - yprev[2]*cnum[2]) / cden[2];
            xprev[2] = x;
            x = xprev[1] - yprev[1];
            yprev[3] = (x + xprev[3] - yprev[3]*cnum[3]) / cden[3];
            xprev[3] = x;
            x = xprev[0] - yprev[0];
            yprev[4] = (x + xprev[4] - yprev[4]*cnum[4]) / cden[4];
            xprev[4] = x;
            x = yprev[4];
            yprev[5] = (x + xprev[5] - yprev[5]*cnum[5]) / cden[5];
            xprev[5] = x;
            x = xprev[4] - yprev[4];
            yprev[6] = (x + xprev[6] - yprev[6]*cnum[6]) / cden[6];
            xprev[6] = x;
            // Calculate the output bands.
            output[0] = yprev[2];
            output[1] = xprev[2] - yprev[2];
            output[2] = yprev[3];
            output[3] = xprev[3] - yprev[3];
            output[4] = yprev[5];
            output[5] = xprev[5] - yprev[5];
            output[6] = yprev[6];
            output[7] = xprev[6] - yprev[6];
        }
    };
}
