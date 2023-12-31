// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mock_authenticator.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/login/user.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

void MockAuthenticator::AuthenticateToLogin(Profile* profile,
                                            const UserCredentials& credentials,
                                            const std::string& login_token,
                                            const std::string& login_captcha) {
  if (expected_username_ == credentials.username &&
      expected_password_ == credentials.password) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&MockAuthenticator::OnLoginSuccess, this, false));
    return;
  }
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&MockAuthenticator::OnLoginFailure, this,
                 LoginFailure::FromNetworkAuthFailure(error)));
}

void MockAuthenticator::CompleteLogin(Profile* profile,
                                      const UserCredentials& credentials) {
  CHECK_EQ(expected_username_, credentials.username);
  CHECK_EQ(expected_password_, credentials.password);
  OnLoginSuccess(false);
}

void MockAuthenticator::AuthenticateToUnlock(
    const UserCredentials& credentials) {
  AuthenticateToLogin(NULL /* not used */, credentials,
                      std::string(), std::string());
}

void MockAuthenticator::LoginAsLocallyManagedUser(
    const UserCredentials& credentials) {
  consumer_->OnLoginSuccess(UserCredentials(expected_username_, "", ""),
                            false,
                            false);
}

void MockAuthenticator::LoginRetailMode() {
  consumer_->OnRetailModeLoginSuccess();
}

void MockAuthenticator::LoginAsPublicAccount(const std::string& username) {
  consumer_->OnLoginSuccess(UserCredentials(expected_username_, "", ""),
                            false,
                            false);
}

void MockAuthenticator::LoginOffTheRecord() {
  consumer_->OnOffTheRecordLoginSuccess();
}

void MockAuthenticator::OnRetailModeLoginSuccess() {
  consumer_->OnRetailModeLoginSuccess();
}

void MockAuthenticator::OnLoginSuccess(bool request_pending) {
  // If we want to be more like the real thing, we could save username
  // in AuthenticateToLogin, but there's not much of a point.
  consumer_->OnLoginSuccess(UserCredentials(expected_username_,
                                            expected_password_,
                                            ""),
                            request_pending,
                            false);
}

void MockAuthenticator::OnLoginFailure(const LoginFailure& failure) {
    consumer_->OnLoginFailure(failure);
}

}  // namespace chromeos
