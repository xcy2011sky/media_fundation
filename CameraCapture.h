#pragma once

#include "FFmpegCodecHelper.h"
#include "MFUtility.h"
#include <chrono>
#include <condition_variable>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <iostream>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

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

class CameraCapture {
public:
  CameraCapture();
  ~CameraCapture();

  // 初始化资源
  HRESULT Initialize();

  // 获取可用相机列表
  const std::vector<CameraInfo> &GetCameraList() const { return m_cameraList; }

  // 选择相机
  HRESULT SelectCamera(int index);

  // 获取当前选中的相机索引
  int GetSelectedCameraIndex() const { return m_selectedCameraIndex; }

  // 获取RGB数据帧
  std::vector<byte> CaptureRGBFrame();

  // 更新帧率
  void UpdateFps();

private:
  bool InitializeFFmpegCodecHelper();
  HRESULT CreateHardwareDevice();
  HRESULT EnumerateCameras();
  HRESULT CreateMediaSourceReader(const std::wstring &symbolicLink);
  void Cleanup();

private:
  // Media Foundation 相关
  ComPtr<IMFSourceReader> m_pSourceReader = nullptr;

  FFmpegCodecHelper m_codecHelper;

  // 帧率统计相关
  std::chrono::steady_clock::time_point m_lastFpsTime;
  int m_frameCount = 0;
  float m_currentFps = 0.0f;

  // 相机信息
  std::vector<CameraInfo> m_cameraList;
  int m_selectedCameraIndex = -1;

  // D3D11 相关
  ComPtr<ID3D11Device> m_d3d11_device;
  ComPtr<ID3D11DeviceContext> m_d3d11_context;
  D3D_FEATURE_LEVEL m_feature_level;
};