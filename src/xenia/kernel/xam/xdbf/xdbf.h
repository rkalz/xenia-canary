/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2016 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_KERNEL_XAM_XDBF_XDBF_H_
#define XENIA_KERNEL_XAM_XDBF_XDBF_H_

#include <string>
#include <vector>

#include "xenia/base/clock.h"
#include "xenia/base/memory.h"
#include "xenia/kernel/xam/xdbf/xdbf_xbox.h"
#include "xenia/xbox.h"

namespace xe {
namespace kernel {
namespace xam {
namespace xdbf {

// https://github.com/oukiar/freestyledash/blob/master/Freestyle/Tools/XEX/SPA.h
// https://github.com/oukiar/freestyledash/blob/master/Freestyle/Tools/XEX/SPA.cpp

enum class SpaID : uint64_t {
  Xach = 'XACH',
  Xstr = 'XSTR',
  Xstc = 'XSTC',
  Xthd = 'XTHD',
  Title = 0x8000,
};

enum class SpaSection : uint16_t {
  kMetadata = 0x1,
  kImage = 0x2,
  kStringTable = 0x3,
};

enum class GpdSection : uint16_t {
  kAchievement = 0x1,
  kImage = 0x2,
  kSetting = 0x3,
  kTitle = 0x4,
  kString = 0x5,
  kProtectedAchievement = 0x6,  // GFWL only
};

inline std::wstring ReadNullTermString(const wchar_t* ptr) {
  std::wstring retval;
  wchar_t data = xe::byte_swap(*ptr);
  while (data != 0) {
    retval += data;
    ptr++;
    data = xe::byte_swap(*ptr);
  }
  return retval;
}

struct TitlePlayed {
  uint32_t title_id = 0;
  std::wstring title_name;
  uint32_t achievements_possible = 0;
  uint32_t achievements_earned = 0;
  uint32_t gamerscore_total = 0;
  uint32_t gamerscore_earned = 0;
  uint16_t reserved_achievement_count = 0;
  X_XDBF_AVATARAWARDS_COUNTER all_avatar_awards = {0, 0};
  X_XDBF_AVATARAWARDS_COUNTER male_avatar_awards = {0, 0};
  X_XDBF_AVATARAWARDS_COUNTER female_avatar_awards = {0, 0};
  uint32_t reserved_flags = 0;
  uint64_t last_played = 0;

  void ReadGPD(const X_XDBF_GPD_TITLEPLAYED* src) {
    title_id = src->title_id;
    achievements_possible = src->achievements_possible;
    achievements_earned = src->achievements_earned;
    gamerscore_total = src->gamerscore_total;
    gamerscore_earned = src->gamerscore_earned;
    reserved_achievement_count = src->reserved_achievement_count;
    all_avatar_awards = src->all_avatar_awards;
    male_avatar_awards = src->male_avatar_awards;
    female_avatar_awards = src->female_avatar_awards;
    reserved_flags = src->reserved_flags;
    last_played = src->last_played;

    auto* txt_ptr = reinterpret_cast<const uint8_t*>(src + 1);
    title_name = ReadNullTermString((const wchar_t*)txt_ptr);
  }

  void WriteGPD(X_XDBF_GPD_TITLEPLAYED* dest) const {
    dest->title_id = title_id;
    dest->achievements_possible = achievements_possible;
    dest->achievements_earned = achievements_earned;
    dest->gamerscore_total = gamerscore_total;
    dest->gamerscore_earned = gamerscore_earned;
    dest->reserved_achievement_count = reserved_achievement_count;
    dest->all_avatar_awards = all_avatar_awards;
    dest->male_avatar_awards = male_avatar_awards;
    dest->female_avatar_awards = female_avatar_awards;
    // todo: reserved_flags may need to be updated with # of newly unlocked (not
    // synced to live) achievements - # might be obfuscated in some way
    dest->reserved_flags = reserved_flags;
    dest->last_played = last_played;

    auto* txt_ptr = reinterpret_cast<const uint8_t*>(dest + 1);
    xe::copy_and_swap<wchar_t>((wchar_t*)txt_ptr, title_name.c_str(),
                               title_name.size());
  }
};

enum class AchievementType : uint32_t {
  kCompletion = 1,
  kLeveling = 2,
  kUnlock = 3,
  kEvent = 4,
  kTournament = 5,
  kCheckpoint = 6,
  kOther = 7,
};

enum class AchievementPlatform : uint32_t {
  kX360 = 0x100000,
  kPC = 0x200000,
  kMobile = 0x300000,
  kWebGames = 0x400000,
};

enum class AchievementFlags : uint32_t {
  kTypeMask = 0x7,
  kShowUnachieved = 0x8,
  kAchievedOnline = 0x10000,
  kAchieved = 0x20000,
  kNotAchievable = 0x40000,
  kWasNotAchievable = 0x80000,
  kPlatformMask = 0x700000,
  kColorizable = 0x1000000,  // avatar awards only?
};

struct Achievement {
  uint16_t id = 0;
  std::wstring label;
  std::wstring description;
  std::wstring unachieved_desc;
  uint32_t image_id = 0;
  uint32_t gamerscore = 0;
  uint32_t flags = 0;
  uint64_t unlock_time = 0;

