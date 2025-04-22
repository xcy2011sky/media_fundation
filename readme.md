# Media Foundation Camera Capture Project

This project demonstrates how to capture video frames from a camera using Media Foundation and render them using Direct3D 11.

## Features

- Enumerates available cameras.
- Captures video frames in H.264 format.
- Decodes H.264 frames to RGB32 format using an MFT (Media Foundation Transform).
- Renders decoded frames using Direct3D 11.

## Directory Structure

- `CameraCapture.cpp`: Main implementation file.
- `build/`: Directory for build artifacts (ignored by Git).
- `.vscode/`: Configuration files for Visual Studio Code (ignored by Git).

## Setup

1. Ensure you have the necessary dependencies installed:
   - Windows SDK
   - Media Foundation libraries

2. Build the project using your preferred C++ compiler.

3. Run the executable to start capturing and rendering video frames.

## Notes

- The project uses multi-threading for capturing, processing, and rendering frames.
- FPS is updated every second and printed to the console.

For more details, refer to the source code comments and documentation.