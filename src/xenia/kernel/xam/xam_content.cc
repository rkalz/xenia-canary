/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/app/emulator_window.h"
#include "xenia/base/logging.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xam/xam_module.h"
#include "xenia/kernel/xam/xam_private.h"
#include "xenia/kernel/xboxkrnl/xboxkrnl_module.h"
#include "xenia/kernel/xboxkrnl/xboxkrnl_threading.h"
#include "xenia/kernel/xenumerator.h"
#include "xenia/kernel/xevent.h"
#include "xenia/kernel/xthread.h"
#include "xenia/vfs/devices/disc_image_device.h"
#include "xenia/vfs/devices/host_path_device.h"
#include "xenia/vfs/devices/stfs_container_device.h"
#include "xenia/xbox.h"

DEFINE_int32(license_mask, 0,
             "Set license mask for activated content: "
             "0 - disable all licenses / "
             "1 - enable the first license - usually the full version license "
             "in Xbox Live Arcade games / "
             "-1 or 0xFFFFFFFF - enable all possible licenses.",
             "Content");

#define BUILD_CONTENT_EXPORT_STUB(functionName)                       \
  dword_result_t functionName() { return X_ERROR_FUNCTION_FAILED; } \
  DECLARE_XAM_EXPORT1(functionName, kContent, kStub);

