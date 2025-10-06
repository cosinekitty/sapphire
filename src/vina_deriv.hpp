namespace Sapphire
{
    namespace Vina
    {
        struct VinaDeriv
        {
            PhysicsVector gravity;
            double stiffness = 89.0;
            double restLength = 0.004;
            double mass = 1.0e-03;

            explicit VinaDeriv() {}

            void operator() (vina_state_t& slope, const vina_state_t& state)
            {
                assert(nParticles == slope.size());
                assert(nParticles == state.size());

                slope[0].pos = state[0].vel;
                slope[0].vel = PhysicsVector{};
                for (unsigned i = 1; i < nParticles; ++i)
                {
                    slope[i].pos = state[i].vel;
                    slope[i].vel = isMobileIndex(i) ? gravity : PhysicsVector{};
                    const VinaParticle& a = state.at(i-1);
                    const VinaParticle& b = state.at(i-0);
                    const PhysicsVector dr = b.pos - a.pos;
                    const double length = dr.mag();
                    const double fmag = stiffness*(length - restLength);
                    const PhysicsVector acc = (fmag/(mass * length)) * dr;
                    if (isMobileIndex(i-1))  slope[i-1].vel += acc;
                    if (isMobileIndex(i-0))  slope[i-0].vel -= acc;
                }
            }
        };
    }
}
