#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <stdexcept>

// Sora
#include <sora/camera_device_capturer.h>
#include <sora/sora_client_context.h>

// CLI11
#include <CLI/CLI.hpp>

// Boost
#include <boost/algorithm/string/split.hpp>  
#include <boost/algorithm/string.hpp>   
#include <boost/optional/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
namespace posix = boost::asio::posix;

#include <rtc_base/synchronization/mutex.h>
#include "AudioSink.hpp"
#include "VideoSink.hpp"
#include "MsgOut.hpp"

typedef std::unique_ptr<Sink> SinkPtr;
typedef std::vector<SinkPtr> SinkVector;

struct MomoSampleConfig {
  std::string signaling_url;
  std::string channel_id;
  std::string client_id;
  boost::json::value metadata;
};

class MomoSample : public std::enable_shared_from_this<MomoSample>,
                   public sora::SoraSignalingObserver {
 public:
  MomoSample(std::shared_ptr<sora::SoraClientContext> context,
             MomoSampleConfig config)
    : context_(context), config_(config) {}

  void Run() {
    // audioProcessor_.reset(new AudioProcessor(config_.track_id, config_.fd_out));
    ioc_.reset(new boost::asio::io_context(1));

    sora::SoraSignalingConfig config;
    config.pc_factory = context_->peer_connection_factory();
    config.io_context = ioc_.get();
    config.observer = shared_from_this();
    config.signaling_urls.push_back(config_.signaling_url);
    config.channel_id = config_.channel_id;
    config.client_id = config_.client_id;
    config.multistream = true;
    config.video = true;
    config.audio = true;
    config.role = "recvonly";
    config.metadata = config_.metadata;
    conn_ = sora::SoraSignaling::Create(config);

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard(ioc_->get_executor());

    boost::asio::signal_set signals(*ioc_, SIGINT, SIGTERM);
    signals.async_wait(
        [this](const boost::system::error_code&, int) {
          MsgOut::log("signal received");
          Stop();
        });

    // setup stdin and start reading
    stdin_.reset(new posix::stream_descriptor(*ioc_, STDIN_FILENO));
    read_stdin();

    conn_->Connect();
    ioc_->run();
  }

  void Stop() {
    MsgOut::log("shutting down");
    webrtc::MutexLock lock(&sinks_lock);
    sinks.clear();
    conn_->Disconnect();
  }

  void read_stdin() {
    boost::asio::async_read_until(*stdin_, in_streambuf_, '\n', 
                                  [this](const boost::system::error_code& error, std::size_t bytes_transferred) {
                                    std::string msg;
                                    if (! error) {
                                      in_streambuf_.sgetn(inbuf_, bytes_transferred);
                                      inbuf_[bytes_transferred - 1] = '\0';
                                      msg = std::string(inbuf_);
                                    }
                                    OnStdin(error, msg);
                                  });

  }

  void OnStdin(const boost::system::error_code& error, std::string msg){
    if (error) {
      MsgOut::err("reading from stdin failed: " + error.message());
      conn_->Disconnect();
    }
    else {
      MsgOut::log("stdin:" + msg);

      // parse input command
      while (1) { // use while to allow "break"
        std::vector<std::string> tokens;
        boost::algorithm::split(tokens, msg, boost::algorithm::is_space(), boost::token_compress_on);
        if (tokens[0] == "START"){
          if (tokens.size() < 3) {
            MsgOut::err("usage: START <track_id> <output path>");
            break;
          }
          try {
            auto& sink = getSink(tokens[1]);
            sink->start(tokens[2]);
          }
          catch (...) {
            MsgOut::err("no sink found for " + tokens[1]);
          }
        }
        else if (tokens[0] == "STOP"){
          if (tokens.size() < 2) {
            MsgOut::err("usage: STOP <track_id>");
            break;
          }
          try {
            auto& sink = getSink(tokens[1]);
            sink->stop();
          }
          catch (...) {
            MsgOut::err("no sink found for " + tokens[1]);
          }
        }
        else if (tokens[0] == "SHUTDOWN") {
          Stop();
        }

        break;
      }

      read_stdin(); // keep reading
    }
  }

  void OnSetOffer(std::string offer) override {
  }

  void OnDisconnect(sora::SoraSignalingErrorCode ec,
                    std::string message) override {
    MsgOut::log(message);
    ioc_->stop();
  }

  void OnNotify(std::string msg)
  override {
    MsgOut::send("notify", msg);
  }

  void OnPush(std::string text) override {}
  void OnMessage(std::string label, std::string data) override {}

  void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
    override {
    auto receiver = transceiver->receiver();
    auto track = receiver->track();

    // output message
    boost::property_tree::ptree node_root;
    node_root.put("id", track->id());
    node_root.put("kind", track->kind());
    boost::property_tree::ptree node_streams;
    auto stream_ids = receiver->stream_ids();
    for (auto id: stream_ids){
      boost::property_tree::ptree node;
      node.put("", id);
      node_streams.push_back(std::make_pair("", node));
    }
    node_root.add_child("streams", node_streams);
    std::stringstream ss;
    boost::property_tree::write_json(ss, node_root, false);
    MsgOut::send("addTrack", ss.str());
    
    // create Sink and add to list
    Sink* sink_raw = NULL;
    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
      sink_raw = new VideoSink(track.get());
    }
    else if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
      sink_raw = new AudioSink(track.get());
    }
    if (sink_raw != NULL) {
      SinkPtr sink(sink_raw);
      webrtc::MutexLock lock(&sinks_lock);
      sinks.push_back(std::move(sink));
    }
  }

  void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
    override {
    auto track = receiver->track();

    {
      webrtc::MutexLock lock(&sinks_lock);
      auto itr_end = std::remove_if(sinks.begin(), sinks.end(),
                                    [track](const SinkPtr& sink) {
                                      return sink->track == track;
                                    });
      sinks.erase(itr_end, sinks.end());
    }

    // output message
    boost::property_tree::ptree node_root;
    node_root.put("id", track->id());
    node_root.put("kind", track->kind());
    boost::property_tree::ptree node_streams;
    auto stream_ids = receiver->stream_ids();
    for (auto id: stream_ids){
      boost::property_tree::ptree node;
      node.put("", id);
      node_streams.push_back(std::make_pair("", node));
    }
    node_root.add_child("streams", node_streams);
    std::stringstream ss;
    boost::property_tree::write_json(ss, node_root, false);
    MsgOut::send("removeTrack", ss.str());
  }

  void OnDataChannel(std::string label) override {}

  const SinkPtr&
  getSink(std::string id)
  {
    auto const& it = std::find_if(sinks.begin(), sinks.end(),
                           [id](const SinkPtr& sink) {
                             return sink->track_id == id;
                           });
    if (it == sinks.end()) throw id;
    return *it;
  }

 private:
  std::shared_ptr<sora::SoraClientContext> context_;
  MomoSampleConfig config_;
  std::shared_ptr<sora::SoraSignaling> conn_;
  std::unique_ptr<boost::asio::io_context> ioc_;
  std::unique_ptr<posix::stream_descriptor> stdin_;
  std::unique_ptr<posix::stream_descriptor> stdout_;
  boost::asio::streambuf in_streambuf_;
  char inbuf_[1024];

  webrtc::Mutex sinks_lock;
  SinkVector sinks;
};

