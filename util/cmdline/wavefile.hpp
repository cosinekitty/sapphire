/*
    wavefile.hpp  -  Don Cross <cosinekitty@gmail.com>
    Quick and dirty 16-bit PCM WAV file writer.
*/

#ifndef __COSINEKITTY_WAVEFILE_HPP
#define __COSINEKITTY_WAVEFILE_HPP

#include <cinttypes>
#include <cstdio>
#include <vector>
#include <stdexcept>

class WaveFileWriter
{
private:
    FILE *outfile = nullptr;
    std::vector<int16_t> buffer;
    int nchannels = 0;
    int sampleRateHz = 0;
    int byteLength = 0;

    int16_t ConvertSample(float x)
    {
        if (x < -1.0f || x > +1.0f)
            throw std::range_error("Audio output went out of range.");

        return static_cast<int16_t>(32700.0 * x);
    }

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
            throw std::logic_error("WaveFile is not open.");

        for (int i = 0; i < ndata; ++i)
            buffer.push_back(ConvertSample(data[i]));

        byteLength += (2 * ndata);

        if (buffer.size() >= 10000)
            Flush();
    }
};

#endif // __COSINEKITTY_WAVEFILE_HPP
