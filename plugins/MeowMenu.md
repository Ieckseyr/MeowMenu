# MeowMenu 文档

## 简介

MeowMenu 是一个基于 Meow 插件模板的可视化菜单管理插件，为 Minecraft 基岩版服务器（LeviLamina/LiteLoaderBDS）提供完整的菜单系统。

支持 DDUI 动态表单与 LSE 静态表单双模式渲染、游戏内可视化菜单编辑器、BetterSidebar 变量集成、轮播文字动画、视频帧动画播放、自定义触发物品、入服自动给予物品等功能。

- 作者：伊希娅
- 版本：1.0.0
- 文件位置：`plugins/Meow/plugins/MeowMenu.js`

---

## 前置依赖

### DDUI-lseAPI

- 仓库：https://github.com/Ieckseyr/DDUI-lseAPI
- 类型：C++ 插件（LeviLamina mod）

DDUI（Dynamic Data User Interface）是一个为基岩版服务器提供动态表单能力的 C++ 底层插件。原版 LSE 表单发送后内容固定，无法实时更新；DDUI 通过 Hook 表单发送流程，提供了 `sendUpdate` 能力，允许在表单打开期间动态推送内容更新。

MeowMenu 通过 `plugins/Meow/src/DDUIAPI.js` 封装层调用 DDUI 的底层导出函数，主要使用以下能力：

| 能力 | 说明 |
|------|------|
| `InterceptedForm` | 拦截表单关闭，实现按钮点击后表单不消失的持久交互 |
| `sendRawUpdate` / `updateIntercepted` | 向已打开的表单推送 JSON 更新，实现信息栏刷新、轮播动画、视频逐帧播放 |
| `SimpleForm` / `CustomForm` / `ModalForm` | DDUI 模式下的表单创建 API |

> DDUI 为可选依赖。未安装时 MeowMenu 自动回退到静态表单模式，核心菜单功能不受影响，但信息栏动态刷新、轮播动画循环、视频播放等动态特性将不可用。

### DogeUI

- 仓库：https://github.com/DogeLakeDev/DogeUI
- 类型：资源包（Resource Pack）
- 协议：GPL-3.0

DogeUI 是一个基岩版客户端 UI 美化资源包，通过修改原版 JSON UI 定义文件，为服务器表单提供更美观的视觉样式。它重新设计了 SimpleForm 的布局和外观，使菜单界面更加现代化。

MeowMenu 对 DogeUI 的集成方式非常简单：当配置中 `useDogeUI` 为 `true` 时，主菜单的标题会自动添加 `/D ` 前缀。DogeUI 资源包识别到该前缀后，会对该表单应用特殊的 UI 布局样式。

- 在编辑器的「基础设置」中可开关 DogeUI 选项
- 仅对主菜单生效，子菜单不受影响
- DogeUI 为可选依赖，未安装资源包时 `/D ` 前缀会直接显示在标题中（无副作用，但建议关闭）

### 其他可选依赖

| 依赖 | 说明 |
|------|------|
| BetterSidebar | 提供变量系统，菜单内容可引用 `{yoyo.tps}` 等动态变量（需要添加 `resolveForPlayer` 方法，见下方说明） |
| BEPlaceholderAPI (PAPI) | MeowMenu 通过 BetterSidebar 注册 PAPI 变量供其他插件使用 |
| ffmpeg | 视频帧数据由外置解析工具使用，MeowMenu 本身不直接调用 |

### BetterSidebar 兼容性补丁

MeowMenu 需要调用 BetterSidebar 的变量解析能力，但原版 BetterSidebar 只暴露了 `replaceTokens(str, modifier)` 方法，其中 `modifier` 必须是一个函数。MeowMenu 需要一个更便捷的接口：传入字符串和玩家对象即可自动解析变量。

如果你使用的是原版 BetterSidebar.js，需要手动添加 `resolveForPlayer` 方法，否则会报错：
```
WARNING: [Y]调用扩展 "BetterSidebar.js" 的 "replaceTokens" 接口 出现问题: "modifier is not a function"
```

#### 添加位置

打开 `plugins/Meow/plugins/BetterSidebar.js`，找到文件末尾的 `exports.API` 对象：

```javascript
exports.API = {
  replaceTokens(str, modifier) { return sidebar.replaceTokens(str, modifier); },
  reloadConfig() { sidebar.reloadConfig(); },
  // ... 其他方法
}; //方法仅需插入即可
```

