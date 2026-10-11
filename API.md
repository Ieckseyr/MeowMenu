# MeowMenu · API 参考

所有能力经 **RemoteCall** 导出，命名空间 `MeowMenu`。
复杂数据（菜单 / 按钮 / 配置）一律用 **JSON 字符串** 传递；失败一律返回 `false` / `0` / `""`，不抛异常。

LSE（Node.js）调用示例：

```js
const RemoteCall = require("RemoteCall");

const getMenuIds = RemoteCall.importAs("MeowMenu", "getMenuIds");
const ids = JSON.parse(getMenuIds());               // ["main", "shop", ...]

const addButton = RemoteCall.importAs("MeowMenu", "addButton");
addButton("main", JSON.stringify({ text: "§a签到", type: "command", value: "say hi" }));
```

> 需要 HologramLib 的接口（NPC 对话 / NPC 生成 / 箱子界面）在前置缺失时返回
> `false` / `""` / `0`，可先用 `isNpcMenuAvailable()` 探测。

---

## 菜单管理

| 接口 | 说明 |
|------|------|
| `getMenuIds()` → string | 全部菜单 ID，JSON 数组 |
| `getMenu(menuId)` → string | 菜单 JSON（不存在返回 `"{}"`） |
| `createMenu(id, title, content)` → bool | 新建菜单（默认一级） |
| `createMenuWithLevel(id, title, content, level)` → bool | 新建并指定分级（1 = 一级, 2 = 二级） |
| `saveMenu(menuJson)` → bool | 整体覆盖保存（含按钮） |
| `deleteMenu(menuId)` → bool | 删除菜单 |
| `reloadMenus()` → bool | 重读 `data/*.json` |
| `setMenuTitle(menuId, title)` → bool | 仅改标题 |
| `setMenuContent(menuId, content)` → bool | 仅改内容 |
| `setDogeUI(menuId, enabled, prefix)` → bool | 改 DogeUI 配置 |
| `setMenuLevel(menuId, level)` → bool | 改分级（二级不被触发物品打开） |
| `getMenuLevel(menuId)` → int | 1 / 2（不存在返回 -1） |
| `setMenuTriggerItem(menuId, itemType, displayName)` → bool | 设置一级菜单的触发物品（空串 = 清除） |
| `getMenuByTriggerItem(itemType)` → string | 按触发物品类型查一级菜单 ID（无匹配返回 `""`） |

## 按钮管理

| 接口 | 说明 |
|------|------|
| `getButton(menuId, index)` → string | 按钮 JSON（越界返回 `"{}"`） |
| `addButton(menuId, buttonJson)` → bool | 追加按钮 |
| `updateButton(menuId, index, buttonJson)` → bool | 覆盖指定按钮 |
| `deleteButton(menuId, index)` → bool | 删除按钮 |
| `getButtonCount(menuId)` → int | 按钮数量 |
| `insertButton(menuId, index, buttonJson)` → bool | 在指定位置插入（index 越界自动钳制） |
| `moveButton(menuId, fromIndex, toIndex)` → bool | 调整按钮顺序 |
| `clearButtons(menuId)` → bool | 清空按钮（保留菜单本身） |

## 渲染

| 接口 | 说明 |
|------|------|
| `renderMenu(playerName, menuId)` → bool | 以该菜单配置的样式打开（NPC/箱子需要 HologramLib） |
| `renderMainMenu(playerName)` → bool | 打开主菜单 |
| `triggerButton(playerName, menuId, buttonIndex)` → bool | 代为点击某个按钮（执行其命令/进入子菜单） |

## 触发物品（主菜单的"钥匙"）

| 接口 | 说明 |
|------|------|
| `giveTriggerItem(playerName)` → bool | 发放触发物品（已有则跳过） |
| `clearTriggerItem(playerName)` → bool | 清除玩家背包里的触发物品 |
| `hasTriggerItem(playerName)` → bool | 是否持有 |

## 自定义物品触发

| 接口 | 说明 |
|------|------|
| `registerItemTrigger(itemType, menuId)` → bool | 注册"手持该物品右键 → 打开该菜单"（持久化） |
| `unregisterItemTrigger(itemType)` → bool | 解除绑定 |
| `getItemTriggers()` → string | 全部绑定 JSON：`{"itemType":"menuId"}` |
| `clearItemTriggers()` → bool | 清空全部绑定 |

