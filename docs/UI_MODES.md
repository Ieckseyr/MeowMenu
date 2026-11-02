# 界面样式（三种呈现方式 + 玩家自选）

MeowMenu 的同一份菜单数据可以用三种界面呈现。**玩家可以自己选用哪一种**，菜单作者也能给
单个菜单指定默认样式；两者冲突时以玩家选择为准。

| 样式 | 呈现 | 前置 | 容量 | 数据来源 |
|---|---|---|---|---|
| `form` 普通表单 | LeviLamina SimpleForm | 无 | 不限 | 菜单按钮 |
| `npc` NPC 对话 | 原版 NPC 对话框（NPC 名 + 话语 + 按钮） | HologramLib | **6 个按钮** | 菜单按钮 + `npc.name` / `npc.dialogue` |
| `chest` 箱子界面 | 虚拟箱子（格子即按钮, 可显示物品名与说明） | HologramLib | 3 行 27 格 / 6 行 54 格, 超出自动分页 | 菜单按钮（`item` / `iconItem` 决定格子物品） |

三种样式共用同一套按钮语义：`command` 真执行命令、`admin_*` 查权限、`submenu` 进子菜单、
`safe` 显示项（`header`/`label`/`divider`/`spacer`）在表单外会并入正文或不显示。

---

## 1. 玩家自选样式

```
/mmenu ui       打开选择界面（跟随菜单 / 普通表单 / NPC 对话 / 箱子）
```

- 选择结果按 **xuid** 记录在 `Meowdata/MeowMenu/player_ui.json`，重启后保留
- 选「跟随菜单」(默认) → 用菜单自己配置的样式
- 选中的样式如果前置不可用（例如没装 HologramLib 却选了箱子）→ 自动回退普通表单并记一条日志
- 旧指令 `/mmenu mode` 现在等价于 `/mmenu ui`

脚本/其他插件可用：

```js
RemoteCall.importAs("MeowMenu", "setPlayerUiMode")(playerName, "chest"); // auto|form|npc|chest
const mode = RemoteCall.importAs("MeowMenu", "getPlayerUiMode")(playerName);
```

---

## 2. 固有约束（有些菜单只能走某几种样式）

样式选择**不能越过菜单自身的固有约束**：菜单可以声明自己允许被哪些样式呈现。

```json
{ "id": "admin_panel", "uiAllow": ["form"], "buttons": [ ... ] }
```

- `uiAllow` 空/不写 = 三种都允许
- **表单永远允许**（它是任何情况下的兜底渲染），所以约束实际只会"关掉" NPC 对话与箱子
- 玩家在 `/mmenu ui` 里选的样式若不在该菜单的允许范围内 → 对这个菜单不生效，按菜单自己的配置渲染
- 编辑器入口：`/mmenu admin → 选菜单 → 显示模式 → 固有约束`（勾掉 NPC/箱子即锁定为只能表单）

### 自动规则 1：启用了 DogeUI 的菜单固定为表单

`dogeui.enabled = true` 的菜单**固定用表单呈现**。DogeUI 的作用是"给表单标题加标识符前缀"，
箱子界面与 NPC 对话框都不显示这个前缀，跟着切样式会让标识符失效。所以在主菜单上开了 DogeUI 时，
玩家在 `/mmenu ui` 里选箱子/NPC 对这张菜单不生效；关掉 DogeUI 后它才参与样式选择。

### 自动规则 2：含"菜单编辑器"入口的菜单固定为表单

不用手动配，MeowMenu 会自己认出**哪张菜单是管理入口**：菜单里有按钮执行 `/mmenu admin`
（配置了命令别名也认）→ 该菜单**固定用表单（JSON UI）呈现**，玩家在 `/mmenu ui` 里选的
箱子 / NPC 对话对它不生效。启动时会打一条日志说明是哪张菜单被固定。

