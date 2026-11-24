#include "MetroidPrime/ScriptObjects/CScriptSequenceTimer.hpp"

#include "MetroidPrime/CMain.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/ScriptLoader.hpp"

#include "rstl/math.hpp"

#ifdef TARGET_PC
#include "MetroidPrime/ScriptObjects/CScriptCamera.hpp"
#include "MetroidPrime/ScriptObjects/CScriptRelay.hpp"
#include "MetroidPrime/ScriptObjects/CScriptWorldTeleporter.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "port_overlay.h"
#endif

#include <math.h>

CScriptSequenceTimer::CScriptSequenceTimer(TUniqueId uid, const rstl::string& name,
                                           const CEntityInfo& info,
                                           const SLdrSequenceConnections& connections,
                                           float startTime, float maxTime, float loopStartTime,
                                           const bool autoStart, const bool loop,
                                           const bool takeExternalTime)
: CEntity(uid, info, name, 0)
, mStartTime(startTime)
, mCurrentTime(startTime)
, mMaxTime(maxTime != 0.f ? maxTime : FindMinMaxConnectionTimes(connections.mConnections).second)
, mLoopStartTime(loopStartTime)
, mRunning(autoStart)
, mLoop(loop)
, mTakeExternalTime(takeExternalTime)
, mConnections(connections)
, mStartMessage() {}

CScriptSequenceTimer::~CScriptSequenceTimer() {}

#ifdef TARGET_PC
namespace {
// Whether an object the entity sends to is a world teleporter, or (depth > 0) a relay that sends to
// one: the elevators' sequences message theirs directly, the light beam teleporters' through
// their DoTeleportToChosenWorld relay.
bool PortSendsToTeleporter(const CEntity& entity, CStateManager& mgr, int depth) {
  const rstl::vector< SConnection >& connections = entity.GetConnectionList();
  for (rstl::vector< SConnection >::const_iterator it = connections.begin();
       it != connections.end(); ++it) {
    const CStateManager::TIdListResult ids = mgr.GetIdListForScript(it->objId);
    for (CStateManager::TIdList::const_iterator id = ids.first; id != ids.second; ++id) {
      const CEntity* target = mgr.GetObjectById(id->second);
      if (TCastToConstPtr< CScriptWorldTeleporter >(target) != nullptr) {
        return true;
      }
      if (depth > 0 && TCastToConstPtr< CScriptRelay >(target) != nullptr &&
          PortSendsToTeleporter(*target, mgr, depth - 1)) {
        return true;
      }
    }
  }
  return false;
}

bool PortSendsToCamera(const CEntity& entity, CStateManager& mgr) {
  const rstl::vector< SConnection >& connections = entity.GetConnectionList();
  for (rstl::vector< SConnection >::const_iterator it = connections.begin();
       it != connections.end(); ++it) {
    const CStateManager::TIdListResult ids = mgr.GetIdListForScript(it->objId);
    for (CStateManager::TIdList::const_iterator id = ids.first; id != ids.second; ++id) {
      if (TCastToConstPtr< CScriptCamera >(mgr.GetObjectById(id->second)) != nullptr) {
        return true;
      }
    }
  }
  return false;
}
} // namespace
#endif

void CScriptSequenceTimer::Think(float dt, CStateManager& mgr) {
  if (GetActive() && mRunning && !mTakeExternalTime) {
#ifdef TARGET_PC
    // Port: Elevator and light beam cutscenes. A departure sequence ends at the area's world
    // teleporter; an arrival sequence is the first camera sequence started after a teleport
    // (AcceptScriptMsg), as the light beam teleporters' arrivals reach none.
    if (PortOverlay::TravelCutscenes() == PortOverlay::kTravelCutscenes_Fast &&
        (PortOverlay::IsTravelArrival(GetUniqueId().value) ||
         PortSendsToTeleporter(*this, mgr, 1))) {
      PortOverlay::NoteTravelSequence(mgr.GetUpdateFrameIdx());
    }
#endif
    ApplyTime(0.00001f + (mCurrentTime + dt), mgr);
  }
}

