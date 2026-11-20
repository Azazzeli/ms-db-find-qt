# 03. Подключение к БД стандартных компонентов

Разбор `StdDbHelper` + `MS_DB`. Цель — получить валидный `MStudioDB::IDataBase*`, уже связанный с той же БД, что использует Model Studio / CadLib.

---

## 1. Какая это БД

**БД стандартных компонентов** (Standard Components / CadLib Library):

- Каталог деталей (`pipeItems`, арматура, …)
- Таблицы `Objects`, `ParamDefs`, `Parameters_STR` / `_INT` / `_DBL`, `ParamCategory`, …
- Обычно PostgreSQL (у автора: порт `5433`, БД вроде `12334`) или MSSQL

Учётки и сервер уже сохранены CadLib’ом, например:

`%APPDATA%\CSoft\Model Studio CS\…\lib_settings.xml` / `cadlib_settings.xml`

Плагину **не нужно** заново спрашивать пароль, если сессия MS жива.

---

## 2. API: фабрика и стандартная БД

```cpp
#include "AcadLoader.h"                 // CCSMstudioAcadLoadHelper
#include "mstudioDB/CSDBInterfaces.h" // IDataBase, CMStudioDataInterface

CCSMstudioAcadLoadHelper::LoadModelStudioEnabler();

MStudioDB::IFactoryInterface* pFactory = MStudioDB::CMStudioDataInterface::Get();
if (!pFactory) { /* не загружен mstudio DB module */ }

CComPtr<MStudioDB::IDataBase> pDb;
pFactory->GetStandardComponentsDataBase(&pDb);
```

`GetStandardComponentsDataBase` возвращает объект БД «стандартных компонентов» (не проектную БД чертежа).

---

## 3. UIConnect с флагом bStd

Если ещё не подключены:

```cpp
if (pDb->IsConnected())
    return true;

CComBSTR bDriver;
pDb->GetDriver(&bDriver);
auto platform = pDb->GetPlatform();   // dbPostgreSQL / dbMSSQL / …
int port = pDb->GetPort();

bool canceled = false;
bool ok = pDb->UIConnect(
    platform,
    bDriver.Length() ? bDriver.m_str : L"",
    port,
    /*server*/ nullptr,
    /*database*/ nullptr,
    /*user*/ nullptr,
    /*password*/ nullptr,
    /*bStd*/ true,          // ← брать стандартные настройки CadLib
    L"БД стандартных компонент",
    &canceled);
```

### Что значит `bStd = true`

Документация интерфейса: использовать **стандартные настройки БД**, а не переданные `pszServerName/pszDatabase/pszUser/…`.

Практически:

- подставляются server/db/user из профиля CadLib / последнего входа MS;
- может показаться диалог CadLib, если сессии ещё нет;
- при `canceled == true` пользователь нажал Cancel.

Полный код: `StdDbHelper.cpp` → `EnsureStandardDbConnected`.

---

## 4. Что читать после connect

```cpp
CComBSTR user, server, db;
pDb->GetUserName(&user);
pDb->GetServer(&server);
pDb->GetDatabaseName(&db);
```

В Qt-окне это кладётся в поле «Пользователь» и в `UserSettings` (`%APPDATA%\TestNRX\user_settings.ini`).

---

## 5. Категории объектов

Для поиска деталей трубопроводов:

```cpp
int nCat = pDb->GetObjectCategoryId(L"pipeItems", /*bSysName*/ true);
// часто id = 69 («Детали трубопроводов»)
```

`bSysName = true` — системное имя категории (`pipeItems`), не русская подпись.

Список всех категорий: `GetObjectCategories`.

---

## 6. Определения параметров (ParamDefs)

```cpp
CComPtr<MStudioData::IParamDefs> pDefs;
pDb->GetParamDefs(&pDefs);

for (idx_t i = 0; i < pDefs->GetCount(); ++i) {
    CComPtr<MStudioData::IParamDef> pDef;
    pDefs->GetParam(i, &pDef);

    LPCTSTR sys  = pDef->GetName();      // PART_PIPE_DIAMETER
    LPCTSTR cap  = pDef->GetComment();   // русская подпись
    auto type    = pDef->GetType();      // STR / INT / DBL / list…

    // Категории параметра (не путать с категорией объекта!)
    for (idx_t c = 0; c < pDef->GetCategoriesCount(); ++c)
        LPCTSTR paramCat = pDef->GetCategory(c); // «Основные параметры», …
}
```

Именно `GetCategory` наполняет дерево слева в `MS_DB_FIND`.

### Типы → таблицы значений

| `IParamDef::EType` | Таблица CadLib |
|--------------------|----------------|
| `ptDBString`, `ptDBMemo`, `ptDBListOpen`, … | `Parameters_STR` |
| `ptDBInt`, `ptDBListInt` | `Parameters_INT` |
| `ptDBDouble`, `ptDBListDbl` | `Parameters_DBL` |

**Критично:** диаметр/толщина трубы (`PART_PIPE_DIAMETER`, `PIPE_THICKNESS`) — это **DBL**, не STR.  
Фильтр по `Parameters_STR` всегда даст 0 и спровоцирует «медленный» fallback.

---

## 7. Параметры конкретного объекта

```cpp
CComPtr<MStudioData::IParamsOwner> pParams;
pDb->GetObjectParameters(idObject, &pParams);

CString dia = pParams->GetParameterValue(L"PART_PIPE_DIAMETER", L"");

for (idx_t n = 0; ; ++n) {
    CComPtr<MStudioData::IParam> p;
    if (!pParams->GetParameter(n, &p) || !p) break;
    CComBSTR val; p->GetValue(&val);
    // p->GetName(), GetComment()
}
```

---

## 8. Поток MS_DB (схема)

```text
MS_DB
  → LoadModelStudioEnabler
  → GetStandardComponentsDataBase
  → IsConnected?
        да  → показать статус
        нет → UIConnect(bStd=true)
  → GetServer / GetDatabaseName / GetUserName
```

Команда `MS_DB_FIND` вызывает тот же `EnsureStandardDbConnected` при открытии окна и перед поиском.

---

## 9. Типичные ошибки

| Симптом | Причина |
|---------|---------|
| «Не загружен … DB module» | Нет enabler / MS не в процессе |
| UIConnect cancel | Пользователь закрыл диалог |
| Connect OK, поиск 0 | Неверная таблица STR/DBL или категория |
| Другая БД, не та | `bStd=false` и свои server/db |

Следующий шаг — как собрать SQL-фильтр CadLib: [04_FILTRACIYA_I_POISK.md](04_FILTRACIYA_I_POISK.md).
