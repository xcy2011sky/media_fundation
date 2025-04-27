#include "CameraCapture.h"
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <vector>
#include <mfapi.h>
#include <uuids.h>

CameraCapture::CameraCapture() : m_lastFpsTime(std::chrono::steady_clock::now()){}

CameraCapture::~CameraCapture() { Cleanup(); }

HRESULT CameraCapture::Initialize() {
    HRESULT hr = S_OK;

    // 初始化Media Foundation
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return hr;

    // 枚举可用相机
    hr = EnumerateCameras();
    if (FAILED(hr)) return hr;

     // 初始化MFT
     hr = InitializeMFT();
     if (FAILED(hr)) return hr;

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
    hr = MFSetAttributeSize(pType.Get(), MF_MT_FRAME_SIZE, 1920, 1080);
    if (FAILED(hr)) {
        std::cerr << "Failed to set frame size: " << std::hex << hr << std::endl;
        return hr;
    }

    // 设置帧率
    hr = MFSetAttributeRatio(pType.Get(), MF_MT_FRAME_RATE, 30, 1);
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

std::vector<byte> CameraCapture::CaptureRGBFrame() {
    IMFSample *pSample = nullptr;
    DWORD dwFlags = 0;
    DWORD streamIndex = 0; 
    LONGLONG llTimeStamp = 0;
    std::vector<byte> outputByte;

    HRESULT hr = m_pSourceReader->ReadSample(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        &streamIndex,
        &dwFlags,
        &llTimeStamp,
        &pSample
    );

    if (SUCCEEDED(hr) && pSample) {
        m_CodecHelper.DecodeH264ToTexture(pSample, outputByte);
    }
    SAFE_RELEASE(pSample);

    return outputByte;
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