# Динамические таблицы

Динамические таблицы — это вид таблиц в {{product-name}}, реализующих интерфейс точечного чтения и записи данных по ключу. Они поддерживают транзакции и собственный диалект SQL.

Наличие динамических таблиц добавляет системе {{product-name}} возможности типичного key-value storage.
Элементарными действиями с такими таблицами являются запись, удаление и чтение строки по ключу. Также возможно выполнение SQL-запросов — для работы с диапазонами строк.

Динамические таблицы поддерживают транзакционность по модели [MVCC](https://en.wikipedia.org/wiki/Multiversion_concurrency_control). Работа с ними не требует общения с мастер-сервером {{product-name}}, что дает дополнительные возможности для масштабирования.

Ближайшими аналогами динамических таблиц можно назвать:

* [Apache HBase](http://hbase.apache.org) — с точки зрения организации хранения данных;
* [Spanner](https://ai.google/research/pubs/pub39966) — с точки зрения реализации транзакций;
* [Impala](http://impala.apache.org) — с точки зрения модели вычисления запросов.

Сходства с перечисленными системами будут описаны далее.

## Виды динамических таблиц { #types }

Динамические таблицы бывают [сортированные](../../../user-guide/dynamic-tables/sorted-dynamic-tables.md) и [упорядоченные](../../../user-guide/dynamic-tables/ordered-dynamic-tables.md). Помимо этого, для каждого вида таблиц реализован механизм репликации. Подробнее о репликации в разделе [реплицированные динамические таблицы](../../../user-guide/dynamic-tables/replicated-dynamic-tables.md).

## Архитектура { #architecture }

Архитектурно способ хранения динамических таблиц в {{product-name}} использует идею [Log-structured merge tree](https://en.wikipedia.org/wiki/Log-structured_merge-tree). Это делает ее похожей на системы [BigTable](http://static.googleusercontent.com/media/research.google.com/en//archive/bigtable-osdi06.pdf) и [HBase](https://hbase.apache.org/). Для обеспечения отказоустойчивости и консистентности используется библиотека репликации состояний `Hydra` (аналог [Raft](https://raft.github.io/raft.pdf)), а для поддержки распределенных транзакций — [двухфазный коммит](https://en.wikipedia.org/wiki/Two-phase_commit_protocol), аналогично системе [Google Spanner](https://research.google/pubs/pub39966/).

Более подробное описание архитектуры и модели данных приведено в отдельных разделах, посвящённых [сортированным](../../../user-guide/dynamic-tables/sorted-dynamic-tables.md) и [упорядоченным](../../../user-guide/dynamic-tables/ordered-dynamic-tables.md) динамическим таблицам.


## Атрибуты { #attributes }

Помимо атрибутов, присущих всем таблицам, динамические таблицы имеют ряд уникальных атрибутов, некоторые из которых представлены в таблице:

| **Имя**    | **Тип**  | **Описание**  |
| ---------- | -------- | ------------- |
| chunk_row_count | Int   | Общее количество необъединённых строк во всех чанках таблицы. |
| tablets[*](**)   | TabletInfo | Описание таблетов.   |
| atomicity   | Atomicity | [Режим атомарности](../../../user-guide/dynamic-tables/transactions.md#atomicity). По умолчанию `full`.  |
| in_memory_mode     | InMemoryMode | [In-memory-режим](../../../user-guide/dynamic-tables/sorted-dynamic-tables#in_memory) для таблиц. По умолчанию `none`.   |

Атрибут `chunk_row_count` может использоваться для оценки размера таблицы, но оценка является приближенной по следующим причинам:
- в оценке не учтены строки, находящиеся в памяти узлов кластера;
- строки могут быть учтены несколько раз, пока не произошло объединение чанков;
- строки могут быть не удалены, но помечены как удаленные или устаревшие.

Отдельные атрибуты динамической таблицы влияют на её поведение. В частности, при помощью отдельных атрибутов можно:
- [настраивать фоновую компактификацию](../../../user-guide/dynamic-tables/compaction.md#attributes);
- [устанавливать TTL](../../../user-guide/dynamic-tables/sorted-dynamic-tables.md#remove_old_data) — время жизни отдельных значений;
- [управлять автоматическим шардированием](../../../user-guide/dynamic-tables/resharding.md#auto).

## Ограничения  { #limitations }

На строки и схему динамической таблицы наложен ряд ограничений по размеру и типу содержимого:

- количество колонок в динамической таблице не может быть больше 1024, из них не более 32 могут быть ключевыми;
- максимальная длина значения типа `String` в динамической таблице — 16 мегабайт.

Кроме того, динамические таблицы наследуют [ограничения](../../../user-guide/storage/static-tables.md#limitations) статических таблиц.


## Таблицы, находящиеся в памяти (in-memory) { #in_memory }

Доступ к данным, находящимся в оперативной памяти, на порядки быстрее, чем к данным на диске. В системе {{product-name}} существует возможность настроить таблицу так, чтобы её данные постоянно находились в памяти. Подобная настройка радикально повышает скорость операций с таблицей. У такого способа имеются ограничения:

- Объем оперативной памяти на всех узлах кластера ограничен, поэтому рассматриваемой возможностью стоит пользоваться только тогда, когда это действительно необходимо.

- Любая динамическая сортированная таблица может быть переведена в in-memory режим, а также выведена из данного режима. Для вступления изменений в силу необходимо выполнять `remount_table`.

- Использование динамических таблиц, находящихся в памяти, никак не отражается на уровнях изоляции, консистентности, а также отказоустойчивости. Данные таких таблиц в том числе по-прежнему хранятся в чанках распределенного хранилища, in-memory режим лишь означает, что помимо диска данные таблицы также находятся в оперативной памяти.

- При включении in-memory режима происходит загрузка данных в память узлов кластера. Это может занимать некоторое время.

In-memory режим настраивается с помощью атрибута `in_memory_mode` для таблицы. Возможные значения атрибута:

| **Режим** | **In-memory режим** | **Описание** |
| --------- | ------------------- | ------------ |
| None | Отключен. | Данные таблицы записываются в чанки, а их блоки, сжатые и несжатые, кешируются на общих основаниях. Такой режим подходит на больших таблиц, которые нет возможности зафиксировать в памяти. |
| Compressed | Включен. | Cжатые данные таблицы — содержимое блоков чанков — постоянно присутствует в памяти, но для обращений на чтение необходимо выполнять распаковку блоков данных. Распакованные блоки кешируются на общих основаниях. Такой режим является хорошим сочетанием по соотношению скорость-объём памяти. |
| Uncompressed | Включен. | Несжатые данные таблицы постоянно присутствует в памяти, для обращений на чтение нет необходимости ни в дисковых обращениях, ни в распаковке данных. Данный режим дает максимальную производительность ценой оперативной памяти. |


### Хеш-таблицы для чтения по ключу

Операция чтения по ключу ищет ключи данных таблицы с помощью двоичного поиска. Можно добиться некоторого ускорения, построив предварительно хеш-таблицы для ключей. Для этого необходимо задать атрибут `enable_lookup_hash_table = %true` для таблицы.

Хеш-таблицы имеет смысл использовать только в режиме `uncompressed`, в остальных режимах их использование запрещено.

## Форматы хранения таблиц { #formats }

В системе {{product-name}} реализовано несколько способов физического хранения данных таблиц в чанках. Выбор между ними управляется атрибутом `optimize_for` на таблице. Данный атрибут можно изменить на уже существующей таблице, но эффект будет заметен лишь для вновь создаваемых чанков. С учётом компактификации, старые данные тоже могут быть повторно сжаты в фоновом режиме.

Если таблица уже смонтирована, то для донесения до таблетов информации про изменение формата необходимо выполнить команду `remount_table`.

Возможные значения атрибута `optimize_for`:

- `lookup` — формат, который даёт возможность эффективно читать из таблицы строки по отдельным ключам;
- `scan` — формат `scan` использует поколоночное хранение, а также адаптивные поколоночные алгоритмы сжатия. Вертикальный способ разделения данных хуже подходит для случайных чтений по ключам, так как потребуется читать больше блоков и тратить больше CPU.

## Права доступа { #acl }

На динамические таблицы распространяется система управления доступом:

- Чтобы читать из динамической таблицы (запросами `select_rows`, `lookup_rows`), пользователь должен иметь право `read` на таблицу.
- Чтобы писать в динамическую таблицу (командами `insert_rows`, `delete_rows`), пользователь должен иметь право `write` на таблицу.
- Чтобы выполнять команды `mount_table`, `unmount_table`, `remount_table` и `reshard_table`, пользователь должен иметь право `mount`на таблицу.
- По аналогии с аккаунтами для того чтобы создавать таблицы, привязанные к бандлу, у пользователя должно быть право `use` на данный бандл. То же правило требуется для того, чтобы изменить бандл у таблицы.

[**]: Параметр встречается в ответе несколько раз.
