#include <stdio.h>
#include <unistd.h>
#include <sstream>

// Sora
#include <sora/camera_device_capturer.h>
#include <sora/sora_client_context.h>

// CLI11
#include <CLI/CLI.hpp>

// Boost
#include <boost/optional/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include "boost/property_tree/json_parser.hpp"

#include "AudioProcessor.hpp"

struct MomoSampleConfig {
  int fd_out;
  std::string track_id;
  std::string signaling_url;
  std::string channel_id;
  std::string role = "recvonly";
  std::string client_id;
  bool video = true;
  bool audio = true;
  std::string video_codec_type;
  std::string audio_codec_type;
  boost::json::value metadata;
  boost::optional<bool> multistream = true;
};

class MomoSample : public std::enable_shared_from_this<MomoSample>,
                   public sora::SoraSignalingObserver {
 public:
  MomoSample(std::shared_ptr<sora::SoraClientContext> context,
             MomoSampleConfig config)
      : context_(context), config_(config) {}

  void Run() {
    audioProcessor_.reset(new AudioProcessor(config_.track_id, config_.fd_out));
    ioc_.reset(new boost::asio::io_context(1));

    sora::SoraSignalingConfig config;
    config.pc_factory = context_->peer_connection_factory();
    config.io_context = ioc_.get();
    config.observer = shared_from_this();
    config.signaling_urls.push_back(config_.signaling_url);
    config.channel_id = config_.channel_id;
    config.client_id = config_.client_id;
    config.multistream = config_.multistream;
    config.video = config_.video;
    config.audio = config_.audio;
    config.role = config_.role;
    config.video_codec_type = config_.video_codec_type;
    config.audio_codec_type = config_.audio_codec_type;
    config.metadata = config_.metadata;
    conn_ = sora::SoraSignaling::Create(config);

    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_guard(ioc_->get_executor());

    boost::asio::signal_set signals(*ioc_, SIGINT, SIGTERM);
    signals.async_wait(
        [this](const boost::system::error_code&, int) { conn_->Disconnect(); });

    conn_->Connect();
    ioc_->run();
  }

  void OnSetOffer(std::string offer) override {
    std::string stream_id = rtc::CreateRandomString(16);
    if (audio_track_ != nullptr) {
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>
          audio_result =
              conn_->GetPeerConnection()->AddTrack(audio_track_, {stream_id});
    }
    if (video_track_ != nullptr) {
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>
          video_result =
              conn_->GetPeerConnection()->AddTrack(video_track_, {stream_id});
    }
  }
  void OnDisconnect(sora::SoraSignalingErrorCode ec,
                    std::string message) override {
    msgout("Disconnect", message);
    /*
    if (renderer_ != nullptr) {
      renderer_.reset();
    }
    */
    ioc_->stop();
  }
  void OnNotify(std::string msg)
  override {
    msgout("Notify", msg);
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
    msgout("Track", ss.str());
    
    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
      /*
      renderer_->AddTrack(
          static_cast<webrtc::VideoTrackInterface*>(track.get()));
      */
    }
    else if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
      audioProcessor_->addTrack(
          static_cast<webrtc::AudioTrackInterface*>(track.get()));
    }
  }

  void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
    override {
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
    msgout("RemoveTrack", ss.str());

    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
      /*
      renderer_->RemoveTrack(
          static_cast<webrtc::VideoTrackInterface*>(track.get()));
      */
    }
    else if (track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind) {
      audioProcessor_->removeTrack(
          static_cast<webrtc::AudioTrackInterface*>(track.get()));
    }
  }

  void OnDataChannel(std::string label) override {}

  void msgout(std::string type, std::string msg)
  {
    fprintf(stderr, "sora_recv: %s: %s\n", type.c_str(), msg.c_str());
  }

 private:
  std::shared_ptr<sora::SoraClientContext> context_;
  MomoSampleConfig config_;
  rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track_;
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_;
  std::shared_ptr<sora::SoraSignaling> conn_;
  std::unique_ptr<boost::asio::io_context> ioc_;
  std::unique_ptr<AudioProcessor> audioProcessor_;
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
  MomoSampleConfig config;

  // redirect library output to stderr
  fflush(stdout);
  config.fd_out = dup(1);
  dup2(2, 1);

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

  CLI::App app("Momo Sample for Sora C++ SDK");

  int log_level = (int)rtc::LS_ERROR;
  auto log_level_map = std::vector<std::pair<std::string, int>>(
      {{"verbose", 0}, {"info", 1}, {"warning", 2}, {"error", 3}, {"none", 4}});
  app.add_option("--log-level", log_level, "Log severity level threshold")
      ->transform(CLI::CheckedTransformer(log_level_map, CLI::ignore_case));

  // Sora に関するオプション
  app.add_option("--signaling-url", config.signaling_url, "Signaling URL")->required();
  app.add_option("--channel-id", config.channel_id, "Channel ID")->required();
  app.add_option("--client-id", config.client_id, "Client ID");

  std::string metadata;
  app.add_option("--metadata", metadata,
                 "Signaling metadata used in connect message")
      ->check(is_json);

  app.add_option("--track-id", config.track_id, "track id to output data")->required();

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    exit(app.exit(e));
  }

  // メタデータのパース
  if (!metadata.empty()) {
    config.metadata = boost::json::parse(metadata);
  }

  if (log_level != rtc::LS_NONE) {
    rtc::LogMessage::LogToDebug((rtc::LoggingSeverity)log_level);
    rtc::LogMessage::LogTimestamps();
    rtc::LogMessage::LogThreads();
  }

  auto context =
      sora::SoraClientContext::Create(sora::SoraClientContextConfig());
  auto momosample = std::make_shared<MomoSample>(context, config);
  momosample->Run();

  return 0;
}