#### 添加代码

在 `replaceTokens` 方法后面、`reloadConfig` 方法前面，插入以下代码：

```javascript
exports.API = {
  replaceTokens(str, modifier) { return sidebar.replaceTokens(str, modifier); },

  // ↓↓↓ 新增：供 MeowMenu 等外部插件调用的便捷方法 ↓↓↓
  /**
   * 便捷方法：传入字符串和玩家对象，自动构造 modifier 解析所有变量
   * @param {string} str 包含变量占位符的字符串
   * @param {Object} player 玩家对象
   * @returns {string} 解析后的字符串
   */
  resolveForPlayer(str, player) {
    try {
      const dv = player.getDevice();
      const { yoyo, date, extend } = sidebar.variate;
      const options = { yoyo, date, pl: player, dv, extend };
      return sidebar.replaceTokens(str, (token) => sidebar.variableResolver(token, options));
    } catch (e) {
      return str;
    }
  },
  // ↑↑↑ 新增结束 ↑↑↑

  reloadConfig() { sidebar.reloadConfig(); },
  // ... 其余方法保持不变
};
```

#### 原理说明

- `sidebar.variate` 包含 `yoyo`（服务器信息如TPS）、`date`（日期时间）、`extend`（扩展变量Map）
- `player.getDevice()` 获取玩家设备信息，用于 `{dv.xxx}` 类变量
- 将这些组装为 `options` 对象后，调用 `sidebar.variableResolver(token, options)` 即可解析所有类型的变量占位符
- 外层 try-catch 确保解析失败时返回原始字符串，不会导致崩溃

---

## 数据存储

所有数据存储在 `./Meowdata/MeowMenu/` 目录下：

| 路径 | 说明 |
|------|------|
| `./Meowdata/MeowMenu/config.json` | 全局配置文件（显示模式、触发物品、GiveUs设置等） |
| `./Meowdata/MeowMenu/data/` | 菜单数据文件，每个菜单一个 `{menuId}.json` |
| `./Meowdata/MeowMenu/Video/` | 视频帧数据文件 `{videoId}_frames.json` |
| `./Meowdata/MeowMenu/migrations/` | 菜单迁移目录（放入CD/PMenu格式JSON自动导入） |

---

## 指令

| 指令 | 权限 | 说明 |
|------|------|------|
| `/mmenu` | 所有玩家 | 打开主菜单 |
| `/mmenu admin` | OP | 打开菜单编辑器 |
| `/clear{cmdName}` | 所有玩家 | 清除触发物品（默认 `/clearmmenu`） |

> `/mmenu` 同时会尝试注册为 GiveUs 配置中的命令名（默认也是 `mmenu`）。

---

## 触发方式

1. 执行 `/mmenu` 命令
2. 右键使用触发物品（默认为钟 `minecraft:clock`）
   - 同时监听 `onUseItem` 和 `onUseItemOn` 事件

---

## 配置文件结构

`config.json` 全局配置：

```jsonc
{
  // 显示模式: "ddui"（动态表单）或 "static"（静态表单）
  "displayMode": "ddui",

  // 是否对主菜单启用 DogeUI 布局（标题加 /D 前缀）
  "useDogeUI": false,

  // 主菜单ID，对应 data/ 目录下的菜单文件
  "mainMenuId": "main",

  // 触发物品配置
  "triggerItem": {
    "type": "minecraft:clock",        // 物品类型ID
    "displayName": "菜单",            // 显示名称
    "snbt": "{\"Count\":1b,...}",     // SNBT数据（死亡不掉落）
    "count": 1
  },

  // 入服自动给予配置
  "giveUs": {
    "enabled": true,                   // 是否启用
    "giveOnJoin": true,                // 入服自动给予
    "command": {
      "name": "mmenu",                // 命令名
      "alias": "",
      "description": "§a打开§b服务器§e菜单"
    },
    "messages": {
      "received": "§6你§a获得了§c一个§e{item}§6。",
      "alreadyHave": "§6你§e背包里§c已经§a有{item}了",
      "inventoryFull": "§6你的§b背包§d满了...",
      "cleared": "§a已清除你的{item}"
    }
  }
}
```

消息模板支持的占位符：
- `{item}` — 触发物品显示名称
- `{cmd}` — 命令名称
- `{player}` — 玩家名（仅在命令执行时可用）

