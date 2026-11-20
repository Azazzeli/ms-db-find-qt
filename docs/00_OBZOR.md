# 00. Обзор

## Что делает модуль

`Test.nrx` — лёгкий NRX-плагин для nanoCAD, который:

1. Берёт уже известную Model Studio **сессию** к БД стандартных компонентов (логин/сервер из CadLib, флаг `bStd`).
2. Показывает **Qt-окно поиска**: параметры разложены по категориям, можно добавить несколько условий и искать.
3. Пишет в файл последнего пользователя и последние критерии поиска.

## Команды

| Команда | Что делает |
|---------|------------|
| `MS_DB` | MFC-диалог: `GetStandardComponentsDataBase` + `UIConnect(..., bStd=true)` |
| `MS_DB_FIND` | Qt-диалог: категории → мультиусловия → `GetLibraryObjects(filter)` |

## Слои архитектуры

```text
┌─────────────────────────────────────────────┐
│  nanoCAD host (уже есть QApplication + Qt5) │
├─────────────────────────────────────────────┤
│  Test.nrx                                   │
│   ├─ Test.cpp          — регистрация команд │
│   ├─ StdDbHelper       — connect к Std DB   │
│   ├─ DbConnectDlg      — MFC MS_DB          │
│   ├─ DbFindQt          — Qt MS_DB_FIND      │
│   └─ UserSettings      — %APPDATA%\TestNRX  │
├─────────────────────────────────────────────┤
│  Model Studio DB API (mstudioDB / msData)   │
│   IDataBase, IParamDefs, IParamsOwner       │
├─────────────────────────────────────────────┤
│  PostgreSQL / MSSQL  (CadLib schema)        │
│   Objects, ParamDefs, Parameters_STR/INT/DBL│
└─────────────────────────────────────────────┘
```

## Почему Qt, а не только MFC

- В nanoCAD 24.x уже есть runtime Qt 5 (`Qt5Widgets.dll`, `hostQt.dll` в `bin`).
- В nanoCAD SDK есть sample `samples\NRX\HelloQt` и `include\Qt\qt.props`.
- Для «кастомного» UI (дерево категорий, таблица условий, сплиттеры) Qt удобнее MFC-ресурсов.

**Важно:** плагин **линкуется** с Qt SDK (`Qt5*.lib`) и `hostQt.lib`, но **не поднимает свой** `QApplication` — использует хостовый.

## Безопасность выгрузки (`APPUNLOAD`)

Модальное окно + явное закрытие на `kUnloadAppMsg` (`DbFindQt::Shutdown`).  
Не ставьте хуки `QWinWidget` на главное окно CAD без крайней нужды — при выгрузке NRX это частая причина падений.

## Связанные документы

- Сборка и Qt → [01_SBORKA_I_QT.md](01_SBORKA_I_QT.md)
- Своё окно → [02_QT_OKNO.md](02_QT_OKNO.md)
- Connect → [03_PODKLYUCHENIE_BD.md](03_PODKLYUCHENIE_BD.md)
- Фильтры → [04_FILTRACIYA_I_POISK.md](04_FILTRACIYA_I_POISK.md)
