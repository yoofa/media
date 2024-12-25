/*
 * audio_codec_property.cc
 * Copyright (C) 2023 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#include "audio_codec_property.h"

namespace ave {
namespace media {

AudioCodecProperty::AudioCodecProperty()
    : codec_id(CodecId::AV_CODEC_ID_NONE),
      channels(0),
      samples_per_channel(0),
      sample_rate(0),
      bit_rate(0) {}

}  // namespace media
}  // namespace ave
