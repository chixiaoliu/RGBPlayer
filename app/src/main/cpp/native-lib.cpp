#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#define  LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "FFmpeg",__VA_ARGS__)

SLEngineItf createSLEngin();

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/jni.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
static SLObjectItf engineSL = NULL;
static SLEngineItf  createSLEngine()
{
    SLresult re;
    SLEngineItf en;

    //1 创建 SLObjectItf
    re = slCreateEngine(&engineSL, 0, 0, 0, 0, 0);
    if(re != SL_RESULT_SUCCESS)
    {
        return NULL;
    }

    //2 实现 SLObjectItf
    re = (*engineSL)->Realize(engineSL, SL_BOOLEAN_FALSE);
    if(re != SL_RESULT_SUCCESS){
        return NULL;
    }

    //3 获取SLEngineItf对象
    re = (*engineSL)->GetInterface(engineSL, SL_IID_ENGINE, &en);
    if(re != SL_RESULT_SUCCESS){
        return NULL;
    }

    return en;
}
void pcmcall(SLAndroidSimpleBufferQueueItf bf,void *context) {
    static FILE *fp = NULL;
    static char *buf = NULL;

    if(!buf)
    {
        buf = new char[1024 * 1024];
    }

    if(!fp)
    {
        fp = fopen("/sdcard/storage/emulated/0/test.aac", "rb");
    }

    if(!fp){
        return;
    }

    if(feof(fp) == 0){//如果未读到文件末尾
        //每次读取1024个字节
        int len = fread(buf, 1, 1024, fp);
        if(len > 0){
            //添加到队列
            (*bf)->Enqueue(bf, buf, len);
        }
    }
}
int r2d(AVRational r) {
    return r.num == 0 || r.den == 0 ? (double) 0 : r.num / (double) r.den;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_withyang_rgbplayer_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "PlayRGB";
    return env->NewStringUTF(hello.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_withyang_rgbplayer_RGBPlayerView_open(JNIEnv *env, jobject thiz, jstring url,
                                               jobject surface) {

    const char *path = env->GetStringUTFChars(url, 0);
    av_register_all();
    AVFormatContext *avFormatContext = NULL;
    int result = avformat_open_input(&avFormatContext, path, 0, 0);
    if (result != 0) {
        LOGE("avformat_open_input failed!: %s", av_err2str(result));
    }
    result = avformat_find_stream_info(avFormatContext, 0);
    if (result != 0) {
        LOGE("avformat_find_stream_info failed!: %s", av_err2str(result));
    }

    int fps = 0, videoStreamIndex = 0;
    for (int i = 0; i < avFormatContext->nb_streams; ++i) {
        AVStream *avStream = avFormatContext->streams[i];
        if (avFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;//找到视频流Index
            LOGE("fps = %d, width = %d height = %d codeid = %d pixformat = %d", fps,
                 avStream->codecpar->width,
                 avStream->codecpar->height,
                 avStream->codecpar->codec_id,
                 avStream->codecpar->format);
        }
    }
//软解码
    AVCodec *vcodec = avcodec_find_decoder(
            avFormatContext->streams[videoStreamIndex]->codecpar->codec_id);
    if (!vcodec) {
        LOGE("avcodec_find failed");
    }

    // 解码器初始化
    AVCodecContext *vc = avcodec_alloc_context3(vcodec);

    // 解码器参数赋值
    avcodec_parameters_to_context(vc, avFormatContext->streams[videoStreamIndex]->codecpar);

    //打开解码器
    result = avcodec_open2(vc, 0, 0);
    LOGE("vc timebase = %d/ %d", vc->time_base.num, vc->time_base.den);

    if (result != 0) {
        LOGE("avcodec_open2 video failed!");
    }


    //开始解码
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    //2 像素格式转换的上下文
    SwsContext *vctx = NULL;

    int outwWidth = 1280;
    int outHeight = 720;
    char *rgb = new char[1920 * 1080 * 4];

    //4 显示窗口初始化
    ANativeWindow *nwin = ANativeWindow_fromSurface(env, surface);
    ANativeWindow_setBuffersGeometry(nwin, outwWidth, outHeight, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer wbuf;

    for (;;) {

        int re = av_read_frame(avFormatContext, pkt);
        if (re != 0) {
            LOGE("读取到结尾处！");
            break;
        }

        AVCodecContext *cc = vc;
        //1 发送到线程中解码
        re = avcodec_send_packet(cc, pkt);

        //清理
        int p = pkt->pts;

        av_packet_unref(pkt);

        if (re != 0) {
            LOGE("avcodec_send_packet failed: %s", av_err2str(result));
            continue;
        }

        //每一帧可能对应多个帧数据，所以要遍历取
        for (;;) {
            //2 解帧数据
            re = avcodec_receive_frame(cc, frame);
            if (re != 0) {
                break;
            }
            LOGE("avcodec_receive_frame %lld", frame->pts);

            //如果是视频帧
            if (cc == vc) {
                //3 初始化像素格式转换的上下文
                vctx = sws_getCachedContext(vctx,
                                            frame->width,
                                            frame->height,
                                            (AVPixelFormat) frame->format,
                                            outwWidth,
                                            outHeight,
                                            AV_PIX_FMT_RGBA,
                                            SWS_FAST_BILINEAR,
                                            0, 0, 0);

                if (!vctx) {
                    LOGE("sws_getCachedContext failed!");
                } else {
                    uint8_t *data[AV_NUM_DATA_POINTERS] = {0};
                    data[0] = (uint8_t *) rgb;
                    int lines[AV_NUM_DATA_POINTERS] = {0};
                    lines[0] = outwWidth * 4;
                    int h = sws_scale(vctx,
                                      (const uint8_t **) frame->data,
                                      frame->linesize,
                                      0,
                                      frame->height,
                                      data,
                                      lines);
                    LOGE("sws_scale = %d", h);

                    if (h > 0) {
                        ANativeWindow_lock(nwin, &wbuf, 0);
                        uint8_t *dst = (uint8_t *) wbuf.bits;
                        memcpy(dst, rgb, outwWidth * outHeight * 4);
                        ANativeWindow_unlockAndPost(nwin);
                    }
                }

            }
        }
    }


    delete rgb;
    //关闭上下文
    avformat_close_input(&avFormatContext);
    env->ReleaseStringUTFChars(url, path);
}extern "C"
JNIEXPORT void JNICALL
Java_com_withyang_rgbplayer_MainActivity_palyPcm(JNIEnv *env, jobject thiz) {


}extern "C"
JNIEXPORT void JNICALL
Java_com_withyang_rgbplayer_MainActivity_playPCM(JNIEnv *env, jobject thiz, jstring url) {
    // TODO: implement playPCM()
    SLEngineItf slEngineItf = createSLEngine();
    if (slEngineItf) {
        LOGE("slEngineItf success");
    } else {
        LOGE("slEngineItf failed");
    }
    SLObjectItf mix = NULL;
    SLresult result = 0;
    result = (*slEngineItf)->CreateOutputMix(slEngineItf, &mix, 0, 0, 0);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("CreateOutputMix failed!");
    }

    result = (*mix)->Realize(mix, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Realize failed!");
    }

    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, mix};

    SLDataSink slDataSink = {&outputMix, 0};

    SLDataLocator_AndroidSimpleBufferQueue queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 10};
    SLDataFormat_PCM pcm = {SL_DATAFORMAT_PCM,
                            2,
                            SL_SAMPLINGRATE_44_1,
                            SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_PCMSAMPLEFORMAT_FIXED_16,
                            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                            SL_BYTEORDER_LITTLEENDIAN};
    SLDataSource dataSource = {&queue, &pcm};
    SLObjectItf palyer = NULL;
    SLPlayItf iplayer = NULL;
    SLAndroidSimpleBufferQueueItf simpleBufferQueueItf = NULL;
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[] = {SL_BOOLEAN_TRUE};
    result = (*slEngineItf)->CreateAudioPlayer(slEngineItf, &palyer, &dataSource, &slDataSink,
                                               sizeof(ids) / sizeof(SLInterfaceID), ids, req);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("CreateAudioPlayer failed!");
    } else {

    }

    (*palyer)->Realize(palyer, SL_BOOLEAN_FALSE);
    result = (*palyer)->GetInterface(palyer, SL_IID_PLAY, &iplayer);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("GetInterface failed!");
    } else {

    }
    result = (*palyer)->GetInterface(palyer, SL_IID_BUFFERQUEUE, &simpleBufferQueueItf);
    if(result != SL_RESULT_SUCCESS)
    {
        LOGE("GetInterface SL_IID_BUFFERQUEUE failed");
    }
    (*simpleBufferQueueItf)->RegisterCallback(simpleBufferQueueItf, pcmcall, 0);
    (*iplayer)->SetPlayState(iplayer, SL_PLAYSTATE_PLAYING);
    (*simpleBufferQueueItf)->Enqueue(simpleBufferQueueItf, "", 1);

}




