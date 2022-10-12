#include <stdio.h>
#include <unistd.h>

#include "AudioProcessor.hpp"

AudioProcessor::AudioProcessor(std::string track_id, int fd)
{
  track_id_ = track_id;
  fd_ = fd;
}

AudioProcessor::~AudioProcessor()
{
}


AudioProcessor::Sink::Sink(AudioProcessor* _processor,
                           webrtc::AudioTrackInterface* _track,
                           int _fd)
  : processor(_processor),
    track(_track),
    fd_(_fd)
{
  track->AddSink(this);
  track_id = track->id();
  fprintf(stderr, "AudioProcessor::Sink[%s] created\n", track_id.c_str());
}

AudioProcessor::Sink::~Sink()
{
  if (fd_ > 0) close(fd_);
  fprintf(stderr, "AudioProcessor::Sink[%s] deleted\n", track_id.c_str());
}

void
AudioProcessor::Sink::OnData(const void* audio_data,
                             int bits_per_sample,
                             int sample_rate,
                             size_t number_of_channels,
                             size_t number_of_frames)
{
  if (fd_ > 0){
    /*
    fprintf(stderr, "AudioProcessor::Sink[%s]::OnData called: n_frame=%d, ch=%d, rate=%d, %dbits/sample\n",
            track_id.c_str(),
            (int)number_of_frames,
            (int)number_of_channels,
            sample_rate,
            bits_per_sample);
    */

    size_t bytes = number_of_frames * number_of_channels * bits_per_sample / 8;
    write(fd_, audio_data, bytes);
  }
}

void
AudioProcessor::addTrack(webrtc::AudioTrackInterface* track)
{
  int fd = 0;
  if (track->id() == track_id_){
    fd = fd_;
    fprintf(stderr, "AudioProcessor: track %s START\n", track_id_.c_str());
  }
  std::unique_ptr<Sink> sink(new Sink(this, track, fd));
  webrtc::MutexLock lock(&sinks_lock_);
  sinks_.push_back(std::move(sink));
}

void
AudioProcessor::removeTrack(webrtc::AudioTrackInterface* track)
{
  webrtc::MutexLock lock(&sinks_lock_);
  auto itr_end = std::remove_if(sinks_.begin(), sinks_.end(),
                                [track](const SinkPtr& sink) {
                                  return sink->track == track;
                                });
  sinks_.erase(itr_end, sinks_.end());
  if (track->id() == track_id_){
    fprintf(stderr, "AudioProcessor: track %s STOP\n", track_id_.c_str());
    exit(0);
  }
}

void
AudioProcessor::getSink(std::string id)
{
  auto it = std::find_if(sinks_.begin(), sinks_.end(),
                         [id](const SinkPtr& sink) {
                           return sink->track_id == id;
                         });
}
