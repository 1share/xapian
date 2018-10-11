/** @file  socket_utils.cc
 *  @brief Socket handling utilities.
 */
/* Copyright (C) 2006,2007,2008,2015 Olly Betts
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <config.h>
#include "socket_utils.h"

#include <limits>

#include "realtime.h"
#include "safesyssocket.h"

using namespace std;

#include <arpa/inet.h>
#include "stringutils.h"

#ifdef __WIN32__
# include <io.h>
# include "msvcignoreinvalidparam.h"
# include <cerrno>

/// Convert an fd (which might be a socket) to a WIN32 HANDLE.
extern HANDLE fd_to_handle(int fd) {
    MSVCIgnoreInvalidParameter invalid_handle_value_is_ok;
    HANDLE handle = (HANDLE)_get_osfhandle(fd);
    if (handle != INVALID_HANDLE_VALUE) return handle;
    // On WIN32, a socket fd isn't the same as a non-socket fd - in fact it's
    // already a HANDLE!
    return reinterpret_cast<HANDLE>(fd);
}

/// Close an fd, which might be a socket.
extern void close_fd_or_socket(int fd) {
    MSVCIgnoreInvalidParameter invalid_fd_value_is_ok;
    if (close(fd) == -1 && errno == EBADF) {
	// Bad file descriptor - probably because the fd is actually
	// a socket.
	closesocket(fd);
    }
}

#endif

void
set_socket_timeouts(int fd, double timeout)
{
    (void)fd;
    (void)timeout;
#if defined SO_SNDTIMEO || defined SO_RCVTIMEO
    {
# ifndef __WIN32__
	struct timeval t;
	RealTime::to_timeval(timeout, &t);
# else
	// Just to be different, it's a DWORD counting in milliseconds.
	DWORD t;
	if (usual(timeout < numeric_limits<DWORD>::max() / 1000))
	    t = timeout * 1000;
	else
	    t = numeric_limits<DWORD>::max();
# endif
# ifdef SO_SNDTIMEO
	(void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
			 reinterpret_cast<char*>(&t), sizeof(t));
# endif
# ifdef SO_RCVTIMEO
	(void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
			 reinterpret_cast<char*>(&t), sizeof(t));
# endif
    }
#endif
#ifdef SO_KEEPALIVE
    // SO_SNDTIMEO and SO_RCVTIMEO may be ignored even if they exist, so set
    // SO_KEEPALIVE anyway if it exists, as it will cause stuck connections to
    // time out eventually (though it may take ~2 hours).
    {
# ifndef __WIN32__
	int flag = 1;
# else
	DWORD flag = 1;
# endif
	(void)setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
			 reinterpret_cast<char*>(&flag), sizeof(flag));
    }
#endif
}

int
pretty_ip6(const void* p, char* buf)
{
    const sockaddr_in* a = reinterpret_cast<const sockaddr_in*>(p);
    const sockaddr_in6* a6 = reinterpret_cast<const sockaddr_in6*>(p);
    sa_family_t af = a6->sin6_family;
    in_port_t port;
    if (af == AF_INET6) {
	port = a6->sin6_port;
    } else if (af == AF_INET) {
	port = a->sin_port;
    } else {
	return -1;
    }

#ifndef __WIN32__
    // Under __WIN32__, inet_ntop()'s second parameter isn't const for some
    // reason.  We don't currently use inet_ntop() there, but allow for a
    // non-const second parameter in case it's more widespread.
    void* src = const_cast<void*>(p);
    const char* r = inet_ntop(af, src, buf, PRETTY_IP6_LEN);
    if (!r)
	return -1;
#else
    // inet_ntop() isn't always available, at least with mingw.
    // WSAAddressToString() supports both IPv4 and IPv6, so just use that.
    DWORD size = PRETTY_IP6_LEN;
    if (WSAAddressToString(reinterpret_cast<sockaddr*>(&remote_address),
			   sizeof(remote_address), NULL, buf, &size) != 0) {
	return -1;
    }
    const char* r = buf;
#endif

    if (startswith(r, "::ffff:") || startswith(r, "::FFFF:")) {
	if (strchr(r + 7, '.')) {
	    r += 7;
	}
    }

    if (r != buf)
	memmove(buf, r, strlen(r) + 1);

    return port;
}
