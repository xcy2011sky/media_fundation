#include "MFTCodecHelper.h"
#include <mfapi.h>
#include <mferror.h>
#include <iostream>
#include <wmcodecdsp.h>
#include <codecapi.h>
#include <map>
#include <thread>

#define RETURN_IF_FAILED(hr) if (FAILED(hr)) { std::cerr << "Error: " << std::hex << hr << std::endl; return hr; }

// 构造函数
MFTCodecHelper::MFTCodecHelper() {
    // 初始化成员变量
    m_pD3D11Device = nullptr;
    m_pD3D11Context = nullptr;
    m_pH264DecoderMFT = nullptr;
    m_pEncoderMFT = nullptr;
}

// 析构函数
MFTCodecHelper::~MFTCodecHelper() {
    // 释放资源
    m_pD3D11Device = nullptr;
    m_pD3D11Context = nullptr;
    m_pH264DecoderMFT = nullptr;
    m_pEncoderMFT = nullptr;
}

// 初始化 MFT 编解码器
HRESULT MFTCodecHelper::Initialize(ID3D11Device* pD3D11Device) {
    HRESULT hr = S_OK;

    // 设置 D3D11 设备
    //m_pD3D11Device = pD3D11Device;
    //m_pD3D11Device->GetImmediateContext(&m_pD3D11Context);

    // 初始化 H264 解码器
    hr = InitializeH264Decoder();
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize H264 decoder." << std::endl;
        return hr;
    }
    hr = InitializeYUVDecoder();
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize YUV decoder." << std::endl;
        return hr;
    }

    // 初始化 H264 编码器
    hr = InitializeH264Encoder();
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize H264 encoder." << std::endl;
        return hr;
    }

    return hr;
}

HRESULT CreateMediaBufferFromRawData(BYTE* pData, DWORD cbData, IMFMediaBuffer** ppBuffer) {
    if (!pData || !ppBuffer) return E_INVALIDARG;

    // 1. 创建内存缓冲区
    ComPtr<IMFMediaBuffer> pBuffer;
    RETURN_IF_FAILED(MFCreateMemoryBuffer(cbData, &pBuffer));

    // 2. 锁定缓冲区并拷贝数据
    BYTE* pDest = nullptr;
    RETURN_IF_FAILED(pBuffer->Lock(&pDest, nullptr, nullptr));
    memcpy(pDest, pData, cbData);
    RETURN_IF_FAILED(pBuffer->Unlock());

    // 3. 设置有效长度
    RETURN_IF_FAILED(pBuffer->SetCurrentLength(cbData));

    *ppBuffer = pBuffer.Detach();
    return S_OK;
}

HRESULT CreateSampleFromBuffer(IMFMediaBuffer* pBuffer, IMFSample** ppSample) {
    if (!pBuffer || !ppSample) return E_INVALIDARG;

    // 1. 创建空样本
    ComPtr<IMFSample> pSample;
    RETURN_IF_FAILED(MFCreateSample(&pSample));

    // 2. 添加缓冲区到样本
    RETURN_IF_FAILED(pSample->AddBuffer(pBuffer));

    // 3. 设置时间戳（可选）
    // pSample->SetSampleTime(llTimestamp);

    *ppSample = pSample.Detach();
    return S_OK;
}

HRESULT MFTCodecHelper::DecodeH264ToTexture(std::vector<byte> buffer,std::vector<byte> &outputByte) {
    HRESULT hr = S_OK;
    
    // 1. 输入验证和准备
    if (buffer.empty()) return E_INVALIDARG;

    // 1. 将 byte* 封装为 IMFSample
    ComPtr<IMFMediaBuffer> pInputBuffer;
    RETURN_IF_FAILED(CreateMediaBufferFromRawData(buffer.data(), buffer.size(), &pInputBuffer));

    ComPtr<IMFSample> pInputSample;
    RETURN_IF_FAILED(CreateSampleFromBuffer(pInputBuffer.Get(), &pInputSample));

    byte *byte_buffer = nullptr;
    DWORD max_length{}, current_length{};
    pInputBuffer->Lock(&byte_buffer, &max_length, &current_length);

    return DecodeH264ToTexture(pInputSample.Get(),outputByte);
}

