// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "windowManager/dedicated/dedicatedWindowStub.h"


PlatformWindowManager *CreatePlatformWindowManager()
{
   return new DedicatedWindowMgr;
}
