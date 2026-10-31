#ifndef _CSCRIPTSWITCH
#define _CSCRIPTSWITCH

#include "MetroidPrime/CEntity.hpp"

class CScriptSwitch : public CEntity {
#ifdef TARGET_PC
  friend struct PortEntitySchema; // save-state field schema (platform/include/port_entity_schema.h)
#endif
public:
  CScriptSwitch(TUniqueId uid, const rstl::string& name, const CEntityInfo& info, bool opened,
                bool closeOnOpened);

  // CEntity
  ~CScriptSwitch() override;
  CEntity* TypesMatch(int typeId) const override;
  void AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) override;

  bool IsOpened() const { return mOpened; } // Guessed name.
#ifdef TARGET_PC
  // Save states: the opened flag a load puts back (platform/port_savestate_room.cpp).
  void PortSetOpened(bool opened) { mOpened = opened; }
#endif

private:
  bool mOpened;
  bool mCloseOnOpened;
};
CHECK_SIZEOF(CScriptSwitch, 0x28)

#endif // _CSCRIPTSWITCH
