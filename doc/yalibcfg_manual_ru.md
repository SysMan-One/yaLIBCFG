# Руководство программиста yaLIBCFG

**Деревья конфигураций в синтаксисе libconfig**

Версия программы: yaLIBCFG X.01-03

StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)

Ноябрь 2026

---

## Предисловие

### Для кого это руководство

Руководство написано для программистов, которые пишут на Си программы, хранящие свои настройки в файле. Вы должны знать Си. Знать libconfig не нужно, знать OpenVMS тоже не нужно, но если вы их знаете, многое здесь покажется знакомым.

### Устройство руководства

В руководстве три части и четыре приложения.

* **Часть I, Основные понятия и учебник**, рассказывает, для чего нужна библиотека, как пишется файл конфигурации и как программа читает и меняет его. Читайте её с начала.
* **Часть II, Справочник**, описывает каждую подпрограмму под одними и теми же заголовками, в одном порядке: Формат, Возвращает, Аргументы, Описание, Возвращаемые коды условий, Пример. Пользуйтесь ею, когда пишете код.
* **Часть III, Утилита CFGTOOL**, описывает утилиту библиотеки для командной строки.
* **Приложение A** перечисляет сообщения: что значит каждое и что делать.
* **Приложение B** перечисляет ограничения библиотеки.
* **Приложение C** перечисляет отличия от libconfig.
* **Приложение D** точно определяет отпечаток.

### Условные обозначения

| Обозначение | Значение |
|---|---|
| `cfg$parse_file` | Имя в исходном тексте на Си: подпрограмма, тип, константа. |
| CFG$PARSE_FILE | Та же подпрограмма, названная в тексте по обычаю OpenVMS. |
| `$ command` | Команда, которую вы набираете в оболочке. |
| CFG$_NOTFOUND | Код условия: подсистема CFG, сообщение NOTFOUND. |
| `...` | В примере что-то опущено. |

Все примеры этого руководства лежат в каталоге `examples` комплекта. Они собираются и запускаются вместе с тестами, поэтому код, который вы здесь читаете, рабочий.

---

# Часть I. Основные понятия и учебник

## 1 Введение

### 1.1 Что такое yaLIBCFG

yaLIBCFG - это библиотека на Си. Она читает файлы конфигурации в **дерево**, позволяет программе искать и менять настройки в дереве и записывает дерево обратно в файл.

Файлы пишутся в **синтаксисе libconfig**: любой файл, который читает libconfig, yaLIBCFG читает в то же самое дерево (раздел 2.2 говорит о немногих числах, которые libconfig теряет, а yaLIBCFG сохраняет или отвергает). Обратного обещания нет: yaLIBCFG принимает и собственные формы - радикс DCL `%X1F`, `0b`, `0o`, комментарий `!` - и записывает их обратно так, как они были написаны.

### 1.2 Почему не сам libconfig

libconfig - хорошая библиотека, но у программы, которая и читает свою конфигурацию, **и пишет** её, с ним возникают две беды:

1. **libconfig теряет всё, что не является значением.** Он забывает комментарии, пустые строки, ход мысли автора. Когда программа сохраняет файл, заметки, оставленные человеком, пропадают.
2. **libconfig не умеет сказать, что изменилось.** Программе, которая применяет конфигурацию к работающему оборудованию, нужно знать, чем старый файл отличается от нового.

yaLIBCFG хранит текст. Каждый узел дерева помнит, из какого места файла он пришёл. Когда программа записывает дерево обратно:

* настройка, которую никто не трогал, копируется как была, вместе с комментариями;
* изменённое значение получает новый текст, а имя, `=`, `;` и комментарий остаются;
* удалённая настройка уходит вместе со строками комментария, которые её описывали;
* новая настройка ставится после соседних, с их отступом и с тем концом строки, который принят в файле (LF или CR LF).

Файл, прочитанный и записанный без изменений, **совпадает с исходным байт в байт**. Тесты комплекта проверяют это при каждой сборке.

yaLIBCFG также **сравнивает две конфигурации** - настройки, а не текст - и печатает различия листингом в духе утилиты DIFFERENCES из OpenVMS. Каждая конфигурация получает **отпечаток**, то есть UUID, построенный из её настроек, чтобы по листингу было видно, чьи конфигурации сравнивались.

И ещё она **проверяет** файл **по схеме** - таблице в программе, которая говорит, какие бывают настройки, какого они типа и в каких пределах, - и сообщает пользователю обо всём, что в файле неверно, за один раз, каждую ошибку со строкой и столбцом.

### 1.3 Первый взгляд

Вот файл:

```
# router.cfg - the sample file of the yaLIBCFG manual

# Who we are
system = {
	hostname = "edge-1";		# shown in the prompt
	location = "Lab, rack 3";
};

# The ports of the switch
ports = (
	# management
	{ id = 1;  speed = "1G";  enabled = true; },

	# uplinks
	{ id = 9;  speed = "10G"; enabled = true;  mtu = 9216; },
	{ id = 10; speed = "10G"; enabled = false; mtu = 9216; }
);

vlans = [ 1, 100, 200 ];
```

А вот программа, которая читает из него два значения (`examples/ex1_read.c`):

```c
#include <stdio.h>
#include "yalibcfg.h"

int	main (int argc, char **argv)
{
	CFG$TREE	*tree;
	CFG$NODE	*root;
	const char	*name;
	uint32_t	len;
	int32_t		mtu;
	unsigned	sts;

	cfg$inimsg();					/* the CFG message texts		*/

	sts = cfg$parse_file(argc > 1 ? argv[1] : "router.cfg", 0, &tree);

	if ( !(sts & 1) )				/* failure: the low bit is clear	*/
		{
		cfg$puterrs(tree);			/* %CFG-E-..., file, line, column	*/
		cfg$free(tree);
		return 1;
		}

	cfg$root(tree, &root);

	if ( cfg$get_string(root, "system.hostname", &name, &len) & 1 )
		printf("hostname: %.*s\n", (int) len, name);

	if ( cfg$get_int(root, "ports.[id=9].mtu", &mtu) & 1 )
		printf("port 9 MTU: %d\n", mtu);

	cfg$free(tree);
	return 0;
}
```

```
$ ex1_read router.cfg
hostname: edge-1
port 9 MTU: 9216
```

Обратите внимание на путь `ports.[id=9].mtu`: "в списке PORTS элемент, у которого ID равен 9, и его настройка MTU". О путях подробно рассказано в разделе 3.3.

### 1.4 Сборка и компоновка

yaLIBCFG нужна библиотека StarLet (`libstarlet.a` и её заголовочные файлы): сообщения и механизм FAO берутся оттуда.

```
$ cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/starlet
$ cmake --build build
$ (cd build && ctest)
$ cmake --install build --prefix /opt/yalibcfg
```

Комплект устанавливает `lib/libyalibcfg.a`, `include/yalibcfg.h` и `include/yalibcfgdef.h`. Программа включает `yalibcfg.h` и компонуется с `-lyalibcfg -lstarlet -lpthread -lm`.

---

## 2 Файл конфигурации

### 2.1 Настройки

Файл - это последовательность **настроек**. Настройка состоит из имени, знака `=` (или `:`), значения и необязательного `;` (или `,`):

```
hostname = "edge-1";
mtu      : 9216
```

**Имя** начинается с буквы или `*` и продолжается буквами, цифрами и знаками `-`, `_` и `*`. Регистр в именах различается, если программа не потребует иного (CFG$M_NOCASE, см. CFG$PARSE_FILE). В одной группе имя может встретиться только один раз.

### 2.2 Значения

| Вид | Примеры | Замечания |
|---|---|---|
| Целое | `42`, `-17`, `0x1F`, `0b1011`, `0o755`, `0755` | 32 бита. Ведущий `0` задаёт восьмеричное число, как в libconfig: `0027` - это 23, `09` - ошибка. |
| Целое, система счисления DCL | `%X1F`, `%D27`, `%B1011`, `%O755`, `-%X10` | Шестнадцатеричное, десятичное, двоичное, восьмеричное; буква в любом регистре. `%D0027` - это 27. |
| 64-битное целое | `9223372036854775807L`, `0xFFFFFFFFFFL`, `5000000000` | Суффикс `L` (или `LL`), либо десятичное число, не умещающееся в 32 бита. |
| Вещественное | `0.75`, `1e-3`, `-2.5E+10`, `.5` | Тип double. Записи для бесконечности и NaN нет. |
| Логическое | `true`, `false`, `TRUE`, `False` | В любом регистре. |
| Строка | `"text"`, `"a\tb"`, `"one " "two"` | Escape-последовательности `\\`, `\"`, `\n`, `\r`, `\t`, `\f`, `\xHH`. Строки, стоящие рядом, склеиваются. |
| Группа | `{ w = 640; h = 480; }` | Именованные настройки. |
| Массив | `[ 1, 100, 200 ]` | Скаляры одного типа (32- и 64-битные целые считаются одним типом). |
| Список | `( "abc", 123, { x = 1; } )` | Значения любого вида, в том числе группы и списки. |

Шестнадцатеричное, двоичное или восьмеричное число без суффикса `L`, которое умещается в 32 бита как беззнаковое, - это 32-битное целое со знаковым битом: `0xFFFFFFFF` и `%XFFFFFFFF` равны -1. Так делает и libconfig.

**Файлы libconfig.** Любой файл, который читает libconfig (1.6 и новее), yaLIBCFG читает так же - с двумя его странными правилами для ведущего `0`: с суффиксом `L` число десятичное (`0027L` - это 27), а восьмеричное число за пределами 32 бит - 64-битное без знакового бита (`037777777777` - это 4294967295). Там, где libconfig молча теряет число, yaLIBCFG за ним не следует:

| Текст | libconfig | yaLIBCFG |
|---|---|---|
| `0x100000000` (за пределами 32 бит, без `L`) | обрезается до 32 бит: 0 | 64-битное 4294967296 |
| `0x10000000000000000` (за пределами 64 бит) | -1 | BADNUM |
| `9223372036854775808L` (за пределами 64 бит) | 9223372036854775807 | BADNUM |
| `1e400` | бесконечность | BADNUM |
| `.e5` | 0.0 | BADNUM |

Обратного обещания нет: `0b`, `0o`, радикс `%` и комментарий `!` - собственные средства yaLIBCFG, libconfig их отвергает.

Число, заданное программой, сохраняет вид своей записи: `%X1F`, которому присвоили 255, записывается как `%XFF`, а `0027`, которому присвоили 8, - как `010`.

