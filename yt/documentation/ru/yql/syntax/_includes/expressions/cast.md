---
vcsPath: ydb/docs/ru/core/yql/reference/yql-core/syntax/_includes/expressions/cast.md
sourcePath: ydb/docs/ru/core/yql/reference/yql-core/syntax/_includes/expressions/cast.md
---
## CAST {#cast}

Пробует привести значение к указанному типу. Попытка может оказаться неуспешной и вернуть `NULL`. Для чисел может потерять точность или старшие биты.
Для списков и словарей может либо удалить, либо заменить на `NULL` элементы, преобразование которых неудачно.
Для структур и кортежей удаляет элементы, которых нет в целевом типе.
Подробнее правила преобразований описаны [здесь](../../../types/cast.md).

{% include [decimal_args](../../../_includes/decimal_args.md) %}

**Примеры**

{% include [cast_examples](../../../_includes/cast_examples.md) %}