  AchievementType GetType() {
    return static_cast<AchievementType>(
        flags & static_cast<uint32_t>(AchievementFlags::kTypeMask));
  }

  AchievementPlatform GetPlatform() {
    return static_cast<AchievementPlatform>(
        flags & static_cast<uint32_t>(AchievementFlags::kPlatformMask));
  }

  bool IsUnlockable() {
    return !(flags & static_cast<uint32_t>(AchievementFlags::kNotAchievable)) ||
           (flags & static_cast<uint32_t>(AchievementFlags::kWasNotAchievable));
  }

  bool IsUnlocked() {
    return flags & static_cast<uint32_t>(AchievementFlags::kAchieved);
  }

  bool IsUnlockedOnline() {
    return flags & static_cast<uint32_t>(AchievementFlags::kAchievedOnline);
  }

  void Unlock(bool online = false) {
    if (!IsUnlockable()) {
      return;
    }

    flags |= static_cast<uint32_t>(AchievementFlags::kAchieved);
    if (online) {
      flags |= static_cast<uint32_t>(AchievementFlags::kAchievedOnline);
    }

    unlock_time = Clock::QueryHostSystemTime();
  }

  void Lock() {
    flags = flags & ~(static_cast<uint32_t>(AchievementFlags::kAchieved));
    flags = flags & ~(static_cast<uint32_t>(AchievementFlags::kAchievedOnline));
    unlock_time = 0;
  }

  void ReadGPD(const X_XDBF_GPD_ACHIEVEMENT* src) {
    id = src->id;
    image_id = src->image_id;
    gamerscore = src->gamerscore;
    flags = src->flags;
    unlock_time = src->unlock_time;

    auto* txt_ptr = reinterpret_cast<const uint8_t*>(src + 1);

    label = ReadNullTermString((const wchar_t*)txt_ptr);

    txt_ptr += (label.length() * 2) + 2;
    description = ReadNullTermString((const wchar_t*)txt_ptr);

    txt_ptr += (description.length() * 2) + 2;
    unachieved_desc = ReadNullTermString((const wchar_t*)txt_ptr);
  }
};

struct Setting {
  X_XDBF_SETTING_ID id = (X_XDBF_SETTING_ID)0;
  X_XUSER_DATA value;
  std::vector<uint8_t> extraData;

  Setting() { value.type = X_XUSER_DATA_TYPE::kNull; }
  Setting(X_XDBF_SETTING_ID id, uint32_t value) : id(id) { Value(value); }
  Setting(X_XDBF_SETTING_ID id, uint64_t value) : id(id) { Value(value); }
  Setting(X_XDBF_SETTING_ID id, float value) : id(id) { Value(value); }
  Setting(X_XDBF_SETTING_ID id, const std::wstring& value) : id(id) {
    Value(value);
  }
  Setting(X_XDBF_SETTING_ID id, const std::initializer_list<uint8_t>& value)
      : id(id), extraData(value) {
    this->value.type = X_XUSER_DATA_TYPE::kBinary;
  }

  bool IsTitleSpecific() const {
    return id == XPROFILE_TITLE_SPECIFIC1 || id == XPROFILE_TITLE_SPECIFIC2 ||
           id == XPROFILE_TITLE_SPECIFIC3;
  }

  void ReadGPD(const X_XDBF_GPD_SETTING* src) {
    id = src->setting_id;
    memcpy(&value, &src->value, sizeof(X_XUSER_DATA));

    if (value.type == X_XUSER_DATA_TYPE::kBinary) {
      extraData.resize(src->value.binary.cbData);
      memcpy(extraData.data(), (uint8_t*)&src[1], src->value.binary.cbData);
    } else if (value.type == X_XUSER_DATA_TYPE::kUnicode) {
      extraData.resize(src->value.string.cbData);
      memcpy(extraData.data(), (uint8_t*)&src[1], src->value.string.cbData);
    }
  }