---

## 菜单数据结构

每个菜单存储为 `data/{menuId}.json`：

```jsonc
{
  "id": "main",
  "title": "§l§b服务器菜单",          // 支持§颜色代码和BetterSidebar变量
  "content": "欢迎来到服务器！",       // 显示在按钮上方的文字
  "buttons": [
    {
      "text": "§a商店",               // 按钮显示文字
      "icon": "textures/ui/icon",     // 图标路径（可选）
      "type": "submenu",              // 按钮类型
      "value": "shop_menu"            // 命令字符串或子菜单ID
    }
  ],
  "infoBar": {                         // 信息栏配置
    "enabled": true,
    "items": ["online_count", "player_ping"],
    "order": ["online_count", "player_ping"]
  },
  "carousel": {                        // 轮播动画配置
    "enabled": false,
    "texts": ["公告1", "公告2"],
    "interval": 2000                   // 切换间隔（毫秒）
  },
  "video": {                           // 视频动画配置
    "enabled": false,
    "videoPath": "",
    "videoId": ""                      // 对应 Video/{videoId}_frames.json
  }
}
```

### 按钮类型

| 类型 | 说明 |
|------|------|
| `command` | 以玩家身份执行命令（使用 `player.runcmd`） |
| `admin_command` | 检查 OP 权限后执行命令，无权限则提示 |
| `submenu` | 打开指定子菜单（目标不存在时自动创建） |
| `admin_submenu` | 检查 OP 权限后打开子菜单 |
| `folder` | 创建文件夹——自动生成一个新子菜单并关联 |

---

## 显示模式

### DDUI 模式（推荐）

- 通过 `require('../src/DDUIAPI')` 引入 DDUIAPI 封装层
- 使用 `InterceptedForm.createSimple` 渲染，按钮点击后表单不关闭
- 支持 `sendUpdate` / `updateIntercepted` 动态刷新内容
- 信息栏每秒自动刷新
- 轮播动画按配置间隔循环切换
- 视频动画以帧率逐帧播放

### 静态模式

- 使用 LSE 原生 `mc.newSimpleForm` 渲染
- 轮播动画仅显示第一条文字
- 如果 DDUI Raw API 可用，信息栏仍可动态刷新
- 视频动画不可用

### 自动回退

当配置为 DDUI 模式但 API 不可用时，自动回退到静态模式并在控制台输出警告。

---

## 信息栏

信息栏在菜单内容下方显示实时信息，支持三种来源：

### 内置信息项

| ID | 名称 | 显示内容 |
|----|------|---------|
| `online_count` | 在线人数 | `§5在线玩家: §aX人` |
| `player_level` | 玩家等级 | `§5等级: §bX级` |
| `player_coords` | 玩家坐标 | `§5坐标: §3x, y, z` |
| `player_device` | 设备信息 | `§5设备: §eWin10` 等 |
| `player_ping` | 网络延迟 | `§5延迟: §eXms` |
| `player_gamemode` | 游戏模式 | `§5模式: §b生存` 等 |
| `player_health` | 生命值 | `§5生命: §cX/X` |
| `player_hunger` | 饥饿值 | `§5饥饿: §6X` |
| `player_dimension` | 所在维度 | `§5维度: §d主世界` 等 |

### BetterSidebar 扩展变量

ID 格式为 `sidebar:{extName}`，显示 `{E:extName}` 解析后的值。

### 外部插件注册

通过 API 的 `registerInfoItem(id, name, getter)` 注册自定义信息项。

---

## 轮播动画

- 在编辑器中配置轮播文字数组和切换间隔
- DDUI 模式：按 `Math.floor(elapsed / interval) % texts.length` 循环切换
- 静态模式：仅显示第一条
- 支持 `|` 分隔符实现单条内轮换，如 `"公告1|公告2|公告3"`

---

## 视频动画

视频帧数据由外置工具（如 DDUIVideoPlayer / MeowVideoParser）预先生成。

MeowMenu 仅负责读取和播放：

1. 将帧数据文件放入 `./Meowdata/MeowMenu/Video/{videoId}_frames.json`
2. 在编辑器中启用视频动画并选择视频
3. 打开菜单时自动以配置帧率逐帧播放

帧数据文件格式：

