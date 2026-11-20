#pragma once

#include <atlbase.h>
#include "mstudioDB/CSDBInterfaces.h"

// Attach + connect to Model Studio "Standard Components" DB
// using the current MS session / remembered credentials (bStd).
bool EnsureStandardDbConnected(
    CComPtr<MStudioDB::IDataBase>& pDb,
    CString& err,
    bool allowUi = true);
