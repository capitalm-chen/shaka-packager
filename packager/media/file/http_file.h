// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILE_HTTP_FILE_H_
#define MEDIA_FILE_HTTP_FILE_H_

#include <stdint.h>

#include <string>

#include "packager/base/compiler_specific.h"
#include "packager/media/file/file.h"

namespace shaka {
namespace media {

class ScopedSocket;

/// Implements HttpFile, which receives HTTP unicast and multicast streams.
class HttpFile : public File {
 public:
  /// @param file_name C string containing the address of the stream to receive.
  ///        It should be of the form "<ip_address>:<port>".
  explicit HttpFile(const char* address_and_port);

  /// @name File implementation overrides.
  /// @{
  bool Close() override;
  int64_t Read(void* buffer, uint64_t length) override;
  int64_t Write(const void* buffer, uint64_t length) override;
  int64_t Size() override;
  bool Flush() override;
  bool Seek(uint64_t position) override;
  bool Tell(uint64_t* position) override;
  /// @}

 protected:
  ~HttpFile() override;
  std::string GetLine(ScopedSocket *);
  std::string GetAddrAndPort();
  std::string GetPath();

  bool Open() override;

 private:
  int socket_;

  DISALLOW_COPY_AND_ASSIGN(HttpFile);
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FILE_HTTP_FILE_H_
