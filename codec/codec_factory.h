/*
 * codec_factory.h
 * Copyright (C) 2024 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef CODEC_FACTORY_H
#define CODEC_FACTORY_H

#include <memory>
#include <string>

#include "base/errors.h"

#include "codec.h"
#include "codec_id.h"

namespace ave {
namespace media {

class CodecFactory {
 public:
  virtual ~CodecFactory() = default;

  virtual std::vector<CodecInfo> GetSupportedCodecs() = 0;

  virtual std::shared_ptr<Codec> CreateCodecByType(CodecId codec_id,
                                                   bool encoder) = 0;
  virtual std::shared_ptr<Codec> CreateCodecByName(const std::string& name) = 0;

  virtual std::shared_ptr<Codec> CreateCodecByMime(const std::string& mime,
                                                   bool encoder) = 0;

  virtual std::string name() const = 0;
  virtual int16_t priority() const = 0;
};

status_t RegisterCodecFactory(std::shared_ptr<CodecFactory> factory);

std::shared_ptr<Codec> CreateCodecByType(CodecId codec_id, bool encoder);
std::shared_ptr<Codec> CreateCodecByName(std::string& name);

}  // namespace media
}  // namespace ave

#endif /* !CODEC_FACTORY_H */
