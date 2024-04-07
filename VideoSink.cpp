#include <unistd.h>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <api/video/i420_buffer.h>
#include <third_party/libyuv/include/libyuv.h>
#include "VideoSink.hpp"
#include "MsgOut.hpp"

VideoSink::VideoSink(webrtc::MediaStreamTrackInterface* _track)
  : Sink(_track), width(0), height(0)
{
  track__ = static_cast<webrtc::VideoTrackInterface*>(_track);
}

VideoSink::~VideoSink()
{
  stop();
}

void
VideoSink::start(std::string path)
{
  Sink::start(path);
  track__->AddOrUpdateSink(this, rtc::VideoSinkWants());
  f_running = true;
  MsgOut::log("VideoSink [" + track_id + "] started");
}

void
VideoSink::stop()
{
  if (! f_running) return;
  f_running = false;
  track__->RemoveSink(this);
  Sink::stop();
}

void
VideoSink::OnFrame(const webrtc::VideoFrame& frame)
{
  rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(frame.video_frame_buffer()->ToI420());

  // check rotation
  if (frame.rotation() != webrtc::kVideoRotation_0) {
    buffer = webrtc::I420Buffer::Rotate(*buffer, frame.rotation());
  }

  // check width, height as they may have changed
  int _w = buffer->width();
  int _h = buffer->height();
  if (width != _w || height != _h) {
    width = _w;
    height = _h;
    img_size = width * height * 4;
    img.reset(new std::uint8_t[img_size]);

    boost::property_tree::ptree node_root;
    node_root.put("width", width);
    node_root.put("height", height);
    node_root.put("channels", 4);
    node_root.put("track_id", track_id);
    std::stringstream ss;
    boost::property_tree::write_json(ss, node_root, false);
    MsgOut::send("datainfo", ss.str());
  }

  libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(),
                     buffer->DataU(), buffer->StrideU(),
                     buffer->DataV(), buffer->StrideV(),
                     img.get(), width * 4,
                     buffer->width(), buffer->height());
  send_data(img.get(), img_size);
}