### 2.3 Комментарии

```
# to the end of the line
// to the end of the line
! to the end of the line, as in DCL
/* anywhere,
   over lines */
```

**Комментарии принадлежат настройкам.** Строки комментария прямо над настройкой (без пустой строки между ними) и комментарий на той же строке после неё принадлежат этой настройке. Это важно, когда программа удаляет настройку: её комментарии уходят вместе с ней.

```
# this comment belongs to BETA - it goes when BETA goes
beta = 2;	# and so does this one

# this one is ALONE - the blank line below cuts it off

gamma = 3;
```

### 2.4 Включаемые файлы

```
@include "ports.cfg"
```

Директива стоит там, где может стоять настройка: в начале файла или внутри группы. Настройки включённого файла становятся настройками этой группы. Относительное имя отсчитывается от каталога того файла, в котором стоит директива.

Включённый файл должен содержать **целые настройки**. (libconfig вставляет включённый файл как текст, поэтому половина группы там работает; здесь - нет.) Файл можно включить в дерево **только один раз**: если бы он стоял в двух местах, изменение в одном из них нельзя было бы записать, не потеряв другое (CFG$_INCTWICE).

Когда программа меняет настройку, пришедшую из включённого файла, CFG$WRITE записывает изменение в **этот** файл; строка `@include` главного файла остаётся как была.

---

## 3 Основные понятия программирования

### 3.1 Коды условий

Каждая подпрограмма возвращает **код условия** - число типа `unsigned` в формате OpenVMS:

```
 31            16 15                 3 2   0
+----------------+--------------------+-----+
|   facility     |   message number   | sev |
+----------------+--------------------+-----+
```

Подсистема yaLIBCFG называется CFG, её номер 0x0114. **Младший бит** отличает успех от неудачи: он установлен - успех (SUCCESS, INFO), сброшен - неудача (WARNING, ERROR, FATAL). Поэтому проверка всегда такая:

```c
if ( !(sts & 1) )
	... failure ...
```

Никогда не сравнивайте с `CFG$_NORMAL` через `==`: подпрограмма может вернуть и `STS$K_SUCCESS`. С конкретным кодом сравнивайте только тогда, когда ищете именно его, как здесь:

```c
sts = cfg$get_int(root, "system.snmp.port", &port);

if ( sts == CFG$_NOTFOUND )
	port = 161;			/* not in the file - the default		*/
else if ( !(sts & 1) )
	return sts;			/* there, but not an integer: an error		*/
```

Результаты подпрограммы возвращаются через её **выходные аргументы**, а не через значение функции.

### 3.2 Деревья и узлы

CFG$PARSE_FILE строит **дерево**: `CFG$TREE`. Дереву принадлежат все его узлы и строки; один вызов CFG$FREE освобождает их все.

**Узел** (`CFG$NODE`) - это настройка или элемент. **Корневой** узел - безымянная группа: сам файл. Оба типа непрозрачны: внутрь вы не заглядываете, а вызываете подпрограммы.

```
CFG$TREE ── root (group) ──┬── system (group) ──┬── hostname "edge-1"
                           │                    └── location "Lab, rack 3"
                           ├── ports (list) ────┬── [0] (group) ── id, speed, enabled
                           │                    ├── [1] (group) ── id, speed, enabled, mtu
                           │                    └── [2] (group) ── id, speed, enabled, mtu
                           └── vlans (array) ── [0] 1, [1] 100, [2] 200
```

Указатель на узел действителен до вызова CFG$FREE для его дерева. Удалённый узел не освобождается, но пользоваться им больше не нужно.

Дерево принадлежит **одному потоку в каждый момент**. Разные деревья независимы, и разные потоки могут работать с ними одновременно.

### 3.3 Пути

Большинство подпрограмм принимают **базовый узел** и **путь** от него:

| Путь | Куда ведёт |
|---|---|
| `system.hostname` | к настройке HOSTNAME группы SYSTEM |
| `system/hostname` | то же самое: `/` принимается за `.` |
| `vlans.[1]` | к элементу 1 (счёт с 0) массива VLANS: 100 |
| `ports.[id=9]` | к элементу списка PORTS, у которого настройка ID равна 9 |
| `ports.[id=9].mtu` | ... и к его настройке MTU |
| `ports.[speed="10G"]` | к первому элементу, у которого SPEED - строка "10G" |
| `""` или NULL | к самому базовому узлу |

Элемент, выбранный через `[n]` или `[key=value]`, ищется среди **живых** элементов: удалённые программой не считаются. Значение в `[key=value]` пишется так же, как в файле: `[id=0x1F]`, `[ratio=1.5]`, `[name="a\"b"]`; строку можно писать и без кавычек: `[name=mgmt]`. Любой путь, который выдаёт CFG$DIFF, годится для CFG$LOOKUP.

Путь `""` удобен, когда узел уже у вас в руках:

```c
CFG$NODE	*port;
int32_t		mtu;

cfg$lookup(root, "ports.[id=9]", &port);
cfg$get_int(port, "mtu", &mtu);		/* from the port			*/
cfg$set_int(port, "", 1);		/* no: PORT is a group - CFG$_TYPE	*/
```

### 3.4 Диагностика

Библиотека **сама ничего не выводит**. Когда разбор не удался, подпрограмма возвращает код условия первой ошибки, а дерево хранит запись о ней с файлом, строкой и столбцом. Показывать ли её, решает ваша программа: CFG$PUTERRS выводит запись через `$PUTMSG_FAO` из StarLet:

```
%CFG-E-SYNTAX, syntax error: expected ';' or a setting name, found integer, router.cfg line 12 column 18
```

Вызовите CFG$INIMSG один раз в начале программы, чтобы тексты сообщений были на месте.

**Пропуск плохих настроек.** По умолчанию разбор останавливается на первой ошибке, и дерево годится только для CFG$PUTERRS. Программа, которой нужно идти дальше - взять из файла то, что в нём правильно, а об остальном сказать пользователю, - просит CFG$M_SKIPERRS:

```c
sts = cfg$parse_file(path, CFG$M_SKIPERRS, &tree);

if ( sts == CFG$_SKIPPED )		/* success, with bad settings skipped		*/
	cfg$puterrs(tree);		/* every one of them, with its position		*/
else if ( !(sts & 1) )
	... the file could not be read at all ...
```

Плохая настройка пропускается целиком - её имя, значение и завершитель; внутри группы - только эта настройка группы. Она кончается на своей `;` или `,`, на скобке, закрывающей её группу, или перед именем, с которого начинается следующая строка: `b =`, потерявшая значение, не забирает с собой `c = 3;` со строки ниже. Её текст остаётся в дереве как был: поиск, сравнение, отпечатки и схемы её не видят, но CFG$WRITE записывает её обратно, так что файл с ошибкой, записанный после изменения, - тот же файл с одним этим изменением. Плохая настройка, после которой до конца файла остаётся открытой скобка, строка или комментарий, забирает с собой остаток файла (незакрытая группа пропускается целиком); настройка, добавленная на этом уровне, записывается перед ней, чтобы читаться обратно. CFG$_SKIPPED - успех (младший бит установлен); сколько настроек пропущено, говорит CFG$ERRCOUNT.

### 3.5 Запись обратно

Получить текст дерева можно тремя способами:

| Подпрограмма | Что делает |
|---|---|
| CFG$WRITE_BUF | отдаёт вам в буфере текст одного исходного файла |
| CFG$WRITE | записывает на место каждый исходный файл, текст которого изменился |
| CFG$WRITE_FILE | записывает главный файл в другой файл (копия, "Сохранить как") |

CFG$WRITE и CFG$WRITE_FILE никогда не оставляют наполовину записанный файл: они пишут временный файл в том же каталоге, сбрасывают его на диск и переименовывают поверх старого. Права доступа старого файла сохраняются.

### 3.6 Сравнение и отпечатки

CFG$DIFF сравнивает два дерева **по настройкам**. Комментарии, оформление и порядок настроек в группе не учитываются. Правила такие:

* В группе настройки сопоставляются по имени.
* В списке элементы сопоставляются по **ключу**, если он есть: это настройка, которую схема помечает флагом CFG$M_KEY (раздел 3.7), если программа передала схему; иначе - `id` (а при её отсутствии `name`, а при отсутствии и её `key`), когда каждый элемент обоих списков - группа со скалярной настройкой с таким именем, уникальной в своём списке. Такой элемент называется по ключу: `ports.[id=9]`. Иначе - или всегда, с CFG$M_POSITIONAL - по положению: `list.[2]`.
* Массив - это одно значение: `[1, 2, 3]` и `[1, 3]` различаются целиком.
* Узел, у которого изменился тип, - это изменение всего узла. 32-битное и 64-битное целое с одним значением равны.

Каждое различие бывает одного из трёх видов: CFG$K_DIFF_MOD (значение отличается), CFG$K_DIFF_ADD (есть только в новом дереве), CFG$K_DIFF_DEL (есть только в старом).

**Отпечаток** (CFG$FINGERPRINT) дерева - это UUID версии 5 (RFC 9562), построенный алгоритмом SHA-1 из настроек в каноническом виде, который, как и сравнение (без CFG$M_POSITIONAL), не видит комментариев, оформления, порядка настроек в группе и порядка элементов списка с ключом. Поэтому:

* два файла с одинаковыми настройками имеют одинаковый отпечаток, какие бы в них ни были комментарии;
* любое изменение настройки меняет отпечаток;
* отпечаток одинаков на любой машине и в любой версии yaLIBCFG (Приложение D).

Листинг называет отпечатки обоих деревьев. Храните листинги, и вы всегда сможете сказать, какая конфигурация с какой сравнивалась.

### 3.7 Схемы

**Схема** говорит, что может содержать файл. Это таблица в вашей программе, по одной записи на настройку, в духе таблиц глаголов из `cli_routines`:

```c
typedef struct cfg_schema_t {
	const char	*name;		/* the setting; "*" - any other; NULL - end of the table	*/
	uint32_t	type;		/* CFG$K_TYPE_*						*/
	uint32_t	flags;		/* CFG$M_REQUIRED, CFG$M_KEY, CFG$M_RANGE, ...		*/
	int64_t		min, max;	/* integer range, string length, number of elements	*/
	double		fmin, fmax;	/* float range						*/
	const CFG$KEYWORD *kwd;		/* the words of a CFG$K_TYPE_KWD			*/
	const struct cfg_schema_t *sub;	/* a group: its table; a list, an array: one entry for	*/
					/* the elements						*/
	cfg_check_t	check;		/* a routine of the program, or NULL			*/
	const char	*help;		/* what the setting is for				*/
} CFG$SCHEMA;
```

