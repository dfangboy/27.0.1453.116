// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is an interface for all objects that want to be notified that
// the underlying UDP socket is available for writing (not write blocked
// anymore).

#ifndef NET_QUIC_QUIC_BLOCKED_WRITER_INTERFACE_H_
#define NET_QUIC_QUIC_BLOCKED_WRITER_INTERFACE_H_

namespace net {

class NET_EXPORT_PRIVATE QuicBlockedWriterInterface {
 public:
  virtual ~QuicBlockedWriterInterface() {}

  // Called by the PacketWriter when the underlying socket becomes writable
  // so that the BlockedWriter can go ahead and try writing. This methods
  // should return false if the socket has become blocked while writing.
  virtual bool OnCanWrite() = 0;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_BLOCKED_WRITER_INTERFACE_H_