namespace xe {
namespace kernel {
namespace xam {

struct DeviceInfo {
  uint32_t device_id;
  uint32_t device_type;
  uint64_t total_bytes;
  uint64_t free_bytes;
  wchar_t name[28];
};

// TODO(gibbed): real information.
//
// Until we expose real information about a HDD device, we
// claim there is 3GB free on a 4GB dummy HDD.
//
// There is a possibility that certain games are bugged in that
// they incorrectly only look at the lower 32-bits of free_bytes,
// when it is a 64-bit value. Which means any size above ~4GB
// will not be recognized properly.
#define ONE_GB (1024ull * 1024ull * 1024ull)
static const DeviceInfo dummy_device_info_ = {
    0x00000001,    1,
    120ull * ONE_GB,  // 4GB
    42ull * ONE_GB,  // 3GB, so it looks a little used.
    L"Dummy HDD",
};
#undef ONE_GB

dword_result_t XamContentGetLicenseMask(lpdword_t mask_ptr,
                                        lpunknown_t overlapped_ptr) {
  // Each bit in the mask represents a granted license. Available licenses
  // seems to vary from game to game, but most appear to use bit 0 to indicate
  // if the game is purchased or not.
  *mask_ptr = static_cast<uint32_t>(cvars::license_mask);

  // Halo CEA calls this function to check if it's being ran from a package
  // (maybe to know if it's a demo or something?)
  // If we don't error it'll skip a bunch of things (DVD check/cache mount/...)
  // So make sure to error if it's not running from package:
  if (!kernel_state()->emulator()->is_title_packaged()) {
    // Hack to still allow arcade titles get the license mask:
    if ((kernel_state()->emulator()->title_id() & 0xFFFF0000) != 0x58410000) {
      return X_E_ACCESS_DENIED;
    }
  }

  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedImmediate(overlapped_ptr,
                                                X_ERROR_SUCCESS);
    return X_ERROR_IO_PENDING;
  } else {
    return X_ERROR_SUCCESS;
  }
}
DECLARE_XAM_EXPORT2(XamContentGetLicenseMask, kContent, kStub, kHighFrequency);

dword_result_t XamContentGetDeviceName(dword_t device_id,
                                       lpwstring_t name_buffer,
                                       dword_t name_capacity) {
  if ((device_id & 0x0000000F) != dummy_device_info_.device_id) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  auto name = std::wstring(dummy_device_info_.name);
  if (name_capacity < name.size() + 1) {
    return X_ERROR_INSUFFICIENT_BUFFER;
  }

  xe::store_and_swap<std::wstring>(name_buffer, name);
  ((wchar_t*)name_buffer)[name.size()] = 0;
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamContentGetDeviceName, kContent, kImplemented);

dword_result_t XamContentGetDeviceState(dword_t device_id,
                                        lpunknown_t overlapped_ptr) {
  if ((device_id & 0x0000000F) != dummy_device_info_.device_id) {
    if (overlapped_ptr) {
      kernel_state()->CompleteOverlappedImmediateEx(
          overlapped_ptr, X_ERROR_FUNCTION_FAILED, X_ERROR_DEVICE_NOT_CONNECTED,
          0);
      return X_ERROR_SUCCESS;
    } else {
      return X_ERROR_DEVICE_NOT_CONNECTED;
    }
  }

  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedImmediate(overlapped_ptr,
                                                X_ERROR_SUCCESS);
    return X_ERROR_IO_PENDING;
  } else {
    return X_ERROR_SUCCESS;
  }
}
DECLARE_XAM_EXPORT1(XamContentGetDeviceState, kContent, kStub);

typedef struct {
  xe::be<uint32_t> device_id;
  xe::be<uint32_t> device_type;
  xe::be<uint64_t> total_bytes;
  xe::be<uint64_t> free_bytes;
  xe::be<uint16_t> name[28];
} X_CONTENT_DEVICE_DATA;
static_assert_size(X_CONTENT_DEVICE_DATA, 0x50);

dword_result_t XamContentGetDeviceData(
    dword_t device_id, pointer_t<X_CONTENT_DEVICE_DATA> device_data) {
  if ((device_id & 0x0000000F) != dummy_device_info_.device_id) {
    // TODO(benvanik): memset 0 the data?
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  device_data.Zero();
  const auto& device_info = dummy_device_info_;
  device_data->device_id = device_info.device_id;
  device_data->device_type = device_info.device_type;
  device_data->total_bytes = device_info.total_bytes;
  device_data->free_bytes = device_info.free_bytes;
  xe::store_and_swap<std::wstring>(&device_data->name[0], device_info.name);
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamContentGetDeviceData, kContent, kImplemented);

dword_result_t XamContentResolve(dword_t user_index, lpvoid_t content_data_ptr,
                                 lpunknown_t buffer_ptr, dword_t buffer_size,
                                 dword_t unk1, dword_t unk2, dword_t unk3) {
  auto content_data = XCONTENT_DATA((uint8_t*)content_data_ptr);

  // Result of buffer_ptr is sent to RtlInitAnsiString.
  // buffer_size is usually 260 (max path).
  // Games expect zero if resolve was successful.
  assert_always();
  XELOGW("XamContentResolve unimplemented!");
  return X_ERROR_NOT_FOUND;
}
DECLARE_XAM_EXPORT1(XamContentResolve, kContent, kStub);

// https://github.com/MrColdbird/gameservice/blob/master/ContentManager.cpp
// https://github.com/LestaD/SourceEngine2007/blob/master/se2007/engine/xboxsystem.cpp#L499
dword_result_t XamContentCreateEnumerator(dword_t user_index, dword_t device_id,
                                          dword_t content_type,
                                          dword_t content_flags,
                                          dword_t items_per_enumerate,
                                          lpdword_t buffer_size_ptr,
                                          lpdword_t handle_out) {
  assert_not_null(handle_out);
  if ((device_id && (device_id & 0x0000000F) != dummy_device_info_.device_id) ||
      !handle_out) {
    if (buffer_size_ptr) {
      *buffer_size_ptr = 0;
    }

    // TODO(benvanik): memset 0 the data?
    return X_E_INVALIDARG;
  }

  if (buffer_size_ptr) {
    *buffer_size_ptr = (uint32_t)XCONTENT_DATA::kSize * items_per_enumerate;
  }

  auto e = new XStaticEnumerator(kernel_state(), items_per_enumerate,
                                 XCONTENT_DATA::kSize);
  e->Initialize();

  // Get all content data.
  auto content_datas = kernel_state()->content_manager()->ListContent(
      device_id ? static_cast<uint32_t>(device_id)
                : dummy_device_info_.device_id,
      content_type);
  for (auto& content_data : content_datas) {
    auto ptr = e->AppendItem();
    assert_not_null(ptr);
    content_data.Write(ptr);
  }

  XELOGD("XamContentCreateEnumerator: added %d items to enumerator",
         e->item_count());

  *handle_out = e->handle();
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamContentCreateEnumerator, kContent, kImplemented);

dword_result_t XamContentCreateDeviceEnumerator(dword_t content_type,
                                                dword_t content_flags,
                                                dword_t max_count,
                                                lpdword_t buffer_size_ptr,
                                                lpdword_t handle_out) {
  assert_not_null(handle_out);

  if (buffer_size_ptr) {
    *buffer_size_ptr = sizeof(DeviceInfo) * max_count;
  }

  auto e = new XStaticEnumerator(kernel_state(), max_count, sizeof(DeviceInfo));
  e->Initialize();

  // Copy our dummy device into the enumerator
  DeviceInfo* dev = (DeviceInfo*)e->AppendItem();
  if (dev) {
    xe::store_and_swap(&dev->device_id, dummy_device_info_.device_id);
    xe::store_and_swap(&dev->device_type, dummy_device_info_.device_type);
    xe::store_and_swap(&dev->total_bytes, dummy_device_info_.total_bytes);
    xe::store_and_swap(&dev->free_bytes, dummy_device_info_.free_bytes);
    xe::copy_and_swap(dev->name, dummy_device_info_.name, 28);
  }

  *handle_out = e->handle();
  return X_ERROR_SUCCESS;
}
DECLARE_XAM_EXPORT1(XamContentCreateDeviceEnumerator, kNone, kImplemented);

dword_result_t XamContentCreateEx(dword_t user_index, lpstring_t root_name,
                                  lpvoid_t content_data_ptr, dword_t flags,
                                  lpdword_t disposition_ptr,
                                  lpdword_t license_mask_ptr,
                                  dword_t cache_size, qword_t content_size,
                                  lpvoid_t overlapped_ptr) {
  X_RESULT result = X_ERROR_INVALID_PARAMETER;
  auto content_data = XCONTENT_DATA((uint8_t*)content_data_ptr);

  auto content_manager = kernel_state()->content_manager();
  bool create = false;
  bool open = false;
  switch (flags & 0xF) {
    case 1:  // CREATE_NEW
             // Fail if exists.
      if (content_manager->ContentExists(content_data)) {
        result = X_ERROR_ALREADY_EXISTS;
      } else {
        create = true;
      }
      break;
    case 2:  // CREATE_ALWAYS
             // Overwrite existing, if any.
      if (content_manager->ContentExists(content_data)) {
        content_manager->DeleteContent(content_data);
        create = true;
      } else {
        create = true;
      }
      break;
    case 3:  // OPEN_EXISTING
             // Open only if exists.
      if (!content_manager->ContentExists(content_data)) {
        result = X_ERROR_PATH_NOT_FOUND;
      } else {
        open = true;
      }
      break;
    case 4:  // OPEN_ALWAYS
             // Create if needed.
      if (!content_manager->ContentExists(content_data)) {
        create = true;
      } else {
        open = true;
      }
      break;
    case 5:  // TRUNCATE_EXISTING
             // Fail if doesn't exist, if does exist delete and recreate.
      if (!content_manager->ContentExists(content_data)) {
        result = X_ERROR_PATH_NOT_FOUND;
      } else {
        content_manager->DeleteContent(content_data);
        create = true;
      }
      break;
    default:
      assert_unhandled_case(flags & 0xF);
      break;
  }

  // creation result
  // 0 = ?
  // 1 = created
  // 2 = opened
  uint32_t disposition = create ? 1 : 2;
  if (disposition_ptr) {
    if (overlapped_ptr) {
      // If async always set to zero, but don't set to a real value.
      *disposition_ptr = 0;
    } else {
      *disposition_ptr = disposition;
    }
  }

  if (create) {
    result =
        content_manager->CreateContent(root_name.value(), content_data, flags);
  } else if (open) {
    result = content_manager->OpenContent(root_name.value(), content_data);
  }

  if (!result && content_data.content_type ==
                     (uint32_t)vfs::XContentType::kMarketplaceContent) {
    // Load up spa.bin from this DLC if it has one:
    // TODO: should we do this inside ContentManager instead?
    auto spa_entry = kernel_state()->file_system()->ResolvePath(
        root_name.value() + ":\\spa.bin");
    if (spa_entry) {
      kernel::xam::xdbf::SpaFile spa;
      bool spa_result = false;
      // If the FS supports mapping, map the file in and load from that.
      if (spa_entry->can_map()) {
        // Map.
        auto mmap = spa_entry->OpenMapped(MappedMemory::Mode::kRead);
        if (mmap) {
          // Load the SPA
          spa_result = spa.Read(mmap->data(), mmap->size());
        }
      } else {
        std::vector<uint8_t> buffer(spa_entry->size());

        // Open file for reading.
        vfs::File* file = nullptr;
        auto result2 = spa_entry->Open(vfs::FileAccess::kGenericRead, &file);
        if (!result2) {
          // Read entire file into memory.
          // Ugh.
          size_t bytes_read = 0;
          result2 =
              file->ReadSync(buffer.data(), buffer.size(), 0, &bytes_read);
          if (!result2) {
            // Load the SPA.
            spa_result = spa.Read(buffer.data(), bytes_read);

            // Close the file.
            file->Destroy();
          }
        }
      }
      if (spa_result) {
        XELOGI("Loaded SPA data from DLC package %s (%S)",
               content_data.file_name.c_str(),
               content_data.display_name.c_str());
        xdbf::X_XDBF_XTHD_DATA title_data;
        if (spa.GetTitleData(&title_data)) {
          XELOGI("(SPA version: %d.%d.%d.%d)",
                 (uint32_t)title_data.title_version_major,
                 (uint32_t)title_data.title_version_minor,
                 (uint32_t)title_data.title_version_build,
                 (uint32_t)title_data.title_version_revision);
        }
        // Set/update title SPA
        for (uint32_t i = 0; i < kernel_state()->num_profiles(); i++) {
          auto profile = kernel_state()->user_profile(i);
          if (profile) {
            profile->SetTitleSpaData(spa);
          }
        }
      }
    }
  }

  if (license_mask_ptr && XSUCCEEDED(result)) {
    *license_mask_ptr = 0;  // Stub!
  }

  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedImmediateEx(overlapped_ptr, result, 0,
                                                  disposition);
    return X_ERROR_IO_PENDING;
  } else {
    return result;
  }
}
DECLARE_XAM_EXPORT1(XamContentCreateEx, kContent, kImplemented);

dword_result_t XamContentCreate(dword_t user_index, lpstring_t root_name,
                                lpvoid_t content_data_ptr, dword_t flags,
                                lpdword_t disposition_ptr,
                                lpdword_t license_mask_ptr,
                                lpvoid_t overlapped_ptr) {
  return XamContentCreateEx(user_index, root_name, content_data_ptr, flags,
                            disposition_ptr, license_mask_ptr, 0, 0,
                            overlapped_ptr);
}
DECLARE_XAM_EXPORT1(XamContentCreate, kContent, kImplemented);

dword_result_t XamContentCreateInternal(lpstring_t root_name,
                                        lpvoid_t content_data_ptr,
                                        dword_t flags,
                                        lpdword_t disposition_ptr,
                                        lpdword_t license_mask_ptr,
                                        lpvoid_t overlapped_ptr) {
  return XamContentCreateEx(0xFF, root_name, content_data_ptr, flags,
                            disposition_ptr, license_mask_ptr, 0, 0,
                            overlapped_ptr);
}
DECLARE_XAM_EXPORT1(XamContentCreateInternal, kContent, kSketchy);

dword_result_t XamContentOpenFile(dword_t user_index, lpstring_t root_name,
                                  lpstring_t path, dword_t flags,
                                  lpdword_t disposition_ptr,
                                  lpdword_t license_mask_ptr,
                                  lpvoid_t overlapped_ptr) {
  // TODO(gibbed): arguments assumed based on XamContentCreate.
  return X_ERROR_FILE_NOT_FOUND;
}
DECLARE_XAM_EXPORT1(XamContentOpenFile, kContent, kStub);

dword_result_t XamContentFlush(lpstring_t root_name,
                               lpunknown_t overlapped_ptr) {
  X_RESULT result = X_ERROR_SUCCESS;
  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedImmediate(overlapped_ptr, result);
    return X_ERROR_IO_PENDING;
  } else {
    return result;
  }
}
DECLARE_XAM_EXPORT1(XamContentFlush, kContent, kStub);

