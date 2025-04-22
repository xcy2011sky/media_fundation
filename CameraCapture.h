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

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")


using namespace Microsoft::WRL;

struct CameraInfo {
    std::wstring friendlyName;
    std::wstring symbolicLink;
};

class CameraCapture {
public:
    ComPtr<IMFSourceReader> m_pSourceReader;
    ComPtr<ID3D11Device> m_pDevice;
    ComPtr<ID3D11DeviceContext> m_pContext;
    ComPtr<IDXGISwapChain> m_pSwapChain;
    ComPtr<ID3D11RenderTargetView> m_pRenderTargetView;
    
    // 帧率统计相关
    std::chrono::steady_clock::time_point m_lastFpsTime;
    int m_frameCount = 0;
    float m_currentFps = 0.0f;

    // 相机信息
    std::vector<CameraInfo> m_cameraList;
    int m_selectedCameraIndex = -1;

    // 新增队列和线程相关成员变量
    std::queue<ComPtr<IMFSample>> m_sampleQueue;
    std::queue<std::vector<BYTE>> m_renderQueue;
    std::mutex m_sampleMutex, m_renderMutex;
    std::condition_variable m_sampleCV, m_renderCV;
    bool m_stopThreads = false;

    // 新增MFT相关成员变量
    ComPtr<IMFTransform> m_pMFT;
    ComPtr<IMFMediaType> m_pInputType;
    ComPtr<IMFMediaType> m_pOutputType;

    HRESULT EnumerateCameras();
    HRESULT CreateMediaSourceReader(const std::wstring& symbolicLink);
    HRESULT CreateD3D11DeviceAndSwapChain();
    void UpdateFps();
    void Cleanup();
    HRESULT InitializeMFT();

public:
    CameraCapture();
    ~CameraCapture();

    // 获取可用相机列表
    const std::vector<CameraInfo>& GetCameraList() const { return m_cameraList; }
    
    // 选择相机
    HRESULT SelectCamera(int index);
    
    // 获取当前选中的相机索引
    int GetSelectedCameraIndex() const { return m_selectedCameraIndex; }

    HRESULT Initialize();
    HRESULT RenderFrame();
};