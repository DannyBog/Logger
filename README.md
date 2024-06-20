# Logger
Simple logging utility that sits in system tray (Purposefully hidden) and is able to record the screen, log key strokes and log clipboard text and files.

The recordings and log files are created in the %APPDATA%\Logger folder.

# Features
* Screen recorder that uses [H264/AVC](https://en.wikipedia.org/wiki/Advanced_Video_Coding) for video encoding and [AAC](https://en.wikipedia.org/wiki/Advanced_Audio_Coding) for audio encoding (Press `Ctrl + Alt + Shift + z` to start/stop recording)
* Key logger (Press `Ctrl + Alt + Shift + x` to start/stop key logging)
* Clipboard text logger (Press `Ctrl + Alt + Shift + c` to start/stop clipboard text logging)
* Clipboard file logger (Press `Ctrl + Alt + Shift + v` to start/stop clipboard file logging)

# Download
You can get the latest build as a zip archive here: [Logger.zip](https://github.com/user-attachments/files/15911354/Logger.zip)

# Building
* Install [Visual Studio 2022](https://visualstudio.microsoft.com/vs/)
* Open `x64 Native Tools Command Prompt for VS 2022`
* Run `build.bat`