dword_result_t XamContentClose(lpstring_t root_name,
                               lpunknown_t overlapped_ptr) {
  // Closes a previously opened root from XamContentCreate*.
  auto result =
      kernel_state()->content_manager()->CloseContent(root_name.value());

  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedImmediate(overlapped_ptr, result);
    return X_ERROR_IO_PENDING;
  } else {
    return result;
  }
}
DECLARE_XAM_EXPORT1(XamContentClose, kContent, kImplemented);

dword_result_t XamContentGetCreator(dword_t user_index,
                                    lpvoid_t content_data_ptr,
                                    lpdword_t is_creator_ptr,
                                    lpqword_t creator_xuid_ptr,
                                    lpunknown_t overlapped_ptr) {
  auto result = X_ERROR_SUCCESS;

  auto content_data = XCONTENT_DATA((uint8_t*)content_data_ptr);

  auto user_profile = kernel_state()->user_profile(user_index);
  if (!user_profile) {
    result = X_ERROR_NOT_LOGGED_ON;  // TODO: find right error code
  } else {
    if (content_data.content_type == 1) {
      // User always creates saves.
      *is_creator_ptr = 1;
      if (creator_xuid_ptr) {
        *creator_xuid_ptr = user_profile->xuid();
      }
    } else {
      *is_creator_ptr = 0;
      if (creator_xuid_ptr) {
        *creator_xuid_ptr = 0;
      }
    }
  }
  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedImmediate(overlapped_ptr, result);
    return X_ERROR_IO_PENDING;
  } else {
    return result;
  }
}
DECLARE_XAM_EXPORT1(XamContentGetCreator, kContent, kImplemented);

