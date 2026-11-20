#pragma once

#include "resource.h"
#include <atlbase.h>
#include <vector>
#include "mstudioDB/CSDBInterfaces.h"

struct DbParamDefInfo
{
    CString sysName;   // PART_PIPE_CLASS
    CString caption;   // Russian from ParamDefs.Caption
};

struct DbFindRow
{
    int objectId = 0;
    CString name;
    CString partType;
    CString pipeClass;
    CString diameter;
    CString thickness;
};

class CDbFindDlg : public CDialog
{
public:
    explicit CDbFindDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_DB_FIND };

    static INT_PTR ShowModal(CWnd* pParent);

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    virtual void OnOK() override;

    afx_msg void OnBnFind();
    afx_msg void OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()

private:
    void SetStatus(const CString& text);
    void ApplyUiTexts();
    bool ConnectDb(CString& err);
    bool LoadParamDefs(CString& err);
    void FillParamCombo();
    CString SelectedParamSysName() const;
    CString BuildFilter(int nCat, const CString& nameFilter,
        const CString& paramSys, const CString& paramVal) const;
    void RunSearch();
    void FillList();
    void ShowParamsForSelection();
    static CString EscapeSql(const CString& s);
    static CString ReadParam(MStudioData::IParamsOwner* p, LPCTSTR a, LPCTSTR b = nullptr, LPCTSTR c = nullptr);
    static CString CaptionOf(MStudioData::IParam* p);

    CComPtr<MStudioDB::IDataBase> m_pDb;
    CComPtr<MStudioData::IParamDefs> m_pParamDefs;
    CListCtrl m_list;
    CComboBox m_cmbParam;
    std::vector<DbParamDefInfo> m_paramDefs;
    std::vector<DbFindRow> m_rows;
};
