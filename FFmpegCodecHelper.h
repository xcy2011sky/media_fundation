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
  bool DecodeH264ToTexture(IMFSample *sample, uint8_t *out_buffer,
                           size_t buffer_size);

  bool EncodeFrameToFile();

private:
  void SetupColorConverter();
  bool ConvertFrame(AVFrame *frame, uint8_t *out_buffer);
  void FinalizeDecoding();
  void FinalizeEncoding();
  FrameInfo GetFrameInfo() const;

private:
  // FFmpeg相关成员
  AVCodecContext *m_decoder_ctx = nullptr;
  AVCodecContext *m_encoder_ctx = nullptr;
  AVBufferRef *m_hw_decoder_ctx = nullptr;
  AVBufferRef *m_hw_encoder_ctx = nullptr;
  SwsContext *m_sws_ctx_decoder = nullptr;
  SwsContext *m_sws_ctx_encoder = nullptr;

  // 帧资源
  AVFrame *m_hw_frame = nullptr;
  AVFrame *m_sw_frame = nullptr;
  AVFrame *m_rgb_frame = nullptr;
  AVFrame *m_encode_frame = nullptr;

  FILE *m_output_file = nullptr;

  // 状态信息
  FrameInfo m_frame_info{};
};