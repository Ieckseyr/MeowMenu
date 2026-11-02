# MeowMenu LSE API 文档

MeowMenu 通过 **RemoteCall** 导出全部功能，供 LSE（Legacy Script Engine，Node.js）脚本调用。

- **命名空间**：`MeowMenu`
- **数据传递**：复杂数据（菜单/按钮/配置）一律使用 **JSON 字符串** 传递
- **基础类型**：`bool` / `string` / `int` / `float` 直接传递
- **错误处理**：所有函数对异常兜底，失败返回 `false` 或空串 `""`/`"{}"`，**不会抛出异常**

## 目录

- [调用方式](#调用方式)
- [数据结构 Schema](#数据结构-schema)
- [1. 菜单管理 API](#1-菜单管理-api)
- [2. 按钮管理 API](#2-按钮管理-api)
- [3. 渲染 API](#3-渲染-api)
- [4. 触发物品 API（硬编码 clock）](#4-触发物品-api硬编码-clock)
- [5. 自定义物品触发 API（核心）](#5-自定义物品触发-api核心)
- [6. 按钮处理 API](#6-按钮处理-api)
- [7. 配置 API](#7-配置-api)
- [8. 图标校验 API](#8-图标校验-api)
- [9. 管理员编辑器 API](#9-管理员编辑器-api)
- [10. 工具 API](#10-工具-api)
- [11. 菜单迁移 API（外部格式导入）](#11-菜单迁移-api外部格式导入)
- [完整示例](#完整示例)

---

## 调用方式

### LSE（Node.js）端

```javascript
// 导入函数
const getMenuIds    = RemoteCall.importAs("MeowMenu", "getMenuIds");
const createMenu    = RemoteCall.importAs("MeowMenu", "createMenu");
const renderMenu    = RemoteCall.importAs("MeowMenu", "renderMenu");
const registerItemTrigger = RemoteCall.importAs("MeowMenu", "registerItemTrigger");

// 调用（复杂参数用 JSON.stringify）
const ids = JSON.parse(getMenuIds());                 // ["main","nav"]
createMenu("shop", "§a商店", "欢迎");                  // true
renderMenu("Steve", "shop");                          // true
registerItemTrigger("minecraft:compass", "nav");      // true
```

### 就绪检测

MeowMenu 在 `enable()` 末尾完成 API 注册。LSE 可轮询 `isPluginReady()` 判断是否可调用：

```javascript
function waitForReady(timeoutMs = 5000) {
    return new Promise((resolve) => {
        const start = Date.now();
        const t = setInterval(() => {
            try {
                if (RemoteCall.importAs("MeowMenu", "isPluginReady")()) {
                    clearInterval(t); resolve(true); return;
                }
            } catch (e) {}
            if (Date.now() - start > timeoutMs) { clearInterval(t); resolve(false); }
        }, 100);
    });
}
```

---

## 数据结构 Schema

### Menu（菜单）

```json
{
  "id": "main",
  "title": "§l§b服务器菜单",
  "content": "欢迎来到服务器！",
  "renderMode": "default",
  "buttons": [ Button, ... ],
  "dogeui": { "enabled": false, "prefix": "/D " }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| id | string | 菜单唯一标识 |
| title | string | 标题（支持 § 颜色码、MeowPAPI 变量） |
| content | string | 内容文本 |
| renderMode | string | 菜单自身的界面样式: `default`(表单) / `npc`(NPC 对话) / `chest`(箱子); 玩家可用 `/mmenu ui` 覆盖, 见 [UI_MODES.md](UI_MODES.md) |
| npc.enabled | bool | **NPC 对话模式**：该菜单被任何入口打开都以原版 NPC 对话界面呈现（需 HologramLib 可选前置） |
| npc.name | string | 对话框里显示的 NPC 名（空 = 菜单标题） |
| npc.dialogue | string | 话语/正文（空 = `content`；单行文本里用反斜杠 n 表示换行） |
| uiAllow | string[] | **固有约束**: 该菜单允许被哪些样式呈现（`form`/`npc`/`chest`）；不写 = 三种都允许，`form` 恒允许 |
| buttons | Button[] | 按钮列表（NPC 对话模式最多显示 6 个可点击按钮） |
| dogeui.enabled | bool | 是否启用 DogeUI 标识符前缀 |
| dogeui.prefix | string | DogeUI 前缀（默认 `/D `） |

### Button（按钮）

```json
{
  "text": "§a按钮1",
  "icon": "textures/ui/icon_setting",
  "iconItem": "minecraft:dirt",
  "type": "command",
  "value": "say hi",
  "item": ""
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| text | string | 显示文本（支持 § 颜色码、MeowPAPI 变量） |
| icon | string | 图标纹理路径（如 `textures/ui/icon_setting`）。渲染优先级：icon > iconItem > 无 |
| iconItem | string | 图标物品 ID（如 `minecraft:dirt`），icon 为空时使用 |
| type | string | 按钮类型，见下表 |
| value | string | 命令字符串（command/admin_command）或子菜单 ID（submenu 等） |
| item | string | 箱子 UI 物品（本版本忽略，保留兼容） |

### Button type 取值

| type | 行为 |
|------|------|
| `command` | 以玩家身份执行 `value` 命令（支持 `{player}` 变量、MeowPAPI 变量） |
| `admin_command` | OP 权限校验后执行命令（非 OP 提示权限不足） |
| `submenu` | 打开 `value` 指定的子菜单（不存在自动创建） |
| `admin_submenu` | OP 校验后打开子菜单 |
| `folder` | 同 submenu（向后兼容） |
| `npc_menu` | **用 HologramLib 的 NPC 对话界面打开** `value` 菜单（空 = 主菜单）；子菜单在对话内逐层进入、可返回上一级。需要 HologramLib 可选前置，缺失时按钮给出提示 |
| `npc_spawn` | 在玩家身前生成一只 NPC 并绑定 `value` 菜单（空 = 主菜单），右键点击该 NPC 即打开菜单；持久化，重启后重建。需要 HologramLib |
| `header` | **显示项**：大字体标题行（渲染为表单 header，不可点击；文本用 `text` 字段） |
| `label` | **显示项**：普通文本行（渲染为表单 label，不可点击） |
| `divider` | **显示项**：水平分隔线（不可点击，`text` 字段忽略） |
| `spacer` | 表单模式跳过（不显示） |

> `npc_menu` / `npc_spawn` 详见 [NPC_MENU.md](NPC_MENU.md)。两者都复用同一套执行链：
> 对话里点 `command` 按钮照样真执行命令，`admin_*` 照样做权限校验。
> 未知的 `type` 会在点击时提示可用类型（早期版本是静默无反应）。

> **注意**：`submenu` 只会打开普通表单，不会出现 NPC。要让按钮"调用出 NPC"，
> 必须用 `npc_menu`（NPC 对话形态）或 `npc_spawn`（真的生成一只 NPC）。
> 在游戏内用 `/mmenu admin → 编辑菜单 → 添加菜单项 → NPC 菜单按钮` 可直接创建。

---

## 1. 菜单管理 API

### `getMenuIds`
获取所有菜单 ID 列表。
- **签名**：`() -> string`
- **返回**：JSON 数组，如 `["main","shop","nav"]`
- **示例**：`const ids = JSON.parse(RemoteCall.importAs("MeowMenu","getMenuIds")());`

### `getMenu`
获取菜单完整数据。
- **签名**：`(menuId: string) -> string`
- **返回**：菜单 JSON，不存在返回 `"{}"`
- **示例**：`const menu = JSON.parse(RemoteCall.importAs("MeowMenu","getMenu")("shop"));`

### `createMenu`
创建空菜单（不含按钮）。
- **签名**：(id: string, title: string, content: string) -> bool`
- **返回**：成功 true；菜单已存在返回 false
- **示例**：`RemoteCall.importAs("MeowMenu","createMenu")("shop", "§a商店", "欢迎光临");`

### `saveMenu`
整体覆盖保存菜单（含按钮），用于批量修改或导入菜单。
- **签名**：`(menuJson: string) -> bool`
- **参数**：完整的 Menu JSON 字符串
- **示例**：
```javascript
const menu = {
  id: "shop", title: "§a商店", content: "欢迎", renderMode: "default",
  buttons: [{text:"§a购买", type:"command", value:"shop buy", icon:"", iconItem:"", item:""}],
  dogeui: {enabled:false, prefix:"/D "}
};
RemoteCall.importAs("MeowMenu","saveMenu")(JSON.stringify(menu));
```

### `deleteMenu`
删除菜单（内存 + 磁盘文件）。
- **签名**：`(menuId: string) -> bool`
- **示例**：`RemoteCall.importAs("MeowMenu","deleteMenu")("shop");`

### `reloadMenus`
从磁盘重新加载所有菜单（热重载）。
- **签名**：`() -> bool`
- **示例**：`RemoteCall.importAs("MeowMenu","reloadMenus")();`

### `setMenuTitle`
仅修改菜单标题，保留其余字段（内容/按钮/DogeUI 不变）。
- **签名**：`(menuId: string, title: string) -> bool`
- **示例**：`RemoteCall.importAs("MeowMenu","setMenuTitle")("shop", "§l§a新商店标题");`

### `setMenuContent`
仅修改菜单内容文本，保留其余字段。
- **签名**：`(menuId: string, content: string) -> bool`
- **示例**：`RemoteCall.importAs("MeowMenu","setMenuContent")("shop", "§7今日特价商品");`

### `setDogeUI`
修改菜单的 DogeUI 配置（启用开关 + 标识符前缀）。
- **签名**：`(menuId: string, enabled: bool, prefix: string) -> bool`
- **参数**：`prefix` 传空串时自动使用默认值 `/D `
- **示例**：`RemoteCall.importAs("MeowMenu","setDogeUI")("shop", true, "/Shop ");`

---

## 2. 按钮管理 API

### `getButton`
获取菜单中指定位置的按钮。
- **签名**：`(menuId: string, index: int) -> string`
- **返回**：按钮 JSON，越界/不存在返回 `"{}"`

### `addButton`
向菜单追加按钮。
- **签名**：`(menuId: string, buttonJson: string) -> bool`
- **示例**：
```javascript
const btn = {text:"§a点击", type:"command", value:"say hello", icon:"textures/ui/icon_setting", iconItem:"", item:""};
RemoteCall.importAs("MeowMenu","addButton")("shop", JSON.stringify(btn));
```

### `updateButton`
替换菜单中指定位置的按钮。
- **签名**：`(menuId: string, index: int, buttonJson: string) -> bool`

### `deleteButton`
删除菜单中指定位置的按钮。
- **签名**：`(menuId: string, index: int) -> bool`

### `getButtonCount`
获取菜单按钮数量。
- **签名**：`(menuId: string) -> int`
- **返回**：按钮数量；菜单不存在返回 `-1`

### `insertButton`
在指定位置插入按钮（区别于 `addButton` 的末尾追加）。
- **签名**：`(menuId: string, index: int, buttonJson: string) -> bool`
- **说明**：`index` 越界自动钳制——`<0` 视为 `0`（头部插入），`>size` 视为末尾追加
- **示例**：
```javascript
const btn = {text:"§a置顶按钮", type:"command", value:"say top", icon:"", iconItem:"", item:""};
RemoteCall.importAs("MeowMenu","insertButton")("shop", 0, JSON.stringify(btn)); // 插到最前
```

### `moveButton`
调整按钮顺序（从 `fromIndex` 移动到 `toIndex`）。
- **签名**：`(menuId: string, fromIndex: int, toIndex: int) -> bool`
- **返回**：索引越界返回 false；`fromIndex == toIndex` 视为成功
- **示例**：`RemoteCall.importAs("MeowMenu","moveButton")("shop", 3, 0); // 第4个按钮移到最前`

### `clearButtons`
清空菜单所有按钮，保留菜单本身（标题/内容/DogeUI 不变）。
- **签名**：`(menuId: string) -> bool`
- **示例**：`RemoteCall.importAs("MeowMenu","clearButtons")("shop");`

---

## 3. 渲染 API

### `renderMenu`
为在线玩家打开指定菜单。
- **签名**：`(playerName: string, menuId: string) -> bool`
- **返回**：玩家在线且菜单存在返回 true；玩家离线返回 false

### `renderMainMenu`
为在线玩家打开配置的主菜单（`config.mainMenuId`）。
- **签名**：`(playerName: string) -> bool`

---

## 4. 触发物品 API（硬编码 clock）

管理 MeowMenu 内置的菜单触发物品（`minecraft:clock`，固定 NBT，死亡不掉落）。
该物品与「自定义物品触发」独立，右键它打开主菜单。

### `giveTriggerItem`
给予玩家菜单 clock 物品（背包已有则跳过）。
- **签名**：`(playerName: string) -> bool`

### `clearTriggerItem`
清除玩家背包中的所有菜单 clock 物品。
- **签名**：`(playerName: string) -> bool`

### `hasTriggerItem`
检查玩家背包是否拥有菜单 clock 物品。
- **签名**：`(playerName: string) -> bool`

---

## 5. 自定义物品触发 API（核心）

允许 LSE 注册「使用某物品打开某菜单」的绑定。玩家手持已注册物品右键方块/右键空气时自动打开对应菜单，复用 100ms 冷却防刷屏。

**匹配规则**：按手持物品的类型 ID（`getTypeName()`，如 `minecraft:compass`）精确匹配。与硬编码 clock 独立——clock 仍打开主菜单，自定义物品打开各自绑定的菜单。

**触发路径**：物品使用事务层（`ItemUseInventoryTransaction::$handle`：`ActionType::Use` = useitem / `ActionType::Place` = useitemon）与 LL 事件（`PlayerUseItemEvent` / `PlayerInteractBlockEvent`）两路并行，共用同一份判定与 100ms 冷却，任一路先到都只打开一次菜单。

**持久化**：绑定写入 `Meowdata/MeowMenu/item_triggers.json`，服务器重启后保留。重复注册幂等。

### `registerItemTrigger`
注册物品类型→菜单绑定。
- **签名**：`(itemType: string, menuId: string) -> bool`
- **参数**：
  - `itemType`：物品类型 ID，如 `minecraft:compass`
  - `menuId`：右键该物品时要打开的菜单 ID
- **示例**：
```javascript
// 手持指南针右键打开 nav 菜单
RemoteCall.importAs("MeowMenu","registerItemTrigger")("minecraft:compass", "nav");
```

### `unregisterItemTrigger`
解除指定物品类型的绑定。
- **签名**：`(itemType: string) -> bool`
- **返回**：原本存在并已移除返回 true

### `getItemTriggers`
获取所有绑定。
- **签名**：`() -> string`
- **返回**：JSON 对象，如 `{"minecraft:compass":"nav","minecraft:paper":"info"}`

### `clearItemTriggers`
清空所有绑定。
- **签名**：`() -> bool`

---

## 6. 按钮处理 API

### `triggerButton`
模拟玩家点击某菜单的第 N 个按钮（执行命令/打开子菜单等）。
- **签名**：`(playerName: string, menuId: string, buttonIndex: int) -> bool`
- **说明**：与玩家在表单中点击该按钮效果完全一致（含 OP 权限校验、变量替换）
- **示例**：`RemoteCall.importAs("MeowMenu","triggerButton")("Steve", "shop", 0);`

---

## 7. 配置 API

### `getConfig`
获取完整配置。
- **签名**：`() -> string`
- **返回**：配置 JSON，结构见下

```json
{
  "displayMode": "static",
  "useDogeUI": false,
  "mainMenuId": "main",
  "allowPlayerModeChoice": false,
  "triggerItem": {"type":"minecraft:clock","displayName":"菜单","snbt":"","count":1},
  "giveUs": {
    "enabled": true, "giveOnJoin": true,
    "commandName": "mmenu", "commandAlias": "", "commandDesc": "...",
    "msgReceived": "...", "msgAlreadyHave": "...", "msgInventoryFull": "...", "msgCleared": "..."
  }
}
```

### `setConfig`
设置单项配置（修改后自动保存到磁盘）。
- **签名**：`(key: string, valueJson: string) -> bool`
- **参数**：
  - `key`：配置键，支持 `displayMode` / `useDogeUI` / `mainMenuId` / `allowPlayerModeChoice` / `triggerItem.type` / `triggerItem.displayName` / `triggerItem.snbt` / `triggerItem.count` / `giveUs.enabled` / `giveUs.giveOnJoin` / `giveUs.commandName` / `giveUs.commandAlias` / `giveUs.commandDesc` / `giveUs.msgReceived` / `giveUs.msgAlreadyHave` / `giveUs.msgInventoryFull` / `giveUs.msgCleared`
  - `valueJson`：JSON 编码的值，如 `true` / `"\"main\""` / `123`
- **示例**：
```javascript
RemoteCall.importAs("MeowMenu","setConfig")("mainMenuId", JSON.stringify("shop"));
RemoteCall.importAs("MeowMenu","setConfig")("useDogeUI", JSON.stringify(true));
```

### `reloadConfig`
从磁盘重新加载配置。
- **签名**：`() -> bool`

---

## 8. 图标校验 API

基于编译时嵌入的 `texture_path.json` 数据校验图标。

### `isValidItemId`
判断是否为合法物品 ID。
- **签名**：`(itemId: string) -> bool`

### `isValidTexturePath`
判断是否为合法纹理路径。
- **签名**：`(path: string) -> bool`

### `getTexturePath`
根据物品 ID 获取对应的纹理路径。
- **签名**：`(itemId: string) -> string`
- **返回**：纹理路径，找不到返回空串

### `normalizeIcon`
规范化图标路径。
- **签名**：`(icon: string) -> string`
- **规则**：
  - 合法纹理路径 → 原样返回
  - 合法物品 ID → 转换为纹理路径
  - 以 `textures/` 开头的未知路径 → 原样返回（可能是自定义纹理）
  - 其他 → 返回空串（非法）

---

## 9. 管理员编辑器 API

打开 MeowMenu 内置的管理员表单 UI。**注意**：调用方应自行做权限控制（这些 API 内部不强制 OP 校验，但表单中的敏感操作如删除有二次确认）。

### `showEditor`
打开编辑器主界面。
- **签名**：`(playerName: string) -> bool`

### `showMenuList`
打开菜单列表界面。
- **签名**：`(playerName: string) -> bool`

### `showEditMenu`
打开指定菜单的编辑界面。
- **签名**：`(playerName: string, menuId: string) -> bool`

---

## 10. 工具 API

### `formatMessage`
格式化消息模板（替换 `{item}` → 触发物品显示名，`{cmd}` → 命令名）。
- **签名**：`(template: string) -> string`
- **示例**：`RemoteCall.importAs("MeowMenu","formatMessage")("§a你获得了{item}");`

### `isPluginReady`
检查 MeowMenu 是否已启用并完成 API 注册。
- **签名**：`() -> bool`

---

## 11. 菜单迁移 API（外部格式导入）

支持自动识别并转换以下外部菜单插件格式到 MeowMenu 标准格式：

| 格式 | 识别特征 | 来源 |
|------|----------|------|
| **PMenu** | 对象 `{title, content, buttons:[...]}` | PMenu 插件 |
| **CMENU** | 数组 `[{name, open, type}]`，type 仅 comm/opcomm/form/opform | CMENU 插件 |
| **CD（旧）** | 数组 `[{name, open, type, adminOnly, image, subtitle}]` | CD 菜单插件 |
| **CD（新）** | `{ddui, items:[...]}`，items 同 CD 旧格式 | CD 菜单插件 |

**按钮 type 转换映射**：

| 外部 type | MeowMenu type |
|-----------|---------------|
| `form` | `submenu` |
| `opform` / `opfm` | `admin_submenu` |
| `comm`（无 adminOnly） | `command` |
| `comm`（有 adminOnly）/ `opcomm` / `console` / `opcm` / `vipcm` | `admin_command` |
| `vipfm` / `vipform` | `submenu` |
| `divider` / `label` | 跳过（不导入） |

转换后的菜单标题：CD/CMENU 为 `§l§b<menuId>`，PMenu 沿用原 `title`。所有图标经 `normalizeIcon` 修复非法值。

### `autoMigrateMenus`
手动触发扫描 `Meowdata/MeowMenu/migrations/` 目录下的 `.json` 文件，自动识别格式、转换、保存到 `data/`，导入成功的文件移至 `migrations/done/`。
- **签名**：`() -> int`
- **返回**：成功导入的菜单数量
- **说明**：插件启动时已自动调用一次，此 API 供运行时手动重新扫描
- **覆盖规则**：
  - 普通菜单：目标已存在则跳过（不覆盖），文件保留在 `migrations/`
  - **主菜单特例**：若主菜单仍为初始状态（标题/内容未修改），迁移版本会**直接替换**它，文件移至 `migrations/done/`；若主菜单已被管理员自定义，则跳过
- **示例**：`const n = RemoteCall.importAs("MeowMenu","autoMigrateMenus")(); // 2`

### `importMenu`
直接传入外部格式 JSON 字符串，自动识别格式并转换为 MeowMenu 菜单后保存。
- **签名**：`(menuId: string, formatJson: string) -> bool`
- **参数**：
  - `menuId`：目标菜单 ID（**已存在则覆盖**，与 `autoMigrateMenus` 的跳过语义不同）
  - `formatJson`：外部格式 JSON 字符串（PMenu/CMENU/CD旧/CD新任一）
- **返回**：转换并保存成功返回 true；无法识别格式或解析失败返回 false
- **示例**：
```javascript
// 导入一个 CD 旧格式菜单
const cdJson = JSON.stringify([
  {name:"§a生存", open:"survival", type:"form"},
  {name:"§c封禁", open:"ban Steve", type:"opcomm", adminOnly:true, image:"textures/ui/icon_setting"}
]);
RemoteCall.importAs("MeowMenu","importMenu")("mycd", cdJson); // true

// 导入一个 PMenu 格式菜单
const pmenuJson = JSON.stringify({
  title:"§l§6我的菜单",
  content:"§7请选择",
  buttons:[
    {text:"§a回家", command:"home", type:"comm"},
    {text:"§c管理", command:"kick", type:"opcm"}
  ]
});
RemoteCall.importAs("MeowMenu","importMenu")("mypmenu", pmenuJson); // true
```

---

## 12. NPC 对话菜单 API（HologramLib 可选前置）

把菜单用 HologramLib 的原版 NPC 对话界面呈现，并把 NPC 点击绑定到菜单。
**HologramLib 不是前置依赖**：未安装或 API 版本不足时 `isNpcMenuAvailable()` 返回 `false`，
其余调用返回 `false`/`0`/空串，MeowMenu 其他功能不受影响。完整说明见 [NPC_MENU.md](NPC_MENU.md)。

| 函数 | 签名 | 说明 |
|---|---|---|
| `setMenuNpcMode` | `(menuId: string, enabled: bool, npcName: string, dialogue: string) -> bool` | **设置菜单的 NPC 对话模式**（开启后该菜单被任何入口打开都以 NPC 对话界面呈现；话语里用反斜杠 n 表示换行） |
| `getMenuNpcMode` | `(menuId: string) -> string` | 读模式配置 JSON（含 `clickableButtons` 与 `maxButtons: 6`） |
| `isNpcMenuAvailable` | `() -> bool` | 前置是否已连接且版本达标 |
| `getNpcMenuStatus` | `() -> string` | 连接状态 / ABI 版本 / 绑定数量 |
| `openNpcMenuDialog` | `(playerName: string, menuId: string) -> bool` | 以 NPC 对话打开菜单（`menuId` 空串 = 主菜单） |
| `closeNpcMenuDialog` | `(playerName: string) -> bool` | 关闭该玩家的 NPC 对话 |
| `openChestMenu` | `(playerName: string, menuId: string) -> bool` | **以箱子界面打开菜单**（虚拟容器, 格子即按钮, 超容量自动分页） |
| `closeChestMenu` | `(playerName: string) -> bool` | 关闭该玩家的箱子界面 |
| `setChestSlot` | `(playerName: string, slot: int, itemType: string, name: string) -> bool` | 按槽动态刷新当前箱子界面的一格（无延迟无闪烁；`itemType` 空串 = 清空该格） |
| `getPlayerUiMode` | `(playerName: string) -> string` | 玩家自选界面样式: `auto` / `form` / `npc` / `chest` |
| `setPlayerUiMode` | `(playerName: string, mode: string) -> bool` | 设置玩家自选样式（玩家也可用 `/mmenu ui` 自己改） |
| `getAllPlayerUiModes` | `() -> string` | 全部玩家样式偏好 JSON |
| `spawnNpcMenuAt` | `(npcName: string, menuId: string, x: float, y: float, z: float, dim: int) -> int` | 指定坐标生成"点击即打开菜单"的 NPC；返回库内 id，`<0` 失败（`-2` id 占用 / `-3` 皮肤未注册） |
| `bindNpcMenu` | `(npcId: int64, menuId: string) -> bool` | 把已有 HologramLib NPC 绑定到菜单 |
| `removeNpcMenu` | `(npcId: int64) -> bool` | 删除 NPC 及其绑定 |
| `getNpcMenus` | `() -> string` | 绑定列表 JSON |
| `reloadNpcMenus` | `() -> bool` | 重读 `npc_menus.json` 并重建 NPC |

```javascript
// 前置可用时才用 NPC 对话形态打开菜单
const available = RemoteCall.importAs("MeowMenu", "isNpcMenuAvailable");
if (available()) {
    RemoteCall.importAs("MeowMenu", "openNpcMenuDialog")(player.realName, "main");
}
```

---

## 完整示例

参见 `scripts/lse_example/index.js`。典型流程：

```javascript
// 1. 等待就绪
await waitForReady();

// 2. 创建菜单 + 添加按钮
const createMenu = RemoteCall.importAs("MeowMenu", "createMenu");
const addButton  = RemoteCall.importAs("MeowMenu", "addButton");
createMenu("nav", "§b导航菜单", "选择目的地");
addButton("nav", JSON.stringify({
    text: "§a主城", type: "command", value: "tp 0 100 0",
    icon: "textures/ui/icon_home", iconItem: "", item: ""
}));

// 3. 注册物品触发（手持指南针打开导航菜单）
RemoteCall.importAs("MeowMenu", "registerItemTrigger")("minecraft:compass", "nav");

// 4. 主动为玩家打开菜单
RemoteCall.importAs("MeowMenu", "renderMenu")("Steve", "nav");
```

## 注意事项

1. **玩家离线**：所有需要 `playerName` 的 API 对离线玩家返回 `false`。
2. **并发安全**：菜单/按钮操作通过 `MenuStore` 的读写锁保护，但建议避免同时多次修改同一菜单。
3. **图标自动修复**：`addButton`/`updateButton`/`saveMenu` 会自动调用 `normalizeIcon` 修复非法图标路径。
4. **自定义触发不影响 clock**：硬编码 `minecraft:clock` 始终打开主菜单，自定义物品触发是独立机制。
5. **冷却**：物品触发（含自定义）共享 100ms 冷却，防止右键连点刷屏。
