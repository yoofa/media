#include "media_sink_base.h"

namespace ave {
namespace media {

MediaFrameSink::~MediaFrameSink() {}
MediaPacketSink::~MediaPacketSink() {}

void MediaFrameSink::OnFrame(const std::shared_ptr<MediaFrame>&) {}
void MediaPacketSink::OnFrame(const std::shared_ptr<MediaPacket>&) {}

}  // namespace media
}  // namespace ave
