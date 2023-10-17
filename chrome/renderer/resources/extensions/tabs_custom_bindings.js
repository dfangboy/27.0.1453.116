// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the tabs API.

var binding = require('binding').Binding.create('tabs');

var tabsNatives = requireNative('tabs');
var OpenChannelToTab = tabsNatives.OpenChannelToTab;
var sendRequestIsDisabled = requireNative('process').IsSendRequestDisabled();

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;
  var tabs = bindingsAPI.compiledApi;

  apiFunctions.setHandleRequest('connect', function(tabId, connectInfo) {
    var name = '';
    if (connectInfo) {
      name = connectInfo.name || name;
    }
    var portId = OpenChannelToTab(tabId, extensionId, name);
    return chromeHidden.Port.createPort(portId, name);
  });

  apiFunctions.setHandleRequest('sendRequest',
                                function(tabId, request, responseCallback) {
    if (sendRequestIsDisabled)
      throw new Error(sendRequestIsDisabled);
    var port = tabs.connect(tabId, {name: chromeHidden.kRequestChannel});
    chromeHidden.Port.sendMessageImpl(port, request, responseCallback);
  });

  apiFunctions.setHandleRequest('sendMessage',
                                function(tabId, message, responseCallback) {
    var port = tabs.connect(tabId, {name: chromeHidden.kMessageChannel});
    chromeHidden.Port.sendMessageImpl(port, message, responseCallback);
  });
});

exports.binding = binding.generate();