Кроме типов, которые бывают в файле, у схемы есть типы - строки с особым смыслом:

| Тип | Строка должна быть |
|---|---|
| CFG$K_TYPE_KWD | одним из слов KWD; регистр не важен, если не задан CFG$M_CASEKWD |
| CFG$K_TYPE_IPV4 | адресом IPv4: `"192.168.1.1"` |
| CFG$K_TYPE_IPV6 | адресом IPv6: `"2001:db8::1"` |
| CFG$K_TYPE_IPADDR | любым из них |
| CFG$K_TYPE_MAC | адресом MAC: `"00:1b:44:11:3a:b7"` или с `-` |

Запись CFG$K_TYPE_FLOAT принимает и целое (`load = 1;`); запись CFG$K_TYPE_INT принимает значение, которое помещается в 32 бита, а CFG$K_TYPE_INT64 - любое целое.

Флаги:

| Флаг | Смысл |
|---|---|
| CFG$M_REQUIRED | Настройка должна быть. |
| CFG$M_KEY | В группе, которая является элементом списка: ключ элементов - обязателен и уникален в списке. CFG$DIFF, получив схему, сопоставляет элементы по нему, и диагностика называет элемент по нему: `ports.[id=9]`. |
| CFG$M_RANGE | MIN..MAX относятся к целому, FMIN..FMAX - к вещественному. |
| CFG$M_LENGTH | MIN..MAX относятся к длине строки. |
| CFG$M_COUNT | MIN..MAX относятся к числу элементов списка или массива. |
| CFG$M_DEPRECATED | Всё ещё принимается, но с предупреждением. |
| CFG$M_OPEN | Группа: настройки, которых нет в её таблице, принимаются молча. |
| CFG$M_CASEKWD | Ключевые слова сравниваются с учётом регистра. |

Настройка, которой нет в таблице её группы, - ошибка (CFG$_UNKNOWN), поэтому опечатка в файле никогда не пройдёт молча. Имя `"*"` в таблице означает любую настройку, не названную в ней иначе; оно нужно для групп, где имена настроек - это данные: `users = { amy = {...}; bob = {...}; }`.

Элемент списка или массива описывается одной записью в SUB, общей для всех его элементов; её имя не используется.

Когда правил таблицы не хватает - например, "MTU магистрального порта должен быть больше, чем у порта доступа", - запись получает **подпрограмму проверки**. Она видит узел, только когда пройдены все остальные проверки самого узла и того, что у него внутри, и своими словами говорит, что не так:

```c
typedef unsigned (*cfg_check_t) (CFG$NODE *a_node, void *a_arg, char *a_msg, uint32_t a_msgsz, uint32_t *a_msglen);
```

---

## 4 Учебник

### 4.1 Изменение файла

`examples/ex2_edit.c` меняет одно значение, добавляет новую настройку, удаляет порт 10 и сохраняет файл:

```c
	cfg$root(tree, &root);

	/* A new value for an old setting */
	sts = cfg$set_string(root, "system.location", "Lab, rack 4", 11);

	/* A setting that was not there: made, with the group on its way */
	if ( sts & 1 )
		sts = cfg$set_int(root, "system.snmp.port", 161);

	/* Port 10 goes away, with the comment above it if it had one */
	if ( (sts & 1) && (cfg$lookup(root, "ports.[id=10]", &port) & 1) )
		sts = cfg$delete(port);

	/* Into the file, in its place (a temporary file, then a rename) */
	if ( sts & 1 )
		sts = cfg$write(tree);
```

Файл после этого - сравните его с разделом 1.3: все комментарии на месте, у новой группы SNMP такой же отступ, как у соседей, а `,` после порта 9 исчезла, потому что порт 9 теперь последний элемент:

```
# Who we are
system = {
	hostname = "edge-1";		# shown in the prompt
	location = "Lab, rack 4";
	snmp = {
		port = 161;
	};
};

# The ports of the switch
ports = (
	# management
	{ id = 1;  speed = "1G";  enabled = true; },

	# uplinks
	{ id = 9;  speed = "10G"; enabled = true;  mtu = 9216; }
);
```

### 4.2 Обход дерева

CFG$FIRST и CFG$NEXT обходят потомков узла; CFG$NODE_INFO сообщает, что это за узел (`examples/ex3_walk.c`, сокращённо):

```c
static void	walk (CFG$NODE *node, int depth)
{
	CFG$NODE	*child;
	CFG$INFO	info;
	unsigned	sts;

	for ( sts = cfg$first(node, &child); sts & 1; sts = cfg$next(child, &child) )
		{
		cfg$node_info(child, &info);

		printf("%*s%-10.*s line %3u  ", depth * 2, "",
			(int) (info.name ? info.namelen : 3), info.name ? info.name : "[ ]", info.line);

		switch ( info.type )
			{
			case CFG$K_TYPE_STRING:
				{ const char *s; uint32_t l; cfg$get_string(child, "", &s, &l); printf("\"%.*s\"\n", (int) l, s); break; }
			...
			default:			/* a group, a list or an array: go down */
				printf("... of %u\n", info.count);
				walk(child, depth + 1);
			}
		}
}
```

```
$ ex3_walk router.cfg
system     line   4  group of 2
  hostname   line   5  "edge-1"
  location   line   6  "Lab, rack 3"
ports      line  10  list of 3
  [ ]        line  12  group of 3
    id         line  12  1
...
```

### 4.3 Списки и массивы

CFG$ADD ставит новый узел в конец группы, списка или массива (`examples/ex4_list.c`):

```c
	/* Port 11: a new group at the end of the list */
	sts = cfg$lookup(root, "ports", &ports);

	if ( sts & 1 )
		sts = cfg$add(ports, NULL, CFG$K_TYPE_GROUP, &port);
	if ( sts & 1 )
		sts = cfg$set_int(port, "id", 11);
	if ( sts & 1 )
		sts = cfg$set_string(port, "speed", "10G", 3);

	/* VLAN 300 at the end of the array */
	if ( sts & 1 )
		sts = cfg$lookup(root, "vlans", &vlans);
	if ( sts & 1 )
		sts = cfg$add(vlans, NULL, CFG$K_TYPE_INT, &vlan);
	if ( sts & 1 )
		sts = cfg$set_int(vlan, "", 300);
```

```
	{ id = 10; speed = "10G"; enabled = false; mtu = 9216; },
	{
		id = 11;
		speed = "10G";
	}
);

vlans = [ 1, 100, 200, 300 ];
```

У элементов списка или массива имён нет: передавайте NULL. Массив принимает только скаляры того типа, что и его элементы: CFG$ADD строки в массив целых вернёт CFG$_TYPE.

### 4.4 Обработка ошибок

Хорошая программа говорит пользователю, что не так **и где**:

```c
sts = cfg$parse_file(path, 0, &tree);

if ( !(sts & 1) )
	{
	cfg$puterrs(tree);		/* every error the parse recorded, with its position */
	cfg$free(tree);			/* the tree is there even after a failure		*/
	return sts;
	}
```

```
%CFG-E-DUPNAME, duplicate setting mtu, router.cfg line 15 column 50
```

Неудачные CFG$SET_*, CFG$ADD или CFG$DELETE оставляют дерево как было и ничего в него не записывают: что случилось, говорит только код условия; при желании выведите его через `$PUTMSG_FAO(sts)`.

Дерево, разбор которого не удался, годится только для CFG$PUTERRS и CFG$FREE: CFG$WRITE_BUF, CFG$WRITE, CFG$WRITE_FILE, CFG$DIFF и CFG$FINGERPRINT отказывают ему с CFG$_BROKEN.

### 4.5 Сравнение двух конфигураций

`examples/ex5_diff.c` сравнивает два файла и печатает листинг:

```c
	sts = cfg$parse_file(argv[1], 0, &old);

	if ( sts & 1 )
		sts = cfg$parse_file(argv[2], 0, &new);

	if ( sts & 1 )
		sts = cfg$diff(old, new, NULL, 0, &diff);
	...
	if ( cfg$diff_listing(diff, (argc > 3) ? CFG$M_NOTIME : 0, &text, &len) & 1 )
		{
		fwrite(text, 1, len, stdout);
		free(text);
		}

	cfg$diff_count(diff, &count);

	cfg$diff_free(diff);
	cfg$free(old);
	cfg$free(new);

	return count ? 1 : 0;			/* as diff(1): 1 - they differ		*/
```

`examples/router-new.cfg` - это `router.cfg`, отредактированный вручную: у порта 9 другой MTU, порта 10 нет, порт 11 новый, добавлены один VLAN и сервер syslog, а две настройки SYSTEM поменялись местами (это не различие):

```
$ ex5_diff router.cfg router-new.cfg notime
CONFIGURATION DIFFERENCES

Old: router.cfg
     UUID a0f0b155-8348-5765-97a7-4dfb5164faba
New: router-new.cfg
     UUID 675a82eb-f1b2-5bc6-9b79-4de26c7f01c6

************
File router.cfg
    15   ports.[id=9].mtu = 9216;
******
File router-new.cfg
    11   ports.[id=9].mtu = 1500;
************
File router.cfg
    16   ports.[id=10] = { id = 10; speed = "10G"; enabled = false; mtu = 9216; };
******
File router-new.cfg
        (none)
************
File router.cfg
        (none)
******
File router-new.cfg
    12   ports.[id=11] = { id = 11; speed = "10G"; enabled = true; };
************
File router.cfg
    19   vlans = [ 1, 100, 200 ];
******
File router-new.cfg
    15   vlans = [ 1, 100, 200, 300 ];
************
File router.cfg
        (none)
******
File router-new.cfg
    16   syslog = "10.0.0.5";
************

Number of difference sections found: 5
   changed: 2, added: 2, removed: 1

CONFIGURATION DIFFERENCES /KEYS=(ID,NAME,KEY)-
    router.cfg-
    router-new.cfg
```

Номера строк - это номера строк в каждом из файлов. Без `notime` листинг несёт время своего создания и время последнего изменения обоих файлов (UTC):

```
CONFIGURATION DIFFERENCES                                                        22-NOV-2026 14:38:54.00

Old: router.cfg
     22-NOV-2026 14:31:30.00  UUID a0f0b155-8348-5765-97a7-4dfb5164faba
```

Программа, которой текст не нужен, обходит различия сама:

