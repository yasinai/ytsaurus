---
vcsPath: ydb/docs/ru/core/yql/reference/yql-core/builtins/_includes/basic/staticmap.md
sourcePath: ydb/docs/ru/core/yql/reference/yql-core/builtins/_includes/basic/staticmap.md
---
## StaticMap

Transforms a structure or tuple by applying a lambda function to each item.

**Signature**
```
StaticMap(Struct<...>, lambda)->Struct<...>
StaticMap(Tuple<...>, lambda)->Tuple<...>
```

Arguments:

* Structure or tuple.
* Lambda for processing items.

Result: a structure or tuple with the same number and naming of items as in the first argument, and with item data types determined by lambda results.

**Examples:**
```yql
SELECT *
FROM (
    SELECT
        StaticMap(TableRow(), ($item) -> {
            return CAST($item AS String);
        })
    FROM my_table
) FLATTEN COLUMNS; -- преобразование всех колонок в строки
```

