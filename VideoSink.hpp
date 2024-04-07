#pragma once
#include "Sink.hpp"

class VideoSink
  : public rtc::VideoSinkInterface<webrtc::VideoFrame>,
    public Sink
{
public:
  VideoSink(webrtc::MediaStreamTrackInterface* track);
  ~VideoSink();
  void start(std::string path) override;
  void stop() override;
  void OnFrame(const webrtc::VideoFrame& frame) override;

protected:
  webrtc::VideoTrackInterface* track__;
  std::unique_ptr<uint8_t[]> img;
  int width, height, img_size;
};