  void Value(uint32_t new_value) {
    value.type = X_XUSER_DATA_TYPE::kInt32;
    assert(XPROFILEID_TYPE(id) == value.type);

    value.nData = new_value;
    extraData.clear();
  }

  void Value(uint64_t new_value) {
    value.type = X_XUSER_DATA_TYPE::kInt64;
    if (XPROFILEID_TYPE(id) == X_XUSER_DATA_TYPE::kDateTime) {
      value.type = X_XUSER_DATA_TYPE::kDateTime;
    }

    assert(XPROFILEID_TYPE(id) == value.type);

    value.i64Data = new_value;
    extraData.clear();
  }

  void Value(float new_value) {
    value.type = X_XUSER_DATA_TYPE::kFloat;
    assert(XPROFILEID_TYPE(id) == value.type);

    value.fData = new_value;
    extraData.clear();
  }

  void Value(double new_value) {
    value.type = X_XUSER_DATA_TYPE::kDouble;
    assert(XPROFILEID_TYPE(id) == value.type);

    value.dblData = new_value;
    extraData.clear();
  }

  void Value(const std::wstring& new_value) {
    value.type = X_XUSER_DATA_TYPE::kUnicode;
    assert(XPROFILEID_TYPE(id) == value.type);

    value.i64Data = 0;
    value.string.cbData =
        (uint32_t)((new_value.length() + 1) * sizeof(wchar_t));
    extraData.resize(value.string.cbData);
    xe::copy_and_swap<wchar_t>((wchar_t*)extraData.data(), new_value.c_str(),
                               new_value.length());
    *(wchar_t*)(extraData.data() + value.string.cbData - 2) =
        0;  // null-terminate
  }

  std::wstring ValueString() {
    assert(value.type == X_XUSER_DATA_TYPE::kUnicode);

    std::vector<uint8_t> swapped;
    swapped.resize(extraData.size());
    xe::copy_and_swap<wchar_t>((wchar_t*)swapped.data(),
                               (wchar_t*)extraData.data(),
                               extraData.size() / sizeof(wchar_t));
    return std::wstring((wchar_t*)swapped.data());
  }
};

struct Entry {
  X_XDBF_ENTRY info;
  std::vector<uint8_t> data;
};

// Parses/creates an XDBF (XboxDataBaseFormat) file
// http://www.free60.org/wiki/XDBF
class XdbfFile {
 public:
  XdbfFile() {
    header_.magic = 'XDBF';
    header_.version = 1;
  }

  bool Read(const uint8_t* data, size_t data_size);
  bool Write(uint8_t* data, size_t* data_size);

  Entry* GetEntry(uint16_t section, uint64_t id) const;

  // Updates (or adds) an entry
  bool UpdateEntry(const Entry& entry);

 protected:
  X_XDBF_HEADER header_;
  std::vector<Entry> entries_;
  std::vector<X_XDBF_FILELOC> free_entries_;
};

class SpaFile : public XdbfFile {
 public:
  std::string GetStringTableEntry(XLanguage lang, uint16_t string_id) const;

  uint32_t GetAchievements(XLanguage lang,
                           std::vector<Achievement>* achievements) const;

  Entry* GetIcon() const;
  XLanguage GetDefaultLanguage() const;
  std::string GetTitleName() const;
  bool GetTitleData(X_XDBF_XTHD_DATA* title_data) const;
};

class GpdFile : public XdbfFile {
 public:
  GpdFile() : title_id_(-1) {}
  GpdFile(uint32_t title_id) : title_id_(title_id) {}

  bool GetAchievement(uint16_t id, Achievement* dest);
  uint32_t GetAchievements(std::vector<Achievement>* achievements) const;

  bool GetSetting(X_XDBF_SETTING_ID id, Setting* dest);
  uint32_t GetSettings(std::vector<Setting>* settings) const;

  bool GetTitle(uint32_t title_id, TitlePlayed* title);
  uint32_t GetTitles(std::vector<TitlePlayed>* titles) const;

  // Updates/adds an achievement
  bool UpdateAchievement(const Achievement& ach);

  // Updates/adds a setting
  bool UpdateSetting(const Setting& setting);

  // Updates/adds a title
  bool UpdateTitle(const TitlePlayed& title);

  uint32_t GetTitleId() { return title_id_; }

 private:
  uint32_t title_id_ = -1;
};

}  // namespace xdbf
}  // namespace xam
}  // namespace kernel
}  // namespace xe

#endif  // XENIA_KERNEL_XAM_XDBF_XDBF_H_
