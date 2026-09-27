#include "actor_texture.h"
#include <cassert>
#include <cstdio>
using namespace NorthlightActorTexture;
int main(){std::vector<std::uint8_t> out;std::uint8_t block[16]={};
 // BC1 endpoint order enables real transparent selector3, selector2 is half red.
 std::uint16_t red=0xf800,black=0;std::memcpy(block,&black,2);std::memcpy(block+2,&red,2);block[4]=3|(2<<2)|(1<<4);
 assert(decode(block,8,4,4,8,Format::BC1,out));assert(out[3]==0&&out[4]==127&&out[7]==255&&out[8]==255);
 // BC2 alpha nibbles are independent of BC1 endpoint ordering; color3 is opaque.
 std::memset(block,0,sizeof block);block[0]=0xf0;std::memcpy(block+10,&red,2);block[12]=3;
 assert(decode(block,16,4,4,16,Format::BC2,out));assert(out[3]==0&&out[7]==255&&out[0]==170);
 // BC3 6-alpha mode explicit 0/255 slots, and 8-alpha interpolated mode.
 block[0]=10;block[1]=20;block[2]=6|(7<<3);assert(decode(block,16,4,4,16,Format::BC3,out));assert(out[3]==0&&out[7]==255);
 block[0]=255;block[1]=0;block[2]=2|(7<<3);assert(decode(block,16,4,4,16,Format::BC3,out));assert(out[3]==218&&out[7]==36);
 assert(decode(block,16,1,1,16,Format::BC3,out)&&out.size()==4);
 assert(!decode(block,15,4,4,16,Format::BC3,out)&&out.empty());
 std::uint8_t rgba[]={7,8,9,10};assert(decode(rgba,4,1,1,4,Format::BGRA8,out)&&out==std::vector<std::uint8_t>({9,8,7,10}));
 assert(decode(rgba,4,1,1,4,Format::BGRX8,out)&&out[3]==255);
 assert(!decode(rgba,4,129,1,4,Format::BGRA8,out));
 std::puts("Actual BC1/BC2/BC3 alpha, interpolation, sub-block mips and packed color tests passed");}
