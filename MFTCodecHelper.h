#pragma once
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl.h>
#include <d3d11.h>
#include <string>
#include <initguid.h>
#include <wmcodecdsp.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <vector>
#include "MFUtility.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")
using namespace Microsoft::WRL;

class MFTCodecHelper {
public:
    MFTCodecHelper() = default;
    ~MFTCodecHelper() = default;

    // 初始化MFT编解码器
    HRESULT Initialize(ID3D11Device* pD3D11Device);

    // 解码H264视频流并生成GPU纹理
    HRESULT DecodeH264ToTexture(IMFSample *pSample, std::vector<byte> &outputByte);

private:
    // 初始化H264解码器
    HRESULT InitializeH264Decoder();
    // 初始化YUV解码器
    HRESULT InitializeYUVDecoder();
    // 解码H264到RGB
    HRESULT ConvertH264ToRGB(IMFSample* pInputSample,std::vector<byte> &outputByte);

public:
    // 解码YUV到RGB
    HRESULT ConvertYUVToRGB24(IMFSample* pInputSample, std::vector<byte>& outputByte);

public:
    // 解码相关
    ComPtr<IMFTransform> m_pH264DecoderMFT = nullptr;
    ComPtr<IMFTransform> m_pYUVDecoderMFT = nullptr;
};