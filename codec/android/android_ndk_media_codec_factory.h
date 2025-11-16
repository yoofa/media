/*
 * android_ndk_media_codec_factory.h
 * Copyright (C) 2025 youfa <vsyfar@gmail.com>
 *
 * Distributed under terms of the GPLv2 license.
 */

#ifndef AVE_MEDIA_ANDROID_NDK_MEDIA_CODEC_FACTORY_H_H_
#define AVE_MEDIA_ANDROID_NDK_MEDIA_CODEC_FACTORY_H_H_

#include "../codec_factory.h"

namespace ave {
namespace media {

class AndroidNdkMediaCodecFactory : public CodecFactory {
 public:
  AndroidNdkMediaCodecFactory();
  ~AndroidNdkMediaCodecFactory() override;

  std::vector<CodecInfo> GetSupportedCodecs() override;
  std::shared_ptr<Codec> CreateCodecByType(CodecId codec_id,
                                           bool encoder) override;
  std::shared_ptr<Codec> CreateCodecByName(const std::string& name) override;
  std::shared_ptr<Codec> CreateCodecByMime(const std::string& mime,
                                           bool encoder) override;

  std::string name() const override { return "android_ndk_media_codec"; }
  int16_t priority() const override { return 20; }

 private:
  std::vector<CodecInfo> supported_codecs_;
};

}  // namespace media
}  // namespace ave

#endif /* !AVE_MEDIA_ANDROID_NDK_MEDIA_CODEC_FACTORY_H_H_ */
