#pragma once

// IWYU pragma: private
// ok no namespace fl - POSIX socket API used directly from system headers

#include "platforms/win/is_win.h"

#ifdef FASTLED_HAS_NETWORKING
#if !defined(FL_IS_WIN)

#include "fl/stl/string.h"
#include "fl/stl/stdint.h"

// Minimal POSIX includes for function declarations
#include <sys/types.h>    // For ssize_t, socklen_t
#include <sys/socket.h>   // For struct sockaddr, struct msghdr
#include <netdb.h>        // For struct addrinfo
#include <fcntl.h>        // For F_GETFL, F_SETFL, O_NONBLOCK
#include <netinet/in.h>   // For sockaddr_in, htons, etc.
#include <arpa/inet.h>    // For inet_pton, inet_ntop
#include <unistd.h>       // For close()
#include <errno.h>        // For errno

// On POSIX platforms, socket API is already correct - no wrappers needed
// Just use system socket functions directly (::socket, ::bind, etc.)

// Note: POSIX socket constants and errno values are already defined correctly
// No need to redefine AF_INET, SOCK_STREAM, ECONNREFUSED, etc.

#endif // !defined(FL_IS_WIN)
#endif // FASTLED_HAS_NETWORKING 
