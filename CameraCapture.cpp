#include "CameraCapture.h"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <vector>
#include <mfapi.h>
#include <uuids.h>




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
// 新增线程函数

void ProcessThread(CameraCapture* capture);
void RenderThread(CameraCapture* capture);

CameraCapture::CameraCapture() : m_lastFpsTime(std::chrono::steady_clock::now()), m_stopThreads(false) {}

CameraCapture::~CameraCapture() { Cleanup(); }

HRESULT CameraCapture::Initialize() {
    HRESULT hr = S_OK;

    // 初始化Media Foundation
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return hr;

    // 枚举可用相机
    hr = EnumerateCameras();
    if (FAILED(hr)) return hr;

    // 创建D3D11设备和交换链
    hr = CreateD3D11DeviceAndSwapChain();
    if (FAILED(hr)) return hr;

     // 初始化MFT
     hr = InitializeMFT();
     if (FAILED(hr)) return hr;

    // 启动线程
    // std::thread processThread(ProcessThread, this);
    // std::thread renderThread(RenderThread, this);

    // // 等待线程结束
    // processThread.join();
    // renderThread.join();

    return hr;
}

HRESULT CameraCapture::EnumerateCameras() {
    HRESULT hr = S_OK;
    ComPtr<IMFAttributes> pAttributes;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;

    // 创建属性
    hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

    // 设置设备类型为视频捕获设备
    hr = pAttributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );
    if (FAILED(hr)) return hr;

    // 枚举设备
    hr = MFEnumDeviceSources(pAttributes.Get(), &ppDevices, &count);
    if (FAILED(hr)) return hr;

    // 获取每个设备的信息
    for (UINT32 i = 0; i < count; i++) {
        CameraInfo info;
        
        // 获取友好名称
        WCHAR* friendlyName = nullptr;
        UINT32 friendlyNameLength = 0;
        hr = ppDevices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
            &friendlyName,
            &friendlyNameLength
        );
        if (SUCCEEDED(hr)) {
            info.friendlyName = friendlyName;
            CoTaskMemFree(friendlyName);
        }

        // 获取符号链接
        WCHAR* symbolicLink = nullptr;
        UINT32 symbolicLinkLength = 0;
        hr = ppDevices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
            &symbolicLink,
            &symbolicLinkLength
        );
        if (SUCCEEDED(hr)) {
            info.symbolicLink = symbolicLink;
            CoTaskMemFree(symbolicLink);
        }

        m_cameraList.push_back(info);
    }

    // 释放设备数组
    for (UINT32 i = 0; i < count; i++) {
        ppDevices[i]->Release();
    }
    CoTaskMemFree(ppDevices);

    return hr;
}

HRESULT CameraCapture::SelectCamera(int index) {
    if (index < 0 || index >= static_cast<int>(m_cameraList.size())) {
        return E_INVALIDARG;
    }

    m_selectedCameraIndex = index;
    
    // 如果已经初始化了源读取器，先清理
    if (m_pSourceReader) {
        m_pSourceReader->Flush(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
        m_pSourceReader.Reset();
    }

    // 创建新的源读取器
    return CreateMediaSourceReader(m_cameraList[index].symbolicLink);
}


HRESULT CameraCapture::CreateMediaSourceReader(const std::wstring& symbolicLink) {
    HRESULT hr = S_OK;
    ComPtr<IMFAttributes> pAttributes;
    ComPtr<IMFMediaSource> pMediaSource;

    // 创建属性
    hr = MFCreateAttributes(&pAttributes, 2);
    if (FAILED(hr)) {
        std::cerr << "Failed to create attributes: " << std::hex << hr << std::endl;
        return hr;
    }

    hr=pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        std::cerr << "Failed to set source type: " << std::hex << hr << std::endl;
        return hr;
    }
    // 设置符号链接
    hr = pAttributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, symbolicLink.c_str());
    if (FAILED(hr)) {
        std::cerr << "Failed to set symbolic link: " << std::hex << hr << std::endl;
        return hr;
    }

    // 创建媒体源
    hr = MFCreateDeviceSource(pAttributes.Get(), &pMediaSource);
    if (FAILED(hr)) {
        std::cerr << "Failed to create device source: " << std::hex << hr << std::endl;
        return hr;
    }

    // 创建源读取器
    hr = MFCreateSourceReaderFromMediaSource(pMediaSource.Get(), pAttributes.Get(), &m_pSourceReader);
    if (FAILED(hr)) {
        std::cerr << "Failed to create source reader: " << std::hex << hr << std::endl;
        return hr;
    }

    // 设置输出类型为H264
    ComPtr<IMFMediaType> pType;
    hr = MFCreateMediaType(&pType);
    if (FAILED(hr)) {
        std::cerr << "Failed to create media type: " << std::hex << hr << std::endl;
        return hr;
    }

    hr = pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) {
        std::cerr << "Failed to set major type: " << std::hex << hr << std::endl;
        return hr;
    }

    hr = pType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (FAILED(hr)) {
        std::cerr << "Failed to set subtype: " << std::hex << hr << std::endl;
        return hr;
    }

    // 设置分辨率
    hr = MFSetAttributeSize(pType.Get(), MF_MT_FRAME_SIZE, 3840, 2160);
    if (FAILED(hr)) {
        std::cerr << "Failed to set frame size: " << std::hex << hr << std::endl;
        return hr;
    }

    // 设置帧率
    hr = MFSetAttributeRatio(pType.Get(), MF_MT_FRAME_RATE, 25, 1);
    if (FAILED(hr)) {
        std::cerr << "Failed to set frame rate: " << std::hex << hr << std::endl;
        return hr;
    }

    hr = m_pSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType.Get());
    if (FAILED(hr)) {
        std::cerr << "Failed to set current media type: " << std::hex << hr << std::endl;
        return hr;
    }

    return hr;
}