```c
uint32_t	n, op;
const char	*path;
CFG$NODE	*was, *now;

cfg$diff_count(diff, &n);

for ( uint32_t i = 0; i < n; i++ )
	{
	cfg$diff_entry(diff, i, &op, &path, &was, &now);

	if ( op == CFG$K_DIFF_DEL )
		... take PATH down on the equipment ...
	}
```

### 4.6 Проверка файла

`examples/ex6_schema.c` описывает `router.cfg` схемой и проверяет по ней файл:

```c
/* The words SPEED may take */
static const CFG$KEYWORD	s_speeds[] = {
	{ "1G",  1000 },
	{ "10G", 10000 },
	{ NULL,  0 }
};

/* SYSTEM = { ... } */
static const CFG$SCHEMA	s_system[] = {
	{ .name = "hostname", .type = CFG$K_TYPE_STRING, .flags = CFG$M_REQUIRED | CFG$M_LENGTH, .min = 1, .max = 32,
	  .help = "name of the router, shown in the prompt" },
	{ .name = "location", .type = CFG$K_TYPE_STRING, .help = "where it stands" },
	{ .name = "snmp",     .type = CFG$K_TYPE_GROUP,  .flags = CFG$M_OPEN, .help = "SNMP agent, not checked here" },
	{ .name = NULL }
};

/* An element of PORTS: a group, its key ID */
static const CFG$SCHEMA	s_port[] = {
	{ .name = "id",      .type = CFG$K_TYPE_INT,  .flags = CFG$M_KEY | CFG$M_RANGE, .min = 1, .max = 48 },
	{ .name = "speed",   .type = CFG$K_TYPE_KWD,  .flags = CFG$M_REQUIRED, .kwd = s_speeds },
	{ .name = "enabled", .type = CFG$K_TYPE_BOOL },
	{ .name = "mtu",     .type = CFG$K_TYPE_INT,  .flags = CFG$M_RANGE, .min = 64, .max = 9216 },
	{ .name = NULL }
};

static const CFG$SCHEMA	s_port_elem = { .type = CFG$K_TYPE_GROUP, .sub = s_port };
static const CFG$SCHEMA	s_vlan_elem = { .type = CFG$K_TYPE_INT, .flags = CFG$M_RANGE, .min = 1, .max = 4094 };

/* The file */
static const CFG$SCHEMA	s_router[] = {
	{ .name = "system", .type = CFG$K_TYPE_GROUP, .flags = CFG$M_REQUIRED, .sub = s_system },
	{ .name = "ports",  .type = CFG$K_TYPE_LIST,  .flags = CFG$M_COUNT, .min = 1, .max = 48, .sub = &s_port_elem },
	{ .name = "vlans",  .type = CFG$K_TYPE_ARRAY, .sub = &s_vlan_elem },
	{ .name = NULL }
};

```

```c
	sts = cfg$validate(tree, s_router, NULL, &nerr);

	if ( sts & 1 )
		printf("%s: valid\n", ...);
	else	{
		printf("%s: %u problem(s)\n", ..., nerr);
		cfg$puterrs(tree);			/* each with its line and column		*/
		}
```

Файл с ошибками, `examples/router-bad.cfg`:

```
# router-bad.cfg - router.cfg with mistakes, for example 6

system = {
	hostnme = "edge-1";			# a typo
};

ports = (
	{ id = 1;  speed = "100M"; enabled = true; },
	{ id = 9;  speed = "10G";  enabled = "yes"; mtu = 9216; },
	{ id = 9;  speed = "10G";  enabled = false; mtu = 65000; }
);

vlans = [ 1, 100, 5000 ];
```

Один запуск сообщает обо всём:

```
$ ex6_schema router-bad.cfg
%CFG-E-UNKNOWN, unknown setting: system.hostnme, router-bad.cfg line 4 column 2
%CFG-E-REQUIRED, missing setting: system.hostname, router-bad.cfg line 3 column 1
%CFG-E-BADKWD, not an allowed value: ports.[id=1].speed = "100M", allowed: 1G, 10G, router-bad.cfg line 8 column 13
%CFG-E-VTYPE, wrong type: ports.[id=9].enabled: a boolean expected, found a string, router-bad.cfg line 9 column 29
%CFG-E-DUPKEY, duplicate key: ports.[id=9], the same as line 9, router-bad.cfg line 10 column 2
%CFG-E-RANGE, out of range: ports.[id=9].mtu = 65000, allowed 64..9216, router-bad.cfg line 10 column 46
%CFG-E-RANGE, out of range: vlans.[2] = 5000, allowed 1..4094, router-bad.cfg line 13 column 19
router-bad.cfg: 7 problem(s)
```

(Программа, собранная без `NDEBUG`, перед каждым сообщением ставит время, поток и место в коде, как и все сообщения StarLet.)

Проверяйте файл **до** того, как применять его, и показывайте пользователю весь список: это куда любезнее, чем останавливаться на первой ошибке.

---

# Часть II. Справочник

Каждая подпрограмма возвращает код условия типа `unsigned`. В таблицах аргументов *доступ* говорит, читает ли подпрограмма аргумент (read), пишет ли его (write) или делает и то и другое; *механизм* говорит, как он передаётся.

---

## CFG$INIMSG - Зарегистрировать тексты сообщений

**Формат**

```c
unsigned cfg$inimsg (void);
```

**Возвращает** - код условия.

**Описание**

Регистрирует в StarLet каталог сообщений подсистемы CFG, чтобы `$PUTMSG_FAO` для значения CFG$_ и CFG$PUTERRS выводили тексты из Приложения A. Вызовите её один раз, до первого сообщения; она не безопасна для потоков. Повторный вызов вреда не приносит.

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| STS$K_SUCCESS | Зарегистрировано. |
| STS$K_WARN | Уже было зарегистрировано. Не ошибка. |

---

## CFG$PARSE_FILE - Разобрать файл в дерево

**Формат**

```c
unsigned cfg$parse_file (const char *a_path, uint32_t a_opts, CFG$TREE **a_tree);
```

**Возвращает** - код условия.

**Аргументы**

| Аргумент | Тип | Доступ | Механизм | Описание |
|---|---|---|---|---|
| a_path | char | read | по ссылке, оканчивается нулём | Имя файла. |
| a_opts | uint32_t | read | по значению | Параметры: 0 или сумма CFG$M_NOCASE (имена ищутся без учёта регистра; `A` и `a` в одной группе тогда считаются повторами), CFG$M_NOINCLUDE (`@include` отвергается) и CFG$M_SKIPERRS (плохая настройка пропускается, разбор идёт дальше, раздел 3.4). |
| a_tree | CFG$TREE * | write | по ссылке | Новое дерево. |

**Описание**

Читает файл целиком и разбирает его вместе с включаемыми файлами в новое дерево.

Дерево создаётся **даже тогда, когда разбор не удался**: в нём лежит диагностика для CFG$PUTERRS. Поэтому вызывайте CFG$FREE всякий раз, когда `*a_tree` не равно NULL. Оно равно NULL только тогда, когда не хватило памяти.

Разбор останавливается на первой ошибке - если не задан CFG$M_SKIPERRS.

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| CFG$_NORMAL | Разобрано. |
| CFG$_SKIPPED | Разобрано с CFG$M_SKIPERRS, плохие настройки пропущены (успех); их диагностика лежит в дереве. |
| CFG$_OPENIN, CFG$_READERR | Файл (или включаемый) не удалось прочитать. |
| CFG$_SYNTAX, CFG$_UNTERM, CFG$_BADNUM, CFG$_BADESC, CFG$_DUPNAME, CFG$_MIXARRAY | Ошибка в тексте, см. Приложение A. |
| CFG$_INCLUDE, CFG$_INCDEPTH, CFG$_INCLOOP, CFG$_INCTWICE | Неполадка с `@include`. |
| CFG$_TOOBIG | Слишком велик файл, имя или вложенность (Приложение B). |
| CFG$_NOMEM | Не хватило памяти. |
| CFG$_BADARG | `a_path` равно NULL. |

**Пример**

```c
CFG$TREE	*tree;
unsigned	sts = cfg$parse_file("/etc/router.cfg", CFG$M_NOCASE, &tree);

if ( !(sts & 1) )
	cfg$puterrs(tree);
```

---

## CFG$PARSE_BUF - Разобрать буфер в дерево

**Формат**

```c
unsigned cfg$parse_buf (const char *a_buf, uint32_t a_len, const char *a_name, uint32_t a_opts, CFG$TREE **a_tree);
```

**Аргументы**

| Аргумент | Тип | Доступ | Механизм | Описание |
|---|---|---|---|---|
| a_buf | char | read | по ссылке | Текст; копируется, так что после вызова буфер можно освободить. |
| a_len | uint32_t | read | по значению | Его длина. |
| a_name | char | read | по ссылке, оканчивается нулём | Имя для диагностики и база для относительных имён `@include`; при NULL берётся "<buffer>". |
| a_opts | uint32_t | read | по значению | Как для CFG$PARSE_FILE. |
| a_tree | CFG$TREE * | write | по ссылке | Новое дерево. |

**Описание**

То же, что CFG$PARSE_FILE, но текст берётся из памяти. CFG$WRITE для такого дерева запишет главный текст в файл с именем `a_name`: если собираетесь сохранять, задайте настоящее имя файла.

**Возвращаемые коды условий** - как для CFG$PARSE_FILE.

---

## CFG$PUTERRS - Вывести диагностику дерева

**Формат**

```c
unsigned cfg$puterrs (CFG$TREE *a_tree);
```

**Описание**

Выводит через `$PUTMSG_FAO` каждую диагностику, записанную в дереве, в порядке обнаружения. Хранится не больше CFG$K_MAXERRS записей. NULL допустим и ничего не делает.

**Возвращаемые коды условий** - STS$K_SUCCESS.

---

## CFG$ERRCOUNT - Сосчитать диагностику дерева

**Формат**

```c
unsigned cfg$errcount (CFG$TREE *a_tree, uint32_t *a_count, uint32_t *a_skipped);
```

**Описание**

Сообщает, сколько диагностик в дереве (не сохранённые сверх CFG$K_MAXERRS тоже считаются) и сколько плохих настроек пропустил разбор с CFG$M_SKIPERRS. Любой из выходов может быть NULL.

**Возвращаемые коды условий** - STS$K_SUCCESS, CFG$_BADARG.

---

## CFG$FREE - Освободить дерево

**Формат**

```c
unsigned cfg$free (CFG$TREE *a_tree);
```

**Описание**

