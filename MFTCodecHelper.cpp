#include "MFTCodecHelper.h"
#include <mfapi.h>
#include <mferror.h>
#include <iostream>
#include <wmcodecdsp.h>
#include <codecapi.h>
#include <map>
#include <thread>

#define RETURN_IF_FAILED(hr) if (FAILED(hr)) { std::cerr << "Error: " << std::hex << hr << std::endl; return hr; }

// 初始化 MFT 编解码器
HRESULT MFTCodecHelper::Initialize(ID3D11Device* pD3D11Device) {
    HRESULT hr = S_OK;

    // 初始化 H264 解码器
    hr = InitializeH264Decoder();
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize H264 decoder." << std::endl;
        return hr;
    }
    // 初始化 YUV 解码器
    hr = InitializeYUVDecoder();
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize YUV decoder." << std::endl;
        return hr;
    }

    return hr;
}

HRESULT MFTCodecHelper::DecodeH264ToTexture(IMFSample *pSample ,std::vector<byte> &outputByte) {    
    if (!pSample) return E_INVALIDARG;

    RETURN_IF_FAILED(ConvertH264ToRGB(pSample, outputByte));
    if(outputByte.empty()) {
        return E_FAIL;
    }
    return S_OK;
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
    CHECK_HR(pMFTInputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive), "Error setting interlace mode.");

    MFCreateMediaType(&pMFTOutputMediaType);
    CHECK_HR(pMFTInputMediaType->CopyAllItems(pMFTOutputMediaType.Get()), "Error copying media type attributes tfrom mft input type to mft output type.");
    CHECK_HR(pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_I420), "Error setting video subtype.");
    CHECK_HR(CoCreateInstance(CLSID_CMSH264DecoderMFT, NULL, CLSCTX_INPROC_SERVER,IID_IUnknown, (void**)&spDecTransformUnk), "Failed to create H264 decoder MFT.");
    CHECK_HR(spDecTransformUnk->QueryInterface(IID_PPV_ARGS(&m_pH264DecoderMFT)),"Failed to get IMFTransform interface from H264 decoder MFT object.");

    // 必须添加这些属性
    CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_VIDEO_CHROMA_SITING, MFVideoChromaSubsampling_MPEG2), "Error setting chroma siting.");
    CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE), "Error setting all samples independent.");
    // 明确要求正确的步幅
    LONG stride = 1920; // 根据需要调整对齐
    CHECK_HR(MFSetAttributeRatio(pMFTOutputMediaType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1), "Error setting pixel aspect ratio.");
    CHECK_HR(pMFTOutputMediaType->SetUINT32(MF_MT_DEFAULT_STRIDE, stride), "Error setting default stride.");

    CHECK_HR(m_pH264DecoderMFT->SetInputType(0, pMFTInputMediaType.Get(), 0), "Failed to set input media type on H.264 decoder MFT.");
    CHECK_HR(m_pH264DecoderMFT->SetOutputType(0, pMFTOutputMediaType.Get(), 0), "Failed to set output media type on H.264 decoder MFT.");

    // 最后发送START_OF_STREAM
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

    // 关键：设置正确的YUV色彩属性
    RETURN_IF_FAILED(pMFTInputMediaType->SetUINT32(MF_MT_VIDEO_CHROMA_SITING, MFVideoChromaSubsampling_MPEG2));
    RETURN_IF_FAILED(pMFTInputMediaType->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709)); // 或 BT601
    RETURN_IF_FAILED(pMFTInputMediaType->SetUINT32(MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709));
    
    // 设置输出类型 (RGB32)
    RETURN_IF_FAILED(MFCreateMediaType(&pMFTOutputMediaType));
    CHECK_HR(pMFTInputMediaType->CopyAllItems(pMFTOutputMediaType.Get()), "Error copying media type attributes tfrom mft input type to mft output type.");
    RETURN_IF_FAILED(pMFTOutputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    RETURN_IF_FAILED(pMFTOutputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32));
    RETURN_IF_FAILED(pMFTOutputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
    RETURN_IF_FAILED(pMFTOutputMediaType->SetUINT32(MF_MT_DEFAULT_STRIDE, 1920 * 4));

    CHECK_HR(CoCreateInstance(CLSID_CColorConvertDMO, NULL, CLSCTX_INPROC_SERVER,IID_IUnknown, (void**)&spDecTransformUnk), "Failed to create YUV decoder MFT.");
    CHECK_HR(spDecTransformUnk->QueryInterface(IID_PPV_ARGS(&m_pYUVDecoderMFT)),"Failed to get IMFTransform interface from YUV decoder MFT object.");

    // 设置类型前重置转换器
    CHECK_HR(m_pYUVDecoderMFT->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0), "Failed to process FLUSH command on YUV decoder MFT.");
    CHECK_HR(m_pYUVDecoderMFT->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0), "Failed to process BEGIN_STREAMING command on YUV decoder MFT.");
    
    CHECK_HR(m_pYUVDecoderMFT->SetInputType(0, pMFTInputMediaType.Get(), 0), "Failed to set input media type on YUV decoder MFT.");
    CHECK_HR(m_pYUVDecoderMFT->SetOutputType(0, pMFTOutputMediaType.Get(), 0), "Failed to set output media type on YUV decoder MFT.");

    DWORD mftStatus = 0;
    CHECK_HR(m_pYUVDecoderMFT->GetInputStatus(0, &mftStatus), "Failed to get input status from YUV decoder MFT.");
    if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus) {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT MFTCodecHelper::ConvertH264ToRGB(IMFSample* pInputSample, std::vector<byte>& outputByte) {
    IMFSample* pH264DecodeOutSample = NULL;
    BOOL h264DecodeTransformFlushed = FALSE;
    RETURN_IF_FAILED(m_pH264DecoderMFT->ProcessInput(0, pInputSample, 0));

    HRESULT getDecoderResult = S_OK;
    while (getDecoderResult == S_OK) {

        getDecoderResult = GetTransformOutput(m_pH264DecoderMFT.Get(), &pH264DecodeOutSample, &h264DecodeTransformFlushed);
        if (getDecoderResult != S_OK && getDecoderResult != MF_E_TRANSFORM_NEED_MORE_INPUT) {
            std::cout << "Error getting H264 decoder transform output, error code:" << getDecoderResult << std::endl;
        }

        if (h264DecodeTransformFlushed == TRUE) {
            std::cout << "H264 decoder transform flushed stream" << std::endl;
        }
        else if (pH264DecodeOutSample != NULL) {
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
    SAFE_RELEASE(pYUVDecodeOutSample);
    return S_OK;
}