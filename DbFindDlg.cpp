#include "stdafx.h"
#include "DbFindDlg.h"
#include "StdDbHelper.h"
#include "aced.h"
#include "rxmfcapi.h"

namespace
{
    constexpr int kMaxShow = 200;
    constexpr int kMaxScan = 5000;
    constexpr wchar_t kCatPipeItems[] = L"pipeItems";
}

CDbFindDlg::CDbFindDlg(CWnd* pParent)
    : CDialog(IDD_DB_FIND, pParent)
{
}

INT_PTR CDbFindDlg::ShowModal(CWnd* pParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());
    CDbFindDlg dlg(pParent);
    return dlg.DoModal();
}

void CDbFindDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDbFindDlg, CDialog)
    ON_BN_CLICKED(IDC_FIND_BTN_SEARCH, &CDbFindDlg::OnBnFind)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_FIND_LIST, &CDbFindDlg::OnLvnItemChanged)
END_MESSAGE_MAP()

void CDbFindDlg::OnOK()
{
}

void CDbFindDlg::SetStatus(const CString& text)
{
    SetDlgItemText(IDC_FIND_STATIC_STATUS, text);
}

CString CDbFindDlg::EscapeSql(const CString& s)
{
    CString out = s;
    out.Replace(L"'", L"''");
    return out;
}

CString CDbFindDlg::ReadParam(MStudioData::IParamsOwner* p, LPCTSTR a, LPCTSTR b, LPCTSTR c)
{
    if (!p) return L"";
    if (a && a[0])
    {
        CString v = p->GetParameterValue(a, L"");
        if (!v.IsEmpty()) return v;
    }
    if (b && b[0])
    {
        CString v = p->GetParameterValue(b, L"");
        if (!v.IsEmpty()) return v;
    }
    if (c && c[0])
        return p->GetParameterValue(c, L"");
    return L"";
}

CString CDbFindDlg::CaptionOf(MStudioData::IParam* p)
{
    if (!p) return L"?";
    LPCTSTR cap = p->GetComment();
    if (cap && cap[0]) return cap;
    LPCTSTR nm = p->GetName();
    return nm ? nm : L"?";
}

void CDbFindDlg::ApplyUiTexts()
{
    SetWindowText(L"Test - \x041F\x043E\x0438\x0441\x043A\x0020\x043F\x043E\x0020\x043F\x0430\x0440\x0430\x043C\x0435\x0442\x0440\x0430\x043C\x0020\x0411\x0414");
    SetDlgItemText(IDC_FIND_LBL_NAME,
        L"\x0418\x043C\x044F\x0020\x043E\x0431\x044A\x0435\x043A\x0442\x0430\x003A");
    SetDlgItemText(IDC_FIND_LBL_PARAM,
        L"\x041F\x0430\x0440\x0430\x043C\x0435\x0442\x0440\x003A");
    SetDlgItemText(IDC_FIND_LBL_PVAL, L"\x003D");
    SetDlgItemText(IDC_FIND_BTN_SEARCH, L"\x041D\x0430\x0439\x0442\x0438");
    SetDlgItemText(IDCANCEL, L"\x0417\x0430\x043A\x0440\x044B\x0442\x044C");
    // Default: find elbows via PART_PIPE_CLASS
    SetDlgItemText(IDC_FIND_EDIT_PVAL, L"\x041E\x0442\x0432\x043E\x0434");
}

bool CDbFindDlg::ConnectDb(CString& err)
{
    return EnsureStandardDbConnected(m_pDb, err, true);
}

bool CDbFindDlg::LoadParamDefs(CString& err)
{
    m_paramDefs.clear();
    m_pParamDefs.Release();
    if (!m_pDb)
    {
        err = L"no db";
        return false;
    }
    if (FAILED(m_pDb->GetParamDefs(&m_pParamDefs)) || !m_pParamDefs)
    {
        err = L"GetParamDefs failed";
        return false;
    }

    const idx_t n = m_pParamDefs->GetCount();
    m_paramDefs.reserve(static_cast<size_t>(n) + 1);

    // First entry: any parameter (search value in all Parameters_STR)
    {
        DbParamDefInfo any;
        any.sysName = L"";
        any.caption = L"(\x043B\x044E\x0431\x043E\x0439\x0020\x043F\x0430\x0440\x0430\x043C\x0435\x0442\x0440)";
        m_paramDefs.push_back(any);
    }

    for (idx_t i = 0; i < n; ++i)
    {
        CComPtr<MStudioData::IParamDef> pDef;
        if (FAILED(m_pParamDefs->GetParam(i, &pDef)) || !pDef)
            continue;
        LPCTSTR nm = pDef->GetName();
        if (!nm || !nm[0]) continue;
        DbParamDefInfo info;
        info.sysName = nm;
        LPCTSTR cap = pDef->GetComment();
        info.caption = (cap && cap[0]) ? cap : nm;
        m_paramDefs.push_back(info);
    }
    return true;
}

