#pragma once
#include <stdbool.h>
#include <stdint.h>
void opengate_builtin_reset(void);
int opengate_builtin_read(uint32_t page,void *out,bool raw);
int opengate_builtin_write(uint32_t page,const void *data);
