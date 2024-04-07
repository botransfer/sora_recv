#pragma once
#include <string>
#include <rtc_base/synchronization/mutex.h>

class MsgOut
{
public:
  static void send(std::string type, std::string msg);
  static void log(std::string msg);
  static void err(std::string msg);
};