HRESULT MFTCodecHelper::DecodeH264ToTexture(IMFSample *pSample ,std::vector<byte> &outputByte) {    
    if (!pSample) return E_INVALIDARG;

    // 设置D3D设备管理器（单次初始化）
    //if (!m_bDeviceManagerInitialized) {
    //    UINT resetToken = 0;
    //    ComPtr<IMFDXGIDeviceManager> pDXGIManager;
    //    RETURN_IF_FAILED(MFCreateDXGIDeviceManager(&resetToken, &pDXGIManager));
    //    RETURN_IF_FAILED(pDXGIManager->ResetDevice(m_pD3D11Device.Get(), resetToken));
    //    m_bDeviceManagerInitialized = true;
    //}

    RETURN_IF_FAILED(ConvertH264ToRGB(pSample, outputByte));
    if(outputByte.empty()) {
        //std::cerr << "YUV sample is not ready." << std::endl;
        return E_FAIL;
    }
    // AutoReleasePtr<IMFSample> pOutputSample(rgbSample);
    // CreateD3D11Texture(DXGI_FORMAT_R8G8B8A8_UNORM,pOutputSample, ppOutputTexture);
    return S_OK;
}


// 编码 GPU 纹理为 MP4 文件
HRESULT MFTCodecHelper::EncodeTextureToMP4(ID3D11Texture2D* pInputTexture, const std::wstring& outputFilePath) {
    HRESULT hr = S_OK;

    // TODO: 实现编码逻辑
    // 1. 将输入纹理转换为 IMFSample
    // 2. 使用 H264 编码器 MFT 处理编码
    // 3. 将编码后的数据写入 MP4 文件

    return hr;
}

// 初始化 H264 解码器
HRESULT MFTCodecHelper::InitializeH264Decoder() {
    ComPtr<IMFMediaType> pMFTInputMediaType = nullptr;
    ComPtr<IMFMediaType> pMFTOutputMediaType = nullptr;
    IUnknown* spDecTransformUnk = nullptr;

    MFCreateMediaType(&pMFTInputMediaType);
    CHECK_HR(pMFTInputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Error setting video sub type.");
    CHECK_HR(pMFTInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264), "Error setting video sub type.");
    CHECK_HR(MFSetAttributeSize(pMFTInputMediaType.Get(), MF_MT_FRAME_SIZE, 1920, 1080), "Failed to set resolution");
    CHECK_HR(MFSetAttributeRatio(pMFTInputMediaType.Get(), MF_MT_FRAME_RATE, 30, 1), "Failed to set fps");
    CHECK_HR(pMFTInputMediaType->SetUINT32(MF_MT_AVG_BITRATE, 240000), "Error setting average bit rate.");
    CHECK_HR(pMFTInputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, 2), "Error setting interlace mode.");
    CHECK_HR(MFSetAttributeRatio(pMFTInputMediaType.Get(), MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Base, 1), "Failed to set profile on H264 MFT out type.");
    CHECK_HR(pMFTInputMediaType->SetDouble(MF_MT_MPEG2_LEVEL, 3.1), "Failed to set level on H264 MFT out type.\n");
    CHECK_HR(pMFTInputMediaType->SetUINT32(MF_MT_MAX_KEYFRAME_SPACING, 10), "Failed to set key frame interval on H264 MFT out type.\n");
    CHECK_HR(pMFTInputMediaType->SetUINT32(CODECAPI_AVEncCommonQuality, 100), "Failed to set H264 codec qulaity.\n");

    MFCreateMediaType(&pMFTOutputMediaType);
    CHECK_HR(pMFTInputMediaType->CopyAllItems(pMFTOutputMediaType.Get()), "Error copying media type attributes tfrom mft input type to mft output type.");
    CHECK_HR(pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_I420), "Error setting video subtype.");
    // CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_SA_D3D11_AWARE, TRUE),"Failed to set d3d11 aware.");  // 关键设置
    // CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_SA_D3D11_SHARED, TRUE),"Failed to set d3d11 shader."); // 允许共享纹理
      // Create H.264 decoder.
    CHECK_HR(CoCreateInstance(CLSID_CMSH264DecoderMFT, NULL, CLSCTX_INPROC_SERVER,IID_IUnknown, (void**)&spDecTransformUnk), "Failed to create H264 decoder MFT.");

    CHECK_HR(spDecTransformUnk->QueryInterface(IID_PPV_ARGS(&m_pH264DecoderMFT)),
    "Failed to get IMFTransform interface from H264 decoder MFT object.");

    CHECK_HR(m_pH264DecoderMFT->SetInputType(0, pMFTInputMediaType.Get(), 0), "Failed to set input media type on H.264 decoder MFT.");
    CHECK_HR(m_pH264DecoderMFT->SetOutputType(0, pMFTOutputMediaType.Get(), 0), "Failed to set output media type on H.264 decoder MFT.");

    DWORD mftStatus = 0;
    CHECK_HR(m_pH264DecoderMFT->GetInputStatus(0, &mftStatus), "Failed to get input status from H.264 decoder MFT.");
    if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
        return E_FAIL;
    }
  
    CHECK_HR(m_pH264DecoderMFT->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 decoder MFT.");
    CHECK_HR(m_pH264DecoderMFT->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 decoder MFT.");
    CHECK_HR(m_pH264DecoderMFT->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 decoder MFT.");
    return S_OK;
}

HRESULT MFTCodecHelper::InitializeYUVDecoder() {
    ComPtr<IMFMediaType> pMFTInputMediaType = nullptr;
    ComPtr<IMFMediaType> pMFTOutputMediaType = nullptr;
    IUnknown* spDecTransformUnk = nullptr;

    // 设置输入类型
    RETURN_IF_FAILED(MFCreateMediaType(&pMFTInputMediaType));
    RETURN_IF_FAILED(pMFTInputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    RETURN_IF_FAILED(pMFTInputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_I420));
    CHECK_HR(MFSetAttributeSize(pMFTInputMediaType.Get(), MF_MT_FRAME_SIZE, 1920, 1080), "Failed to set resolution");
    CHECK_HR(MFSetAttributeRatio(pMFTInputMediaType.Get(), MF_MT_FRAME_RATE, 30, 1), "Failed to set fps");
    RETURN_IF_FAILED(pMFTInputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));

    // 设置输出类型 (RGB24)
    RETURN_IF_FAILED(MFCreateMediaType(&pMFTOutputMediaType));
    CHECK_HR(pMFTInputMediaType->CopyAllItems(pMFTOutputMediaType.Get()), "Error copying media type attributes tfrom mft input type to mft output type.");
    RETURN_IF_FAILED(pMFTOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    RETURN_IF_FAILED(pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24));
    RETURN_IF_FAILED(pMFTOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));

    CHECK_HR(CoCreateInstance(CLSID_CColorConvertDMO, NULL, CLSCTX_INPROC_SERVER,IID_IUnknown, (void**)&spDecTransformUnk), "Failed to create YUV decoder MFT.");
    CHECK_HR(spDecTransformUnk->QueryInterface(IID_PPV_ARGS(&m_pYUVDecoderMFT)),"Failed to get IMFTransform interface from YUV decoder MFT object.");

    
    CHECK_HR(m_pYUVDecoderMFT->SetInputType(0, pMFTInputMediaType.Get(), 0), "Failed to set input media type on YUV decoder MFT.");
    CHECK_HR(m_pYUVDecoderMFT->SetOutputType(0, pMFTOutputMediaType.Get(), 0), "Failed to set output media type on YUV decoder MFT.");

    DWORD mftStatus = 0;
    CHECK_HR(m_pYUVDecoderMFT->GetInputStatus(0, &mftStatus), "Failed to get input status from YUV decoder MFT.");
    if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
        return E_FAIL;
    }

    return S_OK;
}

