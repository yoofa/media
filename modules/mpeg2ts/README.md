# MPEG2-TS Module

This module provides MPEG2 Transport Stream (MPEG2-TS) parsing capabilities.

## Overview

This is a port of the Android MPEG2-TS module from the Android Open Source Project,
adapted to fit the coding style and architecture of this media library.

### Original Source

- Original Android source: https://android.googlesource.com/platform/frameworks/av/+/refs/heads/main/media/module/mpeg2ts/
- Original Copyright: Copyright (C) 2010 The Android Open Source Project
- Original License: Apache License 2.0

### Changes from Android Version

1. **Naming Convention**: Removed Android-specific naming prefixes (e.g., `ATSParser` → `TSParser`, `AnotherPacketSource` → `PacketSource`)
2. **Namespace**: Changed from `android` namespace to `ave::media::mpeg2ts`
3. **Dependencies**: Replaced Android-specific classes with this library's equivalents:
   - `ABuffer` → `Buffer`
   - `AMessage` → `Message`
   - `MetaData` → `MediaMeta`
   - `sp<>` (strong pointer) → `std::shared_ptr<>`
4. **Code Style**: Adapted to follow this project's coding style (Google C++ Style Guide)
5. **Removed Features**:
   - CAS (Conditional Access System) support - Android-specific encryption
   - HLS Sample Decryptor - Android-specific DRM feature
   - Some advanced descrambling features

## Architecture

### Data Flow

```
TS Packets (188 bytes, raw Buffer)
    ↓
TSParser (parses PAT/PMT/PES)
    ↓
ESQueue (assembles elementary streams)
    ↓
MediaFrame (parsed access units with meta)
    ↓
PacketSource (buffered frames)
    ↓
MediaSource interface
```

**Key Design:**
- **Buffer**: Used for raw TS packet data and intermediate ES data
- **MediaFrame**: Used for parsed access units with metadata
- **MediaMeta**: Embedded in MediaFrame for format information

### Components

#### TSParser

Main MPEG2-TS parser class. Responsible for:
- Parsing TS packets (188 bytes each)
- Extracting Program Association Table (PAT)
- Extracting Program Map Table (PMT)
- Managing elementary stream parsers
- Handling PES (Packetized Elementary Stream) packets
- Uses `base::BitReader` for bitstream parsing

#### ESQueue

Elementary Stream Queue. Responsible for:
- Buffering raw elementary stream data in `Buffer`
- Assembling access units from ES data
- Creating `MediaFrame` objects with metadata
- Supporting multiple codec types:
  - Video: H.264/AVC, H.265/HEVC, MPEG-2, MPEG-4
  - Audio: AAC (ADTS), AC3, E-AC3, AC4, MPEG Audio

#### PacketSource

Packet source buffer. Responsible for:
- Buffering `MediaFrame` objects (not raw buffers)
- Managing discontinuities
- Providing MediaSource interface
- Thread-safe frame management
- Stores metadata in `MediaMeta` (not Message)

## Usage Example

```cpp
#include "modules/mpeg2ts/ts_parser.h"

using namespace ave::media;
using namespace ave::media::mpeg2ts;

// Create parser
auto parser = std::make_shared<TSParser>();

// Feed TS packets (188 bytes each)
const uint8_t* ts_data = ...; // Your TS data
size_t packet_count = data_size / 188;

for (size_t i = 0; i < packet_count; ++i) {
    TSParser::SyncEvent event(i * 188);
    status_t err = parser->FeedTSPacket(ts_data + i * 188, 188, &event);
    if (err != OK) {
        // Handle error
    }
}

// Get video source (returns MediaSource with MediaFrame output)
auto video_source = parser->GetSource(TSParser::VIDEO);
if (video_source) {
    // Read video frames as MediaFrame
    std::shared_ptr<MediaFrame> frame;
    status_t err = video_source->Read(frame, nullptr);
    if (err == OK) {
        // Frame metadata is in MediaMeta (part of MediaFrame)
        int64_t pts_us = frame->pts().us();
        uint32_t width = frame->width();
        uint32_t height = frame->height();
        const char* mime = frame->mime();
        // Process frame data...
        uint8_t* data = frame->data();
        size_t size = frame->size();
    }
}

// Get audio source
auto audio_source = parser->GetSource(TSParser::AUDIO);
if (audio_source) {
    std::shared_ptr<MediaFrame> frame;
    status_t err = audio_source->Read(frame, nullptr);
    if (err == OK) {
        int64_t pts_us = frame->pts().us();
        uint32_t sample_rate = frame->sample_rate();
        ChannelLayout channels = frame->channel_layout();
        // Process audio data...
    }
}
```

## Stream Type Support

### Video Codecs
- H.264/AVC (STREAMTYPE_H264 = 0x1b)
- H.265/HEVC (STREAMTYPE_H265 = 0x24)
- MPEG-1 Video (STREAMTYPE_MPEG1_VIDEO = 0x01)
- MPEG-2 Video (STREAMTYPE_MPEG2_VIDEO = 0x02)
- MPEG-4 Video (STREAMTYPE_MPEG4_VIDEO = 0x10)

### Audio Codecs
- AAC ADTS (STREAMTYPE_MPEG2_AUDIO_ADTS = 0x0f)
- AC-3 (STREAMTYPE_AC3 = 0x81)
- E-AC-3 (STREAMTYPE_EAC3 = 0x87)
- MPEG Audio Layer I/II/III (STREAMTYPE_MPEG1_AUDIO = 0x03, STREAMTYPE_MPEG2_AUDIO = 0x04)

## Implementation Notes

### Simplified Features

This is a simplified port focusing on core MPEG2-TS parsing functionality:

1. **H.264 and AAC**: Full support for these most common codecs
2. **Other Codecs**: Basic framework in place, but may need additional testing/implementation
3. **No CAS/DRM**: Removed Android-specific conditional access and DRM features
4. **Basic PCR**: Simplified Program Clock Reference handling
5. **Discontinuity**: Basic support for time/format discontinuities

### Future Enhancements

Potential areas for improvement:
- Complete implementation of all audio/video codec parsers in ESQueue
- Enhanced PCR-based timing recovery
- Better error resilience
- Support for multiple programs
- Teletext and subtitle support
- Additional metadata extraction

## License

This module maintains the original Apache 2.0 license from the Android Open Source Project,
with modifications distributed under GPLv2 license as per the project requirements.

## References

- [ISO/IEC 13818-1: MPEG-2 Systems](https://www.iso.org/standard/44169.html)
- [Android MPEG2-TS Module](https://android.googlesource.com/platform/frameworks/av/+/refs/heads/main/media/module/mpeg2ts/)
- [MPEG Transport Stream on Wikipedia](https://en.wikipedia.org/wiki/MPEG_transport_stream)
