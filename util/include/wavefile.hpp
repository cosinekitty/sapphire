/*
    wavefile.hpp  -  Don Cross <cosinekitty@gmail.com>
    Quick and dirty 16-bit PCM WAV file reader/writer.
*/

#ifndef __COSINEKITTY_WAVEFILE_HPP
#define __COSINEKITTY_WAVEFILE_HPP

#include <cinttypes>
#include <cstring>
#include <cstdio>
#include <vector>
#include <stdexcept>
#include <string>


const int IntSampleScale = 32700;


inline int16_t IntSampleFromFloat(float x)
{
    if (x < -1.0f || x > +1.0f)
        throw std::range_error(std::string("Floating-point audio output went out of range: " + std::to_string(x)));

    return static_cast<int16_t>(IntSampleScale * x);
}


inline float FloatFromIntSample(int16_t s)
{
    if (s < -IntSampleScale || s > +IntSampleScale)
        throw std::range_error("Integer audio output went out of range.");

    return static_cast<float>(s) / IntSampleScale;
}


class WaveFileWriter
{
private:
    FILE *outfile = nullptr;
    std::vector<int16_t> buffer;
    int nchannels = 0;
    int sampleRateHz = 0;
    int byteLength = 0;

    void Encode16(uint8_t *header, int offset, int value)
    {
        header[offset+0] = value;
        header[offset+1] = value >> 8;
    }

    void Encode32(uint8_t *header, int offset, int value)
    {
        header[offset+0] = value;
        header[offset+1] = value >> 8;
        header[offset+2] = value >> 16;
        header[offset+3] = value >> 24;
    }

    void WriteHeader()
    {
        // 00000000  52 49 46 46 c4 ea 1a 00  57 41 56 45 66 6d 74 20  |RIFF....WAVEfmt |
        // 00000010  10 00 00 00 01 00 02 00  44 ac 00 00 10 b1 02 00  |........D.......|
        // 00000020  04 00 10 00 64 61 74 61  a0 ea 1a 00 00 00 00 00  |....data........|
        // 00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
        // 00000040  ff ff 00 00 02 00 ff ff  fd ff 02 00 04 00 fe ff  |................|

        uint8_t header[] = {
            0x52, 0x49, 0x46, 0x46,     // "RIFF"
            0, 0, 0, 0,                 // placeholder for size of RIFF chunk
            0x57, 0x41, 0x56, 0x45,     // "WAVE"
            0x66, 0x6d, 0x74, 0x20,     // "fmt "
            0x10, 0x00, 0x00, 0x00,     // length of format payload = 16
            0x01, 0x00,                 // format = 1 = PCM
            0, 0,                       // placeholder for number of channels
            0, 0, 0, 0,                 // placeholder for sample rate
            0, 0, 0, 0,                 // placeholder for bitrate
            0, 0,                       // placeholder for bytes/sample
            0x10, 0x00,                 // bits per sample = 16
            0x64, 0x61, 0x74, 0x61,     // "data"
            0, 0, 0, 0                  // placeholder for data length
        };

        Encode32(header,  4, byteLength+44-8);   // RIFF payload size = (file size) - ("RIFF" + 4 bytes for storing size)
        Encode16(header, 22, nchannels);
        Encode32(header, 24, sampleRateHz);
        Encode32(header, 28, sampleRateHz*(2*nchannels));   // bitrate
        Encode16(header, 32, 2*nchannels);                  // bytes per sample
        Encode32(header, 40, byteLength);

        if (fwrite(header, sizeof(header), 1, outfile) != 1)
            throw std::runtime_error("Cannot write header to WAV file.");
    }

    void Flush()
    {
        if (outfile != nullptr)
        {
            if (fwrite(buffer.data(), sizeof(int16_t), buffer.size(), outfile) != buffer.size())
                throw std::runtime_error("Cannot write audio to WAV file.");
        }
        buffer.clear();
    }

public:
    ~WaveFileWriter()
    {
        Close();
    }

    void Close()
    {
        Flush();
        if (outfile != nullptr)
        {
            fflush(outfile);
            if (fseek(outfile, 0, SEEK_SET))
                throw std::runtime_error("Could not seek back to beginning of WAV file");
            WriteHeader();      // write header again to update data length
            fclose(outfile);
            outfile = nullptr;
        }
    }

    bool Open(const char *filename, int sampleRate, int channels)
    {
        Close();

        sampleRateHz = sampleRate;
        nchannels = channels;
        byteLength = 0;

        outfile = fopen(filename, "wb");
        if (outfile == nullptr)
            return false;

        WriteHeader();
        return true;
    }

    void WriteSamples(const float *data, int ndata)
    {
        if (outfile == nullptr)
            throw std::logic_error("WaveFileWriter is not open.");

        for (int i = 0; i < ndata; ++i)
            buffer.push_back(IntSampleFromFloat(data[i]));

        byteLength += (2 * ndata);

        if (buffer.size() >= 10000)
            Flush();
    }

    void WriteSamples(const int16_t *data, int ndata)
    {
        if (outfile == nullptr)
            throw std::logic_error("WaveFileWriter is not open.");

        for (int i = 0; i < ndata; ++i)
            buffer.push_back(data[i]);

        byteLength += (2 * ndata);

        if (buffer.size() >= 10000)
            Flush();
    }
};


class ScaledWaveFileWriter
{
private:
    WaveFileWriter wave;
    float maximum = 0.0f;
    FILE *tempFile = nullptr;
    std::string tempFileName;

public:
    ~ScaledWaveFileWriter()
    {
        Close();
    }

