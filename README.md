# ms-db-find-qt

NRX-плагин для **nanoCAD + Model Studio CS**: подключение к БД стандартных компонентов и **поиск объектов по нескольким параметрам** с UI на **Qt 5**.

| | |
|---|---|
| Модуль | `deploy/Test.nrx` |
| Команды | `MS_DB`, `MS_DB_FIND` |
| UI поиска | Qt 5 (`DbFindQt`) |
| UI connect | MFC (`DbConnectDlg`) + сессия MS (`bStd`) |
| Настройки | `%APPDATA%\TestNRX\user_settings.ini` |

---

## Быстрый старт

1. Закройте nanoCAD полностью.
2. `APPLOAD` → [`deploy/Test.nrx`](deploy/Test.nrx).
3. В Model Studio (с доступом к БД стандартных компонентов):
   - **`MS_DB`** — подключиться к БД (сессия MS / диалог CadLib).
   - **`MS_DB_FIND`** — Qt-окно: категории параметров → условия → **Найти**.
4. Числа (`PART_PIPE_DIAMETER`, `PIPE_THICKNESS`, …) ищутся **точно** (`159`, `5`, `60.3`).
5. Перед `APPUNLOAD` закройте Qt-окно.

---

## Документация (гайд)

Подробный разбор «как сделать своё Qt-окно», подключение к БД, фильтрация и поиск:

| Файл | Тема |
|------|------|
| [docs/00_OBZOR.md](docs/00_OBZOR.md) | Обзор архитектуры |
| [docs/01_SBORKA_I_QT.md](docs/01_SBORKA_I_QT.md) | Сборка, nanoCAD SDK, установка Qt SDK |
| [docs/02_QT_OKNO.md](docs/02_QT_OKNO.md) | Как сделать своё Qt-окно в NRX |
| [docs/03_PODKLYUCHENIE_BD.md](docs/03_PODKLYUCHENIE_BD.md) | Подключение к БД стандартных компонентов |
| [docs/04_FILTRACIYA_I_POISK.md](docs/04_FILTRACIYA_I_POISK.md) | Фильтрация CadLib, таблицы STR/INT/DBL, мультипоиск |
| [docs/05_STRUKTURA.md](docs/05_STRUKTURA.md) | Структура исходников |

Сайт-гайд (GitHub Pages): после push включите Pages → ветка `main` → папка `/docs`.

---

## Требования для сборки

- Visual Studio 2022 (`v143`) + **MFC**
- [nanoCAD SDK](https://www.nanocad.ru/) (`include`, `lib-x64`, `include\Qt\qt.props`, `hostQt.lib`)
- Исходники / API **Model Studio** (`mstudioDB`, `msData`)
- **Qt 5.15.x** `msvc2019_64` в `C:\Qt\5.15.16\msvc2019_64`  
  (в `qt.props` SDK указан путь `5.15.16`; свободно качается `5.15.2` — сделайте junction)

Раскладка по умолчанию:

```text
…\SAP\
  Test\            ← этот репозиторий
  ncadsdk\
  Model Studio\
```

Пути: [`Test.paths.props`](Test.paths.props) / [`Test.paths.props.example`](Test.paths.props.example).

Сборка: `Test.sln` → **Release NCAD | x64** (MSBuild из VS2022 Community, если в VS 18 нет MFC).

---

## Лицензия / назначение

Учебный / демонстрационный пример API CadLib + Qt в NRX. Адаптируйте под свой продукт.