void add_optional_bool(CLI::App& app,
                       const std::string& option_name,
                       boost::optional<bool>& v,
                       const std::string& help_text) {
  auto f = [&v](const std::string& input) {
    if (input == "true") {
      v = true;
    } else if (input == "false") {
      v = false;
    } else if (input == "none") {
      v = boost::none;
    } else {
      throw CLI::ConversionError(input, "optional<bool>");
    }
  };
  app.add_option_function<std::string>(option_name, f, help_text)
      ->type_name("TEXT")
      ->check(CLI::IsMember({"true", "false", "none"}));
}

int
main(int argc, char* argv[])
{
  // send error outputs to stdout, so that python script can capture them
  dup2(STDOUT_FILENO, STDERR_FILENO);

  auto is_json = CLI::Validator(
      [](std::string input) -> std::string {
        boost::json::error_code ec;
        boost::json::parse(input);
        if (ec) {
          return "Value " + input + " is not JSON Value";
        }
        return std::string();
      },
      "JSON Value");

  CLI::App app("sora receiver");
  MomoSampleConfig config;
  app.add_option("--signaling-url", config.signaling_url, "Signaling URL")->required();
  app.add_option("--channel-id", config.channel_id, "Channel ID")->required();
  app.add_option("--client-id", config.client_id, "Client ID");
  std::string metadata;
  app.add_option("--metadata", metadata, "Signaling metadata used in connect message")
    ->check(is_json);
  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    exit(app.exit(e));
  }
  boost::json::value metadata_json;
  if (!metadata.empty()) {
    config.metadata = boost::json::parse(metadata);
  }

  auto context =
      sora::SoraClientContext::Create(sora::SoraClientContextConfig());
  auto momosample = std::make_shared<MomoSample>(context, config);
  momosample->Run();

  return 0;
}
