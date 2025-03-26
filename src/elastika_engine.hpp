#pragma once
#include <algorithm>
#include "elastika_mesh.hpp"

// Sapphire mesh physics engine, by Don Cross <cosinekitty@gmail.com>
// https://github.com/cosinekitty/sapphire

namespace Sapphire
{
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
        void Inject(PhysicsMesh& mesh, const PhysicsVector& direction, float sample)
        {
            PhysicsVector pos = mesh.GetBallOrigin(ballIndex) + (sample * direction);
            mesh.SetBallPosition(ballIndex, pos);
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
        float Extract(const PhysicsMesh& mesh, const PhysicsVector& direction) const
        {
            PhysicsVector movement = mesh.GetBallDisplacement(ballIndex);
            return Dot(movement, direction);
        }

        PhysicsVector VectorDisplacement(const PhysicsMesh& mesh) const
        {
            return BallPositionFactor * mesh.GetBallDisplacement(ballIndex);
        }
    };


    class ElastikaEngine
    {
    private:
        int outputVerifyCounter;
        ElastikaMesh mesh;
        const MeshAudioParameters mp;
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
        float mix;
        AutomaticGainLimiter agc;
        bool enableAgc = false;

    public:
        ElastikaEngine()
            : mp(ElastikaMesh::getAudioParameters())
        {
            initialize();
        }

        void initialize()
        {
            outputVerifyCounter = 0;

            // Define how stereo inputs go into the mesh.
            leftInput  = MeshInput(mp.leftInputBallIndex);
            rightInput = MeshInput(mp.rightInputBallIndex);

            // Define how to extract stereo outputs from the mesh.
            leftOutput  = MeshOutput(mp.leftOutputBallIndex);
            rightOutput = MeshOutput(mp.rightOutputBallIndex);

            setDcRejectFrequency(20);
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
            setMix();

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
            float x = std::clamp(slider, 0.0f, 1.0f);
            halfLife = TenToPower(-4.5f*x + 1.3f);
        }

        void setSpan(float slider = 0.5f)
        {
            float restLength = 0.0003f*std::clamp(slider, 0.0f, 1.0f) + 0.0008f;
            mesh.SetRestLength(restLength);
        }

        void setStiffness(float slider = 0.5f)
        {
            float x = std::clamp(slider, 0.0f, 1.0f);
            float stiffness = TenToPower(3.4f*x - 0.1f);
            mesh.SetStiffness(stiffness);
        }

        void setCurl(float slider = 0.0f)
        {
            float curl = std::clamp(slider, -1.0f, +1.0f);
            if (curl >= 0.0f)
                mesh.SetMagneticField(curl * PhysicsVector(0.015, 0, 0, 0));
            else
                mesh.SetMagneticField(curl * PhysicsVector(0, 0, -0.015, 0));
        }

        void setMass(float slider = 0.0f)
        {
            const float mass = TenToPower(std::clamp(slider, -1.0f, +1.0f) - 6);
            mesh.SetBallMass(mp.leftVarMassBallIndex, mass);
            mesh.SetBallMass(mp.rightVarMassBallIndex, mass);
        }

        void setDrive(float slider = 1.0f)      // min = 0.0 (-inf dB), default = 1.0 (0 dB), max = 2.0 (+24 dB)
        {
            drive = FourthPower(std::clamp(slider, 0.0f, 2.0f));
        }

        void setGain(float slider = 1.0f)      // min = 0.0 (-inf dB), default = 1.0 (0 dB), max = 2.0 (+24 dB)
        {
            gain = FourthPower(std::clamp(slider, 0.0f, 2.0f));
        }

        void setInputTilt(float slider = 0.5f)
        {
            inTilt = std::clamp(slider, 0.0f, 1.0f);
        }

        void setOutputTilt(float slider = 0.5f)
        {
            outTilt = std::clamp(slider, 0.0f, 1.0f);
        }

        void setMix(float slider = 1.0f)
        {
            mix = std::clamp(slider, 0.0f, 1.0f);
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

        bool process(float sampleRate, float leftIn, float rightIn, float& leftOut, float& rightOut)
        {
            // Feed audio stimulus into the mesh.
            PhysicsVector leftInputDir = Interpolate(inTilt, mp.leftInputDir1, mp.leftInputDir2);
            PhysicsVector rightInputDir = Interpolate(inTilt, mp.rightInputDir1, mp.rightInputDir2);
            leftInput.Inject(mesh, leftInputDir, drive * leftIn);
            rightInput.Inject(mesh, rightInputDir, drive * rightIn);

            // Update the simulation state by one sample's worth of time.
            mesh.Update(1/sampleRate, halfLife);

            // Extract output for the left channel.
            PhysicsVector leftOutputDir = Interpolate(outTilt, mp.leftOutputDir1, mp.leftOutputDir2);
            leftOut = leftOutput.Extract(mesh, leftOutputDir);
            leftOut = leftLoCut.UpdateHiPass(leftOut, sampleRate);
            leftOut = CubicMix(mix, leftIn, gain * leftOut);

            // Extract output for the right channel.
            PhysicsVector rightOutputDir = Interpolate(outTilt, mp.rightOutputDir1, mp.rightOutputDir2);
            rightOut = rightOutput.Extract(mesh, rightOutputDir);
            rightOut = rightLoCut.UpdateHiPass(rightOut, sampleRate);
            rightOut = CubicMix(mix, rightIn, gain * rightOut);

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
                    leftOut = rightOut = 0;
                    return false;   // non-finite output detected
                }
            }

            return true;    // output is OK
        }

        PhysicsVector getOutputVector(bool right) const
        {
            float scalar = gain;
            if (enableAgc)
                scalar /= agc.getFollower();
            const MeshOutput& output = *(right ? &rightOutput : &leftOutput);
            return scalar * output.VectorDisplacement(mesh);
        }
    };
}
