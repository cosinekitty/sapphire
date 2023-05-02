#ifndef __COSINEKITTY_ELASTIKA_ENGINE_HPP
#define __COSINEKITTY_ELASTIKA_ENGINE_HPP

// Sapphire mesh physics engine, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

#include "sapphire_engine.hpp"

namespace Sapphire
{
    struct Spring
    {
        int ballIndex1;         // 0-based index into Mesh::ballList
        int ballIndex2;         // 0-based index into Mesh::ballList

        Spring(int _ballIndex1, int _ballIndex2)
            : ballIndex1(_ballIndex1)
            , ballIndex2(_ballIndex2)
            {}
    };

    struct Ball
    {
        PhysicsVector pos;      // the ball's position [m]
        PhysicsVector vel;      // the ball's velocity [m/s]
        float mass;            // the ball's [kg]

        Ball(float _mass, float _x, float _y, float _z)
            : pos(_x, _y, _z, 0.0f)
            , vel()
            , mass(_mass)
            {}

        Ball(float _mass, PhysicsVector _pos, PhysicsVector _vel)
            : pos(_pos)
            , vel(_vel)
            , mass(_mass)
            {}

        static Ball Anchor(float _x, float _y, float _z)
        {
            return Ball(-1.0, _x, _y, _z);
        }

        static Ball Anchor(PhysicsVector pos)
        {
            return Ball(-1.0, pos.s[0], pos.s[1], pos.s[2]);
        }

        bool IsAnchor() const { return mass <= 0.0; }
        bool IsMobile() const { return mass > 0.0; }
    };

    using SpringList = std::vector<Spring>;
    using BallList = std::vector<Ball>;

    const float MESH_DEFAULT_STIFFNESS = 10.0;
    const float MESH_DEFAULT_REST_LENGTH = 1.0e-3;
    const float MESH_DEFAULT_SPEED_LIMIT = 2.0;

    class PhysicsMesh
    {
    private:
        SpringList springList;
        std::vector<PhysicsVector> originalPositions;
        BallList currBallList;
        BallList nextBallList;
        PhysicsVectorList forceList;                // holds calculated net force on each ball
        PhysicsVector gravity;
        PhysicsVector magnet;
        float stiffness  = MESH_DEFAULT_STIFFNESS;     // the linear spring constant [N/m]
        float restLength = MESH_DEFAULT_REST_LENGTH;   // spring length [m] that results in zero force
        float speedLimit = MESH_DEFAULT_SPEED_LIMIT;

    public:
        void Clear();   // empty out the mesh and start over
        void Quiet();    // put all balls back to their original locations and zero their velocities
        float GetStiffness() const { return stiffness; }
        void SetStiffness(float _stiffness);
        float GetRestLength() const { return restLength; }
        void SetRestLength(float _restLength);
        float GetSpeedLimit() const { return speedLimit; }
        void SetSpeedLimit(float _speedLimit) { speedLimit = _speedLimit; }
        void SetMagneticField(PhysicsVector _magnet) { magnet = _magnet; }
        PhysicsVector GetGravity() const { return gravity; }
        void SetGravity(PhysicsVector _gravity) { gravity = _gravity; }
        int Add(Ball);      // returns ball index, for linking with springs
        bool Add(Spring);   // returns false if either ball index is bad, true if spring added
        const SpringList& GetSprings() const { return springList; }
        BallList& GetBalls() { return currBallList; }
        void Update(float dt, float halflife);
        int NumBalls() const { return static_cast<int>(currBallList.size()); }
        int NumSprings() const { return static_cast<int>(springList.size()); }
        Ball& GetBallAt(int index) { return currBallList.at(index); }
        const Ball& GetBallAt(int index) const { return currBallList.at(index); }
        bool IsAnchor(int ballIndex) const { return GetBallAt(ballIndex).IsAnchor(); }
        bool IsMobile(int ballIndex) const { return GetBallAt(ballIndex).IsMobile(); }
        PhysicsVector GetBallOrigin(int index) const { return originalPositions.at(index); }
        PhysicsVector GetBallDisplacement(int index) const { return currBallList.at(index).pos - originalPositions.at(index); }
        Spring& GetSpringAt(int index) { return springList.at(index); }

    private:
        void CalcForces(
            BallList& blist,
            PhysicsVectorList& forceList);

        static void Dampen(BallList& blist, float dt, float halflife);
        static void Copy(const BallList& source, BallList& target);

        static void Extrapolate(
            float dt,
            float speedLimit,
            const PhysicsVectorList& forceList,
            const BallList& sourceList,
            BallList& targetList
        );
    };

    struct MeshAudioParameters
    {
        int leftInputBallIndex        {-1};
        int rightInputBallIndex       {-1};
        int leftOutputBallIndex       {-1};
        int rightOutputBallIndex      {-1};
        int leftVarMassBallIndex      {-1};
        int rightVarMassBallIndex     {-1};
        PhysicsVector leftInputDir1   {0.0f};
        PhysicsVector leftInputDir2   {0.0f};
        PhysicsVector rightInputDir1  {0.0f};
        PhysicsVector rightInputDir2  {0.0f};
        PhysicsVector leftOutputDir1  {0.0f};
        PhysicsVector leftOutputDir2  {0.0f};
        PhysicsVector rightOutputDir1 {0.0f};
        PhysicsVector rightOutputDir2 {0.0f};
    };