原因：菜单编辑器本身只能由表单承载（编辑器各页面是直接构造的表单），入口菜单跟着变成箱子或
NPC 对话框会让管理入口难以辨认，也容易在箱子里被误点。编辑器里打开
「显示模式」页时会看到提示："本菜单含菜单编辑器入口 → 已固定为表单样式"。

**为什么还需要手动 `uiAllow`**（典型例子）：

| 场景 | 原因 |
|---|---|
| **菜单编辑器本身** | 编辑器各页面（菜单列表 / 编辑 / 按钮操作等）直接构造表单，**不经过样式分派** —— 它们永远是 JSON UI，这是硬性固有行为 |
| 依赖 § 颜色码排版的菜单 | NPC 对话框会剥离 § 颜色码，颜色/加粗全丢 |
| 按钮很多的菜单 | NPC 对话框只有 6 个槽位，超出的按钮不显示 |
| 依赖图标 / dogeui 前缀的菜单 | NPC 对话框与箱子都不显示表单图标 |

示例菜单 `npc_test_admin` 就钉成了 `uiAllow: ["form"]`，可以拿它对比：把玩家样式设成「箱子」后，
其它菜单会变箱子，它仍然保持表单。

---

## 3. 菜单作者指定默认样式

编辑器：`/mmenu admin → 选菜单 → 显示模式` → 选「普通表单 / NPC 对话 / 箱子」。

手写 JSON 用 `renderMode`（`npc` 也可由 `npc.enabled` 表达）：

```json
{ "id": "shop", "renderMode": "chest", "title": "§b商店", "buttons": [ ... ] }
```

- `npc` 样式还可以配对话框内容：`npc.name`（NPC 名）/ `npc.dialogue`（话语正文，
  支持 MeowPAPI 占位符与 `{player}`，单行文本里用反斜杠 n 换行）
- `chest` 样式：每个按钮占一格，格子物品取 `item` → `iconItem` → 纸；物品名 = 按钮文本，
  说明行会标出按钮类型与用途；超过容量自动分页，末尾固定出现「← 上一页 / 下一页 →」；
  从子菜单进来时还会有一格「返回上一级」
- 编辑器里的「NPC 对话设置」页只有 `npc` 样式用得上；「显示模式」页是三种样式通用的开关

---

## 4. 三种样式的行为差异

| 项目 | 普通表单 | NPC 对话 | 箱子界面 |
|---|---|---|---|
| § 颜色码 | 支持 | **剥离**（原版对话框不解析） | 支持（物品名/说明支持染色） |
| 图标 | `icon` / `iconItem` | 不显示 | 用 `item` / `iconItem` 作为格子物品 |
| 显示项 | header/label/divider 原样排版 | 并入正文, 分隔线不显示 | 不显示 |
| 按钮上限 | 无 | 6（超出不显示并记日志） | 27 / 54（超出自动分页） |
| 子菜单 | 新开表单 | 目标菜单也是 NPC 样式 → 同一对话内推进 + 返回 | 目标菜单也是箱子样式 → 换箱子 + 返回 |
| 点击后 | 表单关闭 | 服务端立即重开（删旧载体+建新载体+Open, 同一 tick）—— **实测 0 闪烁** | 界面保持; 翻页/进子菜单走 `update`+逐格推送, 0 延迟 0 闪烁 |
| 关闭 | 表单自带 | ESC | ESC / 关闭箱子 |

---

## 5. 箱子界面的刷新与延迟

箱子界面的开销分三种，MeowMenu 按 HologramLib 提供的能力分别处理：

> NPC 对话的刷新走 `open()`（重建载体）而非 `update()`：原版界面在点按钮时客户端已自行收起，
> 只重发对话包不会重新弹出；重建载体的三步在同一 tick 到达，实测同样 0 闪烁。详见 [NPC_MENU.md](NPC_MENU.md)。

