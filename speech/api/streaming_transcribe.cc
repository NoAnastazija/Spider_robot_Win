// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <windows.h>
#include <mmeapi.h>
#include "parse_arguments.h"
#include <google/cloud/speech/speech_client.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#pragma comment(lib, "winmm.lib")

// [START speech_streaming_recognize]
namespace speech = ::google::cloud::speech;
using RecognizeStream = ::google::cloud::AsyncStreamingReadWriteRpc<
    speech::v1::StreamingRecognizeRequest,
    speech::v1::StreamingRecognizeResponse>;

auto constexpr kUsage = R"""(Usage:
  streaming_transcribe [--bitrate N] audio.(raw|ulaw|flac|amr|awb)
)""";

auto constexpr kChunkSize = 64 * 1024;

struct waveInProc_param_t {
  speech::v1::StreamingRecognizeRequest* request;
  RecognizeStream* stream;
};

static void CALLBACK waveInProc(
   HWAVEIN   hwi,
   UINT      uMsg,
   DWORD_PTR dwInstance,
   DWORD_PTR dwParam1,
   DWORD_PTR dwParam2) {
  if (uMsg != WIM_DATA)
    return;
  waveInProc_param_t* param = (waveInProc_param_t*)dwInstance;
  WAVEHDR *hdr = (WAVEHDR*)dwParam1;
  // Write the chunk to the stream.
  if (hdr->dwBytesRecorded > 0) {
    param->request->set_audio_content(hdr->lpData, hdr->dwBytesRecorded);
    std::cout << "Sending " << hdr->dwBytesRecorded / 1024 << "k bytes." << std::endl;
    param->stream->Write(*param->request, grpc::WriteOptions());
  }
  waveInAddBuffer(hwi, hdr, sizeof(*hdr));
}

// Write the audio in 64k chunks at a time, simulating audio content arriving
// from a microphone.
void MicrophoneThreadMain(RecognizeStream& stream,
                          int32_t sample_rate_hertz) {
  speech::v1::StreamingRecognizeRequest request;
  HWAVEIN hwi;
  WAVEFORMATEX fmt = {
    WAVE_FORMAT_PCM,              // wFormatTag
    1,                            // nChannels
    (DWORD)sample_rate_hertz,     // nSamplesPerSec
    (DWORD)sample_rate_hertz * 2, // nAvgBytesPerSec
    2,                            // nBlockAlign
    16,                           // wBitsPerSample
    0                             // cbSize
  };
  waveInProc_param_t param = {
    &request,
    &stream
  };
  MMRESULT res = waveInOpen(&hwi, WAVE_MAPPER, &fmt, (DWORD_PTR)waveInProc, (DWORD_PTR)&param, CALLBACK_FUNCTION);
  if (res != MMSYSERR_NOERROR)
    throw std::exception("waveInOpen failed");
  WAVEHDR hdr[2];
  std::vector<char> buf[_countof(hdr)];
  for (size_t i = 0; i < _countof(hdr); ++i) {
    buf[i].resize(kChunkSize);
    memset(&hdr[i], 0, sizeof(hdr[i]));
    hdr[i].lpData = buf[i].data();
    hdr[i].dwBufferLength = kChunkSize;
    if (waveInPrepareHeader(hwi, &hdr[i], sizeof(hdr[i])) != MMSYSERR_NOERROR)
      throw std::exception("waveInPrepareHeader failed");
    if (waveInAddBuffer(hwi, &hdr[i], sizeof(hdr[i])) != MMSYSERR_NOERROR)
      throw std::exception("waveInAddBuffer failed");
  }
  if (waveInStart(hwi) != MMSYSERR_NOERROR)
      throw std::exception("waveInStart failed");
  Sleep(1000*60*10);
  stream.WritesDone().get();
  waveInReset(hwi);
  for (size_t i = 0; i < _countof(hdr); ++i)
    waveInUnprepareHeader(hwi, &hdr[i], sizeof(hdr[i]));
  waveInClose(hwi);
}

const std::string WHITESPACE = " \n\r\t\f\v";

static std::string ltrim(const std::string &s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}
 
static std::string rtrim(const std::string &s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}
 
static std::string trim(const std::string &s) {
    return rtrim(ltrim(s));
}

constexpr size_t STACK_BUFFER_BYTES = 0x100;

