#include "../src/native_output.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <cstring>
using Microsoft::WRL::ComPtr;
void check(HRESULT result, const char* operation) {
    if (FAILED(result)) { std::cerr << operation << " HRESULT=" << std::hex << result << '\n'; std::exit(1); }
}
int main() {
    check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "COM");
    check(MFStartup(MF_VERSION), "Media Foundation");
    const auto path = std::filesystem::absolute("build") / L"mf-4k-smoke.mp4";
    {
        ComPtr<IMFSinkWriter> writer;
        check(MFCreateSinkWriterFromURL(path.c_str(), nullptr, nullptr, writer.GetAddressOf()), "Create writer");
        ComPtr<IMFMediaType> output, input;
        check(MFCreateMediaType(output.GetAddressOf()), "Output type");
        check(output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Video");
        check(output->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264), "H264");
        check(output->SetUINT32(MF_MT_AVG_BITRATE, 80000000), "Bitrate");
        check(output->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive), "Progressive");
        check(MFSetAttributeSize(output.Get(), MF_MT_FRAME_SIZE, 3840, 2160), "4K dimensions");
        check(MFSetAttributeRatio(output.Get(), MF_MT_FRAME_RATE, 30, 1), "Frame rate");
        check(MFSetAttributeRatio(output.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Pixel aspect");
        DWORD stream = 0;
        check(writer->AddStream(output.Get(), &stream), "4K H264 stream");
        check(MFCreateMediaType(input.GetAddressOf()), "Input type");
        check(output->CopyAllItems(input.Get()), "Copy input attributes");
        check(input->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12), "NV12");
        check(writer->SetInputMediaType(stream, input.Get(), nullptr), "4K input");
        check(writer->BeginWriting(), "Begin");
        constexpr DWORD size = 3840 * 2160 * 3 / 2;
        for (unsigned frame = 0; frame < 4; ++frame) {
            ComPtr<IMFMediaBuffer> buffer; ComPtr<IMFSample> sample;
            check(MFCreateMemoryBuffer(size, buffer.GetAddressOf()), "Frame buffer");
            BYTE* pixels = nullptr;
            check(buffer->Lock(&pixels, nullptr, nullptr), "Lock");
            std::memset(pixels, 16 + frame * 32, 3840 * 2160);
            std::memset(pixels + 3840 * 2160, 128, size - 3840 * 2160);
            check(buffer->Unlock(), "Unlock");
            check(buffer->SetCurrentLength(size), "Frame length");
            check(MFCreateSample(sample.GetAddressOf()), "Sample");
            check(sample->AddBuffer(buffer.Get()), "Sample pixels");
            check(sample->SetSampleTime(frame * 10000000ll / 30), "Timestamp");
            check(sample->SetSampleDuration(10000000ll / 30), "Duration");
            check(writer->WriteSample(stream, sample.Get()), "Encode frame");
        }
        check(writer->Finalize(), "Finalize");
    }
    std::filesystem::path delivered;
    if (!vyne::copy_native_mp4(path, std::filesystem::absolute("build") / L"mf-4k-delivery", delivered, 3840, 2160) ||
        vyne::copy_native_mp4(path, std::filesystem::absolute("build") / L"mf-4k-delivery", delivered, 1920, 1080)) return 1;
    MFShutdown(); CoUninitialize();
    std::cout << "PASS: real Windows H264 encoder accepts/finalizes 3840x2160 frames; output dimension gate rejects wrong size. This synthetic clip does not prove GTA export.\n";
}