| 动作 | 怎么做的 | 延迟 / 闪烁 |
|---|---|---|
| **首次打开**（或玩家关掉后再开） | `IContainerMenu::open` | 库的打开节奏，默认 **7 tick ≈ 350ms**（`ContainerMenuSpec::openDelayTicks`；库注释：实测 7 能开、6 不行，换设备/高负载可能要调大） |
| **翻页 / 进子菜单 / 返回 / 命令后重推** | `update()` 换服务端内容与方块实体 NBT **＋ 逐格 `setItem()` 把整页推给客户端** | **无延迟、无闪烁** |
| 单个格子变化（任务打勾 / 数量、价格变化） | 导出 `setChestSlot(playerName, slot, itemType, name)`（库的 `setItem`） | **无延迟、无闪烁**，只发一条 `InventorySlot` |

### 为什么"就地换页"要 update + 逐格推送

实测发现：只调 `update()` **客户端内容不会变**。原因是库的 `update()` 发的是
「方块实体 NBT + ContainerOpen」，而客户端对**已经打开**的容器不会因为重发 ContainerOpen 去重读
方块实体（条目在打开时是按那份 NBT 填进格子的）。此时服务端已换页、客户端还是旧的一页，
**点格子就会对不上**（服务端按新页解释槽位）。

所以 MeowMenu 在 `update()` 之后再逐格调 `setItem()`：`InventorySlot` 是真实箱子同步内容用的
同一种包，客户端会就地替换该格，于是整页内容正确、无延迟、无闪烁。

### 顺带修掉的一个库 bug（逐格推送曾把整页刷成空气）

第一版逐格推送上线后实测"刷新完所有格子都空了"。根因在库的 `makeItemStack`
（`src/container/ContainerMenuManager.cpp`）：它手写 SNBT 再 `fromSnbt` 解析，而字面量里的引号被
写成了双份（`""Count""`），并且 `Count` 用了 Short。SNBT 非法 → 解析失败 → 该函数返回**空栈**，
于是逐格推送等于把每一格都设成了空气。

已改成与库内"逐字节对拍过的物品 NBT 方言"（`src/trade/TradeOfferNbt.h::itemToNbt`）一致的写法：

```
{Name:"minecraft:paper", Count:1b, Damage:0s, WasPickedUp:0b, tag:{display:{Name:"…", Lore:["…"]}}}
```

验证方式：旧 DLL 里能搜到 `""Count""` 字面量，新 DLL 里为 0 且出现 `Count:{}b,Damage:{}s`。
（对照 `update()` 只发方块实体 NBT + ContainerOpen：客户端对已打开容器不重读，所以内容不变。）

> 更省流量的做法仍在库那一侧：加一个"整容器内容"包（一次下发全部格子）替换 N 条 `InventorySlot`，
> 或让 `update()` 内部自己推一遍内容。目前 MeowMenu 用逐格方式达到同样效果。

### 兜底开关

`Meowdata/MeowMenu/config.json` 里的 `chestInPlaceRefresh`（默认 `true`）：

- `true` = 翻页/导航就地刷新（无延迟）
- `false` = 回退"整体重开"（每次付打开节奏，最稳但慢）

改完 `/mmenu reload` 生效（也可用 `setConfig("chestInPlaceRefresh", false)`）。
行数变化（3 行 ↔ 6 行，例如翻页跨到另一个菜单）时一律重新 `open`：同一个客户端箱子的槽位数
是打开时定的，就地刷新拿不到正确格子数。

---

## 6. 前置与降级

HologramLib 是**可选前置**（不链接、只运行时解析 ABI 符号）：
- 没装 → 配了 `npc` / `chest` 的菜单自动回退普通表单（启动日志一条提示），`/mmenu ui` 里对应项标灰
- 装了但版本 < `0x011B00` → 同样回退（NPC 对话与箱子界面都是这一线加入的能力域）
- `/mnpc status` 可查看两个能力域是否可用

细节与排错见 [NPC_MENU.md](NPC_MENU.md)（NPC 对话那条线写得更细：时序、6 按钮上限、点击回传格式）。