void CDbFindDlg::FillParamCombo()
{
    m_cmbParam.ResetContent();
    int selPipeClass = 0;
    for (size_t i = 0; i < m_paramDefs.size(); ++i)
    {
        const DbParamDefInfo& d = m_paramDefs[i];
        CString label;
        if (d.sysName.IsEmpty())
            label = d.caption;
        else
            label.Format(L"%s  [%s]", d.caption.GetString(), d.sysName.GetString());
        const int idx = m_cmbParam.AddString(label);
        m_cmbParam.SetItemData(idx, static_cast<DWORD_PTR>(i));
        if (d.sysName.CompareNoCase(L"PART_PIPE_CLASS") == 0)
            selPipeClass = idx;
    }
    m_cmbParam.SetCurSel(selPipeClass);
}

CString CDbFindDlg::SelectedParamSysName() const
{
    const int sel = m_cmbParam.GetCurSel();
    if (sel < 0) return L"";
    const size_t i = static_cast<size_t>(m_cmbParam.GetItemData(sel));
    if (i >= m_paramDefs.size()) return L"";
    return m_paramDefs[i].sysName;
}

CString CDbFindDlg::BuildFilter(int nCat, const CString& nameFilter,
    const CString& paramSys, const CString& paramVal) const
{
    // CadLib style (from logs):
    // , Parameters_STR PT0 where (O.idObjectCategory = N) AND (PT0.idObject = O.idObject)
    //   AND (PT0.idParamDef = ...) AND (PT0.Value='...') AND (O.idParentObject is null)
    CString f;
    const bool hasParamVal = !paramVal.IsEmpty();
    const CString escVal = EscapeSql(paramVal);
    const CString escName = EscapeSql(nameFilter);
    const CString escParam = EscapeSql(paramSys);

    if (hasParamVal)
    {
        if (!paramSys.IsEmpty())
        {
            // Specific parameter by ParamDefs.Name
            f.Format(
                L", Parameters_STR PT0, ParamDefs PD"
                L" where (O.idObjectCategory = %d)"
                L" AND (O.idParentObject is null)"
                L" AND (PT0.idObject = O.idObject)"
                L" AND (PT0.idParamDef = PD.idParamDef)"
                L" AND (PD.Name = '%s')"
                L" AND (PT0.Value like '%%%s%%')",
                nCat, escParam.GetString(), escVal.GetString());
        }
        else
        {
            // Any parameter value
            f.Format(
                L", Parameters_STR PT0"
                L" where (O.idObjectCategory = %d)"
                L" AND (O.idParentObject is null)"
                L" AND (PT0.idObject = O.idObject)"
                L" AND (PT0.Value like '%%%s%%')",
                nCat, escVal.GetString());
        }
    }
    else
    {
        f.Format(
            L" where (O.idObjectCategory = %d) AND (O.idParentObject is null)",
            nCat);
    }

    if (!nameFilter.IsEmpty())
    {
        CString part;
        part.Format(L" AND (O.Name like '%%%s%%')", escName.GetString());
        f += part;
    }
    return f;
}

BOOL CDbFindDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    ApplyUiTexts();

    m_cmbParam.SubclassDlgItem(IDC_FIND_CMB_PARAM, this);
    m_list.SubclassDlgItem(IDC_FIND_LIST, this);
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);
    m_list.InsertColumn(0, L"Id", LVCFMT_RIGHT, 60);
    m_list.InsertColumn(1, L"\x0418\x043C\x044F", LVCFMT_LEFT, 200);
    m_list.InsertColumn(2, L"\x0422\x0438\x043F", LVCFMT_LEFT, 110);
    m_list.InsertColumn(3, L"\x041A\x043B\x0430\x0441\x0441", LVCFMT_LEFT, 90);
    m_list.InsertColumn(4, L"\x0414\x0438\x0430\x043C\x0435\x0442\x0440", LVCFMT_RIGHT, 70);
    m_list.InsertColumn(5, L"\x0422\x043E\x043B\x0449\x0438\x043D\x0430", LVCFMT_RIGHT, 70);

    CString err;
    if (!ConnectDb(err))
    {
        SetStatus(err);
        return TRUE;
    }
    if (!LoadParamDefs(err))
    {
        SetStatus(err);
        return TRUE;
    }
    FillParamCombo();

    CString st;
    st.Format(L"\x041F\x0430\x0440\x0430\x043C\x0435\x0442\x0440\x043E\x0432\x0020\x0432\x0020\x0411\x0414\x003A\x0020\x0025\x0075\x002E\x0020\x041D\x0430\x0436\x043C\x0438\x0442\x0435\x0020\x00AB\x041D\x0430\x0439\x0442\x0438\x00BB",
        static_cast<unsigned>(m_paramDefs.size() > 0 ? m_paramDefs.size() - 1 : 0));
    SetStatus(st);
    return TRUE;
}

