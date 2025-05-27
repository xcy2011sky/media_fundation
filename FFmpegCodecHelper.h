// rgb_decoder.h
#pragma once
#include <libavutil/hwcontext_d3d11va.h>
#include <mfidl.h>
#include <vector>
#include <wrl/client.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

using Microsoft::WRL::ComPtr;

class FFmpegCodecHelper {
public:
  struct FrameInfo {
    int width;
    int height;
    AVPixelFormat format;
  };

  FFmpegCodecHelper();
  ~FFmpegCodecHelper();

  bool InitializeDecoder(ID3D11Device *d3d11_device);
  bool InitializeEncoder(const char *output_path);
  bool ProcessSample(IMFSample *sample, uint8_t *out_buffer,
                     size_t buffer_size);
  FrameInfo GetFrameInfo() const;

  bool EncodeFrameFromHW();
  void FinalizeEncoding();

private:
  // FFmpeg相关成员
  AVCodecContext *m_decoder_ctx_ = nullptr;
  AVBufferRef *m_hw_decoder_ctx_ = nullptr;
  SwsContext *m_sws_ctx_ = nullptr;
  SwsContext *m_sws_ctx_encode_ = nullptr;

  AVCodecContext *m_encoder_ctx_ = nullptr;
  AVBufferRef *m_device_ctx_ = nullptr;
  AVFrame *m_encode_frame_ = nullptr;
  FILE *m_output_file_ = nullptr;

  // 帧资源
  AVFrame *m_hw_frame_ = nullptr;
  AVFrame *m_sw_frame_ = nullptr;
  AVFrame *m_rgb_frame_ = nullptr;

  // 状态信息
  FrameInfo frame_info_{};
  bool initialized_ = false;

  void SetupColorConverter();
  bool ConvertFrame(AVFrame *frame, uint8_t *out_buffer);
};