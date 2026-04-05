// Compile the stb_truetype implementation exactly once.
// STBTT_DEF extern  — functions have external linkage so Font.cpp can link to them.
#define STBTT_DEF extern
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
