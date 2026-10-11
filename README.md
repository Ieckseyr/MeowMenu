# MeowMenu

BDS / LeviLamina 服务器菜单插件（C++ 实现，替代早期 JS 版 —— 旧版见 `legacy-js` 分支）。

菜单数据与早期 JS 版格式兼容，可直接沿用 `Meowdata/MeowMenu/data/*.json`。

## 功能

- **表单菜单**：LeviLamina Form API 渲染，每菜单可独立配置 DogeUI 标识符前缀
- **NPC 对话菜单**：菜单可呈现为原版 NPC 对话框（多层级、逐层进入/返回、超过 6 个按钮自动分页）
- **箱子界面**：菜单可呈现为虚拟容器（物品格 = 按钮，翻页/进子菜单就地刷新，无延迟无闪烁）
- **触发物品**：手持指定物品右键打开菜单；主菜单的"钥匙"（默认钟）进服自动发放一次
- **自定义触发**：LSE / 其它插件可注册"某物品 → 某菜单"的绑定
- **游戏内编辑器**：`/mmenu admin` 可视化增删改菜单、按钮、图标、分级、触发物品
- **玩家界面偏好**：玩家可用 `/mmenu ui` 自选喜欢的呈现方式（表单 / NPC 对话 / 箱子）
- **RemoteCall 导出**：全部能力经 RemoteCall 暴露给 LSE（Node.js）与其它插件，见 [API.md](./API.md)
- **迁移导入**：自动识别并导入 PMenu / CMENU / CD 旧新格式的菜单文件
- **玩家进服发放记录**：发放成功即记账（`players.json`），之后进服直接跳过，失败自动重试

## 环境要求

- BDS **1.26.40**（协议 2168）+ **LeviLamina 26.40**
- **LegacyRemoteCall**（RemoteCall API）
- **HologramLib**（可选前置）—— NPC 对话菜单 / 箱子界面 / NPC 生成需要；缺失时本插件照常加载，仅这几项功能禁用
- 变量翻译（`%name%` / `{name}` / `{player}`）内置，无需额外安装

## 安装

把 `MeowMenu.dll` 与 `manifest.json` 放进 `plugins/MeowMenu/`，重启服务器。
首次启动会在 `Meowdata/MeowMenu/` 生成默认配置与主菜单。

## 命令

| 命令 | 权限 | 说明 |
|------|------|------|
| `/mmenu` | 所有玩家 | 打开主菜单 |
| `/mmenu ui [样式]` | 所有玩家 | 选择界面样式：`auto`（跟随菜单）/ `form` / `npc` / `chest` |
| `/mmenu admin` | OP | 打开菜单编辑器 |
| `/mmenu reload` | OP | 热重载配置与菜单数据 |
| `/mmgv` | 所有玩家 | 获取菜单触发物品 |
| `/clearmmenu` | 所有玩家 | 清除自己的菜单触发物品 |
| `/mnpc status / list / open / close` | 所有玩家 | NPC 菜单连接状态 / 绑定列表 / 打开 / 关闭 |
| `/mnpc test / spawn / bind / remove / reload` | OP | 测试 NPC / 生成绑定 NPC / 修改绑定 / 重载重建 |

## 数据目录（`Meowdata/MeowMenu/`）

| 文件 | 说明 |
|------|------|
| `config.json` | 全局配置（触发物品、进服发放、菜单分级默认值、消息文案） |
| `data/<菜单id>.json` | 菜单数据（标题/内容/按钮/分级/触发物品/NPC 模式/界面样式） |
| `players.json` | 玩家进服发放记录（`firstJoin` / `menuGiven` / `menuGivenAt`） |
| `item_triggers.json` | LSE 注册的自定义物品触发绑定 |
| `player_ui.json` | 玩家自选的界面样式偏好 |
| `npc_menus.json` | 已生成菜单 NPC 的位置/绑定/皮肤快照 |
| `migrations/` | 外部格式菜单文件投放目录（启动时自动导入，成功者移入 `done/`） |

## 菜单数据要点

```jsonc
{
  "id": "main",
  "title": "§l§b服务器菜单",
  "content": "欢迎来到服务器！",
  "level": 1,                 // 1 = 一级菜单（可被触发物品打开）, 2 = 二级菜单（只能从别的菜单进入）
  "triggerItem": { "type": "minecraft:compass", "displayName": "商店菜单" },
  "renderMode": "default",    // default(表单) / npc(NPC 对话) / chest(箱子)
  "npc": { "enabled": false, "name": "", "dialogue": "", "avatarType": "", "avatarVariant": -1 },
  "buttons": [
    { "text": "开始游戏", "type": "command",      "value": "say hi" },
    { "text": "子菜单",   "type": "submenu",      "value": "shop" },
    { "text": "标题行",   "type": "header" }
  ]
}
```

- **按钮类型**：`command` / `admin_command` / `submenu` / `admin_submenu` / `folder` /
  `npc_menu`（NPC 对话形态打开）/ `npc_spawn`（生成可右键打开的 NPC）/ `header` / `label` / `divider` / `spacer`
- **触发物品**：`triggerItem.type` 按**物品类型**匹配（不看 NBT），只有一级菜单生效；
  主菜单的触发物品由 `config.json` 的 `triggerItem` 决定（默认钟，进服发放一次）
- **头像/载体**：`npc.avatarType` 换载体实体类型（如 `minecraft:zombie`）、
  `npc.avatarVariant` 换 NPC 内置皮肤变体（0..59）

## 文档

- [API.md](./API.md) —— RemoteCall 导出接口完整参考（LSE / 其它插件调用）

## 构建

```bash
xmake build MeowMenu
```

产物在 `build/windows/x64/release/`（`bin/` 为打包输出）。
图标库 `src/embedded_texture_path.h` 已随源码提供，开箱即可构建；
需重新生成时运行 `gen_texture_path_embed.ps1`。