// 初始化 H264 编码器
HRESULT MFTCodecHelper::InitializeH264Encoder() {
    HRESULT hr = S_OK;

    return hr;
}

// 创建 D3D11 纹理
HRESULT MFTCodecHelper::CreateD3D11Texture(DXGI_FORMAT format,IMFSample* pSample, ID3D11Texture2D** ppTexture) {
    HRESULT hr = S_OK;

    ComPtr<IMFMediaBuffer> pBuffer;
    RETURN_IF_FAILED(pSample->GetBufferByIndex(0, &pBuffer));

    // 1. 锁定缓冲区
    BYTE* pData = nullptr;
    LONG stride = 0;
    DWORD maxLen = 0, curLen = 0;
    RETURN_IF_FAILED(pBuffer->Lock(&pData, &maxLen, &curLen));

    // 2. 获取视频格式信息
    ComPtr<IMFMediaType> pType;
    m_pH264DecoderMFT->GetOutputCurrentType(0, &pType);
    
    GUID subtype = GUID_NULL;
    UINT32 width = 0, height = 0;
    pType->GetGUID(MF_MT_SUBTYPE, &subtype);
    MFGetAttributeSize(pType.Get(), MF_MT_FRAME_SIZE, &width, &height);

    GetDefaultStride(pType.Get(), &stride);

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    D3D11_SUBRESOURCE_DATA initData = {pData, stride, 0};
    hr = m_pD3D11Device->CreateTexture2D(&desc, &initData, ppTexture);
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 texture." << std::endl;
        return hr;
    }

    return hr;
}


// 处理积压的输出数据
HRESULT MFTCodecHelper::ProcessPendingOutputs(ID3D11Texture2D** ppTexture) {
    HRESULT hr = S_OK;
    while (true) {
        MFT_OUTPUT_DATA_BUFFER output = { 0 };
        DWORD dwStatus;
        hr = m_pH264DecoderMFT->ProcessOutput(0, 1, &output, &dwStatus);
        
        if (FAILED(hr)) break;
        if (output.pSample) output.pSample->Release(); // 丢弃积压帧
    }
    return (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) ? S_OK : hr;
}

