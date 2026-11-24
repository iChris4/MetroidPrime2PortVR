#include "MetroidPrime/CScriptMailbox.hpp"

#include "MetroidPrime/CEntity.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CWorldSaveGameInfo.hpp"
#include "MetroidPrime/ScriptObjects/CScriptMemoryRelay.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/Streams/CBitStreamReader.hpp"
#include "Kyoto/Streams/CBitStreamWriter.hpp"

#ifdef TARGET_PC
#include "MetroidPrime/CWorld.hpp"

// PORT (#123): two Sanctuary Fortress echo locks count their shot sonic emitters in a counter that a
// room reload resets, while each emitter's memory relay keeps it shot, so a lock left part-solved
// can never open (retail bug, NTSC and PAL). The dark rooms' relays re-increment the counter on
// load; these two rooms get the same here, unless the lock's own relay says it already opened.
namespace {
struct SPortEchoLock {
  uint mCounter;
  uint mSolved;
  uint mEmitters[3];
};
const uint kPortEchoLockWorld = 0x1baa96c2;
const SPortEchoLock kPortEchoLocks[] = {
    {0x100b0315, 0x100b0316, {0x100b02de, 0x100b02fa, 0x100b030d}}, // Main Research
    {0x204100be, 0x204100b9, {0x2041007f, 0x2041008d, 0x204100b6}}, // Aerie
};

void PortRecountEchoLocks(const CScriptMailbox& mailbox, const TAreaId& areaId,
                          CStateManager& mgr) {
  if (mgr.GetWorld() == nullptr ||
      static_cast< uint >(mgr.GetWorld()->GetWorldAssetId()) != kPortEchoLockWorld) {
    return;
  }
  for (int i = 0; i < static_cast< int >(sizeof(kPortEchoLocks) / sizeof(kPortEchoLocks[0])); ++i) {
    const SPortEchoLock& lock = kPortEchoLocks[i];
    if (TEditorId(lock.mCounter).AreaNum() != areaId.Value() ||
        mailbox.HasMsg(TEditorId(lock.mSolved))) {
      continue;
    }
    int shot = 0;
    for (int j = 0; j < 3; ++j) {
      if (mailbox.HasMsg(TEditorId(lock.mEmitters[j]))) {
        ++shot;
      }
    }
    CEntity* counter = mgr.ObjectById(mgr.GetIdForScript(TEditorId(lock.mCounter)));
    for (int j = 0; counter != nullptr && j < shot; ++j) {
      mgr.SendScriptMsg(counter, kInvalidUniqueId, kSM_Increment);
    }
  }
}
} // namespace
#endif

CScriptMailbox::CScriptMailbox() { CMemory::OffsetFakeStatics(static_cast< int >(sizeof(*this))); }

CScriptMailbox::CScriptMailbox(CBitStreamReader& in, const CWorldSaveGameInfo& saveWorld) {
  rstl::vector< bool > relayStates(saveWorld.GetRelays().size(), false);
  for (int i = 0; i < relayStates.size(); ++i) {
    relayStates[i] = in.ReadBits(1);
  }

  for (int i = 0; i < relayStates.size(); ++i) {
    if (relayStates[i]) {
      mRelays.push_back(saveWorld.GetRelays()[i]);
    }
  }

  CMemory::OffsetFakeStatics(static_cast< int >(sizeof(*this)));
}

CScriptMailbox::~CScriptMailbox() {
  CMemory::OffsetFakeStatics(-static_cast< int >(sizeof(*this)));
}

void CScriptMailbox::PutTo(CBitStreamWriter& out, const CWorldSaveGameInfo& saveWorld) const {
  rstl::vector< bool > relayStates(saveWorld.GetRelays().size(), false);

  rstl::reserved_vector< TEditorId, 512 >::const_iterator it = mRelays.begin();
  for (; it != mRelays.end(); ++it) {
    TEditorId id = *it;
    relayStates[saveWorld.GetRelayIndex(id)] = true;
  }

  for (int i = 0; i < relayStates.size(); ++i) {
    out.WriteBits(relayStates[i] ? 1 : 0, 1);
  }
}

void CScriptMailbox::SendMsgs(const TAreaId& areaId, CStateManager& mgr) {
  rstl::reserved_vector< TEditorId, 512 >::iterator it = mRelays.begin();
  for (; it != mRelays.end(); ++it) {
    if (it->AreaNum() == areaId.Value()) {
      if (CEntity* entity = mgr.ObjectById(mgr.GetIdForScript(*it))) {
        entity->SendScriptMsgs(kSS_Active, mgr);
      }
    }
  }
#ifdef TARGET_PC
  PortRecountEchoLocks(*this, areaId, mgr);
#endif

  bool removed = true;
  while (removed) {
    removed = false;
    for (rstl::reserved_vector< TEditorId, 512 >::iterator it = mRelays.begin();
         it != mRelays.end(); ++it) {
      if (it->AreaNum() != areaId.Value()) {
        continue;
      }
      CEntity* entity = mgr.ObjectById(mgr.GetIdForScript(*it));
      if (entity != nullptr && static_cast< CScriptMemoryRelay* >(entity)->GetDefaultActive()) {
        mRelays.erase(it);
        removed = true;
        break;
      }
    }
  }
}

void CScriptMailbox::AddMsg(const TEditorId& id) {
  rstl::reserved_vector< TEditorId, 512 >::iterator it = mRelays.begin();
  for (; it != mRelays.end(); ++it) {
    if (*it == id) {
      return;
    }
  }

  mRelays.push_back(id);
}

void CScriptMailbox::RemoveMsg(const TEditorId& id) {
  rstl::reserved_vector< TEditorId, 512 >::iterator it = mRelays.begin();
  for (; it != mRelays.end(); ++it) {
    if (*it == id) {
      mRelays.erase(it);
      return;
    }
  }
}
