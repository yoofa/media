# Codec

codec是模仿Android的MediaCodec定义的一个通用的编解码器接口， 使用它来对FFmpeg / Android Ndk MediaCodec / IOS Media Foundation / intel/nvida的gpu编解码器 等进行封装。
也可以对独立的编解码库进行封装， 比如 fdkaac, opeh264, libvpx等等。

# 工作流程
>
> 和 Android MediaCodec类似
>
## 1. 先进行构造和config

```
auto codec = CodecFactory::CreateCodec(xxx);
codec->Configure(xxx);
```

## 2. 开始工作
>
> 和MediaCodec一样， 有两种工作流程
>
### 2.1 单线程循环

```
while (not end) {
// input
    auto index = codec->DequeueInputBuffer(timeout);
    std::shared_ptr<CodecBuffer> input_buffer;
    codec->DequeueInputBuffer(index, input_buffer);

    // get input data from file/network/...
    memcpy(input_buffer->data(), data, size);
    input_buffer->SetRange(xxx);
    codec->QueueInputBuffer(index);
    
// output
    auto index = codec->DequeueOutputBuffer(timeout);
    std::shared_ptr<CodecBuffer> output_buffer;
    codec->DequeueOutputBuffer(index, output_buffer);
    // do avsync ... 
    codec->ReleaseOutputBuffer(index, render);
}
```

### 2.2 callback mode

```
// a. decoder/encoder thread:
init() {
    codec->SetCallback(this);
}

HandleAnInputBuffer(index) {
codec->DequeueInputBuffer(index, input_buffer);
    // copy data to input buffer
    // get input data from file/network/...
    memcpy(input_buffer->data(), data, size);
    input_buffer->setRange(xxx);

    // queue input buffer
    codec->QueueInputBuffer(index);
}

HandleAnOutputBuffer(index) {
codec->DequeueOutputBuffer(index, output_buffer);
    // avsync 

    // queue input buffer
    codec->ReleaseOutputBuffer(index, render);
}


// b. codec internal thread, callback
OnInputBufferAvailable(index) {
    handle_thread->post([](){
        HandleAnInputBUffer(index);
    };);
}
OutputBufferAvailable(index) {
    handle_thread->post([](){
        HandleAnOutputBUffer(index);
    };);
}
```