template<class _Traits1, class _Ax1, class _Traits2, class _Ax2>
static _Success_(return != 0) int MultiByteToWideChar(_In_ UINT CodePage, _In_ DWORD dwFlags, _In_ const std::basic_string<char, _Traits1, _Ax1> &sMultiByteStr, _Out_ std::basic_string<wchar_t, _Traits2, _Ax2> &sWideCharStr) noexcept
{
    WCHAR szStackBuffer[STACK_BUFFER_BYTES/sizeof(WCHAR)];

    // Try to convert to stack buffer first.
    int cch = ::MultiByteToWideChar(CodePage, dwFlags, sMultiByteStr.c_str(), (int)sMultiByteStr.length(), szStackBuffer, _countof(szStackBuffer));
    if (cch) {
        // Copy from stack.
        sWideCharStr.assign(szStackBuffer, cch);
    } else if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        // Query the required output size. Allocate buffer. Then convert again.
        cch = ::MultiByteToWideChar(CodePage, dwFlags, sMultiByteStr.c_str(), (int)sMultiByteStr.length(), NULL, 0);
        std::unique_ptr<WCHAR[]> szBuffer(new WCHAR[cch]);
        cch = ::MultiByteToWideChar(CodePage, dwFlags, sMultiByteStr.c_str(), (int)sMultiByteStr.length(), szBuffer.get(), cch);
        sWideCharStr.assign(szBuffer.get(), cch);
    }

    return cch;
}

template<class _Traits1, class _Ax1, class _Traits2, class _Ax2>
static _Success_(return != 0) int WideCharToMultiByte(_In_ UINT CodePage, _In_ DWORD dwFlags, _In_ const std::basic_string<wchar_t, _Traits1, _Ax1> &sWideCharStr, _Out_ std::basic_string<char, _Traits2, _Ax2> &sMultiByteStr, _In_opt_z_ LPCSTR lpDefaultChar, _Out_opt_ LPBOOL lpUsedDefaultChar) noexcept
{
    CHAR szStackBuffer[STACK_BUFFER_BYTES/sizeof(CHAR)];

    // Try to convert to stack buffer first.
    int cch = ::WideCharToMultiByte(CodePage, dwFlags, sWideCharStr.c_str(), (int)sWideCharStr.length(), szStackBuffer, _countof(szStackBuffer), lpDefaultChar, lpUsedDefaultChar);
    if (cch) {
        // Copy from stack.
        sMultiByteStr.assign(szStackBuffer, cch);
    } else if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        // Query the required output size. Allocate buffer. Then convert again.
        cch = ::WideCharToMultiByte(CodePage, dwFlags, sWideCharStr.c_str(), (int)sWideCharStr.length(), NULL, 0, lpDefaultChar, lpUsedDefaultChar);
        std::unique_ptr<CHAR[]> szBuffer(new CHAR[cch]);
        cch = ::WideCharToMultiByte(CodePage, dwFlags, sWideCharStr.c_str(), (int)sWideCharStr.length(), szBuffer.get(), cch, lpDefaultChar, lpUsedDefaultChar);
        sMultiByteStr.assign(szBuffer.get(), cch);
    }

    return cch;
}