dword_result_t XamContentGetThumbnail(dword_t user_index,
                                      lpvoid_t content_data_ptr,
                                      lpvoid_t buffer_ptr,
                                      lpdword_t buffer_size_ptr,
                                      lpunknown_t overlapped_ptr) {
  assert_not_null(buffer_size_ptr);
  uint32_t buffer_size = *buffer_size_ptr;
  auto content_data = XCONTENT_DATA((uint8_t*)content_data_ptr);

  // Get thumbnail (if it exists).
  std::vector<uint8_t> buffer;
  auto result = kernel_state()->content_manager()->GetContentThumbnail(
      content_data, &buffer);

  *buffer_size_ptr = uint32_t(buffer.size());

  if (XSUCCEEDED(result)) {
    // Write data, if we were given a pointer.
    // This may have just been a size query.
    if (buffer_ptr) {
      if (buffer_size < buffer.size()) {
        // Dest buffer too small.
        result = X_ERROR_INSUFFICIENT_BUFFER;
      } else {
        // Copy data.
        std::memcpy((uint8_t*)buffer_ptr, buffer.data(), buffer.size());
      }
    }
  }

  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedImmediate(overlapped_ptr, result);
    return X_ERROR_IO_PENDING;
  } else {
    return result;
  }
}
DECLARE_XAM_EXPORT1(XamContentGetThumbnail, kContent, kImplemented);

