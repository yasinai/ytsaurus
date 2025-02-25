---
vcsPath: ydb/docs/ru/core/yql/reference/yql-core/syntax/_includes/group_by/distinct.md
sourcePath: ydb/docs/ru/core/yql/reference/yql-core/syntax/_includes/group_by/distinct.md
---
## DISTINCT {#distinct}

Применение агрегатных функций только к уникальным значениям столбца.

{% note info %}

Применение `DISTINCT` к вычислимым значениям на данный момент не реализовано. С этой целью можно использовать [подзапрос](../../select.md#from) или выражение `GROUP BY ... AS ...`.

{% endnote %}

**Пример**

```sql
SELECT
  key,
  COUNT(DISTINCT value) AS count -- топ-3 ключей по количеству уникальных значений
FROM my_table
GROUP BY key
ORDER BY count DESC
LIMIT 3;
```

Также ключевое слово `DISTINCT` может использоваться для выборки уникальных строк через [`SELECT DISTINCT`](../../select.md#distinct).

