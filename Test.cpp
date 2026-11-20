#include "stdafx.h"
#include "aced.h"
#include "rxmfcapi.h"
#include "DbConnectDlg.h"
#include "DbFindQt.h"

class CTestApp : public CWinApp
{
public:
    virtual BOOL InitInstance() override { return TRUE; }
};
CTestApp theApp;

void msDbConnectCmd()
{
    CWnd* pParent = CWnd::FromHandle(adsw_acadMainWnd());
    CDbConnectDlg::ShowModal(pParent);
    acutPrintf(L"\n[Test] MS_DB done.");
}

void msDbFindCmd()
{
    DbFindQt::ShowModal(adsw_acadMainWnd());
    acutPrintf(L"\n[Test] MS_DB_FIND done.");
}

#if defined(acrxEntryPoint)
#   undef acrxEntryPoint
#endif
#if defined(acrxGetApiVersion)
#   undef acrxGetApiVersion
#endif

#if defined(_WIN64) || defined(_AC64)
#   pragma comment(linker, "/EXPORT:ncrxEntryPoint")
#   pragma comment(linker, "/EXPORT:acrxEntryPoint")
#   pragma comment(linker, "/EXPORT:ncrxGetApiVersion")
#   pragma comment(linker, "/EXPORT:acrxGetApiVersion")
#else
#   pragma comment(linker, "/EXPORT:ncrxEntryPoint=_ncrxEntryPoint@8")
#   pragma comment(linker, "/EXPORT:acrxEntryPoint=_acrxEntryPoint@8")
#   pragma comment(linker, "/EXPORT:ncrxGetApiVersion=_ncrxGetApiVersion@0")
#   pragma comment(linker, "/EXPORT:acrxGetApiVersion=_acrxGetApiVersion@0")
#endif

extern "C" __declspec(dllexport) const ACHAR* __stdcall acrxGetApiVersion()
{
#if defined(ACRX_API_VERSION)
    return _T(ACRX_API_VERSION);
#elif defined(ACRXAPI_REVISION)
    return _T(ACRXAPI_REVISION);
#else
    return _T("1");
#endif
}

extern "C" __declspec(dllexport) const ACHAR* __stdcall ncrxGetApiVersion()
{
    return acrxGetApiVersion();
}

extern "C" __declspec(dllexport) AcRx::AppRetCode __stdcall ncrxEntryPoint(AcRx::AppMsgCode msg, void* appId)
{
    switch (msg)
    {
    case AcRx::kInitAppMsg:
        acrxDynamicLinker->unlockApplication(appId);
        acrxDynamicLinker->registerAppMDIAware(appId);
        acedRegCmds->addCommand(L"TEST_GROUP", L"_MS_DB", L"MS_DB",
            ACRX_CMD_MODAL, msDbConnectCmd);
        acedRegCmds->addCommand(L"TEST_GROUP", L"_MS_DB_FIND", L"MS_DB_FIND",
            ACRX_CMD_MODAL, msDbFindCmd);
        acutPrintf(L"\nTest.nrx: MS_DB, MS_DB_FIND");
        break;
    case AcRx::kUnloadAppMsg:
        DbFindQt::Shutdown();
        acedRegCmds->removeGroup(L"TEST_GROUP");
        break;
    }
    return AcRx::kRetOK;
}

extern "C" __declspec(dllexport) AcRx::AppRetCode __stdcall acrxEntryPoint(AcRx::AppMsgCode msg, void* appId)
{
    return ncrxEntryPoint(msg, appId);
}
