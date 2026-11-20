# 05. Структура исходников

```text
Test/
  Test.sln
  Test.vcxproj
  Test.paths.props              # пути к ncadsdk + Model Studio
  Test.paths.props.example
  Test.def
  Test.rc / resource.h          # MFC-диалоги MS_DB / старый DbFind

  Test.cpp                      # entry point, команды MS_DB / MS_DB_FIND
  stdafx.h / stdafx.cpp

  StdDbHelper.h/.cpp            # EnsureStandardDbConnected
  DbConnectDlg.h/.cpp           # MFC: MS_DB
  DbFindDlg.h/.cpp              # MFC: старый поиск (в сборке, команда на Qt)
  DbFindQt.h/.cpp               # Qt: MS_DB_FIND
  UserSettings.h/.cpp           # %APPDATA%\TestNRX\user_settings.ini

  deploy/Test.nrx               # готовая сборка
  docs/                         # этот гайд (GitHub Pages)
```

## Зависимости путей

`Test.paths.props` ожидает рядом:

```text
SAP/
  Test/           ← репозиторий
  ncadsdk/
  Model Studio/
```

Qt: `C:\Qt\5.15.16\msvc2019_64` (см. [01_SBORKA_I_QT.md](01_SBORKA_I_QT.md)).

## Что смотреть в первую очередь

| Задача | Файл |
|--------|------|
| Регистрация команд / unload | `Test.cpp` |
| Connect bStd | `StdDbHelper.cpp` |
| Qt UI + BuildFilter | `DbFindQt.cpp` |
| INI настроек | `UserSettings.cpp` |
| qt.props import | `Test.vcxproj` |
