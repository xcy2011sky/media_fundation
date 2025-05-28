#include "FFmpegCodecHelper.h"
#include <iostream>

FFmpegCodecHelper::FFmpegCodecHelper() { avcodec_register_all(); }

FFmpegCodecHelper::~FFmpegCodecHelper() {
  FinalizeEncoding();
  FinalizeDecoding();
}

bool FFmpegCodecHelper::InitializeDecoder(ID3D11Device *d3d11_device) {

  // 创建硬件设备上下文
  AVHWDeviceContext *device_ctx;
  m_hw_decoder_ctx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
  device_ctx = reinterpret_cast<AVHWDeviceContext *>(m_hw_decoder_ctx->data);
  auto d3d11_ctx =
      reinterpret_cast<AVD3D11VADeviceContext *>(device_ctx->hwctx);

  // 共享D3D11设备
  d3d11_device->AddRef();
  d3d11_ctx->device = d3d11_device;

  if (av_hwdevice_ctx_init(m_hw_decoder_ctx) < 0) {
    return false;
  }

  // 初始化解码器
  const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  m_decoder_ctx = avcodec_alloc_context3(codec);
  m_decoder_ctx->hw_device_ctx = av_buffer_ref(m_hw_decoder_ctx);

  if (avcodec_open2(m_decoder_ctx, codec, nullptr) < 0) {
    return false;
  }

  // 初始化帧
  m_hw_frame = av_frame_alloc();
  m_sw_frame = av_frame_alloc();
  return true;
}

