# npc_abi - MeowMenu ↔ HologramLib ABI 一致性校验

MeowMenu 把 HologramLib 当**可选前置**：不链接、不引用它的头文件，只在自己的
[`src/HologramLibAbi.h`](../../src/HologramLibAbi.h) 里镜像接口布局，运行时用
`GetProcAddress` 解析 `?getInstance@IHologramLib@hologramlib@@SAAEAV12@XZ`。

镜像一旦与真实接口漂移，运行时就会按错误的 vtable 槽位调用虚函数 —— 直接崩溃且难查。
这套测试把漂移变成可见的失败。

## 运行

```bat
tests\npc_abi\run.bat
rem 或指定 HologramLib 的 include 目录
tests\npc_abi\run.bat D:\path\to\HologramLib\include
```

依赖：Visual Studio 生成工具（`cl.exe`）+ node。

## 三层校验

| 层 | 文件 | 检查内容 |
|---|---|---|
| 1. 编译期 | `abi_conformance.cpp` | ① 6 个跨 ABI 结构体的 `sizeof` 与关键字段 `offsetof` 必须与真实头文件一致；② 用**镜像里的签名**实现真实接口，每个方法都标 `override` —— 参数/返回类型/const 任一不一致就编译失败 |
| 2. 文本比对 | `check_abi.js` | 3 个接口的**虚函数声明顺序**（= vtable 槽位顺序，顺序错了类型照样能过编译）、6 个结构体的字段顺序、`HOLOGLIB_API_VERSION` 与镜像最低要求、导出符号串 |
| 3. 部署 DLL | `run.bat` 内联 `dumpbin /exports` | 头文件可能比已编译的 DLL 新，所以直接查实际 DLL 是否导出那个 `getInstance` 符号 |

第 1 层是编译期硬保证，第 2 层覆盖编译器管不到的**槽位顺序**，第 3 层覆盖"头文件与 DLL 不同步"。

## 覆盖的类型

- 接口：`IHologramLib`（19 项含析构）、`INpcDialogue`（8 项）、`IPlayerNpc`（33 项）
- 结构体：`GhostInteractEvent`、`PlayerNpcSkin`、`PlayerNpcConfig`、
  `NpcDialogButton`、`NpcDialogSpec`、`NpcDialogClickEvent`

`INpcDialogue` / `IPlayerNpc` 是 MeowMenu 实际调用的接口；`IHologramLib` 只需保证
`npcDialogs()` / `playerNpcs()` / `version()` 之前（含自身）的槽位对齐，但因为 HologramLib
的冻结契约是"只在尾部追加"，所以整体逐项校验。

## HologramLib 升级后

跑一次 `run.bat`：

- 全部通过 → 镜像仍匹配，无需改动
- 某项失败 → 按输出把 `src/HologramLibAbi.h` 对应声明同步过来（只允许尾部追加的能力域；
  若中间插入了方法，说明 HologramLib 破坏了 ABI 兼容约定，需要和库作者确认）

## 注意

- 镜像里**有意不声明** `IHologramLib::getInstance()`：一旦声明并直接调用，MeowMenu.dll
  就会出现 HologramLib 的导入项，可选前置立刻变成加载期硬依赖。入口只能走 `GetProcAddress`。
- `abi_conformance.cpp` 会 `#define MEOW_HOLOGRAM_ABI_NAMESPACE mirror` 后再包含镜像，
  这样同一编译单元里能同时引用真实头文件和镜像（两者类名/命名空间相同，必须改名）。
