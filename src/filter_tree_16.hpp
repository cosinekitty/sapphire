#pragma once
namespace Sapphire
{
    // filter[0]: corner=632.4555320336759
    // filter[1]: corner=112.46826503806982, input=0, select=0
    // filter[2]: corner=47.4274741132331, input=1, select=0
    // filter[3]: corner=30.79853052118984, input=2, select=0
    // filter[4]: corner=73.03482545096753, input=2, select=1
    // filter[5]: corner=266.70428643266484, input=1, select=1
    // filter[6]: corner=173.1928646720131, input=5, select=0
    // filter[7]: corner=410.70500529142925, input=5, select=1
    // filter[8]: corner=3556.5588200778457, input=0, select=1
    // filter[9]: corner=1499.7884186649117, input=8, select=0
    // filter[10]: corner=973.9350503317263, input=9, select=0
    // filter[11]: corner=2309.563969378916, input=9, select=1
    // filter[12]: corner=8433.930068571644, input=8, select=1
    // filter[13]: corner=5476.839268528723, input=12, select=0
    // filter[14]: corner=12987.632631524226, input=12, select=1
    // band[0]: findex=3, fsel=0, lo=20.0, hi=30.79853052118984
    // band[1]: findex=3, fsel=1, lo=30.79853052118984, hi=47.4274741132331
    // band[2]: findex=4, fsel=0, lo=47.4274741132331, hi=73.03482545096753
    // band[3]: findex=4, fsel=1, lo=73.03482545096753, hi=112.46826503806982
    // band[4]: findex=6, fsel=0, lo=112.46826503806982, hi=173.1928646720131
    // band[5]: findex=6, fsel=1, lo=173.1928646720131, hi=266.70428643266484
    // band[6]: findex=7, fsel=0, lo=266.70428643266484, hi=410.70500529142925
    // band[7]: findex=7, fsel=1, lo=410.70500529142925, hi=632.4555320336759
    // band[8]: findex=10, fsel=0, lo=632.4555320336759, hi=973.9350503317263
    // band[9]: findex=10, fsel=1, lo=973.9350503317263, hi=1499.7884186649117
    // band[10]: findex=11, fsel=0, lo=1499.7884186649117, hi=2309.563969378916
    // band[11]: findex=11, fsel=1, lo=2309.563969378916, hi=3556.5588200778457
    // band[12]: findex=13, fsel=0, lo=3556.5588200778457, hi=5476.839268528723
    // band[13]: findex=13, fsel=1, lo=5476.839268528723, hi=8433.930068571644
    // band[14]: findex=14, fsel=0, lo=8433.930068571644, hi=12987.632631524226
    // band[15]: findex=14, fsel=1, lo=12987.632631524226, hi=20000.0

    template <typename value_t>
    class FilterTree_16
    {
    public:
        static const int NBANDS = 16;

    private:
        static const int NFILTERS = 15;
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
            c = sampleRateHz / 96.75643722673102;
            cnum[3] = 1 - c;
            cden[3] = 1 + c;
            c = sampleRateHz / 229.4456710929724;
            cnum[4] = 1 - c;
            cden[4] = 1 + c;
            c = sampleRateHz / 837.8762269377678;
            cnum[5] = 1 - c;
            cden[5] = 1 + c;
            c = sampleRateHz / 544.1014313077675;
            cnum[6] = 1 - c;
            cden[6] = 1 + c;
            c = sampleRateHz / 1290.267827416111;
            cnum[7] = 1 - c;
            cden[7] = 1 + c;
            c = sampleRateHz / 11173.25906121654;
            cnum[8] = 1 - c;
            cden[8] = 1 + c;
            c = sampleRateHz / 4711.72427801674;
            cnum[9] = 1 - c;
            cden[9] = 1 + c;
            c = sampleRateHz / 3059.707199195757;
            cnum[10] = 1 - c;
            cden[10] = 1 + c;
            c = sampleRateHz / 7255.709199196485;
            cnum[11] = 1 - c;
            cden[11] = 1 + c;
            c = sampleRateHz / 26495.97274431474;
            cnum[12] = 1 - c;
            cden[12] = 1 + c;
            c = sampleRateHz / 17205.99801090193;
            cnum[13] = 1 - c;
            cden[13] = 1 + c;
            c = sampleRateHz / 40801.85126271958;
            cnum[14] = 1 - c;
            cden[14] = 1 + c;
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
            x = yprev[2];
            yprev[3] = (x + xprev[3] - yprev[3]*cnum[3]) / cden[3];
            xprev[3] = x;
            x = xprev[2] - yprev[2];
            yprev[4] = (x + xprev[4] - yprev[4]*cnum[4]) / cden[4];
            xprev[4] = x;
            x = xprev[1] - yprev[1];
            yprev[5] = (x + xprev[5] - yprev[5]*cnum[5]) / cden[5];
            xprev[5] = x;
            x = yprev[5];
            yprev[6] = (x + xprev[6] - yprev[6]*cnum[6]) / cden[6];
            xprev[6] = x;
            x = xprev[5] - yprev[5];
            yprev[7] = (x + xprev[7] - yprev[7]*cnum[7]) / cden[7];
            xprev[7] = x;
            x = xprev[0] - yprev[0];
            yprev[8] = (x + xprev[8] - yprev[8]*cnum[8]) / cden[8];
            xprev[8] = x;
            x = yprev[8];
            yprev[9] = (x + xprev[9] - yprev[9]*cnum[9]) / cden[9];
            xprev[9] = x;
            x = yprev[9];
            yprev[10] = (x + xprev[10] - yprev[10]*cnum[10]) / cden[10];
            xprev[10] = x;
            x = xprev[9] - yprev[9];
            yprev[11] = (x + xprev[11] - yprev[11]*cnum[11]) / cden[11];
            xprev[11] = x;
            x = xprev[8] - yprev[8];
            yprev[12] = (x + xprev[12] - yprev[12]*cnum[12]) / cden[12];
            xprev[12] = x;
            x = yprev[12];
            yprev[13] = (x + xprev[13] - yprev[13]*cnum[13]) / cden[13];
            xprev[13] = x;
            x = xprev[12] - yprev[12];
            yprev[14] = (x + xprev[14] - yprev[14]*cnum[14]) / cden[14];
            xprev[14] = x;
            // Calculate the output bands.
            output[0] = yprev[3];
            output[1] = xprev[3] - yprev[3];
            output[2] = yprev[4];
            output[3] = xprev[4] - yprev[4];
            output[4] = yprev[6];
            output[5] = xprev[6] - yprev[6];
            output[6] = yprev[7];
            output[7] = xprev[7] - yprev[7];
            output[8] = yprev[10];
            output[9] = xprev[10] - yprev[10];
            output[10] = yprev[11];
            output[11] = xprev[11] - yprev[11];
            output[12] = yprev[13];
            output[13] = xprev[13] - yprev[13];
            output[14] = yprev[14];
            output[15] = xprev[14] - yprev[14];
        }
    };
}
