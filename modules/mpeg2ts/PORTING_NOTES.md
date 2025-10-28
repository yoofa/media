# MPEG2-TS Module Porting Notes

## Summary

Successfully ported the Android MPEG2-TS module to the AVE media library.

### Statistics

- **Original Android Code**: ~7,500 lines (ATSParser.cpp: 2704, ESQueue.cpp: 2657, AnotherPacketSource.cpp: 708, headers: ~850, CasManager: 348, HlsSampleDecryptor: 341)
- **Ported Code**: ~2,184 lines
  - ts_parser.h/cc: 952 lines
  - es_queue.h/cc: 549 lines
  - packet_source.h/cc: 539 lines
  - example.cc: 144 lines
- **Reduction**: ~71% (due to simplification and removal of Android-specific features)

### Files Created

```
modules/mpeg2ts/
├── BUILD.gn              # GN build configuration
├── README.md             # Module documentation
├── PORTING_NOTES.md      # This file
├── example.cc            # Usage example
├── packet_source.h       # PacketSource class (from AnotherPacketSource)
├── packet_source.cc
├── es_queue.h            # Elementary Stream Queue (from ElementaryStreamQueue)
├── es_queue.cc
├── ts_parser.h           # TS Parser (from ATSParser)
└── ts_parser.cc
```

## Key Changes

### 0. Dependencies

**Base Repository Integration:**
- Uses `base::BitReader` from https://github.com/yoofa/base for bitstream parsing
- Uses `base::logging.h` for LOG() macros
- Removed local copies of BitReader and logging utilities

**Buffer vs MediaFrame:**
- `Buffer`: Used for raw TS data and intermediate elementary stream buffers
- `MediaFrame`: Used for parsed access units with embedded metadata
- `MediaMeta`: Replaces Message/AMessage for metadata storage
- **Design Principle**: Raw data → Buffer, Parsed frame → MediaFrame

### 1. Naming Conventions

| Android | AVE Media |
|---------|-----------|
| `ATSParser` | `TSParser` |
| `AnotherPacketSource` | `PacketSource` |
| `ElementaryStreamQueue` | `ESQueue` |
| `ABitReader` | `BitReader` |
| `ABuffer` | `Buffer` |
| `AMessage` | `Message` |
| `MetaData` | `MediaMeta` |
| `ABitReader` | `base::BitReader` |

### 2. Namespace Changes

- From: `namespace android { ... }`
- To: `namespace ave { namespace media { namespace mpeg2ts { ... } } }`

### 3. Smart Pointer Changes

- From: `sp<T>` (Android strong pointer)
- To: `std::shared_ptr<T>` (C++ standard)

### 4. Removed Features

#### CasManager (348 lines)
- **Reason**: Android-specific Conditional Access System
- **Impact**: No support for encrypted/scrambled streams
- **Future**: Can be replaced with a generic descrambling interface if needed

#### HlsSampleDecryptor (341 lines)
- **Reason**: Android-specific HLS DRM/encryption
- **Impact**: No support for HLS sample-level encryption
- **Future**: Can be added if HLS support is required

#### Advanced Descrambling
- **Reason**: Android hardware-specific descrambling API
- **Impact**: No hardware descrambling support
- **Future**: Can be implemented with platform-specific plugins

### 5. Simplified Features

#### PCR (Program Clock Reference) Handling
- **Original**: Full PCR-based timing recovery with system time synchronization
- **Ported**: Basic PCR storage, simplified timing
- **Impact**: May have less accurate timing in some edge cases

#### Stream Format Parsing
- **Original**: Complete parsing for all audio/video formats
- **Ported**: Full support for H.264 and AAC, basic framework for others
- **Impact**: Some formats may need additional testing/implementation

### 6. Code Modernization

- Used C++14/17 features where appropriate
- Replaced `List<T>` with `std::list<T>`
- Replaced `Vector<T>` with `std::vector<T>`
- Replaced `KeyedVector<K,V>` with `std::map<K,V>`
- Used range-based for loops
- Used `nullptr` instead of `NULL`

### 7. Metadata Architecture

**Original Android:**
```cpp
sp<ABuffer> buffer = new ABuffer(size);
sp<AMessage> meta = buffer->meta();
meta->setInt64("timeUs", time_us);
meta->setObject("format", format);
```

**AVE Media:**
```cpp
// For parsed frames (access units)
auto frame = MediaFrame::CreateShared(size, MediaType::VIDEO);
frame->SetPts(base::Timestamp::Micros(time_us));
frame->SetMime(MEDIA_MIMETYPE_VIDEO_AVC);
frame->SetWidth(width);
// Meta is part of MediaFrame itself

// For raw buffers (unparsed TS/ES data)
auto buffer = std::make_shared<Buffer>(size);
// No meta needed for raw data
```

