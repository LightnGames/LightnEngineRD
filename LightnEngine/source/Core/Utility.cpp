#include "Utility.h"
#include <ThiredParty/xxHash/xxhash.h>

namespace ltn {
u32 StrLength(const char* str) {
    return static_cast<u32>(strlen(str));
}
u64 StrHash64(const char* str) {
    return XXH64(str, StrLength(str), 0);
}
u64 BinHash64(const void* bin, u32 length) {
    return XXH64(bin, length, 0);
}
}
