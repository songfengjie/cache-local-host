#ifndef MEMORY_SIZE_UTILS_H
#define MEMORY_SIZE_UTILS_H

#if defined(_WIN32)
    #include <malloc.h>
    #define GET_MEMORY_SIZE(ptr) _msize((ptr))
#elif defined(__APPLE__)
    #include <malloc/malloc.h>
    #define GET_MEMORY_SIZE(ptr) malloc_size((ptr))
#elif defined(__linux__)
    #include <malloc.h>
    #define GET_MEMORY_SIZE(ptr) malloc_usable_size((ptr))
#else
    #error "Unsupported platform"
#endif

#endif // MEMORY_SIZE_UTILS_H
