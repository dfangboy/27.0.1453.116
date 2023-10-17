// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CERT_VERIFY_PROC_WIN_H_
#define NET_BASE_CERT_VERIFY_PROC_WIN_H_

#include "net/base/cert_verify_proc.h"

namespace net {

// Performs certificate path construction and validation using Windows'
// CryptoAPI.
class CertVerifyProcWin : public CertVerifyProc {
 public:
  CertVerifyProcWin();

  virtual bool SupportsAdditionalTrustAnchors() const OVERRIDE;

 protected:
  virtual ~CertVerifyProcWin();

 private:
  virtual int VerifyInternal(X509Certificate* cert,
                             const std::string& hostname,
                             int flags,
                             CRLSet* crl_set,
                             const CertificateList& additional_trust_anchors,
                             CertVerifyResult* verify_result) OVERRIDE;
};

}  // namespace net

#endif  // NET_BASE_CERT_VERIFY_PROC_WIN_H_
