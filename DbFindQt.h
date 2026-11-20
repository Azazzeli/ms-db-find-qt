#pragma once

#include <windows.h>

// Qt5 modal search dialog (categories + multi-param criteria + user settings file).
namespace DbFindQt
{
    void ShowModal(HWND parent);
    // Close dialog + flush Qt deferred deletes before Test.nrx unload.
    void Shutdown();
}
