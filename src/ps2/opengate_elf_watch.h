#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <strings.h>
typedef struct {int fd; bool complete;} opengate_elf_watch_t;
static inline void opengate_elf_watch_open(opengate_elf_watch_t *w,int fd,const char *path,bool active){
    while(*path=='/')path++;
    if(active && fd>=0 && !strcasecmp(path,"BOOT/BOOT.ELF")){w->fd=fd;w->complete=false;}
}
static inline void opengate_elf_watch_read(opengate_elf_watch_t *w,int fd,int result,int64_t position,int64_t size){
    if(fd==w->fd && result>0 && size>=52 && position>=size)w->complete=true;
}
static inline bool opengate_elf_watch_close(opengate_elf_watch_t *w,int fd,int result){
    if(fd!=w->fd)return false;
    bool done=result==0 && w->complete;w->fd=-1;w->complete=false;return done;
}
