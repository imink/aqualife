#pragma once

#include <cstdint>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(addr))
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr) (*(addr))
#endif