**Rationale:**
- `MediaFrame` combines data and metadata, cleaner API
- No need for separate meta() call and Message/AMessage
- Type-safe setters/getters for common fields
- Follows existing foundation/media_frame design

## API Changes

### TSParser

```cpp
// Android
ATSParser parser(flags);
parser.feedTSPacket(data, size, &event);
sp<AnotherPacketSource> source = parser.getSource(ATSParser::VIDEO);

// AVE Media
TSParser parser(flags);
parser.FeedTSPacket(data, size, &event);
std::shared_ptr<PacketSource> source = parser.GetSource(TSParser::VIDEO);
```

### PacketSource

```cpp
// Android
sp<AnotherPacketSource> source = new AnotherPacketSource(meta);
source->queueAccessUnit(buffer);
source->dequeueAccessUnit(&buffer);

// AVE Media
auto source = std::make_shared<PacketSource>(meta);
source->QueueAccessUnit(buffer);
source->DequeueAccessUnit(buffer);
```

### ESQueue

```cpp
// Android
ElementaryStreamQueue queue(ElementaryStreamQueue::H264);
queue.appendData(data, size, timeUs);
sp<ABuffer> accessUnit = queue.dequeueAccessUnit();

// AVE Media
ESQueue queue(ESQueue::Mode::H264);
queue.AppendData(data, size, time_us);
std::shared_ptr<Buffer> access_unit = queue.DequeueAccessUnit();
```

## Implementation Status

### Fully Implemented
- ✅ TS packet parsing
- ✅ PAT (Program Association Table) parsing
- ✅ PMT (Program Map Table) parsing
- ✅ PES (Packetized Elementary Stream) parsing
- ✅ H.264/AVC elementary stream parsing
- ✅ AAC/ADTS elementary stream parsing
- ✅ PacketSource buffer management
- ✅ Discontinuity handling
- ✅ Multiple program support

### Partially Implemented
- ⚠️ Other video codecs (HEVC, MPEG-2, MPEG-4) - framework in place
- ⚠️ Other audio codecs (AC3, E-AC3, MPEG Audio) - framework in place
- ⚠️ PCR-based timing - basic support only

### Not Implemented
- ❌ CAS/descrambling
- ❌ HLS sample encryption
- ❌ Teletext/subtitles
- ❌ DVB-specific features
- ❌ Multiple audio/video tracks selection

## Foundation Library Modifications

### Buffer Class Enhancement

Added `meta()` method to `foundation/buffer.h`:

```cpp
// Added to Buffer class
std::shared_ptr<Message> meta() {
  if (!meta_) {
    meta_ = std::make_shared<Message>();
  }
  return meta_;
}

private:
  std::shared_ptr<Message> meta_;  // Added member
```

This allows buffers to carry metadata similar to Android's `ABuffer`.

## Testing Recommendations

1. **Basic TS Parsing**
   - Test with standard MPEG2-TS files
   - Verify PAT/PMT parsing
   - Check stream detection

2. **Video Streams**
   - H.264 with different profiles
   - Various resolutions and frame rates
   - B-frames and GOP structures

3. **Audio Streams**
   - AAC with different sample rates
   - Mono/stereo/multichannel
   - Different bitrates

4. **Edge Cases**
   - Discontinuities (time, format changes)
   - Partial packets
   - Corrupted data
   - Multiple programs

5. **Performance**
   - Large file parsing
   - Real-time streaming
   - Memory usage

## Known Limitations

1. **No Encryption Support**: Scrambled/encrypted streams are not supported
2. **Limited Format Support**: Some audio/video formats have placeholder implementations
3. **Simplified Timing**: PCR-based timing is simplified compared to Android
4. **No Hardware Acceleration**: No hardware descrambling or decoding hooks
5. **Single Stream Selection**: No explicit API for selecting among multiple streams

## Future Improvements

1. **Complete Format Support**: Implement all audio/video format parsers
2. **Enhanced Timing**: Improve PCR-based timing and synchronization
3. **Error Resilience**: Better handling of corrupted/incomplete data
4. **Performance**: Optimize buffer management and parsing
5. **Streaming Support**: Add better support for live streaming use cases
6. **Metadata**: Extract and expose more metadata (program info, etc.)
7. **Testing**: Add comprehensive unit tests

## References

- [Android Source](https://android.googlesource.com/platform/frameworks/av/+/refs/heads/main/media/module/mpeg2ts/)
- [ISO/IEC 13818-1: MPEG-2 Systems](https://www.iso.org/standard/44169.html)
- [DVB Standards](https://www.dvb.org/standards)
- [ATSC Standards](https://www.atsc.org/standards/)

## Conclusion

The porting is successful and provides core MPEG2-TS parsing functionality suitable for most use cases. The simplified implementation removes Android-specific features while maintaining the essential parsing logic. The code is ready for integration and can be extended with additional features as needed.