bool FFmpegCodecHelper::DecodeH264ToTexture(IMFSample *sample,
                                            uint8_t *out_buffer,
                                            size_t buffer_size) {
  // 从IMF Sample获取数据
  ComPtr<IMFMediaBuffer> media_buffer;
  BYTE *data = nullptr;
  DWORD max_len = 0, cur_len = 0;

  sample->ConvertToContiguousBuffer(&media_buffer);
  media_buffer->Lock(&data, &max_len, &cur_len);

  // 准备AVPacket
  AVPacket pkt;
  av_init_packet(&pkt);
  pkt.data = data;
  pkt.size = cur_len;

  // 处理时间戳
  LONGLONG sample_time = 0;
  sample->GetSampleTime(&sample_time);
  pkt.pts = pkt.dts = sample_time;

  // 发送到解码器
  int ret = avcodec_send_packet(m_decoder_ctx, &pkt);
  media_buffer->Unlock();

  if (ret < 0)
    return false;

  // 接收解码帧
  while (ret >= 0) {
    ret = avcodec_receive_frame(m_decoder_ctx, m_hw_frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      break;

    if (!m_frame_info.width) {
      m_frame_info.width = m_hw_frame->width;
      m_frame_info.height = m_hw_frame->height;
      SetupColorConverter();
    }

    // 转换到系统内存
    if (av_hwframe_transfer_data(m_sw_frame, m_hw_frame, 0) < 0) {
      return false;
    }

    // 转换到RGB
    return ConvertFrame(m_sw_frame, out_buffer);
  }

  return true;
}

void FFmpegCodecHelper::SetupColorConverter() {
  // 初始化SWS上下文
  m_sws_ctx_decoder = sws_getContext(
      m_frame_info.width, m_frame_info.height, AV_PIX_FMT_NV12,
      m_frame_info.width, m_frame_info.height, AV_PIX_FMT_RGB24,
      SWS_BILINEAR | SWS_ACCURATE_RND, nullptr, nullptr, nullptr);

  m_sws_ctx_encoder =
      sws_getContext(m_frame_info.width, m_frame_info.height, AV_PIX_FMT_RGB24,
                     m_frame_info.width, m_frame_info.height, AV_PIX_FMT_NV12,
                     SWS_BILINEAR, nullptr, nullptr, nullptr);

  // 准备RGB帧
  m_rgb_frame = av_frame_alloc();
  m_rgb_frame->width = m_frame_info.width;
  m_rgb_frame->height = m_frame_info.height;
  m_rgb_frame->format = AV_PIX_FMT_RGB24;
  av_frame_get_buffer(m_rgb_frame, 0);
}

bool FFmpegCodecHelper::ConvertFrame(AVFrame *frame, uint8_t *out_buffer) {
  // 执行颜色转换
  sws_scale(m_sws_ctx_decoder, frame->data, frame->linesize, 0, frame->height,
            m_rgb_frame->data, m_rgb_frame->linesize);

  // 拷贝到输出缓冲区
  const int required_size = m_frame_info.width * m_frame_info.height * 3;
  if (m_rgb_frame->linesize[0] == m_frame_info.width * 3) {
    memcpy(out_buffer, m_rgb_frame->data[0], required_size);
  } else {
    // 处理内存对齐
    for (int y = 0; y < m_frame_info.height; ++y) {
      memcpy(out_buffer + y * m_frame_info.width * 3,
             m_rgb_frame->data[0] + y * m_rgb_frame->linesize[0],
             m_frame_info.width * 3);
    }
  }

  return true;
}

bool FFmpegCodecHelper::InitializeEncoder(const char *output_path) {
  const AVCodec *encoder = avcodec_find_encoder_by_name("h264_nvenc");
  if (!encoder)
    return false;

  if (av_hwdevice_ctx_create(&m_hw_encoder_ctx, AV_HWDEVICE_TYPE_D3D11VA,
                             nullptr, nullptr, 0) < 0)
    return false;

  m_encoder_ctx = avcodec_alloc_context3(encoder);
  m_encoder_ctx->width = 1920;
  m_encoder_ctx->height = 1080;
  m_encoder_ctx->pix_fmt = AV_PIX_FMT_NV12;
  m_encoder_ctx->time_base = {1, 30};
  m_encoder_ctx->framerate = {30, 1};
  m_encoder_ctx->hw_device_ctx = av_buffer_ref(m_hw_encoder_ctx);

  if (avcodec_open2(m_encoder_ctx, encoder, nullptr) < 0)
    return false;

  m_encode_frame = av_frame_alloc();
  m_encode_frame->format = AV_PIX_FMT_NV12;
  m_encode_frame->width = 1920;
  m_encode_frame->height = 1080;
  if (av_frame_get_buffer(m_encode_frame, 32) < 0) {
    return false;
  }

  m_output_file = fopen(output_path, "wb");
  return m_output_file != nullptr;
}

bool FFmpegCodecHelper::EncodeFrameToFile() {
  if (!m_encode_frame || !m_rgb_frame || !m_sws_ctx_encoder)
    return false;

  if (av_frame_make_writable(m_encode_frame) < 0)
    return false;

  // RGB → NV12
  sws_scale(m_sws_ctx_encoder, m_rgb_frame->data, m_rgb_frame->linesize, 0,
            m_frame_info.height, m_encode_frame->data,
            m_encode_frame->linesize);

  m_encode_frame->pts++;

  if (avcodec_send_frame(m_encoder_ctx, m_encode_frame) < 0)
    return false;

  AVPacket pkt;
  av_init_packet(&pkt);

  while (avcodec_receive_packet(m_encoder_ctx, &pkt) == 0) {
    fwrite(pkt.data, 1, pkt.size, m_output_file);
    av_packet_unref(&pkt);
  }

  return true;
}

void FFmpegCodecHelper::FinalizeEncoding() {
  if (!m_encode_frame || !m_sws_ctx_encoder) {
    return;
  }
  avcodec_send_frame(m_encoder_ctx, nullptr);
  AVPacket pkt;
  av_init_packet(&pkt);
  while (avcodec_receive_packet(m_encoder_ctx, &pkt) == 0) {
    fwrite(pkt.data, 1, pkt.size, m_output_file);
    av_packet_unref(&pkt);
  }
  fclose(m_output_file);
  m_output_file = nullptr;

  avcodec_free_context(&m_encoder_ctx);
  av_frame_free(&m_encode_frame);
  av_buffer_unref(&m_hw_encoder_ctx);
}

void FFmpegCodecHelper::FinalizeDecoding() {
  av_frame_free(&m_hw_frame);
  av_frame_free(&m_sw_frame);
  av_frame_free(&m_rgb_frame);

  sws_freeContext(m_sws_ctx_decoder);
  m_sws_ctx_decoder = nullptr;

  avcodec_free_context(&m_decoder_ctx);
  av_buffer_unref(&m_hw_decoder_ctx);
  m_hw_decoder_ctx = nullptr;
}

FFmpegCodecHelper::FrameInfo FFmpegCodecHelper::GetFrameInfo() const {
  return m_frame_info;
}