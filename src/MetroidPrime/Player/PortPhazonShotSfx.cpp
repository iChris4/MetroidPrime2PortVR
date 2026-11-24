#include "MetroidPrime/Player/PortPhazonShotSfx.hpp"

#include "Kyoto/Audio/CAudioGroupSet.hpp"
#include "Kyoto/CPakFile.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/SObjectTag.hpp"
#include "Kyoto/TToken.hpp"
#include "dolphin/os.h"

#include <stdio.h>
#include <string.h>

namespace {
// SamusPhazonGun (group 0x29E), the same id on every disc that has it.
const CAssetId kGroupId = 0x6D318F8C;
const char kGroupName[] = "SamusPhazonGun";
const char kPakName[] = "Metroid1.pak";
// The group is 94 KB on the retail disc; anything far past that is not it.
const uint kMaxGroupSize = 1 << 20;
const int kMaxPumps = 1 << 20;

TToken< CAudioGrpSetLoc >* sPooled; // the mounted paks have the group
CAudioGrpSetLoc* sDirect;           // read from Metroid1.pak
bool sFailed;                       // not on this disc: don't reread the pak every frame

CAudioGrpSetLoc* LoadFromPak() {
  if (!CDvdFile::FileExists(kPakName)) {
    return nullptr;
  }
  CPakFile pak(rstl::string_l(kPakName), false, false);
  for (int pumps = 0; !pak.IsCompletelyLoaded() && pumps < kMaxPumps; ++pumps) {
    pak.AsyncIdle();
    if (!pak.IsCompletelyLoaded()) {
      OSYieldThread();
    }
  }
  const CPakFile::CResInfo* info = pak.GetResInfo(kGroupId);
  if (info == nullptr || info->GetType() != 'AGSC' || info->IsCompressed()) {
    return nullptr;
  }
  const uint size = info->GetSize();
  // Version, name and its terminator, group id, four sizes.
  if (size < 4 + sizeof(kGroupName) + 2 + 16 || size > kMaxGroupSize) {
    return nullptr;
  }
  rstl::auto_ptr< uchar > data(rs_new uchar[size]);
  pak.DvdFile().SyncSeekRead(data.get(), size, kSO_Set, info->GetOffset());
  if (memcmp(data.get() + 4, kGroupName, sizeof(kGroupName)) != 0) {
    return nullptr;
  }
  return rs_new CAudioGrpSetLoc(data, size);
}
} // namespace

namespace PortPhazonShotSfx {
void Ensure() {
  if (sPooled != nullptr || sDirect != nullptr || sFailed) {
    return;
  }
  const SObjectTag tag('AGSC', kGroupId);
  if (gpSimplePool->HasObject(tag)) {
    sPooled = rs_new TToken< CAudioGrpSetLoc >(gpSimplePool->GetObj(tag));
    sPooled->Lock();
    return;
  }
  sDirect = LoadFromPak();
  if (sDirect != nullptr) {
    fprintf(stderr, "[audio] Phazon shot cheat: %s group read from %s\n", kGroupName, kPakName);
  } else {
    sFailed = true;
    fprintf(stderr, "[audio] Phazon shot cheat: no %s group in %s, the shot stays silent\n",
            kGroupName, kPakName);
  }
}

void Release() {
  delete sPooled;
  sPooled = nullptr;
  delete sDirect;
  sDirect = nullptr;
  sFailed = false;
}
} // namespace PortPhazonShotSfx
