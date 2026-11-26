// Flash (PROGMEM) access that also compiles on the host for unit tests.
#pragma once
#include <stdint.h>
#ifdef ARDUINO
#include <pgmspace.h>
#else
#include <cstring>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#define memcpy_P memcpy
#define strlen_P strlen
#define strncpy_P strncpy
#define pgm_read_ptr(p) (*(const void *const *)(p))
#endif
