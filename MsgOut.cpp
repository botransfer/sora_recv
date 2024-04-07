#include "MsgOut.hpp"

static webrtc::Mutex msg_lock;

void
MsgOut::send(std::string type, std::string msg)
{
  webrtc::MutexLock lock(&msg_lock);
  printf("sora_recv:%s:%s\n", type.c_str(), msg.c_str());
  fflush(stdout);
}

void
MsgOut::log(std::string msg)
{
  send("log", msg);
}

void
MsgOut::err(std::string msg)
{
  send("ERR", msg);
}
