# 04. Фильтрация и поиск параметров

Сердце `MS_DB_FIND`: метод

```cpp
HRESULT IDataBase::GetLibraryObjects(LPCTSTR pszFilter, ILibObjectInfoArr** ppRes);
```

`pszFilter` — **не полный SQL**, а **фрагмент**, который CadLib подставляет к выборке из `Objects O`.

---

## 1. Формат фильтра CadLib (как в логах)

Из `log_cadlib.txt` типичная форма:

```text
, Parameters_STR PT0
  where ((O.idObjectCategory = 1))
    AND (PT0.idObject = O.idObject)
    AND (PT0.idParamDef = 1)
    AND (PT0.Value='Информация проекта')
    AND (O.idParentObject is null)
```

Смысл:

1. Фильтр **начинается с запятой** — это дополнительные таблицы в `FROM`.
2. Дальше `where …` с условиями на `O` и `PTn`.
3. `idParamDef` — **числовой** id из `ParamDefs` (не обязательно join).
4. Корневые объекты: `O.idParentObject is null`.

### Что **нельзя** делать

- Синтаксис MSSQL (`[brackets]`, `N'...'`) на PostgreSQL → часто `objects=0`.
- Всегда `Parameters_STR`, даже для double → **0 строк**.
- Fallback «взять всю категорию 93k и фильтровать в памяти» → nanoCAD «синеет» и падает.

---

## 2. Как строит фильтр этот проект

Код: `DbFindQt.cpp` → `BuildFilter`.

Для каждого условия с непустым системным именем параметра:

```text
, Parameters_DBL PT0
where (O.idObjectCategory = 69)
  AND (O.idParentObject is null)
  AND (PT0.idObject = O.idObject)
  AND (PT0.idParamDef = (select idParamDef from ParamDefs where Name = 'PART_PIPE_DIAMETER'))
  AND (PT0.Value = 159)
```

Два условия → `PT0` и `PT1` (и при необходимости разные таблицы):

```text
, Parameters_DBL PT0, Parameters_DBL PT1
where …
  AND (PT0… PART_PIPE_DIAMETER … Value = 159)
  AND (PT1… PIPE_THICKNESS … Value = 5)
  AND (O.Name like '%Отвод%')   -- опционально
```

### Выбор таблицы по типу ParamDef

```cpp
LPCTSTR ParamTableName(IParamDef::EType t)
{
    switch (t) {
    case ptDBInt: case ptDBListInt:       return L"Parameters_INT";
    case ptDBDouble: case ptDBListDbl:    return L"Parameters_DBL";
    default:                              return L"Parameters_STR";
    }
}
```

Тип берётся один раз при `GetParamDefs` и кладётся в `m_typeBySys[sysName]`.

### Числа vs строки

| Тип значения | Условие |
|--------------|---------|
| Число (парсится `wcstod` целиком) | `PTn.Value = 159` или `= 5.6` (**точное**) |
| Строка / список | `PTn.Value like '%подстрока%'` |

Примеры реальных id (БД автора, для ориентира):

| Name | idParamDef | Таблица | Пример |
|------|------------|--------|--------|
| `PART_PIPE_CLASS` | 1617 | STR | `Отвод` |
| `PART_PIPE_DIAMETER` | 1618 | DBL | `159` |
| `PIPE_THICKNESS` | 1849 | DBL | `5` |
| `PART_PIPE_DN` | 1621 | DBL | `150` |

Проверка в psql:

```sql
SELECT count(*) FROM Objects O, Parameters_DBL PT0, Parameters_DBL PT1
WHERE O.idObjectCategory = 69 AND O.idParentObject IS NULL
  AND PT0.idObject = O.idObject AND PT0.idParamDef = 1618 AND PT0.Value = 159
  AND PT1.idObject = O.idObject AND PT1.idParamDef = 1849 AND PT1.Value = 5;
-- → сотни отводов 159x5
```

---

## 3. Почему раньше было «криво и долго»

