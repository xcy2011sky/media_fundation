#pragma once

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <mftransform.h>
#include <mfobjects.h>
#include "MFTCodecHelper.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")



using namespace Microsoft::WRL;

struct CameraInfo {
    std::wstring friendlyName;
    std::wstring symbolicLink;
};

class CameraCapture{
public:
    CameraCapture();
    ~CameraCapture();

    //初始化资源
    HRESULT Initialize();

    // 获取可用相机列表
    const std::vector<CameraInfo>& GetCameraList() const { return m_cameraList; }
    
    // 选择相机
    HRESULT SelectCamera(int index);
    
    // 获取当前选中的相机索引
    int GetSelectedCameraIndex() const { return m_selectedCameraIndex; }

    // 获取RGB数据帧
    std::vector<byte> CaptureRGBFrame();

    // 更新帧率
    void UpdateFps();
private:
    HRESULT InitializeMFT();
    HRESULT EnumerateCameras();
    HRESULT CreateMediaSourceReader(const std::wstring& symbolicLink);
    void Cleanup();
    
private:
    // Media Foundation 相关
    ComPtr<IMFSourceReader> m_pSourceReader = nullptr;
    ComPtr<ID3D11Device> m_pDevice = nullptr;
    ComPtr<IMFTransform> m_pMFT = nullptr;
    MFTCodecHelper m_CodecHelper;

    // 帧率统计相关
    std::chrono::steady_clock::time_point m_lastFpsTime;
    int m_frameCount = 0;
    float m_currentFps = 0.0f;

    // 相机信息
    std::vector<CameraInfo> m_cameraList;
    int m_selectedCameraIndex = -1;
};