dword_result_t XamContentSetThumbnail(dword_t user_index,
                                      lpvoid_t content_data_ptr,
                                      lpvoid_t buffer_ptr, dword_t buffer_size,
                                      lpunknown_t overlapped_ptr) {
  auto content_data = XCONTENT_DATA((uint8_t*)content_data_ptr);

  // Buffer is PNG data.
  auto buffer = std::vector<uint8_t>((uint8_t*)buffer_ptr,
                                     (uint8_t*)buffer_ptr + buffer_size);
  auto result = kernel_state()->content_manager()->SetContentThumbnail(
      content_data, std::move(buffer));

  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedImmediate(overlapped_ptr, result);
    return X_ERROR_IO_PENDING;
  } else {
    return result;
  }
}
DECLARE_XAM_EXPORT1(XamContentSetThumbnail, kContent, kImplemented);

dword_result_t XamContentDelete(dword_t user_index, lpvoid_t content_data_ptr,
                                lpunknown_t overlapped_ptr) {
  auto content_data = XCONTENT_DATA((uint8_t*)content_data_ptr);

  auto result = kernel_state()->content_manager()->DeleteContent(content_data);

  if (overlapped_ptr) {
    kernel_state()->CompleteOverlappedImmediate(overlapped_ptr, result);
    return X_ERROR_IO_PENDING;
  } else {
    return result;
  }
}
DECLARE_XAM_EXPORT1(XamContentDelete, kContent, kImplemented);

