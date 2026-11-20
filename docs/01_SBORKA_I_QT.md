# 01. Сборка и Qt SDK

## 1. Что нужно установить

### Visual Studio 2022

- Workload: **Desktop development with C++**
- Individual component: **MFC for latest v143 build tools (x86 & x64)**
- Toolset в проекте: `v143`

> Если `MSB8041` (нет MFC) — соберите через MSBuild из **VS2022 Community**, а не из VS без MFC.

### nanoCAD SDK

Корень SDK (у автора: `D:\WORK\SAP\ncadsdk`):

```text
ncadsdk\
  include\          — arxgate, nrx*, hostQtUi.h
  include\Qt\qt.props
  include-x64\
  lib-x64\hostQt.lib
  samples\NRX\HelloQt\
```

В `Test.paths.props` свойство `NCadSdkRoot` должно указывать на этот корень.

### Model Studio API / Source

Нужны заголовки:

- `mstudioDB\CSDBInterfaces.h` — `IDataBase`
- `msData\CSDataInterfaces.h` — `IParamDef`, `IParamsOwner`

И возможность линковки/загрузки runtime Model Studio (enabler), как в обычных MS-плагинах.

### Qt 5.15 (dev SDK, не только DLL из nanoCAD)

`qt.props` в SDK жёстко ждёт:

```text
C:\Qt\5.15.16\msvc2019_64
```

В открытых зеркалах Qt **5.15.16 нет** (LTS/commercial). Свободно ставится **5.15.2**.

#### Установка через aqtinstall

```powershell
pip install -U aqtinstall
aqt install-qt windows desktop 5.15.2 win64_msvc2019_64 -O C:\Qt --base https://mirrors.ocf.berkeley.edu/qt
```

Junction под путь из `qt.props`:

```powershell
New-Item -ItemType Directory -Force -Path C:\Qt\5.15.16 | Out-Null
cmd /c mklink /J C:\Qt\5.15.16\msvc2019_64 C:\Qt\5.15.2\msvc2019_64
C:\Qt\5.15.16\msvc2019_64\bin\qmake.exe -v
```

Проверьте наличие:

- `include\QtWidgets\QDialog`
- `lib\Qt5Widgets.lib`
- `bin\uic.exe`, `bin\moc.exe` (если делаете `.ui` / `Q_OBJECT`)

## 2. Подключение Qt к vcxproj

В `Test.vcxproj` для Debug/Release NCAD:

```xml
<Import Project="$(NCadSDK)include\Qt\qt.props"
        Condition="Exists('$(NCadSDK)include\Qt\qt.props')" />
```

`qt.props` добавляет:

- include: `QtCore`, `QtGui`, `QtWidgets`, `QtXml`, `QtWinExtras`
- link: `Qt5Core.lib; Qt5Gui.lib; Qt5Widgets.lib; Qt5Xml.lib; Qt5WinExtras.lib; hostQt.lib`

### Обязательно: `/utf-8`

Русские строки в исходниках + `QString::fromWCharArray(L"…")` требуют:

```xml
<AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
```

Иначе UI будет «кракозябрами», а данные из БД (wchar) останутся нормальными.

## 3. Сборка

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "D:\WORK\SAP\Test\Test.sln" /p:Configuration="Release NCAD" /p:Platform=x64 /m
```

Результат:

- `bin-x64\Release NCAD\Test.nrx`
- копия в `deploy\Test.nrx` (PostBuild)

## 4. Загрузка в nanoCAD

1. Запущен Model Studio / доступна БД стандартных компонентов.
2. `APPLOAD` → `deploy\Test.nrx`
3. Команды: `MS_DB`, `MS_DB_FIND`

Runtime Qt уже в `nanoCAD\bin` — **не копируйте** Qt DLL рядом с `.nrx`, если версия ABI совместима с хостом.

## 5. Частые ошибки сборки

| Симптом | Причина | Что делать |
|---------|---------|------------|
| `MSB8041` MFC | Нет компонента MFC | VS Installer → MFC, или другой VS |
| `Cannot open Qt5Widgets.lib` | Нет SDK Qt / нет junction | Установить 5.15.2 + junction 5.15.16 |
| `hostQt.lib` not found | Неверный `NCadSdkLibX64` | Проверить `Test.paths.props` |
| Кракозябры в Qt UI | Нет `/utf-8` | Добавить в ClCompile |
| Падение на APPUNLOAD | Живые Qt-объекты / QWinWidget | См. [02_QT_OKNO.md](02_QT_OKNO.md) |