```jsonc
{
  "videoId": "badapple",
  "fps": 5,
  "frameCount": 50,
  "width": 20,
  "height": 11,
  "frames": [
    "§0█§f█...\n§0█§f█...",   // 每帧为带§颜色代码的文本
    // ...
  ]
}
```

> 需要 DDUI 模式支持。静态模式下视频不可用。

---

## 菜单迁移

支持从 CD 和 PMenu 格式自动迁移：

1. 将原格式的菜单 JSON 文件复制到 `./Meowdata/MeowMenu/migrations/` 目录
2. 重启服务器，插件自动识别格式并转换导入
3. 导入成功的文件移动到 `migrations/done/` 避免重复

支持的格式：
- CD 格式（旧版数组格式、新版 `{items:[...]}` 格式）
- PMenu 格式（`{title, content, buttons:[...]}` 格式）

---

## BetterSidebar 变量集成

- 菜单标题和内容中可使用 BetterSidebar 变量占位符，如 `{yoyo.tps}`、`{pl.realName}`
- 渲染时自动调用 `$Y.i.BetterSidebar.replaceTokens` 解析
- BetterSidebar 未加载时保留原始占位符文本
- 插件加载完成后自动注册 PAPI 变量：
  - `{E:meowmenu.menu_count}` — 菜单总数
  - `{E:meowmenu.display_mode}` — 当前显示模式
  - `{E:meowmenu.main_menu_title}` — 主菜单标题

---

## 入服自动给予（GiveUs）

完全内嵌了 GiveUs 插件的核心逻辑：

- 玩家加入服务器时延迟 500ms 检查背包
- 背包中无触发物品则自动给予（支持 SNBT 精确匹配）
- 背包已满时发送提示消息
- 默认物品 SNBT 包含 `minecraft:keep_on_death` 标签（死亡不掉落）

---

## 导出 API

其他 Meow 插件可通过 `$Y.i.MeowMenu` 调用以下接口：

```javascript
// 获取全局配置
$Y.i.MeowMenu.getConfig()

// 向玩家渲染指定菜单
$Y.i.MeowMenu.renderMenu(player, menuId)

// 获取菜单数据
$Y.i.MeowMenu.getMenu(menuId)
$Y.i.MeowMenu.getMainMenu()

// 菜单 CRUD
$Y.i.MeowMenu.createMenu(menuId, title, content)
$Y.i.MeowMenu.deleteMenu(menuId)
$Y.i.MeowMenu.addButton(menuId, button)
$Y.i.MeowMenu.removeButton(menuId, index)

// 打开编辑器
$Y.i.MeowMenu.showEditor(player)

// 解析变量
$Y.i.MeowMenu.resolveVariables(text, player)

// 注册/移除自定义信息栏项
$Y.i.MeowMenu.registerInfoItem(id, name, getter)
$Y.i.MeowMenu.removeInfoItem(id)
```

### registerInfoItem 示例

```javascript
$Y.i.MeowMenu.registerInfoItem(
  'my_money',
  '玩家余额',
  (player) => `§5余额: §e${getBalance(player)}`
);
```

---

## 编辑器功能

通过 `/mmenu admin` 打开，提供以下功能：

- 菜单列表 — 查看和管理所有菜单
- 基础设置 — 显示模式切换、DogeUI开关、入服给予开关
- 物品设置 — 修改触发物品类型、名称、SNBT
- 从手持物品设置 — 直接读取手中物品信息作为触发物品

菜单编辑界面：
- 编辑标题/内容
- 添加/编辑/删除/排序按钮
- 信息栏显示设置（选择启用项、调整排列顺序）
- 轮播动画设置（添加/管理文字、设置间隔）
- 视频动画设置（选择已有帧数据文件）
- 删除菜单

---

## 配置热重载

插件启动后自动监听文件变更：
- `config.json` 变更时自动重载全局配置
- `data/` 目录下菜单文件变更时自动重载菜单缓存
- 使用 500ms 防抖避免频繁重载

---

## 生命周期

| 阶段 | 执行内容 |
|------|---------|
| `onAllLoaded` | 加载配置 → 执行菜单迁移 → 确保主菜单存在 → 注册PAPI变量 → Hook BetterSidebar.setExtend |
| `onServerStarted` | 注册指令 → 注册GiveUs指令 → 监听物品使用事件 → 监听玩家加入/离开事件 → 启动文件监听 |
| 玩家离开 | 停止该玩家的信息栏刷新、轮播动画、视频播放定时器 |
