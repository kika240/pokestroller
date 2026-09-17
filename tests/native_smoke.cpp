#include "emulator.h"
#include <fstream>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <algorithm>
#include <string>

static std::vector<uint8_t> read(const char *path) {
    std::ifstream file(path,std::ios::binary);
    if(!file){fprintf(stderr,"Cannot read input\n");exit(1);}
    return {std::istreambuf_iterator<char>(file),{}};
}
static uint32_t frame(PWEmulator *pw,const char *path=nullptr) {
    uint32_t pixels[PW_WIDTH*PW_HEIGHT],hash=2166136261u;pw_frame(pw,pixels);
    FILE *file=path ? fopen(path,"wbx") : nullptr;
    if(path && !file){perror(path);exit(1);}
    if(file)fprintf(file,"P6\n96 64\n255\n");
    for(auto pixel:pixels){hash=(hash^pixel)*16777619u;if(file){fputc(pixel>>16,file);fputc(pixel>>8,file);fputc(pixel,file);}}
    if(file)fclose(file);return hash;
}
int main(int argc,char **argv) {
    if(argc<3 || argc>4){fprintf(stderr,"Usage: native-smoke ROM EEPROM [temporary-output-directory]\n");return 2;}
    auto rom=read(argv[1]),ee=read(argv[2]);char error[1024];
    if(ee.size()!=PW_EEPROM_SIZE)return 2;
    // The source device may be muted. Enable its buzzer in this RAM-only fixture,
    // updating both reliable HealthData checksums; never write either input.
    for(size_t base:{0x156,0x256}) {
        ee[base+23]=(ee[base+23]&~6)|4;
        uint8_t sum=1;for(size_t i=0;i<24;i++)sum+=ee[base+i];ee[base+24]=sum;
    }
    auto pw=pw_create(rom.data(),rom.size(),ee.data(),ee.size(),error,sizeof(error));
    if(!pw){puts(error);return 1;}pw_clock_origin(pw,1789680000);
    unsigned sounding=0;uint64_t samples=0;
    auto run=[&](double seconds){for(int i=0;i<int(seconds*120);i++){
        if(!pw_run(pw,PW_CLOCK/120)){fprintf(stderr,"PC=%04x %s\n",pw_pc(pw),pw_error(pw));exit(1);}
        int16_t audio[1024];size_t n=pw_audio(pw,audio,1024);samples+=n;
        for(size_t j=0;j<n;j++)if(audio[j])sounding++;
    }};
    auto press=[&](uint8_t key){pw_button(pw,key,true);run(.15);pw_button(pw,key,false);run(.35);};
    auto capture=[&](const char *label){std::string path=argc==4 ? std::string(argv[3])+"/"+label+".ppm" : "";
        uint32_t hash=frame(pw,path.empty()?nullptr:path.c_str());printf("%s frame=%08x PC=%04x\n",label,hash,pw_pc(pw));return hash;};
    run(1);press(PW_CENTER);run(1);press(PW_RIGHT);run(1);press(PW_RIGHT);run(1);
    uint32_t menu=capture("menu");uint16_t before=pw_watts(pw);
    press(PW_CENTER);run(1);capture("radar");
    press(PW_RIGHT);capture("choice");press(PW_CENTER);run(3);capture("wrong-bush");
    run(4);press(PW_CENTER);run(1);capture("after-radar");
    // With a paired, active-session EEPROM, radar consumes 10 W and this path
    // produces the wrong-bush message. Captures allow inspection for other ROMs.
    assert(before>=10 && pw_watts(pw)<=before-10 && frame(pw)!=menu);
    unsigned initial=pw_steps(pw);pw_walk(pw,true);run(35);pw_walk(pw,false);run(3);
    printf("walking: %u -> %u steps, %u W; audio: %llu samples, %u nonzero\n",
        initial,pw_steps(pw),pw_watts(pw),(unsigned long long)samples,sounding);
    assert(pw_steps(pw)>initial+10 && sounding>100 && samples>PW_AUDIO_RATE);
    std::vector<uint8_t> saved(PW_EEPROM_SIZE);pw_eeprom(pw,saved.data());
    printf("EEPROM changed: %s\n",saved==ee?"no":"yes");
    assert(saved!=ee);
    uint16_t savedWatts=pw_watts(pw);
    pw_destroy(pw);
    pw=pw_create(rom.data(),rom.size(),saved.data(),saved.size(),error,sizeof(error));assert(pw);
    pw_clock_origin(pw,1789680060);run(4);
    printf("EEPROM restart: %u steps, %u W\n",pw_steps(pw),pw_watts(pw));
    assert(pw_watts(pw)==savedWatts);
    pw_destroy(pw);puts("PASS real-ROM radar/walking/audio/EEPROM restart (inputs read-only)");
}
