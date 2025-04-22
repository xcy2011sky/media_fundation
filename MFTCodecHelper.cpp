#include "MFTCodecHelper.h"
#include <mfapi.h>
#include <mferror.h>
#include <iostream>

// 构造函数
MFTCodecHelper::MFTCodecHelper() {
    // 初始化成员变量
    m_pD3D11Device = nullptr;
    m_pD3D11Context = nullptr;
    m_pH264DecoderMFT = nullptr;
    m_pH264EncoderMFT = nullptr;
}

// 析构函数
MFTCodecHelper::~MFTCodecHelper() {
    // 释放资源
    m_pD3D11Device = nullptr;
    m_pD3D11Context = nullptr;
    m_pH264DecoderMFT = nullptr;
    m_pH264EncoderMFT = nullptr;
}

// 初始化 MFT 编解码器
HRESULT MFTCodecHelper::Initialize(ID3D11Device* pD3D11Device) {
    HRESULT hr = S_OK;

    // 设置 D3D11 设备
    m_pD3D11Device = pD3D11Device;
    m_pD3D11Device->GetImmediateContext(&m_pD3D11Context);

    // 初始化 H264 解码器
    hr = InitializeH264Decoder();
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize H264 decoder." << std::endl;
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

// 解码 H264 视频流并生成 GPU 纹理
HRESULT MFTCodecHelper::DecodeH264ToTexture( ComPtr<IMFSample> pSample, DWORD cbData, ID3D11Texture2D** ppOutputTexture) {
    HRESULT hr = S_OK;

    // TODO: 实现解码逻辑
    // 1. 将输入数据封装为 IMFSample
    // 2. 使用 H264 解码器 MFT 处理解码
    // 3. 将解码后的数据转换为 D3D11 纹理

    return hr;
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
    HRESULT hr = S_OK;
    IUnknown* spDecTransformUnk = NULL;
    CHECK_HR(CoCreateInstance(CLSID_CMSH264DecoderMFT, NULL, CLSCTX_INPROC_SERVER,IID_IUnknown, (void**)&spDecTransformUnk), 
    "Failed to create H264 decoder MFT.");
    
    CHECK_HR(spDecTransformUnk->QueryInterface(IID_PPV_ARGS(&pDecoderTransform)),
        "Failed to get IMFTransform interface from H264 decoder MFT object.");
    
      MFCreateMediaType(&pDecInputMediaType);
      CHECK_HR(pMFTOutputMediaType->CopyAllItems(pDecInputMediaType), "Error copying media type attributes to decoder input media type.");
      CHECK_HR(pDecoderTransform->SetInputType(0, pDecInputMediaType, 0), "Failed to set input media type on H.264 decoder MFT.");
    
      MFCreateMediaType(&pDecOutputMediaType);
      CHECK_HR(pMFTInputMediaType->CopyAllItems(pDecOutputMediaType), "Error copying media type attributes to decoder output media type.");
      CHECK_HR(pDecoderTransform->SetOutputType(0, pDecOutputMediaType, 0), "Failed to set output media type on H.264 decoder MFT.");
    
      CHECK_HR(pDecoderTransform->GetInputStatus(0, &mftStatus), "Failed to get input status from H.264 decoder MFT.");
      if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
        printf("H.264 decoder MFT is not accepting data.\n");
        return E_FAIL;
      }
      CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL), "Failed to process FLUSH command on H.264 decoder MFT.");
      CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL), "Failed to process BEGIN_STREAMING command on H.264 decoder MFT.");
      CHECK_HR(pDecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL), "Failed to process START_OF_STREAM command on H.264 decoder MFT.");
   
    return hr;
}

// 初始化 H264 编码器
HRESULT MFTCodecHelper::InitializeH264Encoder() {
    HRESULT hr = S_OK;

    // 创建 H264 编码器 MFT
    hr = CoCreateInstance(CLSID_CMSH264EncoderMFT, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_pH264EncoderMFT));
    if (FAILED(hr)) {
        std::cerr << "Failed to create H264 encoder MFT." << std::endl;
        return hr;
    }

    // 设置编码器输入类型
    hr = MFCreateMediaType(&m_pEncoderInputType);
    if (FAILED(hr)) return hr;

    hr = m_pEncoderInputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) return hr;

    hr = m_pEncoderInputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    if (FAILED(hr)) return hr;

    hr = m_pH264EncoderMFT->SetInputType(0, m_pEncoderInputType.Get(), 0);
    if (FAILED(hr)) {
        std::cerr << "Failed to set input type for H264 encoder MFT." << std::endl;
        return hr;
    }

    // 设置编码器输出类型
    hr = MFCreateMediaType(&m_pEncoderOutputType);
    if (FAILED(hr)) return hr;

    hr = m_pEncoderOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) return hr;

    hr = m_pEncoderOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (FAILED(hr)) return hr;

    hr = m_pH264EncoderMFT->SetOutputType(0, m_pEncoderOutputType.Get(), 0);
    if (FAILED(hr)) {
        std::cerr << "Failed to set output type for H264 encoder MFT." << std::endl;
        return hr;
    }

    return hr;
}

// 创建 D3D11 纹理
HRESULT MFTCodecHelper::CreateD3D11Texture(UINT width, UINT height, DXGI_FORMAT format, ID3D11Texture2D** ppTexture) {
    HRESULT hr = S_OK;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    hr = m_pD3D11Device->CreateTexture2D(&desc, nullptr, ppTexture);
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 texture." << std::endl;
        return hr;
    }

    return hr;
}

// 处理 MFT 输出
HRESULT MFTCodecHelper::ProcessMFTOutput(IMFTransform* pMFT, IMFSample** ppOutputSample) {
    HRESULT hr = S_OK;

    MFT_OUTPUT_STREAM_INFO streamInfo = {};
    MFT_OUTPUT_DATA_BUFFER outputDataBuffer = {};
    DWORD processOutputStatus = 0;

    // 获取输出流信息
    hr = pMFT->GetOutputStreamInfo(0, &streamInfo);
    if (FAILED(hr)) return hr;

    // 创建输出样本
    hr = MFCreateSample(ppOutputSample);
    if (FAILED(hr)) return hr;

    ComPtr<IMFMediaBuffer> pBuffer;
    hr = MFCreateMemoryBuffer(streamInfo.cbSize, &pBuffer);
    if (FAILED(hr)) return hr;

    (*ppOutputSample)->AddBuffer(pBuffer.Get());

    outputDataBuffer.dwStreamID = 0;
    outputDataBuffer.pSample = *ppOutputSample;

    // 处理输出
    hr = pMFT->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        // 需要更多输入数据
        SAFE_RELEASE(*ppOutputSample);
        return hr;
    }

    if (FAILED(hr)) {
        std::cerr << "Failed to process MFT output." << std::endl;
        SAFE_RELEASE(*ppOutputSample);
        return hr;
    }

    return hr;
}