# 02. Как сделать своё Qt-окно в NRX

Цель: из команды nanoCAD открыть **модальный** Qt-диалог, не роняя CAD при `APPUNLOAD`.

Ориентир в SDK: `ncadsdk\samples\NRX\HelloQt`.  
В этом проекте рабочий пример: `DbFindQt.cpp` / `DbFindQt.h`, команда `MS_DB_FIND`.

---

## 1. Минимальный каркас команды

В `ncrxEntryPoint` / `acrxEntryPoint`:

```cpp
acedRegCmds->addCommand(L"TEST_GROUP", L"_MS_DB_FIND", L"MS_DB_FIND",
    ACRX_CMD_MODAL, msDbFindCmd);
```

Обработчик:

```cpp
void msDbFindCmd()
{
    DbFindQt::ShowModal(adsw_acadMainWnd());
}
```

На выгрузке:

```cpp
case AcRx::kUnloadAppMsg:
    DbFindQt::Shutdown();   // закрыть диалог, если ещё открыт
    acedRegCmds->removeGroup(L"TEST_GROUP");
    break;
```

`ACRX_CMD_MODAL` + модальный `QDialog::exec()` — пока окно открыто, команда не завершена; это упрощает выгрузку.

---

## 2. Не создавайте свой QApplication

nanoCAD уже создал `QApplication`. В плагине:

```cpp
QCoreApplication::instance(); // не nullptr внутри CAD
```

Достаточно создать виджеты и показать диалог.

---

## 3. Диалог без QWinWidget (рекомендуется)

Sample HelloQt использует `QWinWidget` (qtwinmigrate) — удобно встроить окно в HWND CAD, но хук на главное окно **часто валит CAD при выгрузке `.nrx`**.

Надёжный вариант для модального UI:

```cpp
void DbFindQt::ShowModal(HWND parent)
{
    FindDialog dlg(nullptr);
    dlg.setWindowModality(Qt::ApplicationModal);

    if (parent) {
        RECT rc{};
        if (GetWindowRect(parent, &rc)) {
            dlg.move(
                rc.left + (rc.right - rc.left - dlg.width()) / 2,
                rc.top  + (rc.bottom - rc.top - dlg.height()) / 2);
        }
    }

    g_activeFindDlg = &dlg;
    dlg.exec();
    g_activeFindDlg = nullptr;

    // Сбросить отложенные deleteLater, пока код .nrx ещё в памяти
    if (QCoreApplication::instance())
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
```

### Shutdown

```cpp
void DbFindQt::Shutdown()
{
    if (g_activeFindDlg) {
        g_activeFindDlg->ForceClose();
        g_activeFindDlg = nullptr;
    }
    if (QCoreApplication::instance())
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
```

**Не вызывайте** `QCoreApplication::processEvents()` на unload — это re-entrancy и краши.

---

## 4. Класс диалога без moc (проще для NRX)

Можно **не** использовать `Q_OBJECT` / `moc`, если слоты заменяете лямбдами:

```cpp
class FindDialog : public QDialog
{
public:
    explicit FindDialog(QWidget* parent = nullptr) : QDialog(parent)
    {
        auto* btn = new QPushButton(QString::fromWCharArray(L"Найти"), this);
        connect(btn, &QPushButton::clicked, this, [this]{ RunSearch(); });
    }
};
```

Если нужен `Q_OBJECT` — как в HelloQt: custom build step `moc.exe` → `moc_*.cpp` в IntDir.

---

## 5. Строки UI — только UTF-16 + `/utf-8`

Плохо (часто даёт «P¹Pµ…»):

```cpp
setWindowTitle(QString::fromUtf8(u8"Поиск")); // без /utf-8 ломается
```

Хорошо:

```cpp
static QString tw(const wchar_t* s) { return QString::fromWCharArray(s); }
setWindowTitle(tw(L"Поиск по параметрам БД"));
```

И в `.vcxproj`: `/utf-8`.

Данные из MS API (`LPCTSTR` / `BSTR`) уже UTF-16:

```cpp
QString qs(LPCTSTR s) { return s ? QString::fromWCharArray(s) : QString(); }
```

---

## 6. Типичная компоновка «поискового» окна

Как в `DbFindQt`:

| Зона | Виджет | Назначение |
|------|--------|------------|
| Верх | `QLineEdit` user / name | пользователь + фильтр имени объекта |
| Лево | `QTreeWidget` | ParamDefs по `GetCategory` |
| Право верх | `QTableWidget` | список условий (param + value) |
| Право низ | `QTableWidget` + `QTextEdit` | результаты + параметры выбранного |

Двойной клик по параметру в дереве → новая строка условия.

---

## 7. Связь с MFC в том же `.nrx`

Можно держать и MFC (`DbConnectDlg`), и Qt (`DbFindQt`) в одном модуле:

- `UseOfMfc=Dynamic`
- `CWinApp` в DLL (как в HelloNRX)
- Qt-файлы просто `#include <QDialog>` после `stdafx.h`

Не смешивайте создание MFC-диалога и Qt-диалога одновременно без нужды.

---

## 8. Checklist «своё Qt-окно за час»

1. Импортировать `qt.props`, добавить `/utf-8`.
2. Установить Qt 5.15.x → путь как в `qt.props`.
3. Файл `MyQtDlg.h/.cpp` с `QDialog`, без `Q_OBJECT` (или с moc).
4. `ShowModal(HWND)` + глобальный указатель + `Shutdown()` на unload.
5. Команда `ACRX_CMD_MODAL`.
6. Не использовать `QWinWidget`, пока не разберётесь с lifetime.
7. Собрать Release NCAD | x64 → `APPLOAD` → проверить `APPUNLOAD` с закрытым окном.

Дальше: данные в окно → [03_PODKLYUCHENIE_BD.md](03_PODKLYUCHENIE_BD.md).
