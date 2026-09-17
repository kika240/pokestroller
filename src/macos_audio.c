#include "macos_audio.h"
#include <AudioToolbox/AudioToolbox.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#define RING_SIZE 8192
#define BUFFER_SAMPLES 512
struct PWAudio {
    AudioQueueRef queue;
    int16_t ring[RING_SIZE];
    atomic_uint read,write;
    atomic_bool active;
    _Atomic(float) volume;
};
static void render(void *context, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    PWAudio *a=context;
    int16_t *out=buffer->mAudioData;
    unsigned read=atomic_load_explicit(&a->read,memory_order_relaxed);
    unsigned write=atomic_load_explicit(&a->write,memory_order_acquire);
    bool active=atomic_load(&a->active);
    float volume=atomic_load(&a->volume);
    if(!active) read=write;
    for(size_t i=0;i<BUFFER_SAMPLES;i++)
        out[i]=read!=write ? (int16_t)(a->ring[read++ % RING_SIZE]*volume) : 0;
    atomic_store_explicit(&a->read,read,memory_order_release);
    buffer->mAudioDataByteSize=BUFFER_SAMPLES*sizeof(int16_t);
    AudioQueueEnqueueBuffer(queue,buffer,0,NULL);
}
PWAudio *pw_audio_open(void) {
    PWAudio *a=calloc(1,sizeof(*a));
    if(!a)return NULL;
    atomic_init(&a->read,0);atomic_init(&a->write,0);
    atomic_init(&a->active,false);atomic_init(&a->volume,0.35f);
    AudioStreamBasicDescription format={.mSampleRate=32000,.mFormatID=kAudioFormatLinearPCM,
        .mFormatFlags=kLinearPCMFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked,
        .mBytesPerPacket=2,.mFramesPerPacket=1,.mBytesPerFrame=2,.mChannelsPerFrame=1,.mBitsPerChannel=16};
    if(AudioQueueNewOutput(&format,render,a,NULL,NULL,0,&a->queue)){free(a);return NULL;}
    for(int i=0;i<3;i++){
        AudioQueueBufferRef buffer;
        if(AudioQueueAllocateBuffer(a->queue,BUFFER_SAMPLES*2,&buffer)){pw_audio_close(a);return NULL;}
        render(a,a->queue,buffer);
    }
    if(AudioQueueStart(a->queue,NULL)){pw_audio_close(a);return NULL;}
    return a;
}
void pw_audio_close(PWAudio *a){if(a){AudioQueueDispose(a->queue,true);free(a);}}
void pw_audio_submit(PWAudio *a,const int16_t *samples,size_t count){
    if(!a || !atomic_load(&a->active))return;
    unsigned read=atomic_load_explicit(&a->read,memory_order_acquire);
    unsigned write=atomic_load_explicit(&a->write,memory_order_relaxed);
    for(size_t i=0;i<count && write-read<RING_SIZE;i++)a->ring[write++ % RING_SIZE]=samples[i];
    atomic_store_explicit(&a->write,write,memory_order_release);
}
void pw_audio_active(PWAudio *a,bool value){if(a)atomic_store(&a->active,value);}
void pw_audio_volume(PWAudio *a,float value){if(a)atomic_store(&a->volume,value<0?0:value>1?1:value);}
