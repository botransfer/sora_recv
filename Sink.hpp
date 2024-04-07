#pragma once
#include <string>
#include <api/media_stream_interface.h>
#include <api/scoped_refptr.h>

class Sink
{
public:
  Sink(webrtc::MediaStreamTrackInterface* track); 
 virtual ~Sink();
  virtual void start(std::string path);
  virtual void stop();

  std::string track_id;
  webrtc::MediaStreamTrackInterface* track;

protected:
  bool send_data(const void* data, size_t len);
  int fd;
  bool f_running;

private:
  std::string msg_pre;
};
