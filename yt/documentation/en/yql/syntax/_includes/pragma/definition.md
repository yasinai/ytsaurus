---
vcsPath: ydb/docs/ru/core/yql/reference/yql-core/syntax/_includes/pragma/definition.md
sourcePath: ydb/docs/ru/core/yql/reference/yql-core/syntax/_includes/pragma/definition.md
---
## Definition

Redefinition of settings.

**Syntax**

`PRAGMA x.y = "z";` или `PRAGMA x.y("z", "z2", "z3");`:

* `x`: (optional) The category of the setting.
* `y`: The name of the setting.
* `z`: (optional for flags) The value of the setting. The following suffixes are acceptable:
   * `Kb`, `Mb`, `Gb`: For the data amounts.
   * `sec`,`min`, `h`, `d`: For the time values.

**Examples**

```yql
PRAGMA AutoCommit;
```

```yql
PRAGMA TablePathPrefix = "home/yql";
```

```yql
PRAGMA Warning("disable", "1101");
```

With some exceptions, you can return the settings values to their default states using `PRAGMA my_pragma = default;`.

For the complete list of available settings, [see the table below](#pragmas).

### Scope {#pragmascope}

Unless otherwise specified, a pragma affects all the subsequent expressions up to the end of the module where it's used.
If necessary and logically possible, you can change the value of this setting several times within a given query to make it different at different execution steps.
There are also special scoped pragmas with the scope defined by the same rules as the scope of [named expressions](../../expressions.md#named-nodes).
Unlike scoped pragmas, regular pragmas can only be used in the global scope of visibility (not inside lambda functions, ACTION, SUBQUERY, etc.).