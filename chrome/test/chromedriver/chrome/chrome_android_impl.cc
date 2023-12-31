// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/sync_websocket_impl.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"

ChromeAndroidImpl::ChromeAndroidImpl(
    URLRequestContextGetter* context_getter,
    int port,
    const SyncWebSocketFactory& socket_factory)
    : ChromeImpl(context_getter, port, socket_factory) {}

ChromeAndroidImpl::~ChromeAndroidImpl() {}

Status ChromeAndroidImpl::Launch(const std::string& package_name) {
  // TODO(frankf): Figure out how this should be installed to
  // make this work for all platforms.
  base::FilePath adb_commands(FILE_PATH_LITERAL("adb_commands.py"));
  CommandLine command(adb_commands);
  command.AppendSwitchASCII("package", package_name);
  command.AppendSwitch("launch");
  command.AppendSwitchASCII("port", base::IntToString(GetPort()));

  std::string output;
  if (!base::GetAppOutput(command, &output)) {
    if (output.empty())
      return Status(
          kUnknownError,
          "failed to run adb_commands.py. Make sure it is set in PATH.");
    else
      return Status(kUnknownError, "android app failed to start.\n" + output);
  }

  Status status = Init();
  if (status.IsError()) {
    Quit();
    return status;
  }

  return Status(kOk);
}

std::string ChromeAndroidImpl::GetOperatingSystemName() {
  return "ANDROID";
}

Status ChromeAndroidImpl::Quit() {
  // NOOP.
  return Status(kOk);
}

