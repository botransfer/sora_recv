#include <unistd.h>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "AudioSink.hpp"
#include "MsgOut.hpp"

AudioSink::AudioSink(webrtc::MediaStreamTrackInterface* _track)
  : Sink(_track),
    bits_per_sample(0),
    sample_rate(0),
    number_of_channels(0),
    number_of_frames(0),
    frame_bytes(0)
{
  track__ = static_cast<webrtc::AudioTrackInterface*>(_track);
}

AudioSink::~AudioSink()
{
  stop();
}

void
AudioSink::start(std::string path)
{
  Sink::start(path);
  track__->AddSink(this);
  f_running = true;
  MsgOut::log("AudioSink [" + track_id + "] started");
}

void
AudioSink::stop()
{
  if (! f_running) return;
  f_running = false;
  track__->RemoveSink(this);
  Sink::stop();
}

void
AudioSink::OnData(const void* audio_data,
                  int _bits_per_sample,
                  int _sample_rate,
                  size_t _number_of_channels,
                  size_t _number_of_frames)
{
  if (bits_per_sample != _bits_per_sample ||
      sample_rate != _sample_rate ||
      number_of_channels != _number_of_channels ||
      number_of_frames != _number_of_frames) {
    bits_per_sample = _bits_per_sample;
    sample_rate = _sample_rate;
    number_of_channels = _number_of_channels;
    number_of_frames = _number_of_frames;
    frame_bytes = number_of_frames * number_of_channels * bits_per_sample / 8;

    boost::property_tree::ptree node_root;
    node_root.put("bits_per_sample", bits_per_sample);
    node_root.put("sample_rate", sample_rate);
    node_root.put("number_of_channels", number_of_channels);
    node_root.put("number_of_frames", number_of_frames);
    node_root.put("track_id", track_id);
    std::stringstream ss;
    boost::property_tree::write_json(ss, node_root, false);
    MsgOut::send("datainfo", ss.str());
  }
  send_data(audio_data, frame_bytes);
}