    // GridMap is a read-write 2D array whose indices can be positive or negative.
    template <typename TElement>
    class GridMap
    {
    private:
        const int umin;      // the minimum value of a horizontal index
        const int umax;      // the maximum value of a horizontal index
        const int vmin;      // the minimum value of a vertical index
        const int vmax;      // the maximum value of a vertical index
        std::vector<TElement> array;

    public:
        GridMap(int _umin, int _umax, int _vmin, int _vmax, TElement _init)
            : umin(_umin)
            , umax(_umax)
            , vmin(_vmin)
            , vmax(_vmax)
            , array((_umax-_umin+1)*(_vmax-_vmin+1), _init)
            {}

        TElement& at(int u, int v)
        {
            if (u < umin || u > umax)
                throw std::out_of_range("u");

            if (v < vmin || v > vmax)
                throw std::out_of_range("v");

            return array.at((u-umin) + (umax-umin+1)*(v-vmin));
        }
    };

    MeshAudioParameters CreateHex(PhysicsMesh& mesh);

    const int ELASTIKA_FILTER_LAYERS = 3;

    class MeshInput     // facilitates injecting audio into the mesh
    {
    private:
        int ballIndex;

    public:
        MeshInput()
            : ballIndex(-1)
            {}

        explicit MeshInput(int _ballIndex)
            : ballIndex(_ballIndex)
            {}

        // Inject audio into the mesh
        void Inject(Sapphire::PhysicsMesh& mesh, const Sapphire::PhysicsVector& direction, float sample)
        {
            Sapphire::Ball& ball = mesh.GetBallAt(ballIndex);
            ball.pos = mesh.GetBallOrigin(ballIndex) + (sample * direction);
        }
    };


    class MeshOutput    // facilitates extracting audio from the mesh
    {
    private:
        int ballIndex;

    public:
        MeshOutput()
            : ballIndex(-1)
            {}

        explicit MeshOutput(int _ballIndex)
            : ballIndex(_ballIndex)
            {}

        // Extract audio from the mesh
        float Extract(const Sapphire::PhysicsMesh& mesh, const Sapphire::PhysicsVector& direction)
        {
            using namespace Sapphire;

            PhysicsVector movement = mesh.GetBallDisplacement(ballIndex);
            return Dot(movement, direction);
        }
    };


    class ElastikaEngine
    {
    private:
        int outputVerifyCounter;
        PhysicsMesh mesh;
        MeshAudioParameters mp;
        SliderMapping frictionMap;
        SliderMapping stiffnessMap;
        SliderMapping spanMap;
        SliderMapping curlMap;
        SliderMapping massMap;
        SliderMapping tiltMap;
        MeshInput leftInput;
        MeshInput rightInput;
        MeshOutput leftOutput;
        MeshOutput rightOutput;
        StagedFilter<float, ELASTIKA_FILTER_LAYERS> leftLoCut;
        StagedFilter<float, ELASTIKA_FILTER_LAYERS> rightLoCut;
        float halfLife;
        float drive;
        float gain;
        float inTilt;
        float outTilt;
        AutomaticGainLimiter agc;
        bool enableAgc = false;

    public:
        ElastikaEngine()
        {
            initialize();
        }

        void initialize()
        {
            outputVerifyCounter = 0;

            frictionMap = SliderMapping(SliderScale::Exponential, {1.3f, -4.5f});
            stiffnessMap = SliderMapping(SliderScale::Exponential, {-0.1f, 3.4f});
            spanMap = SliderMapping(SliderScale::Linear, {0.0008, 0.0003});
            curlMap = SliderMapping(SliderScale::Linear, {0.0f, 1.0f});
            massMap = SliderMapping(SliderScale::Exponential, {0.0f, 1.0f});
            tiltMap = SliderMapping(SliderScale::Linear, {0.0f, 1.0f});

            mp = CreateHex(mesh);

            // Define how stereo inputs go into the mesh.
            leftInput  = MeshInput(mp.leftInputBallIndex);
            rightInput = MeshInput(mp.rightInputBallIndex);

            // Define how to extract stereo outputs from the mesh.
            leftOutput  = MeshOutput(mp.leftOutputBallIndex);
            rightOutput = MeshOutput(mp.rightOutputBallIndex);

            setDcRejectFrequency(20.0f);
            setFriction();
            setSpan();
            setStiffness();
            setCurl();
            setMass();
            setDrive();
            setGain();
            setInputTilt();
            setOutputTilt();
            setAgcEnabled(true);

            quiet();
        }

        void setDcRejectFrequency(float frequency)
        {
            leftLoCut.SetCutoffFrequency(frequency);
            rightLoCut.SetCutoffFrequency(frequency);
        }

