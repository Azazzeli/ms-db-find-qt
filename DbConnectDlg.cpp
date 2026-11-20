#include "stdafx.h"
#include "DbConnectDlg.h"
#include "StdDbHelper.h"
#include "aced.h"
#include "rxmfcapi.h"

CDbConnectDlg::CDbConnectDlg(CWnd* pParent)
    : CDialog(IDD_DBCONNECT, pParent)
{
}

INT_PTR CDbConnectDlg::ShowModal(CWnd* pParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CDbConnectDlg dlg(pParent);
    return dlg.DoModal();
}

void CDbConnectDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDbConnectDlg, CDialog)
    ON_BN_CLICKED(IDC_DB_BTN_RECONNECT, &CDbConnectDlg::OnBnReconnect)
END_MESSAGE_MAP()

void CDbConnectDlg::OnOK()
{
}

void CDbConnectDlg::SetStatus(const CString& text)
{
    SetDlgItemText(IDC_DB_STATIC_STATUS, text);
}

void CDbConnectDlg::RefreshInfo()
{
    if (!m_pDb || !m_pDb->IsConnected())
    {
        m_connected = false;
        SetDlgItemText(IDC_DB_STATIC_SERVER, L"—");
        SetDlgItemText(IDC_DB_STATIC_DATABASE, L"—");
        SetDlgItemText(IDC_DB_STATIC_PLATFORM, L"—");
        return;
    }

    m_connected = true;

    CComBSTR bServer, bDb, bUser;
    m_pDb->GetServer(&bServer);
    m_pDb->GetDatabaseName(&bDb);
    m_pDb->GetUserName(&bUser);

    SetDlgItemText(IDC_DB_STATIC_SERVER, bServer.m_str ? CString(bServer) : CString(L"—"));
    SetDlgItemText(IDC_DB_STATIC_DATABASE, bDb.m_str ? CString(bDb) : CString(L"—"));

    CString plat;
    switch (m_pDb->GetPlatform())
    {
    case MStudioDB::IDataBase::dbPostgreSQL: plat = L"PostgreSQL"; break;
    case MStudioDB::IDataBase::dbMSSQLNCLI: plat = L"MSSQL NCLI"; break;
    default: plat = L"MSSQL"; break;
    }
    CString line;
    line.Format(L"%s  port=%d  user=%s",
        plat.GetString(),
        m_pDb->GetPort(),
        (bUser.m_str && bUser.Length()) ? bUser.m_str : L"(Windows)");
    SetDlgItemText(IDC_DB_STATIC_PLATFORM, line);
}

BOOL CDbConnectDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetWindowText(L"Test - \x0411\x0414\x0020\x0441\x0442\x0430\x043D\x0434\x0430\x0440\x0442\x043D\x044B\x0445\x0020\x043A\x043E\x043C\x043F\x043E\x043D\x0435\x043D\x0442");
    SetDlgItemText(IDC_DB_STATIC_TITLE,
        L"\x0411\x0414\x0020\x0441\x0442\x0430\x043D\x0434\x0430\x0440\x0442\x043D\x044B\x0445\x0020\x043A\x043E\x043C\x043F\x043E\x043D\x0435\x043D\x0442\x0020\x004D\x006F\x0064\x0065\x006C\x0020\x0053\x0074\x0075\x0064\x0069\x006F");
    SetDlgItemText(IDC_DB_LBL_STATUS, L"\x0421\x0442\x0430\x0442\x0443\x0441\x003A");
    SetDlgItemText(IDC_DB_LBL_SERVER, L"\x0421\x0435\x0440\x0432\x0435\x0440\x003A");
    SetDlgItemText(IDC_DB_LBL_DATABASE, L"\x0411\x0414\x003A");
    SetDlgItemText(IDC_DB_LBL_PLATFORM, L"\x041F\x043B\x0430\x0442\x0444\x043E\x0440\x043C\x0430\x003A");
    SetDlgItemText(IDC_DB_BTN_RECONNECT,
        L"\x041F\x0435\x0440\x0435\x043F\x043E\x0434\x043A\x043B\x044E\x0447\x0438\x0442\x044C");
    SetDlgItemText(IDCANCEL, L"\x0417\x0430\x043A\x0440\x044B\x0442\x044C");
    SetStatus(L"\x041F\x043E\x0434\x043A\x043B\x044E\x0447\x0435\x043D\x0438\x0435\x002E\x002E\x002E");

    CString err;
    if (EnsureStandardDbConnected(m_pDb, err, true))
    {
        RefreshInfo();
        SetStatus(L"\x041F\x043E\x0434\x043A\x043B\x044E\x0447\x0435\x043D\x043E");
        acutPrintf(L"\n[Test] standard components DB OK\n");
    }
    else
    {
        SetStatus(err);
        RefreshInfo();
    }
    return TRUE;
}

void CDbConnectDlg::OnBnReconnect()
{
    SetStatus(L"\x041F\x043E\x0432\x0442\x043E\x0440\x002E\x002E\x002E");
    CString err;

    if (m_pDb && m_pDb->IsConnected())
        m_pDb->Disconnect();
    m_pDb.Release();

    if (EnsureStandardDbConnected(m_pDb, err, true))
    {
        RefreshInfo();
        SetStatus(L"\x041F\x043E\x0434\x043A\x043B\x044E\x0447\x0435\x043D\x043E");
    }
    else
    {
        SetStatus(err);
        RefreshInfo();
    }
}