void CScriptSequenceTimer::ApplyTime(float time, CStateManager& mgr) {
  const float oldTime = mCurrentTime;
  mCurrentTime = time;

  bool wrapped = false;
  if (mLoop && mCurrentTime >= mMaxTime) {
    wrapped = true;
    mCurrentTime = mLoopStartTime + float(fmod(mCurrentTime, mMaxTime));
  }

  const float lowerTime = rstl::min_val(mCurrentTime, oldTime);
  const float upperTime = rstl::max_val(mCurrentTime, oldTime);
  for (rstl::vector< SLdrConnection >::iterator connection = mConnections.mConnections.begin();
       connection != mConnections.mConnections.end(); ++connection) {
    const uint connectionIndex = connection->connectionIndex;
    if (connection->mActivation.second && gpMain->IsMaxSpeed()) {
      continue;
    }

    for (rstl::vector< float >::iterator activation = connection->mActivation.first.begin();
         activation != connection->mActivation.first.end(); ++activation) {
      const float activationTime = *activation;
      bool crossed = false;
      if (!wrapped) {
        if (lowerTime <= activationTime && activationTime < upperTime) {
          crossed = true;
        }
      } else if (upperTime <= activationTime || activationTime < lowerTime) {
        crossed = true;
      }
      if (!crossed) {
        continue;
      }

      const SConnection& target = GetConnectionList()[connectionIndex];
      const CStateManager::TIdListResult ids = mgr.GetIdListForScript(target.objId);
      for (CStateManager::TIdList::const_iterator id = ids.first; id != ids.second; ++id) {
        mgr.SendScriptMsg(CScriptMsg(GetUniqueId(), id->second, target.msg,
                                     mStartMessage.GetOriginator(), target.state));
      }
    }
  }

  if (!mLoop && mCurrentTime >= mMaxTime) {
    SendScriptMsgs(kSS_MaxReached, mgr, mStartMessage.GetOriginator(), kSM_None);
    mRunning = false;
  }
}

void CScriptSequenceTimer::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  if (GetActive()) {
    switch (msg.GetMessage()) {
    case kSM_Start:
      mRunning = true;
      mStartMessage = msg;
      mCurrentTime = mStartTime;
#ifdef TARGET_PC
      if (PortOverlay::TravelArrivalPending() &&
          PortOverlay::TravelCutscenes() == PortOverlay::kTravelCutscenes_Fast &&
          PortSendsToCamera(*this, mgr)) {
        PortOverlay::SetTravelArrival(GetUniqueId().value, mgr.GetUpdateFrameIdx());
      }
#endif
      break;
    case kSM_Stop:
      mRunning = false;
      break;
    case kSM_Play:
      mRunning = true;
      break;
    default:
      break;
    }
  }
  CEntity::AcceptScriptMsg(mgr, msg);
}

void CScriptSequenceTimer::SetCurrentTime(float time, CStateManager&) {
  mCurrentTime = float(fmod(time, mMaxTime));
}

void CScriptSequenceTimer::ReceiveExternalTime(CStateManager& mgr, float time) {
  ApplyTime(time, mgr);
}

CEntity* LoadSequenceTimer(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  SLdrSequenceTimer sldrThis;
#include "MetroidPrime/ScriptLoader/SLdrSequenceTimer.inc"

  return rs_new CScriptSequenceTimer(mgr.AllocateUniqueId(), sldrThis.editorProperties.name,
                                     LdrToEntityInfo(info, sldrThis.editorProperties),
                                     sldrThis.sequenceConnections, sldrThis.startTime,
                                     sldrThis.maxTime, sldrThis.loopStartTime, sldrThis.isAutostart,
                                     sldrThis.isLoop, sldrThis.takeExternalTime);
}