// 解码器重置
HRESULT MFTCodecHelper::ResetDecoder() {
    RETURN_IF_FAILED(m_pH264DecoderMFT->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0));
    m_bDeviceManagerInitialized = false;
    return S_OK;
}

HRESULT MFTCodecHelper::ConvertH264ToRGB(IMFSample* pInputSample, std::vector<byte>& outputByte) {
    IMFSample* pH264DecodeOutSample = NULL;
    BOOL h264DecodeTransformFlushed = FALSE;
    HRESULT getEncoderResult = S_OK;
    RETURN_IF_FAILED(m_pH264DecoderMFT->ProcessInput(0, pInputSample, 0));
    // auto hr = m_pH264DecoderMFT->ProcessInput(0, pInputSample, 0);
    // if (hr == MF_E_NOTACCEPTING) {
    //     // 解码器未就绪时先处理积压输出
    //     RETURN_IF_FAILED(ProcessPendingOutputs(nullptr));
    //     hr = m_pH264DecoderMFT->ProcessInput(0, pInputSample, 0);
    // }
    // RETURN_IF_FAILED(hr);

    // ----- H264 DEcoder transform processing loop. -----
    HRESULT getDecoderResult = S_OK;
    while (getDecoderResult == S_OK) {

        // Apply the H264 decoder transform
        getDecoderResult = GetTransformOutput(m_pH264DecoderMFT.Get(), &pH264DecodeOutSample, &h264DecodeTransformFlushed);
        //if (FAILED(getDecoderResult)) { std::cerr << "Error: " << std::hex << getDecoderResult << std::endl;}
        if (getDecoderResult != S_OK && getDecoderResult != MF_E_TRANSFORM_NEED_MORE_INPUT) {
            std::cout << "Error getting H264 decoder transform output, error code:" << getDecoderResult << std::endl;
        }

        if (h264DecodeTransformFlushed == TRUE) {
            // H264 decoder format changed. Clear the capture file and start again.
            std::cout << "H264 decoder transform flushed stream" << std::endl;
        }
        else if (pH264DecodeOutSample != NULL) {
            // Write decoded sample to capture file.
            //std::cout << "H264 decoder transform output sample." << std::endl;
            auto hr = ConvertYUVToRGB24(pH264DecodeOutSample, outputByte);
            if (hr != S_OK) {
                std::cout << "yuv decode failed." << std::endl;
            }
        }

        SAFE_RELEASE(pH264DecodeOutSample);
    }
    SAFE_RELEASE(pH264DecodeOutSample);
    return S_OK;
}

HRESULT MFTCodecHelper::ConvertYUVToRGB24(IMFSample* pInputSample, std::vector<byte>& outputByte) {
    // 处理输入
    RETURN_IF_FAILED(m_pYUVDecoderMFT->ProcessInput(0, pInputSample, 0));

    // 获取输出
    HRESULT getDecoderResult = S_OK;
    BOOL hYUVDecodeTransformFlushed = FALSE;
    IMFSample* pYUVDecodeOutSample = nullptr;
    getDecoderResult = GetTransformOutput(m_pYUVDecoderMFT.Get(), &pYUVDecodeOutSample, &hYUVDecodeTransformFlushed);
    if (getDecoderResult == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        return E_UNEXPECTED; // 需要更多输入（不应发生）
    }

    if (pYUVDecodeOutSample == nullptr) {
        SAFE_RELEASE(pYUVDecodeOutSample);
        return S_FALSE;;
    }

    // 提取RGB数据
    ComPtr<IMFMediaBuffer> pOutputBuffer;
    RETURN_IF_FAILED(pYUVDecodeOutSample->GetBufferByIndex(0, &pOutputBuffer));

    BYTE* pRGBData = nullptr;
    DWORD cbMaxLength = 0, cbCurrentLength = 0;
    RETURN_IF_FAILED(pOutputBuffer->Lock(&pRGBData, &cbMaxLength, &cbCurrentLength));
    outputByte.resize(cbCurrentLength);
    memcpy(outputByte.data(), pRGBData, cbCurrentLength);
    // static int index = 0;
    // index++;
    // std::string fileName = "./test/output" + std::to_string(index) + ".rgb";
    // FILE *fp = fopen(fileName.c_str(), "wb");
    // if (fp) {
    //     fwrite(pRGBData, 1, cbCurrentLength, fp);
    //     fclose(fp);
    // } else {
    //     std::cerr << "Failed to open output file." << std::endl;
    // }
    //std::cout << "YUV to RGB conversion successful." << std::endl;
    SAFE_RELEASE(pYUVDecodeOutSample);
    return S_OK;
}