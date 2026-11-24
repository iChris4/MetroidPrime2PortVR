#ifndef _CGUIWIDGETIDDB
#define _CGUIWIDGETIDDB

#include "rstl/string.hpp"
#include "rstl/vector.hpp"

class CGuiWidgetIdDB {
public:
  CGuiWidgetIdDB();
  void Reserve(int size);
  short AddWidget(const rstl::string& name);
  short FindWidgetID(const rstl::string& name) const;

  static const short kInvalidWidgetId;
#ifdef TARGET_PC
  // Port: the names by id, for listing a frame's widgets (PortGameUi::DumpFrame, `gameui dump`).
  int PortCount() const { return static_cast< int >(mNames.size()); }
  const rstl::string& PortName(int id) const { return mNames[id]; }
#endif

private:
  rstl::vector< rstl::string > mNames;
  short mUnresolved10;
  rstl::string mInvalidWidgetName;
};
CHECK_SIZEOF(CGuiWidgetIdDB, 0x24)

#endif // _CGUIWIDGETIDDB