// Based on Discswap Prototype from Wildenhaus
// https://github.com/Wildenhaus/xenia/commit/04d2e3951c13fbe4f9574005122d757a2f6e373c
dword_result_t XamSwapDisc(dword_t disc_number,
                           pointer_t<kernel::X_KEVENT> completion_handle,
                           lpstring_t error_message) {

  // error_message not correct type/ptr
  XELOGI("XamSwapDisc requests disc:(%d)", disc_number);
  std::wstring local_path = app::EmulatorWindow::SwapNext(disc_number);
  XELOGD("XamSwapDisc SwapNext returned path:( %S )", local_path.c_str());
  if (local_path != L"") {
    auto filesystem = kernel_state()->file_system();
    auto mount_path = "\\Device\\LauncherData";
    auto last_slash = local_path.find_last_of(xe::kPathSeparator);
    auto last_dot = local_path.find_last_of('.');

    if (filesystem->ResolveDevice(mount_path) != NULL) {
      filesystem->UnregisterDevice(mount_path);
    }

    if (last_dot < last_slash) {
      last_dot = std::wstring::npos;
    }
    if (last_dot == std::wstring::npos) {
      // Likely an STFS container.
      auto dev =
          std::make_unique<vfs::StfsContainerDevice>(mount_path, local_path);
      dev->Initialize();
      filesystem->RegisterDevice(std::move(dev));
    };
    auto extension = local_path.substr(last_dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   tolower);
    if (extension == L".xex" || extension == L".elf" || extension == L".exe") {
      // Treat as a naked xex file.
      auto parent_path = xe::find_base_path(local_path);
      auto dev =
          std::make_unique<vfs::HostPathDevice>(mount_path, parent_path, true);
      dev->Initialize();
      filesystem->RegisterDevice(std::move(dev));
    } else {
      // Assume a disc image.
      auto dev = std::make_unique<vfs::DiscImageDevice>(mount_path, local_path);
      dev->Initialize();
      filesystem->RegisterDevice(std::move(dev));
    }

    // Register the new device to d: and game:
    filesystem->UnregisterSymbolicLink("d:");
    filesystem->UnregisterSymbolicLink("game:");
    filesystem->RegisterSymbolicLink("d:", mount_path);
    filesystem->RegisterSymbolicLink("game:", mount_path);

  }
  // Resolve the pending disc swap event
  auto kevent = xboxkrnl::xeKeSetEvent(completion_handle, 1, 0);

  // Release the completion handle
  auto object = XObject::GetNativeObject<XObject>(
      kernel_state(),
      kernel_memory()->virtual_membase() + (dword_t)completion_handle);

  if (object) {
  XELOGI("XamSwapDisc Release object");
    object->ReleaseHandle();
  }

  return 0;
}
DECLARE_XAM_EXPORT1(XamSwapDisc, kContent, kSketchy);

BUILD_CONTENT_EXPORT_STUB(XamContentResolveInternal)
BUILD_CONTENT_EXPORT_STUB(XamContentLaunchImageInternal)
BUILD_CONTENT_EXPORT_STUB(XamContentDeleteInternal)
BUILD_CONTENT_EXPORT_STUB(XamContentSetThumbnailInternal)
BUILD_CONTENT_EXPORT_STUB(XamContentGetDefaultDevice)
BUILD_CONTENT_EXPORT_STUB(XamContentSetMediaMetaDataInternal)
BUILD_CONTENT_EXPORT_STUB(XamContentGetLocalizedString)
BUILD_CONTENT_EXPORT_STUB(XamContentGetMetaDataInternal)
BUILD_CONTENT_EXPORT_STUB(XamContentGetAttributesInternal)
BUILD_CONTENT_EXPORT_STUB(XamContentAggregateCreateEnumerator)
BUILD_CONTENT_EXPORT_STUB(XamContentCopyInternal)
BUILD_CONTENT_EXPORT_STUB(XamContentMoveInternal)
BUILD_CONTENT_EXPORT_STUB(XamContentOpenFileInternal)
BUILD_CONTENT_EXPORT_STUB(XamSetDashContextEx)
BUILD_CONTENT_EXPORT_STUB(XamContentCreateEnumeratorInternal)

void RegisterContentExports(xe::cpu::ExportResolver* export_resolver,
                            KernelState* kernel_state) {}

}  // namespace xam
}  // namespace kernel
}  // namespace xe
