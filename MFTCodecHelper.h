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

template <typename T>
struct AutoReleasePtr {
    T* ptr;
    AutoReleasePtr(T* p) : ptr(p) {}
    ~AutoReleasePtr() { if (ptr) ptr->Release(); }
    operator T*() const { return ptr; }
};
class MFTCodecHelper {
public:
    MFTCodecHelper();
    ~MFTCodecHelper();

    // 初始化MFT编解码器
    HRESULT Initialize(ID3D11Device* pD3D11Device);

    // 解码H264视频流并生成GPU纹理
    HRESULT DecodeH264ToTexture(std::vector<byte> buffer, std::vector<byte> &outputByte);
    HRESULT DecodeH264ToTexture(IMFSample *pSample, std::vector<byte> &outputByte);

    // 编码GPU纹理为MP4文件
    HRESULT EncodeTextureToMP4(ID3D11Texture2D* pInputTexture, const std::wstring& outputFilePath);

    HRESULT ResetDecoder();

private:
    // 初始化H264解码器
    HRESULT InitializeH264Decoder();
    // 初始化YUV解码器
    HRESULT InitializeYUVDecoder();

    // 初始化H264编码器
    HRESULT InitializeH264Encoder();

    // 创建D3D11纹理
    HRESULT CreateD3D11Texture(DXGI_FORMAT format,IMFSample* pSample, ID3D11Texture2D** ppTexture);
    HRESULT ProcessPendingOutputs(ID3D11Texture2D** ppTexture);
    HRESULT ConvertH264ToRGB(IMFSample* pInputSample,std::vector<byte> &outputByte);

public:
    HRESULT ConvertYUVToRGB24(IMFSample* pInputSample, std::vector<byte>& outputByte);

public:
    ComPtr<ID3D11Device> m_pD3D11Device;
    ComPtr<ID3D11DeviceContext> m_pD3D11Context;

    // 解码相关
    ComPtr<IMFTransform> m_pH264DecoderMFT;
    ComPtr<IMFTransform> m_pYUVDecoderMFT;

    // 编码相关
    ComPtr<IMFTransform> m_pEncoderMFT;

    bool m_bDeviceManagerInitialized = false;
};