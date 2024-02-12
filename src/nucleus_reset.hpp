#pragma once
#include "nucleus_engine.hpp"
#include "nucleus_reset.hpp"

namespace Sapphire
{
    namespace Nucleus
    {
        class CrashChecker      // helps detect and recover from non-finite simulation outputs
        {
        private:
            const int interval = 10000;
            int countdown = 0;

        public:
            bool check(NucleusEngine& engine)
            {
                bool crashed = false;
                if (countdown > 0)
                {
                    --countdown;
                }
                else
                {
                    countdown = interval;

                    const int nparticles = static_cast<int>(engine.numParticles());
                    for (int i = 0; i < nparticles; ++i)
                    {
                        if (!engine.particle(i).isFinite())
                            crashed = true;

                        for (int k = 0; k < 3; ++k)
                            if (!std::isfinite(engine.output(i, k)))
                                crashed = true;
                    }

                    if (crashed)
                    {
                        engine.resetAfterCrash();
                        SetMinimumEnergy(engine);
                    }
                }
                return crashed;
            }
        };
    }
}