        void quiet()
        {
            mesh.Quiet();
            leftLoCut.Reset();
            rightLoCut.Reset();
            agc.initialize();
        }

        void setFriction(float slider = 0.5f)
        {
            halfLife = frictionMap.Evaluate(Clamp(slider));
        }

        void setSpan(float slider = 0.5f)
        {
            float restLength = spanMap.Evaluate(Clamp(slider));
            mesh.SetRestLength(restLength);
        }

        void setStiffness(float slider = 0.5f)
        {
            float stiffness = stiffnessMap.Evaluate(Clamp(slider));
            mesh.SetStiffness(stiffness);
        }

        void setCurl(float slider = 0.0f)
        {
            float curl = curlMap.Evaluate(Clamp(slider, -1.0f, +1.0f));
            if (curl >= 0.0f)
                mesh.SetMagneticField(curl * PhysicsVector(0.005, 0, 0, 0));
            else
                mesh.SetMagneticField(curl * PhysicsVector(0, 0, -0.005, 0));
        }

        void setMass(float slider = 0.0f)
        {
            Ball& lmBall = mesh.GetBallAt(mp.leftVarMassBallIndex);
            Ball& rmBall = mesh.GetBallAt(mp.rightVarMassBallIndex);
            lmBall.mass = rmBall.mass = 1.0e-6 * massMap.Evaluate(Clamp(slider, -1.0f, +1.0f));
        }

        void setDrive(float slider = 1.0f)      // min = 0.0 (-inf dB), default = 1.0 (0 dB), max = 2.0 (+24 dB)
        {
            drive = std::pow(Clamp(slider, 0.0f, 2.0f), 4.0f);
        }

        void setGain(float slider = 1.0f)      // min = 0.0 (-inf dB), default = 1.0 (0 dB), max = 2.0 (+24 dB)
        {
            gain = std::pow(Clamp(slider, 0.0f, 2.0f), 4.0f);
        }

        void setInputTilt(float slider = 0.5f)
        {
            inTilt = Clamp(slider);
        }

        void setOutputTilt(float slider = 0.5f)
        {
            outTilt = Clamp(slider);
        }

        bool getAgcEnabled() const { return enableAgc; }

        void setAgcEnabled(bool enable)
        {
            if (enable && !enableAgc)
            {
                // If the AGC isn't enabled, and caller wants to enable it,
                // re-initialize the AGC so it forgets any previous level it had settled on.
                agc.initialize();
            }
            enableAgc = enable;
        }

        void setAgcLevel(float level)
        {
            // Convert VCV voltages to unit dimensionless quantities.
            float ceiling = level / 5.0f;
            agc.setCeiling(ceiling);
        }

        double getAgcDistortion() const     // returns 0 when no distortion, or a positive value correlated with AGC distortion
        {
            return enableAgc ? (agc.getFollower() - 1.0) : 0.0;
        }

        void process(float sampleRate, float leftIn, float rightIn, float& leftOut, float& rightOut)
        {
            // Feed audio stimulus into the mesh.
            PhysicsVector leftInputDir = Interpolate(inTilt, mp.leftInputDir1, mp.leftInputDir2);
            PhysicsVector rightInputDir = Interpolate(inTilt, mp.rightInputDir1, mp.rightInputDir2);
            leftInput.Inject(mesh, leftInputDir, drive * leftIn);
            rightInput.Inject(mesh, rightInputDir, drive * rightIn);

            // Update the simulation state by one sample's worth of time.
            mesh.Update(1.0/sampleRate, halfLife);

            // Extract output for the left channel.
            PhysicsVector leftOutputDir = Interpolate(outTilt, mp.leftOutputDir1, mp.leftOutputDir2);
            leftOut = leftOutput.Extract(mesh, leftOutputDir);
            leftOut = leftLoCut.UpdateHiPass(leftOut, sampleRate);
            leftOut *= gain;

            // Extract output for the right channel.
            PhysicsVector rightOutputDir = Interpolate(outTilt, mp.rightOutputDir1, mp.rightOutputDir2);
            rightOut = rightOutput.Extract(mesh, rightOutputDir);
            rightOut = rightLoCut.UpdateHiPass(rightOut, sampleRate);
            rightOut *= gain;

            if (enableAgc)
            {
                // Automatic gain control to limit excessive output voltages.
                agc.process(sampleRate, leftOut, rightOut);
            }

            // Final line of defense against NAN/infinite output:
            // Check for invalid output. If found, clear the mesh.
            // Do this about every quarter of a second, to avoid CPU burden.
            // The intention is for the user to notice something sounds wrong,
            // the output is briefly NAN, but then it clears up as soon as the
            // internal or external problem is resolved.
            // The main point is to avoid leaving Elastika stuck in a NAN state forever.
            if (++outputVerifyCounter >= 11000)
            {
                outputVerifyCounter = 0;
                if (!std::isfinite(leftOut) || !std::isfinite(rightOut))
                {
                    quiet();
                    leftOut = rightOut = 0.0f;
                }
            }
        }
    };
}

#endif // __COSINEKITTY_ELASTIKA_ENGINE_HPP
