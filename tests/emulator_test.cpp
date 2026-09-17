#include "emulator.h"
#include "ir_link.h"
#include "core/soc/h838606.h"
#include "core/pokewalker/peripherals/buzzer/buzzer.h"
#include "core/pokewalker/peripherals/bma150/bma150.h"
#include "core/pokewalker/peripherals/bma150/step_sample_provider.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>
#include <vector>

static void registers() {
    Registers r;
    assert(r.Add<uint32_t>(0xffffffff,1)==0 && r.flags.Z && r.flags.C && !r.flags.V);
    assert(r.Add<uint32_t>(0x7fffffff,1)==0x80000000 && r.flags.V && r.flags.N && !r.flags.C);
    assert(r.Add<uint8_t>(0xff,1)==0 && r.flags.Z && r.flags.C && r.flags.H);
    assert(r.Sub<uint16_t>(0,1)==0xffff && r.flags.N && r.flags.C && !r.flags.Z);
    assert(r.Sub<uint32_t>(0x80000000,1)==0x7fffffff && r.flags.V && !r.flags.N);
    r.flags.C=true;
    assert(r.Inc<uint16_t>(0xffff,1)==0 && r.flags.Z && r.flags.C);
    assert(r.Inc<uint16_t>(0x7fff,1)==0x8000 && r.flags.V);
    assert(r.Dec<uint16_t>(0x8000,1)==0x7fff && r.flags.V);
    uint8_t unaligned[5]={0,0x12,0x34,0x56,0x78};
    h8300h_ptr<uint32_t> pointer(reinterpret_cast<uint32_t*>(unaligned+1));
    assert(*pointer==0x12345678); pointer=0xabcdef01;
    assert(unaligned[1]==0xab && unaligned[4]==1);
    // MULXS.W must use only signed low word of ERd, regardless of high word.
    RomBuffer rom{};rom[1]=4;rom[4]=1;rom[5]=0xc0;rom[6]=0x52;rom[7]=0x10;
    H838606 soc(rom);*soc.cpu->reg.Reg32(0)=0x1200020;*soc.cpu->reg.Reg16(1)=1448;
    soc.cpu->Cycle();assert(*soc.cpu->reg.Reg32(0)==32*1448);
    puts("PASS arithmetic flags, unaligned registers, signed multiply");
}
static void rtc_audio() {
    setenv("TZ","UTC",1);tzset();
    RomBuffer rom{};auto soc=std::make_shared<H838606>(rom);
    std::tm date{};date.tm_year=124;date.tm_mon=0;date.tm_mday=6;date.tm_hour=23;date.tm_min=59;date.tm_sec=59;
    time_t now=mktime(&date);soc->rtc->Now=[&]{return now;};soc->rtc->RTCCR1.RUN=true;soc->rtc->RTCCR1.HR24=true;
    auto quarter=[&]{for(unsigned i=0;i<PW_CLOCK/4;i+=16)soc->rtc->Cycle(16);};
    quarter();assert(soc->rtc->RSECDR==0x59 && soc->rtc->RHRDR==0x23 && soc->rtc->RTCCR1.PM);
    assert(!soc->interrupts->RTCFLG.DYIFG && !soc->interrupts->RTCFLG.MNIFG);
    now++;quarter();quarter();quarter();
    assert(soc->rtc->RSECDR==0 && soc->rtc->RMINDR==0 && soc->rtc->RHRDR==0 && !soc->rtc->RTCCR1.PM);
    assert(soc->interrupts->RTCFLG.SEIFG1 && soc->interrupts->RTCFLG.MNIFG &&
           soc->interrupts->RTCFLG.HRIFG && soc->interrupts->RTCFLG.DYIFG);
    Buzzer buzzer(soc->timer_w);unsigned samples=0;float frequency=0;
    buzzer.OnSamplePushed += [&](BuzzerInformation info){samples++;frequency=info.frequency;};
    soc->timer_w->TMRW.CTS=true;*soc->timer_w->GRA=32;
    for(unsigned i=0;i<PW_CLOCK;i+=16)buzzer.Cycle(16);
    assert(samples==PW_AUDIO_RATE && frequency==1000);
    puts("PASS RTC midnight/BCD/PM and exact 32 kHz buzzer sampling");
}
static void transport() {
    auto a=pw_link_create(),b=pw_link_create();
    assert(pw_link_listen(a,0));assert(pw_link_connect(b,"127.0.0.1",pw_link_port(a)));
    auto poll=[&]{pw_link_poll(a);pw_link_poll(b);};
    for(int i=0;i<1000 && (pw_link_state(a)!=2 || pw_link_state(b)!=2);i++){
        poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(pw_link_state(a)==2 && pw_link_state(b)==2);
    // Different directions and more than one socket buffer / poll chunk.
    std::vector<uint8_t> bytes(60000),got(bytes.size());
    for(size_t i=0;i<bytes.size();i++)bytes[i]=(uint8_t)(i*73);
    assert(pw_link_send(a,bytes.data(),bytes.size()));assert(pw_link_send(b,bytes.data(),bytes.size()));
    size_t ac=0,bc=0;
    for(int i=0;i<2000 && (ac<bytes.size() || bc<bytes.size());i++){
        poll();uint8_t buffer[8192];size_t n=pw_link_read(a,buffer,sizeof(buffer));
        for(size_t j=0;j<n;j++)assert(buffer[j]==bytes[ac++]);
        n=pw_link_read(b,buffer,sizeof(buffer));for(size_t j=0;j<n;j++)assert(buffer[j]==bytes[bc++]);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(ac==bytes.size() && bc==bytes.size());
    // SCI3 register-to-register byte exchange over the actual TCP transport.
    RomBuffer rom{};H838606 first(rom),second(rom);
    first.sci3->IRCR.IRE=true;first.sci3->SCR.TE=true;
    second.sci3->ReceiveIR(0x99); // discarded while IR is disabled
    second.sci3->IRCR.IRE=true;second.sci3->SCR.RE=true;
    first.sci3->OnTransmitIR([&](uint8_t byte){assert(pw_link_send(a,&byte,1));});
    first.memory->Write8(SCI3_ADDR_TDR,0xa5);first.sci3->Cycle(160);first.sci3->Cycle(160);
    uint8_t byte=0;
    for(int i=0;i<1000;i++){poll();if(pw_link_read(b,&byte,1))break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    second.sci3->ReceiveIR(byte);second.sci3->Cycle(160);second.sci3->Cycle(160);
    assert(second.sci3->SSR.RDRF && second.memory->Read8(SCI3_ADDR_RDR)==0xa5 && !second.sci3->SSR.RDRF);
    pw_link_close(a);
    for(int i=0;i<1000 && pw_link_state(b)!=-1;i++){poll();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    assert(pw_link_state(b)==-1);
    pw_link_destroy(a);pw_link_destroy(b);
    puts("PASS TCP IR bidirectional 120 kB, SCI3 delivery, disconnect");
}
static void adapter() {
    uint8_t rom[PW_ROM_SIZE]={0},ee[PW_EEPROM_SIZE]={0};rom[1]=4;rom[4]=0x40;rom[5]=0xfe;
    char error[256];assert(!pw_create(rom,3,ee,sizeof(ee),error,sizeof(error)));
    auto a=pw_create(rom,sizeof(rom),ee,sizeof(ee),error,sizeof(error));assert(a);
    assert(pw_run(a,PW_CLOCK/60));uint8_t copy[PW_EEPROM_SIZE];pw_eeprom(a,copy);
    assert(std::equal(copy,copy+sizeof(copy),ee));assert(pw_pc(a)==4);
    pw_destroy(a);
    rom[4]=0xff;rom[5]=0xff; // may be valid MOV; use explicitly unsupported opcode.
    rom[4]=0x01;rom[5]=0x20;
    a=pw_create(rom,sizeof(rom),ee,sizeof(ee),error,sizeof(error));assert(a);
    assert(!pw_run(a,100) && *pw_error(a));pw_destroy(a);
    puts("PASS input validation, synthetic ROM execution, recoverable CPU error");
}
int main(){registers();rtc_audio();transport();adapter();}
