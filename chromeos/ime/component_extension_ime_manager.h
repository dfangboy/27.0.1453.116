// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_COMPONENT_EXTENSION_IME_MANAGER_H_
#define CHROMEOS_IME_COMPONENT_EXTENSION_IME_MANAGER_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/ibus/ibus_component.h"

namespace chromeos {

// Represents a component extension IME.
struct CHROMEOS_EXPORT ComponentExtensionIME {
  ComponentExtensionIME();
  ~ComponentExtensionIME();
  std::string id;  // extension id.
  std::string description;  // description of extension.
  base::FilePath path;
  std::vector<IBusComponent::EngineDescription> engines;
};

// Provides an interface to list/load/unload for component extension IME.
class CHROMEOS_EXPORT ComponentExtentionIMEManagerDelegate {
 public:
  ComponentExtentionIMEManagerDelegate();
  virtual ~ComponentExtentionIMEManagerDelegate();

  // Lists installed component extension IMEs.
  virtual std::vector<ComponentExtensionIME> ListIME() = 0;

  // Loads component extension IME associated with |extension_id|.
  // Returns false if it fails, otherwise returns true.
  virtual bool Load(const std::string& extension_id,
                    const base::FilePath& path) = 0;

  // Unloads component extension IME associated with |extension_id|.
  // Returns false if it fails, otherwise returns true;
  virtual bool Unload(const std::string& extension_id,
                      const base::FilePath& path) = 0;
};

// This class manages component extension input method.
class CHROMEOS_EXPORT ComponentExtentionIMEManager {
 public:
  // This class takes the ownership of |delegate|.
  explicit ComponentExtentionIMEManager(
      ComponentExtentionIMEManagerDelegate* delegate);
  virtual ~ComponentExtentionIMEManager();

  // Initializes component extension manager. This function create internal
  // mapping between input method id and engine components. This function must
  // be called before using any other function.
  void Initialize();

  // Loads |input_method_id| component extension IME. This function returns true
  // on success. This function is safe to call multiple times. Returns false if
  // already corresponding component extension is loaded.
  bool LoadComponentExtensionIME(const std::string& input_method_id);

  // Unloads |input_method_id| component extension IME. This function returns
  // true on success. This function is safe to call multiple times. Returns
  // false if already corresponding component extension is unloaded.
  bool UnloadComonentExtensionIME(const std::string& input_method_id);

  // Returns true if |input_method_id| is component extension ime id.
  void IsComponentExtensionIMEId(const std::string& input_method_id);

  // Returns localized name of |input_method_id|.
  std::string GetName(const std::string& input_method_id);

  // Returns localized description of |input_method_id|.
  std::string GetDescription(const std::string& input_method_id);

  // Returns list of input method id associated with |language|.
  std::vector<std::string> ListIMEByLanguage(const std::string& language);

 private:
  scoped_ptr<ComponentExtentionIMEManagerDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ComponentExtentionIMEManager);
};

}  // namespace chromeos

#endif  // CHROMEOS_IME_COMPONENT_EXTENSION_IME_MANAGER_H_
