#pragma once
#include "Sink.hpp"

class AudioSink
  : public webrtc::AudioTrackSinkInterface,
    public Sink
{
public:
  AudioSink(webrtc::MediaStreamTrackInterface* track);
  ~AudioSink();
  void start(std::string path) override;
  void stop() override;
  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames) override;

protected:
  webrtc::AudioTrackInterface* track__;
  int bits_per_sample, sample_rate, number_of_channels, number_of_frames;
  size_t frame_bytes;
};