Освобождает дерево, все его узлы и строки, а также текст его исходных файлов. После этого все указатели на узлы и все строки, полученные из дерева, недействительны. NULL допустим.

**Возвращаемые коды условий** - STS$K_SUCCESS.

---

## CFG$SOURCE - Узнать файл источника

**Формат**

```c
unsigned cfg$source (CFG$TREE *a_tree, uint32_t a_srcno, const char **a_path);
```

**Аргументы**

| Аргумент | Тип | Доступ | Механизм | Описание |
|---|---|---|---|---|
| a_srcno | uint32_t | read | по значению | 0 - главный файл; 1, 2, ... - включаемые файлы в порядке их появления. |
| a_path | char * | write | по ссылке | Имя файла; принадлежит дереву. |

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| STS$K_SUCCESS | Выполнено. |
| CFG$_NOTFOUND | Такого источника нет - здесь заканчивается цикл `for (s = 0; cfg$source(t, s, &p) & 1; s++)`. |

---

## CFG$ROOT - Получить корневую группу

**Формат**

```c
unsigned cfg$root (CFG$TREE *a_tree, CFG$NODE **a_node);
```

**Описание**

Отдаёт корневую группу дерева - точку отсчёта любого пути.

---

## CFG$LOOKUP - Найти узел по пути

**Формат**

```c
unsigned cfg$lookup (CFG$NODE *a_base, const char *a_path, CFG$NODE **a_node);
```

**Аргументы**

| Аргумент | Тип | Доступ | Механизм | Описание |
|---|---|---|---|---|
| a_base | CFG$NODE | read | по ссылке | Откуда начинается путь. |
| a_path | char | read | по ссылке, оканчивается нулём | Путь (раздел 3.3); NULL или "" - сам базовый узел. |
| a_node | CFG$NODE * | write | по ссылке | Найденный узел. |

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| STS$K_SUCCESS | Найдено. |
| CFG$_NOTFOUND | На пути ничего нет (предупреждение: младший бит сброшен). |
| CFG$_BADPATH | Путь составлен неверно: незакрытая `[`, `[x]` с нецифрой, плохое имя ключа. |

---

## CFG$NODE_INFO - Узнать, что за узел

**Формат**

```c
unsigned cfg$node_info (CFG$NODE *a_node, CFG$INFO *a_info);
```

**Описание**

Заполняет структуру `CFG$INFO`:

| Поле | Смысл |
|---|---|
| name, namelen | Имя - **не оканчивается нулём**; NULL для элемента списка или массива. |
| type | CFG$K_TYPE_GROUP, _LIST, _ARRAY, _INT, _INT64, _FLOAT, _BOOL, _STRING. |
| count | Число живых потомков группы, списка или массива. |
| file | Исходный файл узла. |
| line, col | Где он начинается в файле, счёт с 1; 0 для узла, созданного программой. |

---

## CFG$FIRST, CFG$NEXT - Обойти потомков

**Формат**

```c
unsigned cfg$first (CFG$NODE *a_parent, CFG$NODE **a_node);
unsigned cfg$next  (CFG$NODE *a_node,   CFG$NODE **a_next);
```

**Описание**

CFG$FIRST отдаёт первого живого потомка группы, списка или массива; CFG$NEXT - живого соседа после узла. Удалённые узлы пропускаются. Обычный цикл:

```c
for ( sts = cfg$first(group, &n); sts & 1; sts = cfg$next(n, &n) )
	...
```

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| STS$K_SUCCESS | Потомок есть. |
| CFG$_NOTFOUND | Потомков нет (больше нет). |

---

## CFG$GET_INT, _INT64, _FLOAT, _BOOL, _STRING - Прочитать значение

**Формат**

```c
unsigned cfg$get_int    (CFG$NODE *a_base, const char *a_path, int32_t *a_val);
unsigned cfg$get_int64  (CFG$NODE *a_base, const char *a_path, int64_t *a_val);
unsigned cfg$get_float  (CFG$NODE *a_base, const char *a_path, double *a_val);
unsigned cfg$get_bool   (CFG$NODE *a_base, const char *a_path, int *a_val);
unsigned cfg$get_string (CFG$NODE *a_base, const char *a_path, const char **a_str, uint32_t *a_len);
```

**Описание**

Находят узел по пути (как CFG$LOOKUP) и отдают его значение.

* CFG$GET_INT принимает целое любого размера, значение которого умещается в 32 бита.
* CFG$GET_INT64 принимает целое любого размера.
* CFG$GET_FLOAT принимает вещественное или целое (с преобразованием).
* CFG$GET_STRING отдаёт строку с раскрытыми escape-последовательностями. Она оканчивается нулём, но может содержать и нули (`\x00`), поэтому пользуйтесь длиной. Строка принадлежит дереву.

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| STS$K_SUCCESS | Выполнено. |
| CFG$_NOTFOUND, CFG$_BADPATH | Как для CFG$LOOKUP. |
| CFG$_TYPE | Узел другого типа, или 64-битное значение не умещается в CFG$GET_INT. |

---

## CFG$SET_INT, _INT64, _FLOAT, _BOOL, _STRING - Записать значение

**Формат**

```c
unsigned cfg$set_int    (CFG$NODE *a_base, const char *a_path, int32_t a_val);
unsigned cfg$set_int64  (CFG$NODE *a_base, const char *a_path, int64_t a_val);
unsigned cfg$set_float  (CFG$NODE *a_base, const char *a_path, double a_val);
unsigned cfg$set_bool   (CFG$NODE *a_base, const char *a_path, int a_val);
unsigned cfg$set_string (CFG$NODE *a_base, const char *a_path, const char *a_str, uint32_t a_len);
```

**Описание**

Задают значение узла по пути. Если путь называет настройки, которых нет, они **создаются**: недостающие в середине пути - как группы, последняя - как настройка нужного типа. Элемент (`[n]`, `[key=value]`) никогда не создаётся: добавьте его через CFG$ADD.

Новый текст сохраняет вид старого: число, записанное в шестнадцатеричном виде, остаётся шестнадцатеричным; целое, которое перестало умещаться в 32 бита, становится 64-битным и получает суффикс `L` - но не восьмеричное с ведущим `0`: `L` сделало бы его десятичным, а за пределами 32 бит оно и так 64-битное. Вещественное число записывается наименьшим числом цифр, при котором оно читается обратно как то же значение double. Строка берётся в кавычки, и в ней ставятся escape-последовательности.

Целое и 64-битное целое могут заменять друг друга; любая другая смена типа отвергается. CFG$SET_FLOAT отвергает бесконечность и NaN: в синтаксисе для них нет записи.

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| STS$K_SUCCESS | Выполнено. |
| CFG$_TYPE | Узел другого типа, либо на пути, где нужно создать настройку, стоит группа, список или массив. |
| CFG$_NOTFOUND | Элемента на пути нет. |
| CFG$_BADNAME | У создаваемой настройки имя, которое синтаксис не допускает. |
| CFG$_BADPATH, CFG$_BADARG, CFG$_NOMEM | Как говорят их названия. |

**Пример**

```c
cfg$set_int(root, "system.snmp.port", 161);	/* makes SNMP and PORT if missing */
cfg$set_bool(root, "ports.[id=10].enabled", 1);
```

---

## CFG$ADD - Добавить узел

**Формат**

```c
unsigned cfg$add (CFG$NODE *a_parent, const char *a_name, uint32_t a_type, CFG$NODE **a_node);
```

**Аргументы**

| Аргумент | Тип | Доступ | Механизм | Описание |
|---|---|---|---|---|
| a_parent | CFG$NODE | read | по ссылке | Группа, список или массив. |
| a_name | char | read | по ссылке, оканчивается нулём | Имя в группе; NULL в списке или массиве. |
| a_type | uint32_t | read | по значению | CFG$K_TYPE_*. |
| a_node | CFG$NODE * | write | по ссылке | Новый узел; может быть NULL. |

**Описание**

Ставит новый узел в конец контейнера. Скаляр начинается как 0, 0.0, false или "". Узел записывается после последнего соседа, с его отступом.

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| STS$K_SUCCESS | Добавлено. |
| CFG$_EXISTS | В группе уже есть настройка с таким именем. |
| CFG$_BADNAME | Недопустимое имя. |
| CFG$_TYPE | Родитель - скаляр; или группа/список в массив; или скаляр другого типа в массив. |
| CFG$_BADARG | Нет имени для группы, есть имя для списка или массива, плохой тип. |

---

## CFG$DELETE - Удалить узел

**Формат**

```c
unsigned cfg$delete (CFG$NODE *a_node);
```

**Описание**

Удаляет узел со всем, что под ним. Его текст уходит из файла при следующей записи вместе со строками комментария над ним и комментарием после него. В списке или массиве запятые исправляются.

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| STS$K_SUCCESS | Удалено. |
| CFG$_BADARG | Корень или узел, уже удалённый. |

---

## CFG$WRITE_BUF - Получить текст источника

**Формат**

```c
unsigned cfg$write_buf (CFG$TREE *a_tree, uint32_t a_srcno, char **a_buf, uint32_t *a_len);
```

**Описание**

Строит в новом буфере текст одного исходного файла дерева (0 - главный). Буфер оканчивается нулём; освободите его через `free()`. На диск ничего не пишется.

**Возвращаемые коды условий** - STS$K_SUCCESS, CFG$_BADARG (такого источника нет), CFG$_NOMEM.

---

## CFG$WRITE - Записать изменённые файлы обратно

**Формат**

```c
unsigned cfg$write (CFG$TREE *a_tree);
```

**Описание**

Для каждого источника дерева строит его текст; если текст отличается от прочитанного, записывает его в файл на прежнем месте: временный файл `<name>.yalibcfg-XXXXXX` в том же каталоге, сброшенный на диск, переименованный поверх старого файла. Права доступа и владелец старого файла сохраняются (владелец - насколько процессу позволено его менять); символическая ссылка разыменовывается: ссылка остаётся, заменяется файл, на который она указывает. Файл, который не изменился, не трогается.

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| STS$K_SUCCESS | Выполнено. |
| CFG$_WRITERR | Файл не удалось записать; дерево запоминает, какой именно, и errno. |
| CFG$_BROKEN | Дерево получено из неудачного разбора. |
| CFG$_NOMEM | Не хватило памяти. |

---

## CFG$WRITE_FILE - Записать главный файл в другое место

**Формат**

```c
unsigned cfg$write_file (CFG$TREE *a_tree, const char *a_path);
```

**Описание**

