#include "emulator.h"
#include "ir_link.h"
#include <fstream>
#include <vector>
#include <cstdio>
#include <unistd.h>
#include <cassert>
#include <algorithm>
#include <string>
// Optional integration test: external dumps are read only. A second, distinct
// trainer identity is constructed exclusively in RAM for this local encounter.
// No original data or derived packet/capture is written unless an output folder
// is explicitly provided, and then only rendered screenshots are produced.
int main(int argc,char**argv){
 if(argc<3 || argc>4){fprintf(stderr,"Usage: ir-smoke ROM EEPROM [temporary-output-directory]\n");return 2;}
 std::ifstream r(argv[1],std::ios::binary),e(argv[2],std::ios::binary);std::vector<uint8_t>rom{std::istreambuf_iterator<char>(r),{}},ee{std::istreambuf_iterator<char>(e),{}};
 if(ee.size()!=PW_EEPROM_SIZE)return 2;
 const auto original=ee;
 char err[1024];PWEmulator* pw[2]={pw_create(rom.data(),rom.size(),ee.data(),ee.size(),err,sizeof(err)),nullptr};
 // Distinct synthetic identity for peer 2, only in RAM; both reliable copies.
 for(int base:{0x83,0x183}){ee[base]^=0x5a;uint8_t sum=1;for(int i=0;i<40;i++)sum+=ee[base+i];ee[base+40]=sum;}
 for(int base:{0xed,0x1ed}){ee[base+16]^=0x5a;ee[base+12]^=0x33;uint8_t sum=1;for(int i=0;i<104;i++)sum+=ee[base+i];ee[base+104]=sum;}
 pw[1]=pw_create(rom.data(),rom.size(),ee.data(),ee.size(),err,sizeof(err));
 assert(pw[0] && pw[1]);
 PWLink* link[2]={pw_link_create(),pw_link_create()};pw_link_listen(link[0],0);pw_link_connect(link[1],"127.0.0.1",pw_link_port(link[0]));
 for(int i=0;i<100;i++){pw_link_poll(link[0]);pw_link_poll(link[1]);if(pw_link_state(link[0])==2 &&pw_link_state(link[1])==2)break;usleep(1000);}
 pw_clock_origin(pw[0],1789680000);pw_clock_origin(pw[1],1789680017);
 assert(pw_link_state(link[0])==2 && pw_link_state(link[1])==2);
 unsigned tx[2]={0},rx[2]={0};
 auto run=[&](double sec){for(int i=0;i<int(sec*1200);i++)for(int k=0;k<2;k++){
  uint8_t b[4096];pw_link_poll(link[k]);size_t n=pw_link_read(link[k],b,sizeof(b));rx[k]+=n;pw_ir_receive(pw[k],b,n);
  if(!pw_run(pw[k],PW_CLOCK/1200)){puts(pw_error(pw[k]));exit(1);}n=pw_ir_transmit(pw[k],b,sizeof(b));tx[k]+=n;if(n)assert(pw_link_send(link[k],b,n));
  int16_t a[64];pw_audio(pw[k],a,64);
 }};
 auto press=[&](int key){for(auto p:pw)pw_button(p,key,true);run(.15);for(auto p:pw)pw_button(p,key,false);run(.35);};
 auto frame=[&](const char*tag){
  if(argc==4)for(int k=0;k<2;k++){
   std::string path=std::string(argv[3])+"/ir-"+tag+"-"+std::to_string(k)+".ppm";
   uint32_t px[6144];pw_frame(pw[k],px);FILE*f=fopen(path.c_str(),"wbx");assert(f);
   fprintf(f,"P6\n96 64\n255\n");for(auto p:px){fputc(p>>16,f);fputc(p>>8,f);fputc(p,f);}fclose(f);
  }
  printf("%s: TX %u/%u, RX %u/%u\n",tag,tx[0],tx[1],rx[0],rx[1]);
 };

 run(1);press(1);run(1);press(16);run(1);press(16);run(1);press(16);press(16);frame("menu");pw_button(pw[0],1,true);run(.15);pw_button(pw[0],1,false);run(.173);pw_button(pw[1],1,true);run(.15);pw_button(pw[1],1,false);run(.35);frame("start");run(2);frame("2sec");run(3);frame("5sec");run(5);frame("10sec");run(5);frame("15sec");
 assert(tx[0]>1500 && tx[1]>1500 && tx[0]==rx[1] && tx[1]==rx[0]);
 for(int i=0;i<2;i++){
  std::vector<uint8_t> result(PW_EEPROM_SIZE);pw_eeprom(pw[i],result.data());
  // Gift inventory changed on both devices, beyond a mere serial handshake.
  assert(!std::equal(result.begin()+0xcec8,result.begin()+0xcedc,original.begin()+0xcec8));
  pw_destroy(pw[i]);pw_link_destroy(link[i]);
 }
 puts("PASS game-to-game TCP IR encounter and gift on both devices");
}
