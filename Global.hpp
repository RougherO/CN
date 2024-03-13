#ifndef GLOBAL
#define GLOBAL

#include <cstdint>
#include <cstddef>

using std::int32_t;
using std::size_t;

#define _BUF_SIZE_       255UL
#define _SOCK_ADDR_TYPE_ AF_INET

#if defined(TCP)
#define _SOCK_PROTO_TYPE_ SOCK_STREAM
#elif defined(UDP)
#define _SOCK_PROTO_TYPE_ SOCK_DGRAM
#endif

#endif