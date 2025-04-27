#include <windows.h>
#include <iostream>
#include "CameraCapture.h"
#include "Render.h"
#include <conio.h>

using namespace Microsoft::WRL;

int main() {
    HRESULT hr = S_OK;
    CameraCapture capture;

    // 初始化摄像头捕获
    hr = capture.Initialize();
    if (FAILED(hr)) {
        std::cout << "Failed to initialize camera capture" << std::endl;
        return hr;
    }

    // 显示可用相机列表
    const auto& cameraList = capture.GetCameraList();
    if (cameraList.empty()) {
        std::cout << "No cameras found" << std::endl;
        return E_FAIL;
    }

    //选择Color相机
    std::cout << "Available cameras:" << std::endl;
    int selectedCamera = -1;
    for (size_t i = 0; i < cameraList.size(); ++i) {
        auto name = cameraList[i].friendlyName;
        if(name.find(L"RGB") != std::wstring::npos || name.find(L"Color") != std::wstring::npos){
            selectedCamera = i;
        }
        std::wcout << i << ": " << cameraList[i].friendlyName << std::endl;
    }

    // 初始化选中的相机
    hr = capture.SelectCamera(selectedCamera);
    if (FAILED(hr)) {
        std::cout << "Failed to select camera" << std::endl;
        return hr;
    }

    std::cout << "Press ESC to exit" << std::endl;

    Render render("Camera Capture", 1920, 1080);
    std::vector<byte> rgbData;
    // 主循环
    while (!render.isWindowsClose()) {
         rgbData = capture.CaptureRGBFrame();
         render.RenderFrame();
        if(!rgbData.empty()) {
            render.updateRGBTexture(1920, 1080, rgbData.data());
        }

        // 检查是否按下ESC键退出
        if (_kbhit() && _getch() == 27) {
            break;
        }
        capture.UpdateFps();
    }


    return 0;
}