Записывает текст главного источника в `a_path` тем же безопасным способом, что и CFG$WRITE. Директивы `@include` пишутся как есть; включаемые файлы не копируются. Если включаемый файл изменён, вызов отклоняется с CFG$_INCMODIFIED: это изменение никуда бы не попало - запишите его на месте через CFG$WRITE.

---

## CFG$SET_VALUE - Записать значение из его текста

**Формат**

```c
unsigned cfg$set_value (CFG$NODE *a_base, const char *a_path, const char *a_text, uint32_t a_len);
```

**Описание**

Задаёт скаляр по его тексту в том виде, как он записан в файле - для программы, у которой значение есть в виде текста, например командная строка (CFGTOOL SET). `9216`, `0x1F`, `%X1F`, `0755` (восьмеричное), `12L`, `1.5`, `true`, `"text"` определяют свои типы; любой текст, который не является ровно одним таким значением (`10.0.0.5`, `12abc`, `1 2`, `12 # note`, `09`, пустой), задаётся как строка. Настройка, которая уже есть, сохраняет свой тип, где текст это позволяет: строка принимает текст как есть, вещественное принимает целое. Целое принимает форму нового текста: `%XFF` записывается как `%XFF`, `8` - как `8`, даже если старое значение было `010`; `L` делает его 64-разрядным.

**Возвращаемые коды условий** - как у CFG$SET_*.

---

## CFG$NODE_TEXT - Получить текст узла

**Формат**

```c
unsigned cfg$node_text (CFG$NODE *a_node, uint32_t a_opts, char **a_buf, uint32_t *a_len);
```

**Описание**

Выдаёт канонический текст узла в новом буфере (освободите его через `free()`): значение или, с CFG$M_TEXT_NAME, `name = value;`; группа или список - по строкам с отступом в один знак табуляции или, с CFG$M_TEXT_FLAT, в одну строку.

**Возвращаемые коды условий** - STS$K_SUCCESS, CFG$_NOMEM, CFG$_BADARG (удалённый узел).

---

## CFG$WRITE_CANONICAL - Получить канонический текст дерева

**Формат**

```c
unsigned cfg$write_canonical (CFG$TREE *a_tree, char **a_buf, uint32_t *a_len);
```

**Описание**

Выдаёт настройки всего дерева в каноническом виде: одна настройка на строке, группы раскрыты по строкам с отступом в один знак табуляции, настройки включаемых файлов стоят на месте строк `@include`. **Комментарии и оформление файлов не сохраняются.** Текст читается обратно в те же настройки - с тем же отпечатком. Именно это записывает CFGTOOL FORMAT.

**Возвращаемые коды условий** - STS$K_SUCCESS, CFG$_BROKEN, CFG$_NOMEM, CFG$_BADARG.

---

## CFG$VALIDATE - Проверить дерево по схеме

**Формат**

```c
unsigned cfg$validate (CFG$TREE *a_tree, const CFG$SCHEMA *a_schema, void *a_arg, uint32_t *a_nerr);
```

**Аргументы**

| Аргумент | Тип | Доступ | Механизм | Описание |
|---|---|---|---|---|
| a_tree | CFG$TREE | modify | по ссылке | Дерево. Запись его диагностики начинается заново. |
| a_schema | CFG$SCHEMA | read | по ссылке | Таблица настроек корневой группы, оканчивающаяся записью с именем NULL (раздел 3.7). |
| a_arg | void | read | по ссылке | Передаётся подпрограммам проверки схемы как есть. |
| a_nerr | uint32_t | write | по ссылке | Число диагностик - ошибок и предупреждений, хранимых и нет; может быть NULL. |

**Описание**

Проверяет каждый узел дерева по его записи в схеме: тип, диапазон, длину, ключевые слова, адреса, число элементов, обязательные настройки, настройки, которых схема не знает, уникальность ключей списков и в последнюю очередь подпрограммы проверки программы.

Проверка **не останавливается на первой ошибке**. Каждая проблема записывается в дерево вместе с позицией и путём настройки, для CFG$PUTERRS; хранится не больше CFG$K_MAXERRS записей, остальные подсчитываются, и CFG$PUTERRS сообщает, сколько не сохранено (CFG$_MOREERRS). Об узле неверного типа сообщается один раз, а то, что внутри него, не рассматривается.

Сначала проверяется сама схема: запись KWD без слов, ключ вне элемента списка или не скаляр, два ключа, MIN больше MAX, список без записи для его элементов, схема глубже 64 уровней - всё это ошибки программиста, и они возвращаются как CFG$_BADSCHEMA раньше, чем будет упрекнут какой-либо файл. Схема может ссылаться сама на себя (меню, пункты которого - меню): каждая запись проверяется один раз.

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| STS$K_SUCCESS | Дерево соответствует схеме. В записи могут быть предупреждения (DEPRECATED). |
| CFG$_INVALID | Не соответствует: диагностика лежит в дереве. |
| CFG$_BADSCHEMA | Схема неверна. |
| CFG$_BROKEN | Дерево получено из неудачного разбора. |
| CFG$_BADARG, CFG$_NOMEM | Как говорят их названия. |

---

## CFG$FINGERPRINT - Получить отпечаток дерева

**Формат**

```c
unsigned cfg$fingerprint (CFG$TREE *a_tree, uint8_t a_uuid[16]);
```

**Описание**

Вычисляет отпечаток дерева (раздел 3.6, Приложение D): 16 октетов UUID версии 5 в порядке RFC 9562. Выведите его директивой FAO `!%U`:

```c
uint8_t	uuid[16];

cfg$fingerprint(tree, uuid);
$LOG_FAO(STS$K_INFO, "configuration !%U", uuid);
```

Отпечаток относится к дереву таким, какое оно сейчас: после изменения вычислите его заново.

**Возвращаемые коды условий** - STS$K_SUCCESS, CFG$_NOMEM, CFG$_BADARG.

---

## CFG$DIFF - Сравнить два дерева

**Формат**

```c
unsigned cfg$diff (CFG$TREE *a_old, CFG$TREE *a_new, const CFG$SCHEMA *a_schema, uint32_t a_opts, CFG$DIFF **a_diff);
```

**Аргументы**

| Аргумент | Тип | Доступ | Механизм | Описание |
|---|---|---|---|---|
| a_old, a_new | CFG$TREE | read | по ссылке | Деревья. Они должны жить, пока существует сравнение. |
| a_schema | CFG$SCHEMA | read | по ссылке | NULL или схема деревьев: тогда списки сопоставляются по ключу, который она помечает (CFG$M_KEY), - если он есть у каждого элемента и не повторяется; иначе по положению. |
| a_opts | uint32_t | read | по значению | 0 или CFG$M_POSITIONAL - списки сопоставляются только по положению. |
| a_diff | CFG$DIFF * | write | по ссылке | Сравнение. Освободите его через CFG$DIFF_FREE. |

**Описание**

Сравнивает настройки деревьев по правилам раздела 3.6. Различия идут в порядке старого дерева, добавления - после настроек своей группы.

**Возвращаемые коды условий**

| Значение | Смысл |
|---|---|
| STS$K_SUCCESS | Сравнено - есть различия или нет, спросите у CFG$DIFF_COUNT. |
| CFG$_NOMEM, CFG$_BADARG | Как говорят их названия. |

---

## CFG$DIFF_COUNT, CFG$DIFF_ENTRY - Прочитать различия

**Формат**

```c
unsigned cfg$diff_count (CFG$DIFF *a_diff, uint32_t *a_count);
unsigned cfg$diff_entry (CFG$DIFF *a_diff, uint32_t a_idx, uint32_t *a_op, const char **a_path,
			 CFG$NODE **a_old, CFG$NODE **a_new);
```

**Описание**

CFG$DIFF_COUNT отдаёт число различий, CFG$DIFF_ENTRY - одно из них (счёт с 0):

| Выход | Смысл |
|---|---|
| a_op | CFG$K_DIFF_MOD, CFG$K_DIFF_ADD или CFG$K_DIFF_DEL. |
| a_path | Путь; принадлежит сравнению; работает с CFG$LOOKUP для любого из деревьев. |
| a_old, a_new | Узлы; NULL с той стороны, где настройки нет. |

Любой выходной аргумент может быть NULL.

**Возвращаемые коды условий** - STS$K_SUCCESS; CFG$_NOTFOUND за последней записью.

---

## CFG$DIFF_LISTING - Составить листинг сравнения

**Формат**

```c
unsigned cfg$diff_listing (CFG$DIFF *a_diff, uint32_t a_opts, char **a_buf, uint32_t *a_len);
```

**Аргументы**

| Аргумент | Тип | Доступ | Механизм | Описание |
|---|---|---|---|---|
| a_opts | uint32_t | read | по значению | 0 или CFG$M_NOTIME - без времени: одно и то же сравнение даёт один и тот же текст при каждом запуске (для тестов и для листингов, хранимых под контролем версий). |
| a_buf, a_len | char *, uint32_t | write | по ссылке | Текст, оканчивается нулём; освободите его через `free()`. |

**Описание**

Составляет листинг, показанный в разделе 4.5: заголовок с обоими файлами и их отпечатками, раздел на каждое различие со строкой в каждом файле и настройкой в каноническом виде, итог и концовку с именами файлов и способом сопоставления списков: `/KEYS=(ID,NAME,KEY)`, `/SCHEMA` или `/POSITIONAL`. Значения пишутся в одну строку: группа как `{ a = 1; b = 2; }`.

**Возвращаемые коды условий** - STS$K_SUCCESS, CFG$_NOMEM, CFG$_BADARG.

---

## CFG$DIFF_FREE - Освободить сравнение

**Формат**

```c
unsigned cfg$diff_free (CFG$DIFF *a_diff);
```

**Описание**

Освобождает сравнение и его пути. Деревья не затрагиваются. NULL допустим.

---

# Часть III. Утилита CFGTOOL

CFGTOOL - утилита библиотеки для командной строки: она проверяет, форматирует, сравнивает, читает и меняет файлы конфигурации из оболочки и из сценариев; командная строка - в стиле DCL. Утилита входит в пакет `cfgtool`; `HELP CFGTOOL` показывает её библиотеку справки (пакет HELP).

## CFGTOOL -- формат

```
CFGTOOL command parameters [/qualifiers]
```

| Команда | Что делает |
|---|---|
| CHECK file | Проверяет файл; сообщает его отпечаток |
| FORMAT file | Записывает настройки в каноническом виде |
| DIFF old-file new-file | Сравнивает две конфигурации, выводит листинг |
| GET file path | Выводит значение настройки |
| SET file path value | Меняет настройку, записывает файл обратно вместе с комментариями |
| DELETE file path | Удаляет настройку, записывает файл обратно |
| HELP | Краткая сводка команд |