HRESULT CameraCapture::RenderFrame() {
        ComPtr<IMFSample> pSample;
        DWORD dwFlags = 0;
        DWORD streamIndex = 0;
        LONGLONG llTimeStamp = 0;

        HRESULT hr = m_pSourceReader->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            &streamIndex,
            &dwFlags,
            &llTimeStamp,
            &pSample
        );

      /*  if (SUCCEEDED(hr) && pSample) {
            std::lock_guard<std::mutex> lock(m_sampleMutex);
           m_sampleQueue.push(pSample);
            m_sampleCV.notify_one();
        }*/
     
    
    return S_OK;
}

HRESULT CameraCapture::CreateD3D11DeviceAndSwapChain() {
    HRESULT hr = S_OK;
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 3840;
    sd.BufferDesc.Height = 2160;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 25;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = GetConsoleWindow();
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &m_pSwapChain,
        &m_pDevice,
        &featureLevel,
        &m_pContext
    );
    if (FAILED(hr)) return hr;


    ComPtr<ID3D11Texture2D> pBackBuffer;
    hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (FAILED(hr)) return hr;

    hr = m_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_pRenderTargetView);
    if (FAILED(hr)) return hr;

    return hr;
}

void CameraCapture::UpdateFps() {
    m_frameCount++;
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_lastFpsTime).count();

    if (elapsed >= 1000) { 
        m_currentFps = (float)m_frameCount * 1000.0f / elapsed;
        std::cout << "Current FPS: " << m_currentFps << std::endl;
        m_frameCount = 0;
        m_lastFpsTime = currentTime;
    }
}

void CameraCapture::Cleanup() {
    m_stopThreads = true;
    m_sampleCV.notify_all();
    m_renderCV.notify_all();

    if (m_pSourceReader) {
        m_pSourceReader->Flush(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
    }
    MFShutdown();
}

// 新增MFT初始化函数
HRESULT CameraCapture::InitializeMFT() {
    HRESULT hr = S_OK;
     hr= m_CodecHelper.Initialize(m_pDevice.Get());
     if(hr!=S_OK){
         std::cout<<"MFT初始化失败"<<std::endl;
     }
      return hr;
    
}


void ProcessThread(CameraCapture* capture) {
    while (!capture->m_stopThreads) {
        std::unique_lock<std::mutex> lock(capture->m_sampleMutex);
        capture->m_sampleCV.wait(lock, [capture] { return !capture->m_sampleQueue.empty() || capture->m_stopThreads; });

        if (!capture->m_sampleQueue.empty()) {
            ComPtr<IMFSample> pSample = capture->m_sampleQueue.front();
            capture->m_sampleQueue.pop();
            lock.unlock();
            capture->m_CodecHelper.DecodeH264ToTexture(pSample, 0, nullptr); // TODO: 实现解码逻辑
        

            std::lock_guard<std::mutex> renderLock(capture->m_renderMutex);
            capture->m_renderQueue.push();
            capture->m_renderCV.notify_one();
        }
    }
}

void RenderThread(CameraCapture* capture) {
    while (!capture->m_stopThreads) {
        std::unique_lock<std::mutex> lock(capture->m_renderMutex);
        capture->m_renderCV.wait(lock, [capture] { return !capture->m_renderQueue.empty() || capture->m_stopThreads; });

        if (!capture->m_renderQueue.empty()) {
            std::vector<BYTE> frameData = capture->m_renderQueue.front();
            capture->m_renderQueue.pop();
            lock.unlock();

            // 创建Direct3D 11纹理
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = 3840;
            desc.Height = 2160;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
            desc.CPUAccessFlags = 0;

            ComPtr<ID3D11Texture2D> pTexture;
            HRESULT hr = capture->m_pDevice->CreateTexture2D(&desc, nullptr, &pTexture);
            if (FAILED(hr)) continue;

            // 更新纹理数据
            capture->m_pContext->UpdateSubresource(pTexture.Get(), 0, nullptr, frameData.data(), 3840 * 4, 0);

            // 设置渲染目标
            capture->m_pContext->OMSetRenderTargets(1, capture->m_pRenderTargetView.GetAddressOf(), nullptr);

            // 获取渲染目标资源
            ComPtr<ID3D11Resource> pRenderTargetResource;
            capture->m_pRenderTargetView->GetResource(&pRenderTargetResource);

            // 复制纹理到渲染目标
            capture->m_pContext->CopyResource(pRenderTargetResource.Get(), pTexture.Get());

            // 提交帧
            hr = capture->m_pSwapChain->Present(0, 0);
            if (FAILED(hr)) continue;

            capture->UpdateFps();
        }
    }
}