void CDbFindDlg::FillList()
{
    m_list.DeleteAllItems();
    CString idStr;
    for (size_t i = 0; i < m_rows.size(); ++i)
    {
        const DbFindRow& r = m_rows[i];
        idStr.Format(L"%d", r.objectId);
        const int row = m_list.InsertItem(static_cast<int>(i), idStr);
        m_list.SetItemText(row, 1, r.name);
        m_list.SetItemText(row, 2, r.partType);
        m_list.SetItemText(row, 3, r.pipeClass);
        m_list.SetItemText(row, 4, r.diameter);
        m_list.SetItemText(row, 5, r.thickness);
        m_list.SetItemData(row, static_cast<DWORD_PTR>(i));
    }
}

void CDbFindDlg::ShowParamsForSelection()
{
    POSITION pos = m_list.GetFirstSelectedItemPosition();
    if (!pos || !m_pDb)
    {
        SetDlgItemText(IDC_FIND_EDIT_PARAMS, L"");
        return;
    }

    const int item = m_list.GetNextSelectedItem(pos);
    if (item < 0) return;
    const size_t idx = static_cast<size_t>(m_list.GetItemData(item));
    if (idx >= m_rows.size()) return;

    const int id = m_rows[idx].objectId;
    CComPtr<MStudioData::IParamsOwner> pParams;
    if (FAILED(m_pDb->GetObjectParameters(id, &pParams)) || !pParams)
    {
        SetDlgItemText(IDC_FIND_EDIT_PARAMS, L"(no params)");
        return;
    }

    CString all;
    for (idx_t n = 0; ; ++n)
    {
        CComPtr<MStudioData::IParam> pParam;
        if (!pParams->GetParameter(n, &pParam) || !pParam)
            break;
        CComBSTR bVal;
        pParam->GetValue(&bVal);
        CString line;
        line.Format(L"%s [%s] = %s\r\n",
            CaptionOf(pParam).GetString(),
            pParam->GetName() ? pParam->GetName() : L"?",
            bVal.m_str ? bVal.m_str : L"");
        all += line;
        if (n > 800) break;
    }
    if (all.IsEmpty())
        all = L"(empty)";
    SetDlgItemText(IDC_FIND_EDIT_PARAMS, all);
}

