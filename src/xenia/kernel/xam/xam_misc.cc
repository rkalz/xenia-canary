
#include "xenia/base/logging.h"
#include "xenia/kernel/kernel_state.h"
#include "xenia/kernel/util/shim_utils.h"
#include "xenia/kernel/xam/xam_private.h"
#include "xenia/xbox.h"

DECLARE_bool(xconfig_initial_setup);

#define BUILD_MISC_EXPORT_STUB(functionName)                       \
  dword_result_t functionName() { return X_ERROR_FUNCTION_FAILED; } \
  DECLARE_XAM_EXPORT1(functionName, kMisc, kStub);

namespace xe {
namespace kernel {
namespace xam {

dword_result_t XamDoesOmniNeedConfiguration() {
  return 0;
}
DECLARE_XAM_EXPORT1(XamDoesOmniNeedConfiguration, kMisc, kStub);

dword_result_t XamFirstRunExperienceShouldRun() {
  return cvars::xconfig_initial_setup;
}
DECLARE_XAM_EXPORT1(XamFirstRunExperienceShouldRun, kMisc, kStub);

dword_result_t XamIsSystemTitleId(dword_t title_id) {
  if (title_id == 0) {
    return true;
  }

  if ((title_id & 0xFF000000) == 0x58000000u) {
    return (title_id & 0xFF0000) != 0x410000;  // if 'X' but not 'XA' (XBLA)
  }

  return (title_id >> 16) == 0xFFFE;  // FFFExxxx are always system apps
}
DECLARE_XAM_EXPORT1(XamIsSystemTitleId, kNone, kImplemented);

dword_result_t XamIsXbox1TitleId(dword_t title_id) {
  if (title_id == 0xFFFE0000) {
    return true;  // Xbox OG dashboard ID?
  }

  if (title_id == 0 || (title_id & 0xFF000000) == 0xFF000000) {
    return false;  // X360 system apps
  }

  return (title_id & 0x7FFF) < 0x7D0;  // lower 15 bits smaller than 2000
}
DECLARE_XAM_EXPORT1(XamIsXbox1TitleId, kNone, kImplemented);

dword_result_t XamIsSystemExperienceTitleId(dword_t title_id) {
  if ((title_id >> 16) == 0x584A) {  // 'XJ'
    return true;
  }
  if ((title_id >> 16) == 0x5848) {  // 'XH'
    return true;
  }
  return title_id == 0x584E07D2 || title_id == 0x584E07D1;  // XN-2002 / XN-2001
}
DECLARE_XAM_EXPORT1(XamIsSystemExperienceTitleId, kNone, kImplemented);

BUILD_MISC_EXPORT_STUB(XamAppUnloadStack)
BUILD_MISC_EXPORT_STUB(XamTaskModify)
BUILD_MISC_EXPORT_STUB(XamTaskGetAttributes)
BUILD_MISC_EXPORT_STUB(XamTaskGetCurrentTask)
BUILD_MISC_EXPORT_STUB(XamTaskCloseHandle)
BUILD_MISC_EXPORT_STUB(XamSetDashContext)
BUILD_MISC_EXPORT_STUB(XamLoaderLaunchTitleEx)
BUILD_MISC_EXPORT_STUB(XamLoaderGetDvdTrayState)
BUILD_MISC_EXPORT_STUB(XamInstrumentationLogEventEx)
BUILD_MISC_EXPORT_STUB(XamPlayTimerGetData)
BUILD_MISC_EXPORT_STUB(XamPlayTimerResume)
BUILD_MISC_EXPORT_STUB(XamPlayTimerForceNotification)
BUILD_MISC_EXPORT_STUB(XamPlayTimerSetData)
BUILD_MISC_EXPORT_STUB(XamPlayTimerSuspend)
BUILD_MISC_EXPORT_STUB(XamPlayTimerIsRunning)
BUILD_MISC_EXPORT_STUB(XamPlayTimerIsEnabled)
BUILD_MISC_EXPORT_STUB(XamMuteSound)
BUILD_MISC_EXPORT_STUB(XamPlayTimerGetNextResetDate)
BUILD_MISC_EXPORT_STUB(XamGetOverlappedResult)
BUILD_MISC_EXPORT_STUB(XamUniSortCmpString)
BUILD_MISC_EXPORT_STUB(XamFormatMessage)
BUILD_MISC_EXPORT_STUB(XamPrepareGamerTiles)
BUILD_MISC_EXPORT_STUB(XamLoaderGetPriorTitleId)
BUILD_MISC_EXPORT_STUB(XamGetWCNConfigFile)
BUILD_MISC_EXPORT_STUB(XamUnloadSysApp)
BUILD_MISC_EXPORT_STUB(XamCacheReset)
BUILD_MISC_EXPORT_STUB(XamGetDvrStorage)
BUILD_MISC_EXPORT_STUB(XamCacheCloseFile)
BUILD_MISC_EXPORT_STUB(XamCacheOpenFile)
BUILD_MISC_EXPORT_STUB(XamIptvGetServiceName)
BUILD_MISC_EXPORT_STUB(XamIptvUninstall)
BUILD_MISC_EXPORT_STUB(XamSetDvrStorage)
BUILD_MISC_EXPORT_STUB(XamSetPowerMode)
BUILD_MISC_EXPORT_STUB(XamLoadSysApp)

void RegisterMiscExports(xe::cpu::ExportResolver* export_resolver,
                           KernelState* kernel_state) {}
}  // namespace xam
}  // namespace kernel
}  // namespace xe