#include "sapphire_prog_chaos.hpp"

namespace Sapphire
{
    SlopeVector ProgOscillator::slopes(double x, double y, double z) const
    {
        double vx = 0;
        double vy = 0;
        double vz = 0;
        return SlopeVector(vx, vy, vz);
    }
}
