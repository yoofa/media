/*
 * dummy_codec_factory.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef MEDIA_CODEC_TEST_DUMMY_CODEC_FACTORY_H_
#define MEDIA_CODEC_TEST_DUMMY_CODEC_FACTORY_H_

#include "media/codec/codec_factory.h"

namespace ave {
namespace media {
namespace test {

class DummyCodecFactory : public CodecFactory {
 public:
  DummyCodecFactory();
  ~DummyCodecFactory() override;

  std::vector<CodecInfo> GetSupportedCodecs() override;
  std::shared_ptr<Codec> CreateCodecByType(CodecId codec_id,
                                           bool encoder) override;
  std::shared_ptr<Codec> CreateCodecByName(const std::string& name) override;
  std::shared_ptr<Codec> CreateCodecByMime(const std::string& mime,
                                           bool encoder) override;

  std::string name() const override { return "dummy"; }
  int16_t priority() const override { return 0; }

 private:
  std::vector<CodecInfo> supported_codecs_;
};

}  // namespace test
}  // namespace media
}  // namespace ave

#endif  // MEDIA_CODEC_TEST_DUMMY_CODEC_FACTORY_H_
