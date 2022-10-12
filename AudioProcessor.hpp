// Boost
// #include <boost/asio.hpp>

// WebRTC
#include <api/media_stream_interface.h>
#include <api/scoped_refptr.h>
#include <rtc_base/synchronization/mutex.h>

class AudioProcessor
{
 public:
  AudioProcessor(std::string track_id, int fd);
  ~AudioProcessor();

  void addTrack(webrtc::AudioTrackInterface* track);
  void removeTrack(webrtc::AudioTrackInterface* track);

  protected:
  class Sink : public webrtc::AudioTrackSinkInterface
  {
   public:
    Sink(AudioProcessor* processor,
         webrtc::AudioTrackInterface* track,
         int fd);
    ~Sink();
    void OnData(const void* audio_data,
                int bits_per_sample,
                int sample_rate,
                size_t number_of_channels,
                size_t number_of_frames) override;
    std::string track_id;
    AudioProcessor* processor;
    rtc::scoped_refptr<webrtc::AudioTrackInterface> track;

  private:
    int fd_;
  };

  typedef std::unique_ptr<Sink> SinkPtr;
  void getSink(std::string id);

 private:
  int fd_;
  std::string track_id_;
  webrtc::Mutex sinks_lock_;
  typedef std::vector<SinkPtr> AudioTrackSinkVector;
  AudioTrackSinkVector sinks_;
  AudioProcessor* processor_;
};
