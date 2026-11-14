#include "opengate_builtin.h"
#include <string.h>
#include "opengate_builtin_data.inc"
#define SCRATCH_PAGES 32
static struct {uint32_t page;uint8_t data[512];} scratch[SCRATCH_PAGES];
static unsigned used;
void opengate_builtin_reset(void){used=0;}
static const uint8_t *lookup(uint32_t page) {
    unsigned lo=0,hi=sizeof(gb_pages)/sizeof(gb_pages[0]);
    while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(gb_pages[mid].page<page)lo=mid+1;else hi=mid;}
    if(lo==sizeof(gb_pages)/sizeof(gb_pages[0]) || gb_pages[lo].page!=page)return NULL;
    return gb_blob+gb_pages[lo].offset;
}
int opengate_builtin_read(uint32_t page,void *out,bool raw) {
    if(page>=131072u)return -1;
    if(!raw)for(unsigned i=0;i<used;i++)if(scratch[i].page==page){memcpy(out,scratch[i].data,512);return 0;}
    const uint8_t *p=NULL;
    if(!raw && (page==254 || page==57926 || page==57927))p=lookup(page|0x80000000u);
    if(!p)p=lookup(page);
    if(!p){memset(out,0xff,512);return 0;}
    uint8_t *dst=out;unsigned n=0;
    while(n<512){uint8_t t=*p++;unsigned len=(t&127)+1;if(len>512-n)return -1;
        if(t&128){memset(dst+n,*p++,len);}else{memcpy(dst+n,p,len);p+=len;}n+=len;}
    return 0;
}
int opengate_builtin_write(uint32_t page,const void *data) {
    if(page>=131072u)return -1;
    unsigned i;for(i=0;i<used;i++)if(scratch[i].page==page)break;
    if(i==used){if(used==SCRATCH_PAGES)return -1;used++;scratch[i].page=page;}
    memcpy(scratch[i].data,data,512);return 0;
}
