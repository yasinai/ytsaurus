---
vcsPath: ydb/docs/ru/core/yql/reference/yql-core/syntax/_includes/select/folder.md
sourcePath: ydb/docs/ru/core/yql/reference/yql-core/syntax/_includes/select/folder.md
---

## Перечисление содержимого директории на кластере {#folder}

Указывается как функция `FOLDER` в [FROM](#from).

Аргументы:

1. Путь к директории;
2. Опционально строка со списком интересующих мета атрибутов через точку с запятой.

В результате получается таблица с тремя фиксированными колонками:

1. **Path** (`String`) — полное имя таблицы;
2. **Type** (`String`) — тип узла (table, map_node, file, document и пр.);
3. **Attributes** (`Yson`) — Yson-словарь с заказанными во втором аргументе мета атрибутами.

Рекомендации по использованию:

* Чтобы получить только список таблиц, нужно не забывать добавлять `...WHERE Type == "table"`. Затем, опционально добавив ещё условий, с помощью агрегатной функции `AGGREGATE_LIST` от колонки Path можно получить только список путей и передать их в [EACH](#each).
* Так как колонка Path выдаётся в том же формате, что и результат функции `TablePath`(), то их можно использоваться для JOIN атрибутов таблицы к её строкам.
* C колонкой Attributes рекомендуется работать через [Yson UDF](../../../udf/list/yson.md).

{% note warning "Внимание" %}

Следует с осторожностью использовать FOLDER с атрибутами, содержащими большие значения (`schema` может быть одним из таких). Запрос с  FOLDER на папке с большим число таблиц и тяжелым атрибутом может создать большую нагрузку на мастер {{product-name}}.

{% endnote %}

**Примеры:**

``` yql
USE hahn;

$table_paths = (
    SELECT AGGREGATE_LIST(Path)
    FROM FOLDER("my_folder", "schema;row_count")
    WHERE
        Type = "table" AND
        Yson::GetLength(Attributes.schema) > 0 AND
        Yson::LookupInt64(Attributes, "row_count") > 0
);

SELECT COUNT(*) FROM EACH($table_paths);
```