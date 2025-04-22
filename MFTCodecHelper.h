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
#include "MFUtility.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")
using namespace Microsoft::WRL;

class MFTCodecHelper {
public:
    MFTCodecHelper();
    ~MFTCodecHelper();

    // 初始化MFT编解码器
    HRESULT Initialize(ID3D11Device* pD3D11Device);

    // 解码H264视频流并生成GPU纹理
    HRESULT DecodeH264ToTexture( ComPtr<IMFSample> pSample, DWORD cbData, ID3D11Texture2D** ppOutputTexture);

    // 编码GPU纹理为MP4文件
    HRESULT EncodeTextureToMP4(ID3D11Texture2D* pInputTexture, const std::wstring& outputFilePath);

private:
    // 初始化H264解码器
    HRESULT InitializeH264Decoder();

    // 初始化H264编码器
    HRESULT InitializeH264Encoder();

    // 创建D3D11纹理
    HRESULT CreateD3D11Texture(UINT width, UINT height, DXGI_FORMAT format, ID3D11Texture2D** ppTexture);

    // 处理MFT输出
    HRESULT ProcessMFTOutput(IMFTransform* pMFT, IMFSample** ppOutputSample);

    ComPtr<ID3D11Device> m_pD3D11Device;
    ComPtr<ID3D11DeviceContext> m_pD3D11Context;

    // 解码相关
    ComPtr<IMFMediaType> pMFTOutputMediaType;
    ComPtr<IMFTransform> pDecoderTransform=NULL;
    ComPtr<IMFMediaType> pDecInputMediaType=NULL;
    ComPtr<IMFMediaType> pDecOutputMediaType=NULL;
    DWORD mftStatus = 0;

    // 编码相关
    ComPtr<IMFTransform> m_pH264EncoderMFT;
    ComPtr<IMFMediaType> pMFTOutputMediaType;
    ComPtr<IMFMediaType> m_pEncoderOutputType;
    ComPtr<IMFSinkWriter> m_pSinkWriter;
    DWORD m_videoStreamIndex;
};