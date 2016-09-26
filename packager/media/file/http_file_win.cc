// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/file/http_file.h"

#ifdef WIN32
#include <windows.h>
#include <ws2tcpip.h>
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <gflags/gflags.h>
#include <cstring>

#include <limits>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"

// TODO(tinskip): Adapt to work with winsock.

DEFINE_string(http_interface_address,
              "0.0.0.0",
              "IP address of the HTTP stream");

namespace shaka {
namespace media {

namespace {

const SOCKET kInvalidSocket(INVALID_SOCKET);

bool StringToIpv4Address(const std::string& addr_in, uint32_t* addr_out) {
  DCHECK(addr_out);

  *addr_out = 0;
  size_t start_pos(0);
  size_t end_pos(0);
  for (int i = 0; i < 4; ++i) {
    end_pos = addr_in.find('.', start_pos);
    if ((end_pos == std::string::npos) != (i == 3))
      return false;
    unsigned addr_byte;
    if (!base::StringToUint(addr_in.substr(start_pos, end_pos - start_pos),
                            &addr_byte)
        || (addr_byte > 255))
      return false;
    *addr_out <<= 8;
    *addr_out |= addr_byte;
    start_pos = end_pos + 1;
  }
  return true;
}

bool StringToIpv4AddressAndPort(const std::string& addr_and_port,
                                uint32_t* addr,
                                uint16_t* port) {
  DCHECK(addr);
  DCHECK(port);

  size_t colon_pos = addr_and_port.find(':');
  if (colon_pos == std::string::npos) {
    return false;
  }
  if (!StringToIpv4Address(addr_and_port.substr(0, colon_pos), addr))
    return false;
  unsigned port_value;
  if (!base::StringToUint(addr_and_port.substr(colon_pos + 1),
                          &port_value) ||
      (port_value > 65535))
    return false;
  *port = port_value;
  return true;
}

}  // anonymous namespace

HttpFile::HttpFile(const char* file_name) :
    File(file_name),
    socket_(kInvalidSocket) {}

HttpFile::~HttpFile() {}

bool HttpFile::Close() {
  if (socket_ != kInvalidSocket) {
    closesocket(socket_);
    socket_ = kInvalidSocket;
  }
  delete this;
  return true;
}

int64_t HttpFile::Read(void* buffer, uint64_t length) {
  DCHECK(buffer);
  DCHECK_GE(length, 65535u)
      << "Buffer may be too small to read entire datagram.";

  if (socket_ == kInvalidSocket)
    return -1;

  int64_t result;
  do {
    result = recv(socket_, reinterpret_cast<char *>(buffer), length, 0);
  } while ((result == -1) && (errno == EINTR));

  return result;
}

int64_t HttpFile::Write(const void* buffer, uint64_t length) {
  NOTIMPLEMENTED();
  return -1;
}

int64_t HttpFile::Size() {
  if (socket_ == kInvalidSocket)
    return -1;

  return std::numeric_limits<int64_t>::max();
}

bool HttpFile::Flush() {
  NOTIMPLEMENTED();
  return false;
}

bool HttpFile::Seek(uint64_t position) {
  NOTIMPLEMENTED();
  return false;
}

bool HttpFile::Tell(uint64_t* position) {
  NOTIMPLEMENTED();
  return false;
}

class ScopedSocket {
 public:
  explicit ScopedSocket(SOCKET sock_fd)
      : sock_fd_(sock_fd) {}

  ~ScopedSocket() {
    if (sock_fd_ != kInvalidSocket)
      closesocket(sock_fd_);
  }

  SOCKET get() { return sock_fd_; }

  SOCKET release() {
    SOCKET socket = sock_fd_;
    sock_fd_ = kInvalidSocket;
    return socket;
  }

 private:
  SOCKET sock_fd_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSocket);
};

bool HttpFile::Open() {
  DCHECK_EQ(kInvalidSocket, socket_);

  // TODO(tinskip): Support IPv6 addresses.
  uint32_t dest_addr;
  uint16_t dest_port;
  if (!StringToIpv4AddressAndPort(GetAddrAndPort(),
                                  &dest_addr,
                                  &dest_port)) {
    LOG(ERROR) << "Malformed IPv4 address:port UDP stream specifier.";
    return false;
  }

  ScopedSocket new_socket(socket(AF_INET, SOCK_STREAM, 0));
  if (new_socket.get() == kInvalidSocket) {
    LOG(ERROR) << "Could not allocate socket.";
    return false;
  }

  struct sockaddr_in remote_sock_addr = {0};
  remote_sock_addr.sin_family = AF_INET;
  remote_sock_addr.sin_port = htons(dest_port);
  remote_sock_addr.sin_addr.s_addr  = htonl(dest_addr);

  if (connect(new_socket.get(),
              reinterpret_cast<struct sockaddr*>(&remote_sock_addr),
              sizeof(remote_sock_addr))) {
    LOG(ERROR) << "Could not bind TCP socket";
    return false;
  }

  std::string request = "GET " + GetPath() + " HTTP/1.0\r\n\r\n";
  send(new_socket.get(), request.c_str(), request.size(), 0);

  while (GetLine(&new_socket).size() > 0) {}

  socket_ = new_socket.release();
  return true;
}

std::string HttpFile::GetLine(ScopedSocket *socket) {
  std::string buffer;

  char c;
  int response;

  response = recv(socket->get(), &c, 1, 0);

  while (response > -1 && c != '\n') {
    if (c != '\r' && c != '\n') {
      buffer.push_back(c);
    }
    response = recv(socket->get(), &c, 1, 0);
  }

  return buffer;
}

std::string HttpFile::GetAddrAndPort() {
  size_t slash_pos = file_name().find('/');
  return file_name().substr(0, slash_pos);
}

std::string HttpFile::GetPath() {
  size_t slash_pos = file_name().find('/');

  if (slash_pos != std::string::npos) {
    return file_name().substr(slash_pos);
  }
  else {
    return "/";
  }
}

}  // namespace media
}  // namespace shaka
