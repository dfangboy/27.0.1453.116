// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_H_
#define CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/observer_list_threadsafe.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/storage_monitor/storage_info.h"

class ChromeBrowserMainPartsLinux;
class ChromeBrowserMainPartsMac;
class MediaGalleriesPrivateApiTest;
class MediaGalleriesPrivateEjectApiTest;
class PlatformAppMediaGalleriesBrowserTest;
class SystemInfoStorageApiTest;

namespace chrome {

class MediaFileSystemRegistryTest;
class RemovableStorageObserver;
class TransientDeviceIds;

// Base class for platform-specific instances watching for removable storage
// attachments/detachments.
// Lifecycle contracts: This class is created by ChromeBrowserMain
// implementations before the profile is initialized, so listeners can be
// created during profile construction. The platform-specific initialization,
// which can lead to calling registered listeners with notifications of
// attached volumes, will happen after profile construction.
class StorageMonitor {
 public:
  // This interface is provided to generators of storage notifications.
  class Receiver {
   public:
    virtual ~Receiver();

    virtual void ProcessAttach(const StorageInfo& info) = 0;
    virtual void ProcessDetach(const std::string& id) = 0;
  };

  // Status codes for the result of an EjectDevice() call.
  enum EjectStatus {
    EJECT_OK,
    EJECT_IN_USE,
    EJECT_NO_SUCH_DEVICE,
    EJECT_FAILURE
  };

  // Returns a pointer to an object owned by the BrowserMainParts, with lifetime
  // somewhat shorter than a process Singleton.
  static StorageMonitor* GetInstance();

  // Finds the device that contains |path| and populates |device_info|.
  // Should be able to handle any path on the local system, not just removable
  // storage. Returns false if unable to find the device.
  virtual bool GetStorageInfoForPath(
      const base::FilePath& path,
      StorageInfo* device_info) const = 0;

  // Returns the storage size of the device present at |location|. If the
  // device information is unavailable, returns zero.
  // TODO(gbillock): delete this in favor of GetStorageInfoForPath.
  virtual uint64 GetStorageSize(
      const base::FilePath::StringType& location) const = 0;

// TODO(gbillock): make this either unnecessary (implementation-specific) or
// platform-independent.
#if defined(OS_WIN)
  // Gets the MTP device storage information specified by |storage_device_id|.
  // On success, returns true and fills in |device_location| with device
  // interface details and |storage_object_id| with the string ID that
  // uniquely identifies the object on the device. This ID need not be
  // persistent across sessions.
  virtual bool GetMTPStorageInfoFromDeviceId(
      const std::string& storage_device_id,
      string16* device_location,
      string16* storage_object_id) const = 0;
#endif

  // Returns information for attached removable storage.
  std::vector<StorageInfo> GetAttachedStorage() const;

  void AddObserver(RemovableStorageObserver* obs);
  void RemoveObserver(RemovableStorageObserver* obs);

  std::string GetTransientIdForDeviceId(const std::string& device_id);
  std::string GetDeviceIdForTransientId(const std::string& transient_id) const;

  virtual void EjectDevice(
      const std::string& device_id,
      base::Callback<void(EjectStatus)> callback);

 protected:
  friend class ::MediaGalleriesPrivateApiTest;
  friend class ::MediaGalleriesPrivateEjectApiTest;
  friend class MediaFileSystemRegistryTest;
  friend class ::PlatformAppMediaGalleriesBrowserTest;
  friend class ::SystemInfoStorageApiTest;

  StorageMonitor();
  virtual ~StorageMonitor();

  // Removes the existing singleton for testing.
  // (So that a new one can be created.)
  static void RemoveSingletonForTesting();

  virtual Receiver* receiver() const;

 private:
  class ReceiverImpl;
  friend class ReceiverImpl;

  typedef std::map<std::string, StorageInfo> RemovableStorageMap;

  void ProcessAttach(const StorageInfo& storage);
  void ProcessDetach(const std::string& id);

  scoped_ptr<Receiver> receiver_;

  scoped_refptr<ObserverListThreadSafe<RemovableStorageObserver> >
      observer_list_;

  // For manipulating removable_storage_map_ structure.
  mutable base::Lock storage_lock_;

  // Map of all the attached removable storage devices.
  RemovableStorageMap storage_map_;

  scoped_ptr<TransientDeviceIds> transient_device_ids_;
};

} // namespace chrome

#endif  // CHROME_BROWSER_STORAGE_MONITOR_STORAGE_MONITOR_H_
