/*
 * test_opus_roundtrip.cc
 * Simple test to encode and decode with OpusCodec
 */

#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

extern "C" {
#include <opus.h>
}

int main() {
    const int SAMPLE_RATE = 48000;
    const int CHANNELS = 2;
    const int FRAME_SIZE = 960;  // 20ms at 48kHz
    const int BITRATE = 128000;
    
    // Create encoder
    int error = 0;
    OpusEncoder* encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, 
                                                OPUS_APPLICATION_AUDIO, &error);
    if (error != OPUS_OK) {
        std::cerr << "Failed to create encoder: " << error << std::endl;
        return 1;
    }
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(BITRATE));
    
    // Generate PCM data (1 second of 440Hz sine wave)
    std::vector<opus_int16> pcm_data;
    for (int i = 0; i < SAMPLE_RATE; i++) {
        opus_int16 sample = (opus_int16)(16000 * std::sin(2 * M_PI * 440 * i / SAMPLE_RATE));
        pcm_data.push_back(sample);  // Left channel
        pcm_data.push_back(sample);  // Right channel
    }
    
    // Encode to file
    std::ofstream out_encoded("/tmp/test_opus_raw.opus", std::ios::binary);
    std::vector<unsigned char> encoded_packet(4000);
    
    int num_frames = SAMPLE_RATE / FRAME_SIZE;
    for (int frame_num = 0; frame_num < num_frames; frame_num++) {
        const opus_int16* frame_data = pcm_data.data() + frame_num * FRAME_SIZE * CHANNELS;
        int encoded_bytes = opus_encode(encoder, frame_data, FRAME_SIZE,
                                        encoded_packet.data(), encoded_packet.size());
        
        if (encoded_bytes < 0) {
            std::cerr << "Encoding failed: " << encoded_bytes << std::endl;
            break;
        }
        
        // Write packet size (4 bytes) followed by packet data
        uint32_t packet_size = encoded_bytes;
        out_encoded.write(reinterpret_cast<const char*>(&packet_size), 4);
        out_encoded.write(reinterpret_cast<const char*>(encoded_packet.data()), encoded_bytes);
        
        std::cout << "Encoded frame " << frame_num << ": " << encoded_bytes << " bytes" << std::endl;
    }
    
    out_encoded.close();
    opus_encoder_destroy(encoder);
    
    std::cout << "Created /tmp/test_opus_raw.opus with " << num_frames << " frames" << std::endl;
    std::cout << "Format: Each frame has 4-byte size header + opus packet data" << std::endl;
    
    return 0;
}