int main(int argc, char** argv) try {
  // Create a Speech client with the default configuration
  auto client = speech::SpeechClient(speech::MakeSpeechConnection());

  // Parse command line arguments.
  auto args = ParseArguments(argc, argv);
  auto const file_path = args.path;
  auto const sample_rate_hertz = args.config.sample_rate_hertz();

  speech::v1::StreamingRecognizeRequest request;
  auto& streaming_config = *request.mutable_streaming_config();
  *streaming_config.mutable_config() = args.config;

  // Begin a stream.
  auto stream =
      client.AsyncStreamingRecognize(google::cloud::ExperimentalTag{});
  // The stream can fail to start, and `.get()` returns an error in this case.
  if (!stream->Start().get()) throw stream->Finish().get();
  // Write the first request, containing the config only.
  if (!stream->Write(request, grpc::WriteOptions{}).get()) {
    // Write().get() returns false if the stream is closed.
    throw stream->Finish().get();
  }

  HANDLE serial_port;
  serial_port = CreateFileA(file_path.c_str(), GENERIC_READ | GENERIC_WRITE,
      0,
      0,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
      0);
  if (serial_port == INVALID_HANDLE_VALUE) {
      if (GetLastError() == ERROR_FILE_NOT_FOUND)
          throw std::exception("Path does not exist");
      throw std::exception("CreateFile failed");
  }

  DCB serial_port_param = { 0 };
  serial_port_param.DCBlength = sizeof(serial_port_param);
  if (!GetCommState(serial_port, &serial_port_param))
      throw std::exception("GetCommState failed");

  serial_port_param.BaudRate = CBR_9600;
  serial_port_param.ByteSize = 8;
  serial_port_param.StopBits = ONESTOPBIT;
  serial_port_param.Parity = ODDPARITY;
  if (!SetCommState(serial_port, &serial_port_param))
      throw std::exception("SetCommState failed");

  COMMTIMEOUTS timeout = { 0 };
  timeout.ReadIntervalTimeout = 60;
  timeout.ReadTotalTimeoutConstant = 60;
  timeout.ReadTotalTimeoutMultiplier = 15;
  timeout.WriteTotalTimeoutConstant = 60;
  timeout.WriteTotalTimeoutMultiplier = 8;
  if (!SetCommTimeouts(serial_port, &timeout))
      throw std::exception("SetCommTimeouts failed");

  // Simulate a microphone thread using the file as input.
  auto microphone =
      std::thread(MicrophoneThreadMain, std::ref(*stream), sample_rate_hertz);
  // Read responses.
  std::string command;
  auto read = [&stream] { return stream->Read().get(); };
  for (auto response = read(); response.has_value(); response = read()) {
    // Dump the transcript of all the results.
    for (auto const& result : response->results()) {
      std::cout << "Result stability: " << result.stability() << "\n";
      for (auto const& alternative : result.alternatives()) {
        std::string transcript(trim(alternative.transcript())), transcript_oem;
        std::wstring transcript_w;
        MultiByteToWideChar(CP_UTF8, 0, transcript, transcript_w);
        std::transform(transcript_w.begin(), transcript_w.end(), transcript_w.begin(), ::towlower);
        WideCharToMultiByte(CP_OEMCP, 0, transcript_w, transcript_oem, NULL, NULL);
        std::cout << alternative.confidence() << "\t\"" << transcript_oem << "\"\n";
        auto t = transcript.c_str();
        if (std::strcmp(t, "robot pojdi naprej") == 0 || std::strcmp(t, "robot go forward") == 0)
          command = "f";
        else if (std::strcmp(t, "robot pojdi nazaj") == 0 || std::strcmp(t, "robot go backward") == 0)
          command = "b";
        else if (std::strcmp(t, "robot pojdi levo") == 0 || std::strcmp(t, "robot obrni se levo") == 0 || std::strcmp(t, "robot turn left") == 0)
          command = "l";
        else if (std::strcmp(t, "robot pojdi desno") == 0 || std::strcmp(t, "robot obrni se desno") == 0 || std::strcmp(t, "robot turn right") == 0)
          command = "r";
        else if (std::strcmp(t, "handshake") == 0 || std::strcmp(t, "nice to meet you") == 0 || std::strcmp(t, "moje ime je") == 0)
          command = "s";
        else if (std::strcmp(t, "hej") == 0 || std::strcmp(t, "hejla") == 0 || std::strcmp(t, "hello") == 0 || std::strcmp(t, "hey") == 0)
          command = "w";
        else if (std::strcmp(t, "dance") == 0 || std::strcmp(t, "ples") == 0 || std::strcmp(t, "robot pleši") == 0)
          command = "d";
        else
          command.clear();
        if (!command.empty()) {
          std::cout << command << " >> " << file_path << "\n";
          DWORD bytes_written = 0;
          if (!WriteFile(serial_port, command.c_str(), command.size(), &bytes_written, NULL))
            throw std::exception("WriteFile failed");
        }
      }
    }
  }
  auto status = stream->Finish().get();
  microphone.join();
  CloseHandle(serial_port);
  if (!status.ok()) throw status;
  return 0;
} catch (google::cloud::Status const& s) {
  std::cerr << "Recognize stream finished with an error: " << s << "\n";
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception thrown: " << ex.what() << "\n"
            << kUsage << "\n";
  return 1;
}
// [END speech_streaming_recognize]