Команду и квалификатор можно сокращать, пока сокращение однозначно; квалификатор можно написать и в длинной форме Unix: `--fingerprint` вместо `/FINGERPRINT`. `CFGTOOL command /HELP` перечисляет параметры и квалификаторы команды.

Сообщения идут в стандартный поток ошибок через `$PUTMSG_FAO`; данные - значение, листинг, текст, отпечаток - в стандартный вывод. **Код завершения**:

| Код | Значение |
|---|---|
| 0 | Успех; для DIFF - различий нет |
| 1 | DIFF нашла различия |
| 2 | Ошибка |

Путь пишется так же, как в библиотеке (раздел 3.3): `system.hostname`, `vlans.[1]`, `ports.[id=9].mtu`. Если в пути есть скобки или пробелы, заключите его в кавычки, чтобы оболочка его не разбирала.

Два правила командной строки. Аргумент - не более 255 октетов (иначе CFG$_ARGLONG). Значение или путь, начинающиеся с `/` или `--`, были бы приняты за квалификатор: возьмите их в двойные кавычки внутри кавычек оболочки - `cfgtool set r.cfg system.logdir '"/var/log"'`.

/OUTPUT пишет через временный файл и переименование: старый файл никогда не остаётся записанным наполовину.

Квалификаторы, которые принимают несколько команд:

| Квалификатор | Значение |
|---|---|
| /NOCASE | Имена настроек сравниваются без учёта регистра (CFG$M_NOCASE) |
| /NOINCLUDE | `@include` считается ошибкой (CFG$M_NOINCLUDE) |
| /OUTPUT=file | Данные идут в файл, а не в стандартный вывод |
| /LOG | SET и DELETE сообщают, что они сделали |
| /SKIPERRORS | Плохие настройки пропускаются (CFG$M_SKIPERRS), о каждой сообщается: CHECK тогда сообщает обо всех ошибках файла за один прогон и завершается неудачей; остальные команды продолжают с остатком файла, а SET и DELETE оставляют текст плохих настроек как есть |

## CHECK -- проверка файла

```
CFGTOOL CHECK file [/FINGERPRINT /NOCASE /NOINCLUDE /SKIPERRORS]
```

Разбирает файл вместе с включаемыми файлами. О правильном файле сообщает число его настроек и отпечаток (раздел 3.6); о каждой ошибке - файл, строку и столбец. `/FINGERPRINT` выводит в стандартный вывод один отпечаток - для сценария.

```
$ cfgtool check router.cfg
%CFG-S-CORRECT, router.cfg is correct: 16 settings, UUID a0f0b155-8348-5765-97a7-4dfb5164faba

$ id=$(cfgtool check router.cfg /fingerprint 2>/dev/null)
```

## FORMAT -- запись в каноническом виде

```
CFGTOOL FORMAT file [/OUTPUT=file /NOCASE /NOINCLUDE]
```

Записывает настройки файла в каноническом виде (CFG$WRITE_CANONICAL): одна настройка на строке, группы раскрыты по строкам с отступом в один знак табуляции, настройки включаемых файлов стоят на месте строк `@include`. **Комментарии не сохраняются** - это текст одних лишь настроек: им удобно сравнивать файлы как тексты или приводить файл в аккуратный вид. У канонического текста тот же отпечаток, что и у файла.

## DIFF -- сравнение двух конфигураций

```
CFGTOOL DIFF old-file new-file [/OUTPUT=file /POSITIONAL /NOTIME /NOCASE]
```

Сравнивает настройки (CFG$DIFF) и выводит листинг из раздела 4.5 (CFG$DIFF_LISTING). `/POSITIONAL` сопоставляет элементы списков только по положению, `/NOTIME` убирает из листинга время. Код завершения равен 1, если есть различия, так что сценарий может спросить об этом:

```
$ if ! cfgtool diff running.cfg saved.cfg /notime /output=changes.lis ; then
>     echo "the configuration has changed, see changes.lis"
> fi
```

## GET -- чтение настройки

```
CFGTOOL GET file path [/FLAT /NOCASE]
```

Выводит значение: строку - как есть (escape-последовательности раскрыты, без кавычек), другой скаляр - как в файле, группу или список - по строкам, а с `/FLAT` - в одну строку.

```
$ cfgtool get router.cfg ports.[id=9].mtu
9216
$ cfgtool get router.cfg system /flat
{ hostname = "edge-1"; location = "Lab, rack 3"; }
```

## SET -- изменение настройки

```
CFGTOOL SET file path value [/TYPE={AUTO|STRING} /OUTPUT=file /LOG /NOCASE]
```

Задаёт значение и записывает файл на то же место (CFG$WRITE): меняется только значение, комментарии и оформление остаются. Настройка, которой нет, создаётся вместе с группами на её пути.

При `/TYPE=AUTO` (по умолчанию) тип определяется по тексту, как в файле (CFG$SET_VALUE): `1500` - целое, `0x1F` или `%X1F` - шестнадцатеричное (записывается обратно так же), `0755` - восьмеричное (493), `12L` - 64-разрядное, `1.5` - вещественное, `true` - логическое, `"text"` - строка. Всё, что не является ровно одним таким значением, - `10.0.0.5`, `12abc`, `12 # note`, `09` - строка, как она есть. Настройка, которая уже есть, сохраняет свой тип, где текст это позволяет: строка принимает текст как есть (`12` остаётся строкой "12"), вещественное принимает целое (`2` становится 2.0). `/TYPE=STRING` всегда задаёт текст как строку.

`/OUTPUT` записывает изменённый главный файл в другое место, а оригинал остаётся как был. Команда отклоняется (CFG$_INCMODIFIED), если изменение попадает во включаемый файл: оно было бы потеряно. При записи на месте файл сохраняет права доступа и владельца; символическая ссылка остаётся ссылкой, изменяется файл, на который она указывает.

```
$ cfgtool set router.cfg ports.[id=9].mtu 1500
$ cfgtool set router.cfg system.snmp.community public /log
%CFG-S-MODIFIED, system.snmp.community set in router.cfg
```

## DELETE -- удаление настройки

```
CFGTOOL DELETE file path [/OUTPUT=file /LOG /NOCASE]
```

Удаляет настройку вместе со строками комментария над ней (CFG$DELETE) и записывает файл обратно.

---

# Приложение A. Сообщения

Каждое сообщение имеет вид `%CFG-s-IDENT, text`, где *s* - серьёзность: S - успех, I - информация, W - предупреждение, E - ошибка, F - фатальная ошибка. Сообщения разбора оканчиваются файлом, строкой и столбцом.

**BROKEN, the tree comes from a failed parse**
*Пояснение:* Запись, сравнение или отпечаток запрошены у дерева, разбор которого не удался. *Действия пользователя:* Выведите диагностику разбора (CFG$PUTERRS), исправьте файл и разберите его снова.

**BADARG, bad argument**
*Пояснение:* Подпрограмма получила указатель NULL, тип вне допустимого диапазона, имя там, где имя не допускается. *Действия пользователя:* Это ошибка в программе - проверьте вызов.

**BADESC, bad escape sequence** *esc*
*Пояснение:* В строке за обратной косой чертой стоит что-то, кроме `\ " n r t f x`, либо `\x` не сопровождается двумя шестнадцатеричными цифрами. *Действия пользователя:* Для обратной косой черты пишите `\\`.

**BADNAME, not a valid setting name**
*Пояснение:* Имя должно начинаться с буквы или `*` и состоять из букв, цифр, `-`, `_`, `*`. *Действия пользователя:* Выберите другое имя.

**BADNUM, bad number** *text*
*Пояснение:* Число составлено неверно (`0x` без цифр, `12abc`, двоичная цифра 2) или вне допустимого диапазона (больше 64 бит; вещественное вне диапазона double). *Действия пользователя:* Исправьте число; для 64-битного целого добавьте `L`.

**BADPATH, malformed path**
*Пояснение:* Путь, переданный подпрограмме, составлен неверно. *Действия пользователя:* См. раздел 3.3.

**DUPNAME, duplicate setting** *name*
*Пояснение:* В группе две настройки с одним именем (с CFG$M_NOCASE - с одним именем в любом регистре). *Действия пользователя:* Удалите одну из них.

**EXISTS, a setting of that name exists already**
*Пояснение:* CFG$ADD с именем, которое в группе уже есть. *Действия пользователя:* Измените настройку через CFG$SET_* или сначала удалите её.

**INCDEPTH, include files nested too deep:** *name*
*Пояснение:* Больше CFG$K_MAXINCDEPTH уровней `@include`. *Действия пользователя:* Сделайте файлы менее вложенными.

**INCLOOP, file includes itself:** *name*
*Пояснение:* Файл включает сам себя, прямо или через другие. *Действия пользователя:* Разорвите цикл.

**INCTWICE, file included a second time:** *name*
*Пояснение:* Файл включён в одно дерево дважды. *Действия пользователя:* Включайте его один раз; если двум группам нужны одни и те же настройки, сделайте из них список или скопируйте настройки.

**INCLUDE, cannot include** *name*
*Пояснение:* Включаемый файл не удалось прочитать; почему, говорит предшествующее сообщение OPENIN. *Действия пользователя:* Проверьте имя: оно отсчитывается от каталога включающего файла.

**MIXARRAY, array element** *value* **is not of the type of the first**
*Пояснение:* В массиве скаляры разных типов. *Действия пользователя:* Приведите их к одному типу или используйте список `( )`.

**NOMEM, memory allocation failure**
*Пояснение:* У процесса кончилась память. *Действия пользователя:* Проверьте лимиты процесса.

**NORMAL, normal successful completion**

**NOTFOUND, no setting on the path**
*Пояснение:* Предупреждение: на пути ничего нет или потомков больше нет. *Действия пользователя:* Ничего не нужно, если настройка необязательная.

**OPENIN, cannot open** *file*, **errno=** *n*
*Пояснение:* Файл не удалось открыть. *Действия пользователя:* Проверьте имя и защиту; причину говорит `errno` (2 - нет такого файла, 13 - нет доступа).

**READERR, error reading** *file*, **errno=** *n*
*Пояснение:* Файл не удалось прочитать, он не является обычным файлом (errno 22) или больше CFG$K_MAXFILE (errno 27). *Действия пользователя:* Действуйте по тому, что говорит errno.