Лог пользователя:

```text
filter=..., Parameters_STR PT0, ParamDefs PD0, ... PD0.Name='PART_PIPE_DIAMETER' ... like '%60%'
QT_FIND fallback ... count=93540
matched=0
```

Цепочка:

1. Диаметр лежит в **DBL**, фильтр шёл в **STR** → 0 объектов.
2. Код делал fallback: все `pipeItems` (93 540) + `GetObjectParameters` в цикле.
3. UI nanoCAD зависал (синий экран) / падал.

**Исправление:** правильная таблица + **без** полного fallback. Если фильтр вернул 0 — значит условий нет в БД (например, нет диаметра ровно `60`, есть `60.3` — введите `60.3`).

---

## 4. Алгоритм RunSearch (упрощённо)

```text
1. CollectCriteria() из таблицы условий Qt
2. BuildFilter(cat, name, criteria)
3. GetLibraryObjects(filter)     ← вся фильтрация на стороне SQL
4. Для первых N результатов (лимит показа):
      GetObjectParameters(id) → колонки Тип/Класс/Диаметр/Толщина
5. Клик по строке → все параметры объекта в QTextEdit
```

Лимит показа (`kMaxShow = 200`) — только для UI, не для SQL.

---

## 5. Мультиусловия (AND)

Каждое условие — отдельный alias `PTn` и своя таблица.  
Логика CadLib = **AND** (объект должен удовлетворять всем join’ам).

Пример UI:

| Параметр | Сист. имя | Значение |
|----------|-----------|----------|
| Класс | `PART_PIPE_CLASS` | `Отвод` |
| Наружный диаметр | `PART_PIPE_DIAMETER` | `159` |
| Толщина | `PIPE_THICKNESS` | `5` |

+ опционально имя объекта: `Отвод`.

---

## 6. Категории параметров в UI

Это **не** `pipeItems`, а категории ParamDef:

```cpp
pDef->GetCategoriesCount();
pDef->GetCategory(i);   // «Основные параметры», «Освещение», …
```

Один параметр может попасть в несколько категорий — в дереве он дублируется (удобно искать глазами).

---

## 7. Сохранение критериев

`UserSettings` → `%APPDATA%\TestNRX\user_settings.ini`:

```ini
[User]
LastUser=postgres
LastNameFilter=Отвод

[Criteria]
Count=2
Sys0=PART_PIPE_DIAMETER
Val0=159
Sys1=PIPE_THICKNESS
Val1=5
```

При следующем `MS_DB_FIND` условия восстанавливаются.

---

## 8. Как отладить свой фильтр

1. В консоли nanoCAD смотрите строки `[Test] QT_FIND … filter=…`.
2. Скопируйте фрагмент и оберните в SQL:

```sql
SELECT count(*) FROM Objects O /* + ваш filter без слова SELECT */;
```

3. Проверьте, в какой таблице значение:

```sql
SELECT pd.name, pd.idtype,
       COALESCE(ps.value, pi.value::text, pdb.value::text)
FROM objects o
LEFT JOIN parameters_str ps ON …
LEFT JOIN parameters_int pi ON …
LEFT JOIN parameters_dbl pdb ON …
WHERE o.idobject = <id>;
```

4. Убедитесь, что для DBL в фильтре **нет** `like '%…%'` без `cast` — CadLib/PG ждут число.

---

## 9. Чеклист быстрого поиска

- [ ] Категория объекта верная (`pipeItems` / нужный id)
- [ ] Системные имена параметров (`PART_PIPE_DIAMETER`, не русская подпись)
- [ ] Таблица STR/INT/DBL совпадает с `GetType()`
- [ ] Числа — точные (`159`, не «примерно 60»)
- [ ] Нет fallback на всю категорию
- [ ] Индексы CadLib: `ByObject_*`, `IDX_PARAM_*_VAL` уже есть в схеме

Готово — можно собирать свои сценарии поиска поверх того же `BuildFilter`.