## 配置

| 接口 | 说明 |
|------|------|
| `getConfig()` → string | 配置 JSON |
| `setConfig(key, valueJson)` → bool | 改单项（值为 JSON 编码，如 `true` / `"main"` / `123`），自动保存 |
| `reloadConfig()` → bool | 重读 `config.json` |
| `formatMessage(template)` → string | 替换消息模板里的 `{item}` / `{cmd}` 占位符 |

## 图标校验

| 接口 | 说明 |
|------|------|
| `isValidItemId(itemId)` → bool | 是否为有效物品 ID |
| `isValidTexturePath(path)` → bool | 是否为有效纹理路径 |
| `getTexturePath(itemId)` → string | 物品 ID → 纹理路径 |
| `normalizeIcon(icon)` → string | 规范化图标（物品 ID 转纹理路径，非法清空） |

## 编辑器

| 接口 | 说明 |
|------|------|
| `showEditor(playerName)` → bool | 打开编辑器主界面（需 OP） |
| `showMenuList(playerName)` → bool | 打开菜单列表 |
| `showEditMenu(playerName, menuId)` → bool | 打开指定菜单的编辑页 |

## NPC 对话菜单（需 HologramLib）

| 接口 | 说明 |
|------|------|
| `isNpcMenuAvailable()` → bool | 前置是否已连接且版本达标 |
| `getNpcMenuStatus()` → string | 连接状态 / ABI 版本 / 监听 token / 绑定数量 |
| `openNpcMenuDialog(playerName, menuId)` → bool | 以 NPC 对话界面打开（menuId 空 = 主菜单） |
| `closeNpcMenuDialog(playerName)` → bool | 关闭该玩家的 NPC 对话 |
| `setMenuNpcMode(menuId, enabled, npcName, dialogue)` → bool | 把菜单设为/取消"NPC 对话模式"（任何入口打开都以对话呈现） |
| `setMenuNpcAvatar(menuId, avatarType, avatarVariant)` → bool | 聊天框头像/载体：换实体类型（`minecraft:zombie` 等）或 NPC 内置皮肤变体（0..59；-1 = 默认） |
| `getMenuNpcMode(menuId)` → string | JSON：`{"enabled","name","dialogue","avatarType","avatarVariant","maxButtons","clickableButtons"}` |

## NPC 生成与绑定（需 HologramLib）

| 接口 | 说明 |
|------|------|
| `spawnNpcMenuAt(npcName, menuId, x, y, z, dim)` → int | 在坐标生成"点击即打开菜单"的 NPC（持久化，重启重建成）；返回库内 NPC id，`<0` 失败 |
| `bindNpcMenu(npcId, menuId)` → bool | 把已有 HologramLib NPC 绑定到菜单 |
| `removeNpcMenu(npcId)` → bool | 删除 NPC 及其绑定 |
| `getNpcMenus()` → string | JSON 数组：`[{id,name,menu,x,y,z,dim,skin}, ...]` |
| `reloadNpcMenus()` → bool | 重读 `npc_menus.json` 并重建 NPC |

## 箱子界面（需 HologramLib）

| 接口 | 说明 |
|------|------|
| `openChestMenu(playerName, menuId)` → bool | 以箱子界面打开菜单（物品格 = 按钮） |
| `setChestSlot(playerName, slot, itemType, name)` → bool | 按槽刷新一格（`itemType` 空串 = 清空；无延迟无闪烁） |
| `closeChestMenu(playerName)` → bool | 关闭该玩家的箱子界面 |

## 玩家界面偏好

| 接口 | 说明 |
|------|------|
| `getPlayerUiMode(playerName)` → string | `auto` / `form` / `npc` / `chest` |
| `setPlayerUiMode(playerName, mode)` → bool | 设置玩家自选样式（`auto` = 跟随菜单） |
| `getAllPlayerUiModes()` → string | 全部玩家偏好 JSON |

## 工具与迁移

| 接口 | 说明 |
|------|------|
| `isPluginReady()` → bool | 插件是否已就绪 |
| `autoMigrateMenus()` → int | 扫描 `migrations/` 导入 CD / PMenu / CMENU 格式，返回导入数量 |
| `importMenu(menuId, formatJson)` → bool | 直接转换外部格式 JSON 并保存（已存在则覆盖） |
