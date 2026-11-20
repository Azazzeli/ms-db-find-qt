#pragma once

#include "resource.h"
#include <atlbase.h>
#include "mstudioDB/CSDBInterfaces.h"

class CDbConnectDlg : public CDialog
{
public:
    explicit CDbConnectDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_DBCONNECT };

    static INT_PTR ShowModal(CWnd* pParent);

    MStudioDB::IDataBase* GetDb() const { return m_pDb; }
    bool IsDbConnected() const { return m_connected; }

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override;

    afx_msg void OnBnReconnect();
    DECLARE_MESSAGE_MAP()

private:
    void SetStatus(const CString& text);
    void RefreshInfo();

    CComPtr<MStudioDB::IDataBase> m_pDb;
    bool m_connected = false;
};