    bool Open(const char *filename, int sampleRate, int channels)
    {
        tempFileName = std::string(filename) + ".tmp";
        tempFile = fopen(tempFileName.c_str(), "wb");
        if (tempFile == nullptr)
            return false;

        if (!wave.Open(filename, sampleRate, channels))
        {
            fclose(tempFile);
            remove(tempFileName.c_str());
            return false;
        }

        maximum = 0.0f;
        return true;
    }

    void WriteSamples(const float *data, int ndata)
    {
        if (tempFile == nullptr)
            throw std::logic_error("ScaledWaveFileWriter is not open.");

        for (int i = 0; i < ndata; ++i)
        {
            if (!std::isfinite(data[i]))
                throw std::range_error("Non-finite audio data not allowed.");
            maximum = std::max(maximum, std::abs(data[i]));
        }

        if (static_cast<size_t>(ndata) != fwrite(data, sizeof(float), ndata, tempFile))
            throw std::runtime_error("Error writing to temporary file.");

    }

    void Close()
    {
        if (tempFile)
        {
            fclose(tempFile);
            tempFile = nullptr;

            FILE *rfile = fopen(tempFileName.c_str(), "rb");
            if (rfile == nullptr)
                throw std::logic_error(std::string("Coult not re-open temporary file for read: ") + tempFileName);

            // Handle the case where silence was written. Avoid dividing by zero.
            if (maximum == 0.0f)
                maximum = 1.0f;

            std::vector<float> buffer;
            const size_t bufsize = 1024;
            buffer.resize(bufsize);

            for(;;)
            {
                size_t nread = fread(buffer.data(), sizeof(float), bufsize, rfile);
                for (size_t i = 0; i < nread; ++i)
                    buffer[i] /= maximum;

                wave.WriteSamples(buffer.data(), nread);
                if (nread < bufsize)
                    break;
            }

            fclose(rfile);
            remove(tempFileName.c_str());
        }
    }
};


class WaveFileReader
{
private:
    FILE *infile = nullptr;
    int sampleRate = 0;
    int channels = 0;
    size_t totalSamples = 0;     // total number of 16-bit integer data points
    size_t samplesRead = 0;
    std::vector<int16_t> conversionBuffer;  // for assisting reads into floating point buffer

    int Decode16(const uint8_t *header, int offset)
    {
        return
            static_cast<int>(header[offset+0]) |
            static_cast<int>(header[offset+1] << 8);
    }

    int Decode32(const uint8_t *header, int offset)
    {
        return
            static_cast<int>(header[offset+0]) |
            static_cast<int>(header[offset+1] <<  8) |
            static_cast<int>(header[offset+2] << 16) |
            static_cast<int>(header[offset+3] << 24);
    }

public:
    ~WaveFileReader()
    {
        Close();
    }

    void Close()
    {
        if (infile != nullptr)
        {
            fclose(infile);
            infile = nullptr;
        }
        sampleRate = 0;
        channels = 0;
        totalSamples = 0;
        samplesRead = 0;
    }

    bool Open(const char *filename)
    {
        Close();

        infile = fopen(filename, "rb");
        if (!infile)
            return false;

        uint8_t header[44];
        size_t nread = fread(header, sizeof(header), 1, infile);
        if (nread != 1)
        {
            // Too small to be a valid wave file.
            Close();
            return false;
        }

        // 00000000  52 49 46 46 ba 53 1f 00  57 41 56 45 66 6d 74 20  |RIFF.S..WAVEfmt |
        // 00000010  10 00 00 00 01 00 02 00  44 ac 00 00 10 b1 02 00  |........D.......|
        // 00000020  04 00 10 00 64 61 74 61  f4 52 1f 00 00 00 00 00  |....data.R......|
        // 00000030  01 00 01 00 ff ff fe ff  01 00 01 00 ff ff 00 00  |................|
        // 00000040  01 00 fe ff ff ff 03 00  02 00 fc ff ff ff 03 00  |................|

        if (memcmp(header, "RIFF", 4) || memcmp(&header[8], "WAVEfmt ", 8))
        {
            // Incorrect signature
            Close();
            return false;
        }

        channels = Decode16(header, 22);
        sampleRate = Decode32(header, 24);
        unsigned nbytes = static_cast<unsigned>(Decode32(header, 40));
        totalSamples = nbytes / sizeof(int16_t);    // FIXFIXFIX: should validate sample format is 16-bit
        return true;
    }

    int SampleRate() const { return sampleRate; }
    int Channels() const { return channels; }
    size_t TotalSamples() const { return totalSamples; }

    size_t Read(int16_t *data, size_t requestedSamples)
    {
        size_t remaining = totalSamples - samplesRead;
        size_t attempt = std::min(requestedSamples, remaining);
        size_t received = fread(data, sizeof(int16_t), attempt, infile);
        samplesRead += received;
        return received;
    }

    size_t Read(float *data, size_t requestedSamples)
    {
        size_t remaining = totalSamples - samplesRead;
        size_t attempt = std::min(requestedSamples, remaining);

        if (conversionBuffer.size() < attempt)
            conversionBuffer.resize(attempt);

        size_t received = fread(conversionBuffer.data(), sizeof(int16_t), attempt, infile);
        samplesRead += received;

        for (size_t i = 0; i < received; ++i)
            data[i] = FloatFromIntSample(conversionBuffer[i]);

        return received;
    }
};


#endif // __COSINEKITTY_WAVEFILE_HPP
