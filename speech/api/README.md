# Spider Robot Voice-Activated Controller for Windows

This repository contains the code for Windows to voice control the Spider Robot.

## Requirements

### Git for Windows

Install [Git for Windows](https://gitforwindows.org/).

### Visual Studio 2019

Download and install [Visual Studio 2019 Community Edition](https://visualstudio.microsoft.com/vs/older-downloads/). Select _Desktop development with C++_ workload and make sure _C++ CMake tools for Windows_ component is also selected.

> Note: All commands below should be executed from the _Start_ → _Visual Studio 2019_ → _Visual Studio Tools_ → _Developer Command Prompt for VS 2019_. Some commands requre elevation (Run as Administrator).

### vcpkg

In elevated command prompt run:

```bat
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install
```

### grpc and protobuf

Compile and install grpc and protobuf using vcpkg:

```bat
C:\vcpkg\vcpkg install grpc:x64-windows protobuf:x64-windows
```

This will take a long time.

### roots.pem

gRPC [requires](https://github.com/grpc/grpc/issues/16571) an environment variable to configure the trust store for SSL certificates. This repo already contains roots.pem file. Set the `GRPC_DEFAULT_SSL_ROOTS_FILE_PATH` environment variable to point to the roots.pem file in the root of this repository.

### Create a project in the Google Cloud Platform Console

If you haven't already created a project, create one now. Projects enable you to manage all Google Cloud Platform resources for your app, including deployment, access control, billing, and services.

1. Open the [Cloud Platform Console](https://console.cloud.google.com/).
2. In the drop-down menu at the top, select Create a project.
3. Give your project a name.
4. Make a note of the project ID, which might be different from the project name. The project ID is used in commands
   and in configurations.

### Enable billing for your project

If you haven't already enabled billing for your project, [enable billing now](https://console.cloud.google.com/project/_/settings). Enabling billing allows the application to consume billable resources such as Speech API calls. See [Cloud Platform Console Help](https://support.google.com/cloud/answer/6288653) for more information about billing settings.

### Enable APIs for your project

[Click here](https://console.cloud.google.com/flows/enableapi?apiid=speech&showconfirmation=true) to visit Cloud Platform Console and enable the Speech API.

### If needed, override the Billing Project

If you are using a [user account](https://cloud.google.com/docs/authentication#principals) for authentication, you need to set the `GOOGLE_CLOUD_CPP_USER_PROJECT` environment variable to the project you created in the previous step. Be aware that you must have `serviceusage.services.use` permission on the project.  Alternatively, use a service account as described next.

### Download service account credentials

These samples can use service accounts for authentication.

1. Visit the [Cloud Console](http://cloud.google.com/console), and navigate to: `API Manager > Credentials > Create credentials > Service account key`
2. Under **Service account**, select `New service account`.
3. Under **Service account name**, enter a service account name of your choosing. For example, `transcriber`.
4. Under **Role**, select `Project > Owner`.
5. Under **Key type**, leave `JSON` selected.
6. Click **Create** to create a new service account, and download the json credentials file.
7. Set the `GOOGLE_APPLICATION_CREDENTIALS` environment variable to point to your downloaded service account credentials.

See the [Cloud Platform Auth Guide](https://cloud.google.com/docs/authentication#developer_workflow) for more information.

## Bluetooth

Pair the HC-06 of the Robot Spider with your computer. PIN is `1234`. Use _Start_ → _Control Panel_ → _Hardware and Sound_ → _Devices and Printers_ to examine which COM port the HC-06 was assigned.

## Compilation

```bat
cd speech\api
cmake -S. -B.build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build .build
```

## Run

```bat
cd speech\api
.build\Debug\streaming_transcribe --language-code sl-SI --bitrate 32000 COM5
```

Use the `sl-SI` or `en` (default) language codes.

Use the same COM port assigned to HC-06 in [Bluetooth](bluetooth).

The best recognition success was observed with 32000 Hz sampling frequency and a professional microphone. 16000 Hz prooved a bit too unreliable for female and child voices.

The `streaming_transcribe` will listen for audio on default recording device for 10 minutes. It can be cancelled with `Ctrl+C`.

Recognised commands are:

### sl-SI

| Text                   | Action        |
| ---------------------- | ------------- |
| _robot pojdi naprej_   | step forward  |
| _robot pojdi nazaj_    | step backward |
| _robot pojdi levo_     | turn left     |
| _robot obrni se levo_  | turn left     |
| _robot pojdi desno_    | turn right    |
| _robot obrni se desno_ | turn right    |

### en

| Text                   | Action        |
| ---------------------- | ------------- |
| _robot go forward_     | step forward  |
| _robot go backward_    | step backward |
| _robot turn left_      | turn left     |
| _robot turn right_     | turn right    |
