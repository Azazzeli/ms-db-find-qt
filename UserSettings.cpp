#include "stdafx.h"
#include "UserSettings.h"
#include <shlobj.h>

CString UserSettings::FilePath() const
{
    wchar_t appData[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData)))
        return L"";

    CString dir;
    dir.Format(L"%s\\TestNRX", appData);
    CreateDirectoryW(dir, nullptr);

    CString path = dir;
    path += L"\\user_settings.ini";
    return path;
}

bool UserSettings::Load()
{
    lastUser.Empty();
    lastServer.Empty();
    lastDatabase.Empty();
    lastNameFilter.Empty();
    lastCriteria.clear();

    const CString path = FilePath();
    if (path.IsEmpty())
        return false;

    wchar_t buf[1024] = {};
    GetPrivateProfileStringW(L"User", L"LastUser", L"", buf, 1024, path);
    lastUser = buf;
    GetPrivateProfileStringW(L"User", L"LastServer", L"", buf, 1024, path);
    lastServer = buf;
    GetPrivateProfileStringW(L"User", L"LastDatabase", L"", buf, 1024, path);
    lastDatabase = buf;
    GetPrivateProfileStringW(L"User", L"LastNameFilter", L"", buf, 1024, path);
    lastNameFilter = buf;

    const int count = GetPrivateProfileIntW(L"Criteria", L"Count", 0, path);
    for (int i = 0; i < count && i < 64; ++i)
    {
        CString keySys, keyCap, keyVal;
        keySys.Format(L"Sys%d", i);
        keyCap.Format(L"Cap%d", i);
        keyVal.Format(L"Val%d", i);

        SearchCriterion c;
        GetPrivateProfileStringW(L"Criteria", keySys, L"", buf, 1024, path);
        c.paramSys = buf;
        GetPrivateProfileStringW(L"Criteria", keyCap, L"", buf, 1024, path);
        c.paramCaption = buf;
        GetPrivateProfileStringW(L"Criteria", keyVal, L"", buf, 1024, path);
        c.value = buf;
        if (!c.paramSys.IsEmpty() || !c.value.IsEmpty())
            lastCriteria.push_back(c);
    }
    return true;
}

bool UserSettings::Save() const
{
    const CString path = FilePath();
    if (path.IsEmpty())
        return false;

    WritePrivateProfileStringW(L"User", L"LastUser", lastUser, path);
    WritePrivateProfileStringW(L"User", L"LastServer", lastServer, path);
    WritePrivateProfileStringW(L"User", L"LastDatabase", lastDatabase, path);
    WritePrivateProfileStringW(L"User", L"LastNameFilter", lastNameFilter, path);

    CString countStr;
    countStr.Format(L"%u", static_cast<unsigned>(lastCriteria.size()));
    WritePrivateProfileStringW(L"Criteria", L"Count", countStr, path);

    for (size_t i = 0; i < lastCriteria.size() && i < 64; ++i)
    {
        CString keySys, keyCap, keyVal;
        keySys.Format(L"Sys%d", static_cast<int>(i));
        keyCap.Format(L"Cap%d", static_cast<int>(i));
        keyVal.Format(L"Val%d", static_cast<int>(i));
        WritePrivateProfileStringW(L"Criteria", keySys, lastCriteria[i].paramSys, path);
        WritePrivateProfileStringW(L"Criteria", keyCap, lastCriteria[i].paramCaption, path);
        WritePrivateProfileStringW(L"Criteria", keyVal, lastCriteria[i].value, path);
    }
    return true;
}
