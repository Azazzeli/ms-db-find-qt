#include "stdafx.h"
#include "DbFindQt.h"
#include "StdDbHelper.h"
#include "UserSettings.h"
#include "aced.h"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QApplication>
#include <QSignalBlocker>
#include <QMap>
#include <QAbstractItemView>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QEvent>
#include <map>

// hostQt QWinWidget intentionally not used — hooks on main HWND
// caused nanoCAD crashes on APPUNLOAD.

namespace
{
    constexpr int kMaxShow = 200;
    constexpr int kMaxScan = 5000;
    constexpr wchar_t kCatPipeItems[] = L"pipeItems";

    class FindDialog;
    FindDialog* g_activeFindDlg = nullptr;

    void FlushQtDeferredDeletes()
    {
        // Only DeferredDelete — do NOT processEvents (unsafe on NRX unload).
        if (QCoreApplication::instance())
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    LPCTSTR ParamTableName(MStudioData::IParamDef::EType t)
    {
        using T = MStudioData::IParamDef::EType;
        switch (t)
        {
        case T::ptDBInt:
        case T::ptDBListInt:
            return L"Parameters_INT";
        case T::ptDBDouble:
        case T::ptDBListDbl:
            return L"Parameters_DBL";
        default:
            return L"Parameters_STR";
        }
    }

    bool IsNumericParamType(MStudioData::IParamDef::EType t)
    {
        using T = MStudioData::IParamDef::EType;
        return t == T::ptDBInt || t == T::ptDBListInt
            || t == T::ptDBDouble || t == T::ptDBListDbl;
    }

    bool ParseNumber(const CString& s, double& out)
    {
        CString t = s;
        t.Trim();
        if (t.IsEmpty())
            return false;
        wchar_t* end = nullptr;
        out = wcstod(t.GetString(), &end);
        return end && *end == L'\0';
    }

    // UI strings as UTF-16 (requires /utf-8 in vcxproj). Avoids fromUtf8 mojibake.
    QString tw(const wchar_t* s)
    {
        return QString::fromWCharArray(s);
    }

    QString qs(const CString& s)
    {
        return QString::fromWCharArray(s.GetString(), s.GetLength());
    }

    QString qs(LPCTSTR s)
    {
        return s ? QString::fromWCharArray(s) : QString();
    }

    CString cs(const QString& s)
    {
        return CString(reinterpret_cast<LPCWSTR>(s.utf16()));
    }

    CString EscapeSql(const CString& s)
    {
        CString out = s;
        out.Replace(L"'", L"''");
        return out;
    }

    CString ReadParam(MStudioData::IParamsOwner* p, LPCTSTR a, LPCTSTR b = nullptr, LPCTSTR c = nullptr)
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

    CString CaptionOf(MStudioData::IParam* p)
    {
        if (!p) return L"?";
        LPCTSTR cap = p->GetComment();
        if (cap && cap[0]) return cap;
        LPCTSTR nm = p->GetName();
        return nm ? nm : L"?";
    }

    struct ParamInfo
    {
        CString sysName;
        CString caption;
        CString category;
        MStudioData::IParamDef::EType type = MStudioData::IParamDef::ptDBString;
    };

    struct FindRow
    {
        int objectId = 0;
        CString name;
        CString partType;
        CString pipeClass;
        CString diameter;
        CString thickness;
    };

    class FindDialog : public QDialog
    {
    public:
        explicit FindDialog(QWidget* parent = nullptr)
            : QDialog(parent)
        {
            setWindowTitle(tw(L"Test — поиск по параметрам БД"));
            setModal(true);
            resize(1100, 720);
            BuildUi();
            ConnectSignals();

            m_settings.Load();
            m_editUser->setText(qs(m_settings.lastUser));
            m_editName->setText(qs(m_settings.lastNameFilter));

            CString err;
            if (!EnsureStandardDbConnected(m_pDb, err, true))
            {
                m_status->setText(qs(err));
                return;
            }
            RefreshConnectionInfo();
            if (!LoadParamDefs(err))
            {
                m_status->setText(qs(err));
                return;
            }
            FillParamTree();
            RestoreCriteria(m_settings.lastCriteria);

            m_status->setText(tw(L"Параметров в БД: %1. Добавьте условия и нажмите «Найти».")
                .arg(m_params.size()));
        }

        ~FindDialog() override
        {
            if (g_activeFindDlg == this)
                g_activeFindDlg = nullptr;
            ReleaseDb();
        }

        void ForceClose()
        {
            PersistSettings();
            ReleaseDb();
            reject();
        }

    protected:
        void done(int r) override
        {
            PersistSettings();
            ReleaseDb();
            QDialog::done(r);
        }

        void closeEvent(QCloseEvent* e) override
        {
            PersistSettings();
            ReleaseDb();
            QDialog::closeEvent(e);
        }

    private:
        void ReleaseDb()
        {
            m_pParamDefs.Release();
            m_pDb.Release();
        }

        void BuildUi()
        {
            auto* root = new QVBoxLayout(this);

            auto* top = new QHBoxLayout();
            top->addWidget(new QLabel(tw(L"Пользователь:")));
            m_editUser = new QLineEdit();
            m_editUser->setMinimumWidth(160);
            top->addWidget(m_editUser);
            top->addWidget(new QLabel(tw(L"Имя объекта:")));
            m_editName = new QLineEdit();
            m_editName->setMinimumWidth(200);
            top->addWidget(m_editName, 1);
            m_btnSearch = new QPushButton(tw(L"Найти"));
            m_btnClose = new QPushButton(tw(L"Закрыть"));
            top->addWidget(m_btnSearch);
            top->addWidget(m_btnClose);
            root->addLayout(top);

            m_status = new QLabel(tw(L"Подключение…"));
            root->addWidget(m_status);

            auto* splitMain = new QSplitter(Qt::Horizontal);

            // Left: categories / params
            auto* leftBox = new QWidget();
            auto* leftLay = new QVBoxLayout(leftBox);
            leftLay->setContentsMargins(0, 0, 0, 0);
            leftLay->addWidget(new QLabel(tw(L"Параметры по категориям (двойной клик — добавить):")));
            m_tree = new QTreeWidget();
            m_tree->setHeaderLabels({ tw(L"Категория / параметр"), tw(L"Сист. имя") });
            m_tree->setColumnWidth(0, 260);
            leftLay->addWidget(m_tree);
            splitMain->addWidget(leftBox);

            // Right: criteria + results
            auto* rightBox = new QWidget();
            auto* rightLay = new QVBoxLayout(rightBox);
            rightLay->setContentsMargins(0, 0, 0, 0);

            rightLay->addWidget(new QLabel(tw(L"Условия поиска (все сразу):")));
            m_criteria = new QTableWidget(0, 3);
            m_criteria->setHorizontalHeaderLabels({
                tw(L"Параметр"),
                tw(L"Сист. имя"),
                tw(L"Значение (подстрока)")
            });
            m_criteria->horizontalHeader()->setStretchLastSection(true);
            m_criteria->setSelectionBehavior(QAbstractItemView::SelectRows);
            m_criteria->setMaximumHeight(180);
            rightLay->addWidget(m_criteria);

            auto* critBtns = new QHBoxLayout();
            m_btnAddCrit = new QPushButton(tw(L"Добавить условие"));
            m_btnDelCrit = new QPushButton(tw(L"Удалить условие"));
            m_btnClearCrit = new QPushButton(tw(L"Очистить"));
            critBtns->addWidget(m_btnAddCrit);
            critBtns->addWidget(m_btnDelCrit);
            critBtns->addWidget(m_btnClearCrit);
            critBtns->addStretch();
            rightLay->addLayout(critBtns);

            auto* splitRes = new QSplitter(Qt::Vertical);
            m_results = new QTableWidget(0, 6);
            m_results->setHorizontalHeaderLabels({
                QStringLiteral("Id"),
                tw(L"Имя"),
                tw(L"Тип"),
                tw(L"Класс"),
                tw(L"Диаметр"),
                tw(L"Толщина")
            });
            m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
            m_results->setSelectionMode(QAbstractItemView::SingleSelection);
            m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
            m_results->horizontalHeader()->setStretchLastSection(true);
            splitRes->addWidget(m_results);

            m_paramsView = new QTextEdit();
            m_paramsView->setReadOnly(true);
            m_paramsView->setPlaceholderText(tw(L"Параметры выбранного объекта"));
            splitRes->addWidget(m_paramsView);
            splitRes->setStretchFactor(0, 3);
            splitRes->setStretchFactor(1, 2);
            rightLay->addWidget(splitRes, 1);

            splitMain->addWidget(rightBox);
            splitMain->setStretchFactor(0, 1);
            splitMain->setStretchFactor(1, 2);
            root->addWidget(splitMain, 1);
        }

        void ConnectSignals()
        {
            connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);
            connect(m_btnSearch, &QPushButton::clicked, this, [this] { RunSearch(); });
            connect(m_btnAddCrit, &QPushButton::clicked, this, [this] { AddCriterionFromSelection(); });
            connect(m_btnDelCrit, &QPushButton::clicked, this, [this] {
                const int row = m_criteria->currentRow();
                if (row >= 0)
                    m_criteria->removeRow(row);
            });
            connect(m_btnClearCrit, &QPushButton::clicked, this, [this] {
                m_criteria->setRowCount(0);
            });
            connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
                if (!item || item->data(0, Qt::UserRole).toString().isEmpty())
                    return;
                AddCriterionRow(
                    cs(item->text(0)),
                    cs(item->data(0, Qt::UserRole).toString()),
                    CString());
            });
            connect(m_results, &QTableWidget::itemSelectionChanged, this, [this] {
                ShowParamsForSelection();
            });
        }

        void RefreshConnectionInfo()
        {
            if (!m_pDb) return;
            CComBSTR bUser, bServer, bDb;
            m_pDb->GetUserName(&bUser);
            m_pDb->GetServer(&bServer);
            m_pDb->GetDatabaseName(&bDb);
            if (bUser.Length() && m_editUser->text().trimmed().isEmpty())
                m_editUser->setText(qs(bUser.m_str));
            m_connServer = bServer.m_str ? bServer.m_str : L"";
            m_connDatabase = bDb.m_str ? bDb.m_str : L"";
        }

        bool LoadParamDefs(CString& err)
        {
            m_params.clear();
            m_typeBySys.clear();
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
            m_params.reserve(static_cast<size_t>(n));
            for (idx_t i = 0; i < n; ++i)
            {
                CComPtr<MStudioData::IParamDef> pDef;
                if (FAILED(m_pParamDefs->GetParam(i, &pDef)) || !pDef)
                    continue;
                LPCTSTR nm = pDef->GetName();
                if (!nm || !nm[0]) continue;

                ParamInfo info;
                info.sysName = nm;
                LPCTSTR cap = pDef->GetComment();
                info.caption = (cap && cap[0]) ? cap : nm;
                info.type = pDef->GetType();
                m_typeBySys[info.sysName] = info.type;

                const idx_t catCount = pDef->GetCategoriesCount();
                if (catCount > 0)
                {
                    for (idx_t c = 0; c < catCount; ++c)
                    {
                        ParamInfo copy = info;
                        LPCTSTR cat = pDef->GetCategory(c);
                        copy.category = (cat && cat[0]) ? cat : L"(без категории)";
                        m_params.push_back(copy);
                    }
                }
                else
                {
                    info.category = L"(без категории)";
                    m_params.push_back(info);
                }
            }
            return true;
        }

        MStudioData::IParamDef::EType TypeOf(const CString& sys) const
        {
            const auto it = m_typeBySys.find(sys);
            if (it != m_typeBySys.end())
                return it->second;
            return MStudioData::IParamDef::ptDBString;
        }

        void FillParamTree()
        {
            m_tree->clear();
            QMap<QString, QTreeWidgetItem*> cats;
            for (const auto& p : m_params)
            {
                const QString cat = qs(p.category);
                QTreeWidgetItem* parent = cats.value(cat, nullptr);
                if (!parent)
                {
                    parent = new QTreeWidgetItem(m_tree);
                    parent->setText(0, cat);
                    parent->setFirstColumnSpanned(true);
                    cats.insert(cat, parent);
                }
                auto* child = new QTreeWidgetItem(parent);
                child->setText(0, qs(p.caption));
                child->setText(1, qs(p.sysName));
                child->setData(0, Qt::UserRole, qs(p.sysName));
            }
            m_tree->sortItems(0, Qt::AscendingOrder);
        }

        void AddCriterionRow(const CString& caption, const CString& sys, const CString& value)
        {
            const int row = m_criteria->rowCount();
            m_criteria->insertRow(row);
            auto* capItem = new QTableWidgetItem(qs(caption.IsEmpty() ? sys : caption));
            auto* sysItem = new QTableWidgetItem(qs(sys));
            sysItem->setFlags(sysItem->flags() & ~Qt::ItemIsEditable);
            auto* valItem = new QTableWidgetItem(qs(value));
            m_criteria->setItem(row, 0, capItem);
            m_criteria->setItem(row, 1, sysItem);
            m_criteria->setItem(row, 2, valItem);
            m_criteria->setCurrentCell(row, 2);
            m_criteria->editItem(valItem);
        }

        void AddCriterionFromSelection()
        {
            QTreeWidgetItem* item = m_tree->currentItem();
            if (item && !item->data(0, Qt::UserRole).toString().isEmpty())
            {
                AddCriterionRow(
                    cs(item->text(0)),
                    cs(item->data(0, Qt::UserRole).toString()),
                    CString());
                return;
            }
            AddCriterionRow(L"(любой параметр)", L"", CString());
        }

        void RestoreCriteria(const std::vector<SearchCriterion>& list)
        {
            m_criteria->setRowCount(0);
            for (const auto& c : list)
                AddCriterionRow(c.paramCaption, c.paramSys, c.value);
        }

        std::vector<SearchCriterion> CollectCriteria() const
        {
            std::vector<SearchCriterion> out;
            for (int r = 0; r < m_criteria->rowCount(); ++r)
            {
                SearchCriterion c;
                const auto* cap = m_criteria->item(r, 0);
                const auto* sys = m_criteria->item(r, 1);
                const auto* val = m_criteria->item(r, 2);
                c.paramCaption = cap ? cs(cap->text()) : L"";
                c.paramSys = sys ? cs(sys->text()) : L"";
                c.value = val ? cs(val->text().trimmed()) : L"";
                if (!c.value.IsEmpty() || !c.paramSys.IsEmpty())
                    out.push_back(c);
            }
            return out;
        }

        void PersistSettings()
        {
            m_settings.lastUser = cs(m_editUser->text().trimmed());
            m_settings.lastServer = m_connServer;
            m_settings.lastDatabase = m_connDatabase;
            m_settings.lastNameFilter = cs(m_editName->text().trimmed());
            m_settings.lastCriteria = CollectCriteria();
            m_settings.Save();
        }

        CString BuildFilter(int nCat, const CString& nameFilter,
            const std::vector<SearchCriterion>& criteria) const
        {
            // CadLib filter fragment: ", Parameters_XXX PTn where ... AND (PTn.idParamDef = id) ..."
            // Numbers live in Parameters_DBL/INT — Parameters_STR always returns 0 for them.
            CString joins;
            CString where;
            where.Format(
                L" where (O.idObjectCategory = %d) AND (O.idParentObject is null)",
                nCat);

            unsigned used = 0;
            for (size_t i = 0; i < criteria.size(); ++i)
            {
                const SearchCriterion& c = criteria[i];
                if (c.value.IsEmpty() || c.paramSys.IsEmpty())
                    continue;

                const auto type = TypeOf(c.paramSys);
                const LPCTSTR table = ParamTableName(type);
                const CString escParam = EscapeSql(c.paramSys);

                CString pt;
                pt.Format(L"PT%u", used);
                ++used;

                CString j;
                j.Format(L", %s %s", table, pt.GetString());
                joins += j;

                CString w;
                w.Format(
                    L" AND (%s.idObject = O.idObject)"
                    L" AND (%s.idParamDef = (select idParamDef from ParamDefs where Name = '%s'))",
                    pt.GetString(), pt.GetString(), escParam.GetString());
                where += w;

                double num = 0;
                if (IsNumericParamType(type) && ParseNumber(c.value, num))
                {
                    CString numCond;
                    // Exact match for numeric params (159, 5, 60.3, ...)
                    if (type == MStudioData::IParamDef::ptDBInt
                        || type == MStudioData::IParamDef::ptDBListInt)
                    {
                        numCond.Format(L" AND (%s.Value = %d)", pt.GetString(),
                            static_cast<int>(num));
                    }
                    else
                    {
                        CString numStr;
                        numStr.Format(L"%.10g", num);
                        numStr.Replace(L',', L'.');
                        numCond.Format(L" AND (%s.Value = %s)", pt.GetString(), numStr.GetString());
                    }
                    where += numCond;
                }
                else
                {
                    CString sCond;
                    sCond.Format(L" AND (%s.Value like '%%%s%%')",
                        pt.GetString(), EscapeSql(c.value).GetString());
                    where += sCond;
                }
            }

            if (!nameFilter.IsEmpty())
            {
                CString part;
                part.Format(L" AND (O.Name like '%%%s%%')", EscapeSql(nameFilter).GetString());
                where += part;
            }
            return joins + where;
        }

        void FillResults()
        {
            QSignalBlocker block(m_results);
            m_results->setRowCount(0);
            for (size_t i = 0; i < m_rows.size(); ++i)
            {
                const FindRow& r = m_rows[i];
                const int row = m_results->rowCount();
                m_results->insertRow(row);
                auto* idItem = new QTableWidgetItem(QString::number(r.objectId));
                idItem->setData(Qt::UserRole, static_cast<qulonglong>(i));
                m_results->setItem(row, 0, idItem);
                m_results->setItem(row, 1, new QTableWidgetItem(qs(r.name)));
                m_results->setItem(row, 2, new QTableWidgetItem(qs(r.partType)));
                m_results->setItem(row, 3, new QTableWidgetItem(qs(r.pipeClass)));
                m_results->setItem(row, 4, new QTableWidgetItem(qs(r.diameter)));
                m_results->setItem(row, 5, new QTableWidgetItem(qs(r.thickness)));
            }
        }

        void ShowParamsForSelection()
        {
            m_paramsView->clear();
            if (!m_pDb) return;
            const auto selected = m_results->selectedItems();
            if (selected.isEmpty()) return;
            const int row = selected.first()->row();
            auto* idItem = m_results->item(row, 0);
            if (!idItem) return;
            const size_t idx = static_cast<size_t>(idItem->data(Qt::UserRole).toULongLong());
            if (idx >= m_rows.size()) return;

            CComPtr<MStudioData::IParamsOwner> pParams;
            if (FAILED(m_pDb->GetObjectParameters(m_rows[idx].objectId, &pParams)) || !pParams)
            {
                m_paramsView->setPlainText(QStringLiteral("(no params)"));
                return;
            }

            QString all;
            for (idx_t n = 0; ; ++n)
            {
                CComPtr<MStudioData::IParam> pParam;
                if (!pParams->GetParameter(n, &pParam) || !pParam)
                    break;
                CComBSTR bVal;
                pParam->GetValue(&bVal);
                all += qs(CaptionOf(pParam));
                all += QStringLiteral(" [");
                all += qs(pParam->GetName());
                all += QStringLiteral("] = ");
                all += qs(bVal.m_str ? bVal.m_str : L"");
                all += QLatin1Char('\n');
                if (n > 800) break;
            }
            if (all.isEmpty())
                all = QStringLiteral("(empty)");
            m_paramsView->setPlainText(all);
        }

        void RunSearch()
        {
            PersistSettings();
            m_rows.clear();
            FillResults();
            m_paramsView->clear();

            if (!m_pDb || !m_pDb->IsConnected())
            {
                CString err;
                if (!EnsureStandardDbConnected(m_pDb, err, true))
                {
                    m_status->setText(qs(err));
                    return;
                }
                RefreshConnectionInfo();
            }

            const CString nameFilter = cs(m_editName->text().trimmed());
            const auto criteria = CollectCriteria();

            bool hasParamCrit = false;
            for (const auto& c : criteria)
            {
                if (!c.value.IsEmpty() && !c.paramSys.IsEmpty())
                {
                    hasParamCrit = true;
                    break;
                }
            }
            if (!hasParamCrit && nameFilter.IsEmpty())
            {
                m_status->setText(tw(L"Задайте имя и/или условия по параметрам."));
                return;
            }

            int nCat = m_pDb->GetObjectCategoryId(kCatPipeItems, true);
            if (nCat <= 0)
                nCat = 69;

            const CString filter = BuildFilter(nCat, nameFilter, criteria);
            acutPrintf(L"\n[Test] QT_FIND cat=%d criteria=%u\n filter=%s\n",
                nCat, static_cast<unsigned>(criteria.size()), filter.GetString());

            m_status->setText(tw(L"Поиск…"));
            QApplication::setOverrideCursor(Qt::WaitCursor);

            CComPtr<MStudioDB::ILibObjectInfoArr> pObjects;
            const HRESULT hrObj = m_pDb->GetLibraryObjects(filter, &pObjects);
            acutPrintf(L"\n[Test] QT_FIND hr=0x%08X count=%d\n",
                static_cast<unsigned>(hrObj), pObjects ? pObjects->GetCount() : -1);

            if (FAILED(hrObj) || !pObjects)
            {
                QApplication::restoreOverrideCursor();
                m_status->setText(tw(L"GetLibraryObjects failed"));
                return;
            }

            // No full-category fallback: that scanned 90k objects and froze nanoCAD.
            const int total = pObjects->GetCount();
            const int scan = (total > kMaxShow) ? kMaxShow : total;
            int shown = 0;

            for (int i = 0; i < scan; ++i)
            {
                const int id = pObjects->GetObjectIdByIndex(i);
                const LPCTSTR pszName = pObjects->GetObjectNameByIndex(i);

                CComPtr<MStudioData::IParamsOwner> pParams;
                CString partType, pipeClass, diameter, thickness;
                if (SUCCEEDED(m_pDb->GetObjectParameters(id, &pParams)) && pParams)
                {
                    partType = ReadParam(pParams, L"PART_TYPE");
                    pipeClass = ReadParam(pParams, L"PART_PIPE_CLASS");
                    diameter = ReadParam(pParams, L"PART_PIPE_DIAMETER", L"PART_PIPE_DN", L"DIM_DIAMETER");
                    thickness = ReadParam(pParams, L"PIPE_THICKNESS", L"DIM_THICKNESS", L"ELBOW_THICKNESS");
                }

                FindRow row;
                row.objectId = id;
                row.name = pszName ? pszName : L"";
                row.partType = partType;
                row.pipeClass = pipeClass;
                row.diameter = diameter;
                row.thickness = thickness;
                m_rows.push_back(row);
                ++shown;
            }

            QApplication::restoreOverrideCursor();
            FillResults();
            m_status->setText(tw(L"Найдено: %1 (показано %2). Числа — точное совпадение.")
                .arg(total).arg(shown));
            acutPrintf(L"\n[Test] MS_DB_FIND(Qt) matched=%d shown=%d\n", total, shown);
        }

        CComPtr<MStudioDB::IDataBase> m_pDb;
        CComPtr<MStudioData::IParamDefs> m_pParamDefs;
        UserSettings m_settings;
        CString m_connServer;
        CString m_connDatabase;
        std::vector<ParamInfo> m_params;
        std::map<CString, MStudioData::IParamDef::EType> m_typeBySys;
        std::vector<FindRow> m_rows;

        QLineEdit* m_editUser = nullptr;
        QLineEdit* m_editName = nullptr;
        QLabel* m_status = nullptr;
        QTreeWidget* m_tree = nullptr;
        QTableWidget* m_criteria = nullptr;
        QTableWidget* m_results = nullptr;
        QTextEdit* m_paramsView = nullptr;
        QPushButton* m_btnSearch = nullptr;
        QPushButton* m_btnClose = nullptr;
        QPushButton* m_btnAddCrit = nullptr;
        QPushButton* m_btnDelCrit = nullptr;
        QPushButton* m_btnClearCrit = nullptr;
    };
}

void DbFindQt::ShowModal(HWND parent)
{
    // Prefer plain Qt dialog (no QWinWidget hook on main HWND) —
    // safer for APPUNLOAD. Parent HWND kept only for positioning.
    FindDialog dlg(nullptr);
    dlg.setWindowModality(Qt::ApplicationModal);
    if (parent)
    {
        RECT rc = {};
        if (GetWindowRect(parent, &rc))
        {
            const int x = rc.left + ((rc.right - rc.left) - dlg.width()) / 2;
            const int y = rc.top + ((rc.bottom - rc.top) - dlg.height()) / 2;
            dlg.move(x, y);
        }
    }

    g_activeFindDlg = &dlg;
    dlg.exec();
    if (g_activeFindDlg == &dlg)
        g_activeFindDlg = nullptr;

    FlushQtDeferredDeletes();
}

void DbFindQt::Shutdown()
{
    if (g_activeFindDlg)
    {
        g_activeFindDlg->ForceClose();
        g_activeFindDlg = nullptr;
    }
    FlushQtDeferredDeletes();
}
