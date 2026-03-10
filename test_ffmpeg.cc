#include <iostream>
extern "C" {
#include <libavcodec/avcodec.h>
}

int main() {
  const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
  if (codec) {
    std::cout << "AAC encoder found: " << codec->name << std::endl;
  } else {
    std::cout << "AAC encoder NOT found" << std::endl;
  }

  codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
  if (codec) {
    std::cout << "OPUS encoder found: " << codec->name << std::endl;
  } else {
    std::cout << "OPUS encoder NOT found" << std::endl;
  }

  return 0;
}