void CDbFindDlg::RunSearch()
{
    m_rows.clear();
    FillList();
    SetDlgItemText(IDC_FIND_EDIT_PARAMS, L"");

    if (!m_pDb || !m_pDb->IsConnected())
    {
        CString err;
        if (!ConnectDb(err))
        {
            SetStatus(err);
            return;
        }
    }

    CString nameFilter, paramVal;
    GetDlgItemText(IDC_FIND_EDIT_NAME, nameFilter);
    GetDlgItemText(IDC_FIND_EDIT_PVAL, paramVal);
    nameFilter.Trim();
    paramVal.Trim();
    const CString paramSys = SelectedParamSysName();

    int nCat = m_pDb->GetObjectCategoryId(kCatPipeItems, true);
    if (nCat <= 0)
        nCat = 69;

    CString filter = BuildFilter(nCat, nameFilter, paramSys, paramVal);
    acutPrintf(L"\n[Test] FIND cat=%d param=%s val=%s\n filter=%s\n",
        nCat, paramSys.GetString(), paramVal.GetString(), filter.GetString());

    SetStatus(L"\x041F\x043E\x0438\x0441\x043A\x002E\x002E\x002E");
    CWaitCursor wait;

    CComPtr<MStudioDB::ILibObjectInfoArr> pObjects;
    HRESULT hrObj = m_pDb->GetLibraryObjects(filter, &pObjects);
    bool bMemFilter = false;

    if (FAILED(hrObj) || !pObjects || pObjects->GetCount() == 0)
    {
        // Fallback: category only, filter params in memory
        CString filterCat;
        filterCat.Format(L" where (O.idObjectCategory = %d) AND (O.idParentObject is null)", nCat);
        if (!nameFilter.IsEmpty())
        {
            CString part;
            part.Format(L" AND (O.Name like '%%%s%%')", EscapeSql(nameFilter).GetString());
            filterCat += part;
        }
        pObjects.Release();
        hrObj = m_pDb->GetLibraryObjects(filterCat, &pObjects);
        acutPrintf(L"\n[Test] FIND fallback cat-only hr=0x%08X count=%d\n",
            static_cast<unsigned>(hrObj), pObjects ? pObjects->GetCount() : -1);
        if (FAILED(hrObj) || !pObjects)
        {
            SetStatus(L"GetLibraryObjects failed");
            return;
        }
        bMemFilter = !paramVal.IsEmpty();
    }

    const int total = pObjects->GetCount();
    const int scan = (total > kMaxScan) ? kMaxScan : total;
    int shown = 0;
    int matched = 0;

    CString paramValLower = paramVal;
    paramValLower.MakeLower();
    CString nameLowerWant = nameFilter;
    nameLowerWant.MakeLower();

    for (int i = 0; i < scan; ++i)
    {
        const int id = pObjects->GetObjectIdByIndex(i);
        const LPCTSTR pszName = pObjects->GetObjectNameByIndex(i);
        CString name = pszName ? pszName : L"";

        if (!nameLowerWant.IsEmpty() && bMemFilter)
        {
            CString nl = name;
            nl.MakeLower();
            if (nl.Find(nameLowerWant) < 0)
                continue;
        }

        CComPtr<MStudioData::IParamsOwner> pParams;
        CString partType, pipeClass, diameter, thickness;
        if (SUCCEEDED(m_pDb->GetObjectParameters(id, &pParams)) && pParams)
        {
            partType = ReadParam(pParams, L"PART_TYPE");
            pipeClass = ReadParam(pParams, L"PART_PIPE_CLASS");
            diameter = ReadParam(pParams, L"DIM_DIAMETER", L"PART_PIPE_DN", L"ELBOW_DN");
            thickness = ReadParam(pParams, L"DIM_THICKNESS", L"ELBOW_THICKNESS");

            if (bMemFilter && !paramValLower.IsEmpty())
            {
                bool ok = false;
                if (!paramSys.IsEmpty())
                {
                    CString v = pParams->GetParameterValue(paramSys, L"");
                    v.MakeLower();
                    ok = (v.Find(paramValLower) >= 0);
                }
                else
                {
                    for (idx_t n = 0; ; ++n)
                    {
                        CComPtr<MStudioData::IParam> pParam;
                        if (!pParams->GetParameter(n, &pParam) || !pParam)
                            break;
                        CComBSTR bVal;
                        pParam->GetValue(&bVal);
                        CString v = bVal.m_str ? bVal.m_str : L"";
                        v.MakeLower();
                        if (v.Find(paramValLower) >= 0)
                        {
                            ok = true;
                            break;
                        }
                        if (n > 800) break;
                    }
                }
                if (!ok)
                    continue;
            }
        }
        else if (bMemFilter)
        {
            continue;
        }

        ++matched;
        if (shown >= kMaxShow)
            continue;

        DbFindRow row;
        row.objectId = id;
        row.name = name;
        row.partType = partType;
        row.pipeClass = pipeClass;
        row.diameter = diameter;
        row.thickness = thickness;
        m_rows.push_back(row);
        ++shown;
    }

    FillList();

    CString st;
    st.Format(
        L"\x041D\x0430\x0439\x0434\x0435\x043D\x043E\x003A\x0020\x0025\x0064\x0020\x0028\x043F\x043E\x043A\x0430\x0437\x0430\x043D\x043E\x0020\x0025\x0064\x0029\x002C\x0020\x0432\x0020\x0432\x044B\x0431\x043E\x0440\x043A\x0435\x0020\x004F\x0062\x006A\x0065\x0063\x0074\x0073\x003D\x0025\x0064",
        matched, shown, total);
    SetStatus(st);
    acutPrintf(L"\n[Test] MS_DB_FIND matched=%d shown=%d objects=%d\n", matched, shown, total);
}

void CDbFindDlg::OnBnFind()
{
    RunSearch();
}

void CDbFindDlg::OnLvnItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
    if (pNMLV && (pNMLV->uChanged & LVIF_STATE) &&
        (pNMLV->uNewState & LVIS_SELECTED))
    {
        ShowParamsForSelection();
    }
    *pResult = 0;
}
