#include "stdafx.h"
#include "StdDbHelper.h"
#include "AcadLoader.h"

bool EnsureStandardDbConnected(
    CComPtr<MStudioDB::IDataBase>& pDb,
    CString& err,
    bool allowUi)
{
    CCSMstudioAcadLoadHelper::LoadModelStudioEnabler();

    MStudioDB::IFactoryInterface* pFactory = MStudioDB::CMStudioDataInterface::Get();
    if (!pFactory)
    {
        err.Format(L"\x041D\x0435\x0020\x0437\x0430\x0433\x0440\x0443\x0436\x0435\x043D\x0020\x0025\x0073",
            MODEL_STUDIO_DB_MODULE_NAME);
        return false;
    }

    pDb.Release();
    if (FAILED(pFactory->GetStandardComponentsDataBase(&pDb)) || !pDb)
    {
        err = L"\x041D\x0435\x0020\x043F\x043E\x043B\x0443\x0447\x0435\x043D\x0430\x0020\x0411\x0414\x0020\x0441\x0442\x0430\x043D\x0434\x0430\x0440\x0442\x043D\x044B\x0445\x0020\x043A\x043E\x043C\x043F\x043E\x043D\x0435\x043D\x0442";
        return false;
    }

    if (pDb->IsConnected())
        return true;

    if (!allowUi)
    {
        err = L"\x0411\x0414\x0020\x0441\x0442\x0430\x043D\x0434\x0430\x0440\x0442\x043D\x044B\x0445\x0020\x043A\x043E\x043C\x043F\x043E\x043D\x0435\x043D\x0442\x0020\x043D\x0435\x0020\x043F\x043E\x0434\x043A\x043B\x044E\x0447\x0435\x043D\x0430\x0020\x0432\x0020\x004D\x006F\x0064\x0065\x006C\x0020\x0053\x0074\x0075\x0064\x0069\x006F";
        return false;
    }

    CComBSTR bDriver;
    pDb->GetDriver(&bDriver);
    const MStudioDB::IDataBase::DbPlatform platform = pDb->GetPlatform();
    const int port = pDb->GetPort();

    bool canceled = false;
    const bool ok = pDb->UIConnect(
        platform,
        bDriver.Length() ? bDriver.m_str : L"",
        port,
        nullptr, nullptr, nullptr, nullptr,
        true,
        L"\x0411\x0414\x0020\x0441\x0442\x0430\x043D\x0434\x0430\x0440\x0442\x043D\x044B\x0445\x0020\x043A\x043E\x043C\x043F\x043E\x043D\x0435\x043D\x0442",
        &canceled);

    if (canceled)
    {
        err = L"\x041E\x0442\x043C\x0435\x043D\x0430";
        return false;
    }
    if (!ok || !pDb->IsConnected())
    {
        err = L"\x041D\x0435\x0020\x0443\x0434\x0430\x043B\x043E\x0441\x044C\x0020\x043F\x043E\x0434\x043A\x043B\x044E\x0447\x0438\x0442\x044C\x0441\x044F\x0020\x043A\x0020\x0411\x0414\x0020\x0441\x0442\x0430\x043D\x0434\x0430\x0440\x0442\x043D\x044B\x0445\x0020\x043A\x043E\x043C\x043F\x043E\x043D\x0435\x043D\x0442";
        return false;
    }
    return true;
}
