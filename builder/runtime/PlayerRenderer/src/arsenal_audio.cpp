#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <string.h>
#include "effects.h"
namespace {
struct Sample {const unsigned char* pcm;uint32_t bytes;};Sample samples[8]={};
struct Event {int kind;};Event queue[64];unsigned read_index,write_index;
SRWLOCK queue_lock=SRWLOCK_INIT;
struct Voice {const unsigned char* pcm;uint32_t bytes,position;};
DWORD WINAPI audio_worker(void*){
 WAVEFORMATEX format={};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=44100;format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=176400;
 HWAVEOUT out=nullptr;if(waveOutOpen(&out,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)return 0;
 const unsigned frames=512;short data[4][frames*2]={};WAVEHDR headers[4]={};Voice voices[32]={};unsigned next_voice=0;
 for(unsigned i=0;i<4;i++){headers[i].lpData=(LPSTR)data[i];headers[i].dwBufferLength=sizeof(data[i]);if(waveOutPrepareHeader(out,&headers[i],sizeof(WAVEHDR))!=MMSYSERR_NOERROR){waveOutClose(out);return 0;}headers[i].dwFlags|=WHDR_DONE;}
 for(;;){
  for(unsigned b=0;b<4;b++){
   if(!(headers[b].dwFlags&WHDR_DONE))continue;
   AcquireSRWLockExclusive(&queue_lock);
   while(read_index!=write_index){auto event=queue[read_index++%64];auto sample=samples[event.kind];if(sample.pcm)voices[next_voice++%32]={sample.pcm,sample.bytes,0};}
   ReleaseSRWLockExclusive(&queue_lock);
   for(unsigned i=0;i<frames;i++){
    int mixed=0;for(auto& v:voices)if(v.pcm&&v.position+2<=v.bytes){int16_t value;memcpy(&value,v.pcm+v.position,2);mixed+=value;v.position+=2;if(v.position==v.bytes)v.pcm=nullptr;}
    mixed=mixed>32767?32767:mixed<-32768?-32768:mixed;data[b][i*2]=data[b][i*2+1]=(short)mixed;
   }
   if(waveOutWrite(out,&headers[b],sizeof(WAVEHDR))!=MMSYSERR_NOERROR){waveOutReset(out);return 0;}
  }
  Sleep(3);
 }
}
}
extern "C" void native_sound_sample(int kind,const unsigned char* pcm,uint32_t bytes){if(kind>=0&&kind<8&&pcm&&bytes&&!(bytes&1)&&bytes<=((kind==7)?8*44100*2:441000))samples[kind]={pcm,bytes};}
extern "C" int native_sound_init(){auto thread=CreateThread(nullptr,0,audio_worker,nullptr,0,nullptr);if(!thread)return 0;CloseHandle(thread);return 1;}
extern "C" void native_sound_play(int kind){
 if(kind<0||kind>=8)return;
 if(!TryAcquireSRWLockExclusive(&queue_lock))return;
 if(write_index-read_index<64)queue[write_index++%64]={kind};ReleaseSRWLockExclusive(&queue_lock);
}