**SYNTAX, syntax error:** *expected X, found Y*
*Пояснение:* Текст не следует синтаксису из главы 2. *Действия пользователя:* Посмотрите на указанные строку и столбец. Частая причина - пропущенная `;` или `}` на строке **перед** ними. Другая - `,` перед закрывающей `]` или `)`: это не допускается, как и в libconfig.

**TOOBIG,** *what* **is too big**
*Пояснение:* Имя длиннее 255 символов, слишком длинное имя файла, значения вложены глубже CFG$K_MAXDEPTH. *Действия пользователя:* Сократите.

**TYPE, the setting is of another type**
*Пояснение:* Чтение или запись типа, которого у узла нет. *Действия пользователя:* Используйте подпрограмму нужного типа либо удалите узел и добавьте его заново.

**UNTERM, unterminated** *what*
*Пояснение:* Строка, комментарий или скобка не закрыты до конца строки (для строк) или файла. Позиция указывает, где они начались. *Действия пользователя:* Закройте их.

**WRITERR, error writing** *file*, **errno=** *n*
*Пояснение:* Файл не удалось записать: нет места (28), нет доступа к каталогу (13). Старый файл остался как был. *Действия пользователя:* Действуйте по тому, что говорит errno.

Сообщения проверки (CFG$VALIDATE) начинаются с пути настройки:

**BADKWD, not an allowed value:** *path = value, allowed: words*
*Пояснение:* Настройка-ключевое слово содержит слово, которого нет в её схеме. *Действия пользователя:* Используйте одно из названных слов.

**BADADDR, not a valid address:** *path = value is not ...*
*Пояснение:* Строка не является адресом IPv4, IPv6 или MAC, как требует схема. *Действия пользователя:* Исправьте адрес.

**BADSCHEMA, the schema is wrong**
*Пояснение:* Ошибка программиста в таблице схемы (см. CFG$VALIDATE). *Действия пользователя:* Исправьте таблицу.

**CHECK,** *path: text*
*Пояснение:* Подпрограмма проверки, написанная в программе, отвергла значение; текст принадлежит программе. *Действия пользователя:* Действуйте по тому, что говорит текст.

**COUNT, wrong number of elements:** *path has n, allowed min..max*
*Действия пользователя:* Добавьте или уберите элементы.

**DEPRECATED, setting no longer in use:** *path*
*Пояснение:* Предупреждение: настройка пока принимается, но её уберут. *Действия пользователя:* Смотрите документацию программы: что приходит ей на смену.

**DUPKEY, duplicate key:** *path, the same as line n [of file]*
*Пояснение:* У двух элементов списка один и тот же ключ; указана строка первого из них (в другом файле, если он пришёл из включённого) или "an element added by the program" - элемент, добавленный программой. *Действия пользователя:* Дайте им разные ключи или удалите один.

**INVALID, the configuration does not match its schema**
*Пояснение:* Код условия CFG$VALIDATE, когда она нашла ошибки; сама диагностика лежит в дереве.

**LENGTH, wrong length:** *path = value is n long, allowed min..max*
*Действия пользователя:* Сократите или удлините строку.

**MOREERRS,** *n* **more diagnostics not kept**
*Пояснение:* Проблем было больше, чем CFG$K_MAXERRS. *Действия пользователя:* Исправьте показанные и проверьте снова.

**RANGE, out of range:** *path = value, allowed min..max*
*Действия пользователя:* Задайте значение в пределах диапазона.

**REQUIRED, missing setting:** *path*
*Пояснение:* Обязательной настройки (или ключа элемента списка) нет; позиция - это позиция группы, в которой она должна быть. *Действия пользователя:* Добавьте её.

**UNKNOWN, unknown setting:** *path*
*Пояснение:* Схема не знает такой настройки - чаще всего это опечатка. *Действия пользователя:* Проверьте написание.

**VTYPE, wrong type:** *path: X expected, found Y*
*Пояснение:* Значение имеет не тот тип, который хочет схема; то, что внутри него, не проверяется. *Действия пользователя:* Исправьте значение: строка в кавычках, логическое - `true` или `false`, группа - в фигурных скобках.

Сообщения CFGTOOL:

**CANTSET, cannot change** *path* **in** *file*
*Пояснение:* SET или DELETE отказали; почему - говорит следующее сообщение (TYPE, BADNAME, NOTFOUND). *Действия пользователя:* Действуйте по следующему сообщению. Элемент списка командой SET не создаётся никогда.

**CORRECT,** *file* **is correct:** *n* **settings, UUID** *uuid*
*Пояснение:* CHECK нашла файл правильным.

**DELETED,** *path* **deleted from** *file*
*Пояснение:* DELETE /LOG выполнила удаление.

**DIFFERENT,** *n* **differences between** *old* **and** *new*
*Пояснение:* DIFF нашла различия; код завершения 1.

**IDENTICAL, no differences between** *old* **and** *new*
*Пояснение:* DIFF не нашла различий.

**MODIFIED,** *path* **set in** *file*
*Пояснение:* SET /LOG выполнила изменение.

**NOSETTING, no setting** *path* **in** *file*
*Пояснение:* GET или DELETE ничего не нашли по пути. *Действия пользователя:* Проверьте путь: выведите командой GET группу выше.

**SKIPPED,** *n* **bad settings skipped**
*Пояснение:* Успех при CFG$M_SKIPERRS (CFGTOOL /SKIPERRORS): настройки, названные предшествующими сообщениями, пропущены, остальное в файле принято. *Действия пользователя:* Исправьте их.

**ARGLONG, argument** *n* **is longer than 255 octets**
*Пояснение:* Аргумент командной строки слишком длинен, чтобы принять его целиком. *Действия пользователя:* Сократите его - длинное значение лучше задавать из программы через CFG$SET_*.

**INCMODIFIED, a change in an included file cannot go into another file**
*Пояснение:* CFG$WRITE_FILE (CFGTOOL /OUTPUT) попросили записать главный файл в другое место, но изменённая настройка находится во включаемом файле. *Действия пользователя:* Записывайте на месте (CFG$WRITE, CFGTOOL без /OUTPUT).

**OPENOUT, cannot create** *file*, **errno=** *n*
*Пояснение:* Файл из /OUTPUT не удалось создать. *Действия пользователя:* Действуйте по тому, что говорит errno.

---

# Приложение B. Ограничения

| Константа | Значение | Что это |
|---|---|---|
| CFG$K_MAXNAME | 255 | Самое длинное имя настройки. |
| CFG$K_MAXFILE | 64 MB | Самый большой читаемый файл. |
| CFG$K_MAXSRC | 64 | Исходных файлов в одном дереве (главный и включаемые). |
| CFG$K_MAXINCDEPTH | 10 | Вложенность `@include`. |
| CFG$K_MAXDEPTH | 256 | Вложенность значений. |
| CFG$K_MAXERRS | 32 | Диагностик, хранимых в дереве. |

---

# Приложение C. Отличия от libconfig

| | libconfig | yaLIBCFG |
|---|---|---|
| Комментарии и оформление при записи | теряются | сохраняются; файл без изменений записывается байт в байт |
| Десятичное целое больше 32 бит без `L` | 1.5: молча обрезается до 32 бит; 1.7: 64-битное | 64-битное |
| `@include` | вставляет текст, где угодно | только целые настройки, там, где может стоять настройка |
| Один файл включён дважды | разрешено | отказ (CFG$_INCTWICE): изменение нельзя записать обратно в оба места |
| Изменение во включённом файле | при сохранении пишется в главный файл | пишется во включённый файл |
| Ошибки | одна, текст и строка | коды условий с файлом, строкой и столбцом |
| Запись | на месте | временный файл и переименование - никогда не половина файла |
| Сравнение двух конфигураций | нет | CFG$DIFF, листинг, отпечаток |
| Библиотека | LGPL-2.1 | Apache-2.0 |

---

# Приложение D. Отпечаток

Отпечаток - это договор: одни и те же настройки дают один и тот же UUID на любой машине и в любой версии yaLIBCFG. Он определён здесь, чтобы его могли вычислять и другие программы.

1. Начните SHA-1 с 16 октетов пространства имён yaLIBCFG
   `6b2b0e1e-3f6a-5c4e-9d1a-0c5f9a7e2b14`.
2. Подайте корневую группу по таким правилам (все числа - старшим октетом вперёд):

   | Узел | Октеты |
   |---|---|
   | целое (32 или 64 бита) | `'I'`, значение в виде 8 октетов |
   | вещественное | `'F'`, 8 октетов числа double по IEEE 754 |
   | логическое | `'B'`, 8 октетов: 1 или 0 |
   | строка | `'S'`, длина в виде 8 октетов, октеты строки (escape-последовательности раскрыты) |
   | группа | `'G'`, число настроек в виде 8 октетов, затем для каждой настройки в порядке имён (побайтово; для дерева, разобранного с CFG$M_NOCASE, - имён в нижнем регистре): один октет длины имени, имя, настройка |
   | список | `'L'`, число элементов в виде 8 октетов, элементы в их порядке - а если у списка есть ключ (каждый элемент - группа со скалярной настройкой `id`, иначе `name`, иначе `key`, уникальной в списке), то в порядке ключей: сначала по типу (целое < вещественное < логическое < строка, как CFG$K_TYPE_*), затем целые, вещественные и логические - по их 8 октетам как беззнаковым числам, строки - побайтово, более короткая раньше |
   | массив | `'A'`, число элементов в виде 8 октетов, элементы в их порядке |

3. Возьмите первые 16 октетов результата; установите старшие четыре бита октета 6 в 5 (версия) и старшие два бита октета 8 в двоичное 10 (вариант).

Два значения, по которым можно проверить реализацию:

| Файл | Отпечаток |
|---|---|
| пустой | `7b6d66a7-5b7b-5f45-8072-b9bfd2711fa0` |
| `a = 1;` | `1acc6d4e-04d6-5cd7-9e3f-8588fda68b52` |

На Python:

```python
import hashlib, uuid
ns = uuid.UUID("6b2b0e1e-3f6a-5c4e-9d1a-0c5f9a7e2b14")
data = b"G" + (1).to_bytes(8, "big") + bytes([1]) + b"a" + b"I" + (1).to_bytes(8, "big")
b = bytearray(hashlib.sha1(ns.bytes + data).digest()[:16])
b[6] = (b[6] & 0x0F) | 0x50
b[8] = (b[8] & 0x3F) | 0x80
print(uuid.UUID(bytes=bytes(b)))	# 1acc6d4e-04d6-5cd7-9e3f-8588fda68b52
```
