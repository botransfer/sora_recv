#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "Sink.hpp"
#include "MsgOut.hpp"

Sink::Sink(webrtc::MediaStreamTrackInterface* _track)
  : track(_track), fd(0), f_running(false)
{
  track_id = track->id();
  msg_pre = "Sink [" + track->kind() + "][" + track_id + "] ";
  MsgOut::log(msg_pre + "created");
}

Sink::~Sink()
{
  // stop();
  MsgOut::log(msg_pre + "deleted");
}

void
Sink::start(std::string path)
{
  // open fd
  fd = open(path.c_str(), O_WRONLY);
  if (fd < 0) {
    MsgOut::err(msg_pre + "failed to open file [" + path + "]");
    // XXX: handle error
  }
}

void
Sink::stop()
{
  if (fd > 0) close(fd);
  fd = 0;
  MsgOut::log(msg_pre + "stopped");
}

bool
Sink::send_data(const void* data, size_t _len)
{
  if (fd > 0) {
    // send header
    uint32_t len = htonl(_len);
    ::write(fd, (void*)&len, sizeof(len));
    // send data
    ::write(fd, data, _len);
    // XXX: check error
  }
  return true;
}
