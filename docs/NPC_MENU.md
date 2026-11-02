# MeowMenu × HologramLib NPC 对话菜单

NPC 对话是 MeowMenu 的一种**菜单模式**：菜单把这个模式打开后，**被任何入口打开时都直接以原版
NPC 对话界面呈现**（NPC 名 + 话语 + 按钮列表），不需要额外按钮、也不会生成任何 NPC 实体。
对话里的按钮真实执行（命令 / 权限检查 / 占位符替换全部复用原有执行链），子菜单可以逐层进入。

HologramLib 是**可选前置**：装了就用这个模式，没装则该菜单自动回退成普通表单，
MeowMenu 其余部分（表单菜单、编辑器、触发物品、RemoteCall API）完全不受影响。

> 三种界面样式（表单 / NPC 对话 / 箱子）与「玩家自选样式」的总览见 [UI_MODES.md](UI_MODES.md)；
> 本文只讲 NPC 对话这一条线的细节。

---

## 1. 怎么开启这个模式

**编辑器（推荐）**：

```
/mmenu admin → 选菜单 → 「NPC 对话模式」
  ├─ 启用 NPC 对话模式   ← 开关
  ├─ 对话框里的 NPC 名   ← 显示在对话界面上的名字（留空 = 菜单标题）
  └─ 话语/正文           ← 对话框正文, 单行输入里用 \n 表示换行（留空 = 菜单 content）
```

**手写 JSON**：菜单里加一个 `npc` 块（`renderMode: "npc"` 也可以，等价简写）

```json
{
  "id": "shop",
  "title": "§b商店",
  "content": "欢迎光临",
  "npc": {
    "enabled": true,
    "name": "杂货铺老板",
    "dialogue": "欢迎, {player}!\n想买点什么?"
  },
  "buttons": [ ... ]
}
```

**LSE / 其他插件**：`RemoteCall.importAs("MeowMenu","setMenuNpcMode")("shop", true, "杂货铺老板", "欢迎, {player}!")`

开启后不用做别的：`/mmenu`、子菜单按钮、触发物品、`renderMenu` API —— 所有原本打开这个菜单的
入口都会直接弹出 NPC 对话。前置不可用时自动回退普通表单（并记一条日志），菜单不会打不开。

**话语支持占位符**：与表单一致，先走 MeowPAPI（`%name%` / `{name}`），再兜底替换 `{player}`。

---

## 2. 按钮数量上限：6

原版 NPC 对话框只有 **6 个按钮槽位**：

- 可点击按钮 **≤ 6** → 全部显示；从上一层进入且还有空槽时，末尾补一个「返回上一级」
- 可点击按钮 **> 6** → **自动分页**：
  - 每页内容项：**普通菜单 4 项**，**主菜单 5 项**（主菜单 = `config.mainMenuId`）
  - 普通菜单：首格 = 「← 上一页」，末格 = 「下一页 →」（1 + 4 + 1 = 6）
  - 主菜单：6 格里放不下 5 内容 + 双向导航，所以只剩 1 格导航 → **优先「下一页」，最后一页显示「上一页」**
    （把 `kPageSizeMain` 改成 4 就变成双向导航都在，代价是每页 4 项）
  - 方向不可用时该槽显示占位 `§8——`（点击无反应），布局保持稳定
  - 分页时槽位被内容与导航占满，**不追加「返回上一级」**：请在该菜单里自带一个返回按钮
  - 翻页是**就地刷新**（见下），不关界面、不闪烁、无延迟
- `header` / `label` 会并入正文文本，`divider` / `spacer` 不显示 —— 它们不占槽位
- 关闭对话：游戏内按 `ESC`（不占槽位）
- 编辑器页面里会实时显示「本菜单可点击按钮: N / 上限 6」
- 翻页/换层**实测 0 闪烁**（服务端删旧载体、建新载体、发 Open 在同一 tick 完成）

### 翻页 / 进子菜单：服务端立即重开，实测 0 闪烁

翻页、进子菜单、返回上一级、命令执行后重推，都走 `INpcDialogue::open()`：删旧载体 → 建新载体 →
发 `NpcDialoguePacket(Open)`。这三步在同一 tick 内到达客户端，**界面是无缝换内容的**（实测 0 闪烁，
日志显示 `NPC 对话菜单已重开: ... (第 2/3 页)`）。

- 为什么不用 `INpcDialogue::update()`：原版对话界面在**点击按钮时客户端就自行收起了**，
  而 `update` 只重发对话包、不会让客户端重新弹出界面（实测现象：点了就关、没有新界面）。
  `update` 仍保留在库里，适合"界面还开着时的内容微调"（非点击触发）。头文件已写明该限制。
- 配套的库侧修复（本次一并改在 HologramLib 里）：**点击后的清理从"按玩家"改成"按本次事件对应的对话"**。
  以前按玩家无脑删，回调里刚重开的那一层会被连同载体一起删掉 —— 现象就是"界面直接关闭、新层没出来"。
  现在回调里换过/更新过对话时只清旧载体实体，新对话保持不动。

---

## 3. 与表单模式的差异

| 项目 | 普通表单 | NPC 对话模式 |
|---|---|---|
| 外观 | LeviLamina SimpleForm | 原版 NPC 对话框（NPC 名 + 话语 + 按钮列表） |
| § 颜色码 | 支持 | **剥离**（原版对话框不解析，保留会显示成乱码） |
| header / label | 不可点击的行 | 并入正文文本 |
| divider / spacer | 分隔线 / 空行 | 不显示 |
| 图标 | 支持 icon / iconItem | 不显示（对话框按钮无图标） |
| 按钮上限 | 无 | 6 |
| 子菜单 | 新开一个表单 | 目标菜单**也是** NPC 模式 → 同一对话内推进一层（可返回）；否则打开表单 |
| 权限 | admin_* 点击时校验 | 相同（admin_submenu 进入前校验，管理按钮前缀 `[管理]`） |

两种形态共用同一份菜单数据，互不影响；同一个菜单树里可以混用（一部分菜单开对话模式、
一部分用表单），点进哪个菜单就按**那个菜单自己**的模式渲染。

---

## 4. 前置关系（为什么说是"可选"）

| 场景 | 表现 |
|---|---|
| 未安装 HologramLib | 启动日志一条：`未检测到 HologramLib → NPC 菜单功能已禁用（可选前置, 其余功能不受影响）`；配了对话模式的菜单自动回退普通表单 |
| 装了但 API 版本低于 `0x011B00` | 日志提示版本不足，同上回退表单（升级 HologramLib 即自动启用） |
| 装了且版本达标 | 日志：`HologramLib 已连接 (ABI 0x011B00): NPC 对话菜单可用; 点击监听=..., NPC 域=可用, ghost 监听=...` |

保证来自三点实现（不是靠"约定"）：

1. **不链接**：`xmake.lua` 没有 `add_deps`/`add_links`/`add_includedirs` 指向 HologramLib，
   插件源码也不 `#include` 它的头文件。
   `dumpbin /dependents MeowMenu.dll` 的导入表里**没有** `HologramLib.dll`。
2. **不引用其头文件**：接口布局由 [`src/HologramLibAbi.h`](../src/HologramLibAbi.h) 镜像。
   直接 `#include "hologramlib/HologramLib.h"` 会因为 `__declspec(dllimport)` 让
   HologramLib 变成**加载期硬前置**（缺库则 MeowMenu 整个加载失败），所以必须镜像。
3. **运行时解析符号**：`GetModuleHandleW("HologramLib.dll")` → 取不到再 `LoadLibraryW`，
   然后 `GetProcAddress` 解析它导出的唯一 C++ 入口符号：

   ```
   ?getInstance@IHologramLib@hologramlib@@SAAEAV12@XZ      // hologramlib::IHologramLib::getInstance()
   ```

   拿到 `IHologramLib&` 后先查 `version()`，版本达标才用尾部的 `npcDialogs()` / `playerNpcs()`。

镜像与真实接口的一致性由 [`tests/npc_abi/`](../tests/npc_abi/) 校验（编译期 + 文本比对 + 部署 DLL 符号），
HologramLib 升级后跑一次 `tests\npc_abi\run.bat` 即可确认没有 ABI 漂移。

---

## 5. 快速开始（测试）

```
1) 确认服务器已装 HologramLib（plugins/HologramLib/HologramLib.dll, 26.40.2+）
2) 重启服务器（让 MeowMenu 加载新 DLL）
3) 游戏内 /mnpc status   → 应显示 "已连接, ABI 版本 0x11b00"
4) 游戏内 /mmenu         → 点「NPC 菜单 (测试)」按钮 → 直接弹出 NPC 对话界面
```

直接预览（不用点按钮）：

```
/mnpc open                 以 NPC 对话形式打开主菜单（config.mainMenuId）
/mnpc open npc_test_main   打开指定的测试菜单
/mnpc close                关闭自己的对话
```

### 测试菜单结构（三层嵌套）

```
npc_test_main  ← 主菜单的「NPC 菜单 (测试)」按钮 或 /mnpc open npc_test_main
   (本菜单已开 NPC 对话模式: 名 "菜单引导员", 话语可编辑)
├─ 功能演示 (三层嵌套) ──→ npc_test_fun            【第二层】
│                            ├─ 加速 30 秒                effect @s speed 30 1
│                            ├─ 立刻回满血                effect @s instant_health 1 1
│                            ├─ 夜间视觉 60 秒            effect @s night_vision 60 1
│                            ├─ 粒子与音效 ──→ npc_test_particles   【第三层】
│                            │                  ├─ 脚下粒子: 作物生长   particle minecraft:crop_growth_emitter ~ ~1 ~
│                            │                  ├─ 脚下粒子: 末地烛     particle minecraft:endrod ~ ~1 ~
│                            │                  ├─ 跳得更高 (30 秒)     effect @s jump_boost 30 2
│                            │                  ├─ 播放雷声(仅自己)     playsound ambient.weather.thunder @s
│                            │                  ├─ 返回上一层           → npc_test_fun
│                            │                  └─ 返回主菜单           → npc_test_main
│                            ├─ 给自己 16 个苹果           give @s minecraft:apple 16
│                            └─ 返回主菜单                 → npc_test_main
├─ 世界与时间控制 (需OP) ─→ npc_test_world         【admin_command 演示】
│                            ├─ 设为白天 / 设为夜晚        time set day|night
│                            ├─ 天气转晴 / 开始下雨        weather clear|rain
│                            ├─ 向上传送到 5 格高处        tp @s ~ ~5 ~
│                            └─ 返回主菜单                 → npc_test_main
├─ 管理专区 (需OP) ──────→ npc_test_admin         【admin_submenu 入口 + admin_command 按钮】
├─ 领取测试礼包                 give @s minecraft:bread 8
├─ 广播一句 ({player} 占位符)   say {player} 正在使用 NPC 对话菜单
└─ 给自己发一条消息             tellraw @s {...}
```

数据文件（用游戏内编辑器 `/mmenu admin` 也能直接改）：

```
Meowdata/MeowMenu/data/npc_test_main.json
Meowdata/MeowMenu/data/npc_test_fun.json
Meowdata/MeowMenu/data/npc_test_particles.json
Meowdata/MeowMenu/data/npc_test_world.json
Meowdata/MeowMenu/data/npc_test_admin.json
```

**测试要点**：

- 层数推进：`功能演示` → `粒子与音效`（第三层）→ 底部 `<< 返回上一级` 回到第二层
- 真实执行：每个按钮点下去立即有游戏内效果（药水效果 / 粒子 / 声音 / 物品 / 公开广播）
- 占位符：`{player}` 会被替换成点击者名字（顺序是 MeowPAPI 翻译 → `{player}` 兜底）
- 权限分流：非 OP 点管理类按钮得到 `权限不足`；`管理专区` 连入口都进不去
- NPC 绑定：右键 NPC → 打开它绑定的菜单；`/mnpc list` 可看到绑定与坐标

---

## 6. 命令一览（`/mnpc`）

| 命令 | 权限 | 说明 |
|---|---|---|
| `/mnpc status` | 所有人 | 连接状态 / ABI 版本 / 监听 token / 绑定数量 |
| `/mnpc list` | 所有人 | 已绑定菜单的 NPC（JSON） |
| `/mnpc open [菜单ID]` | 所有人 | 以 NPC 对话形式打开菜单（省略 = 主菜单） |
| `/mnpc close` | 所有人 | 关闭自己的 NPC 对话 |
| `/mnpc test` | OP | 一键测试：生成"菜单测试"NPC + 立即打开测试菜单 |
| `/mnpc spawn <NPC名> <菜单ID>` | OP | 身前 2 格生成可点击 NPC（皮肤采集你自己；持久化） |
| `/mnpc bind <npcId> <菜单ID>` | OP | 把已有 HologramLib NPC 绑定到菜单 |
| `/mnpc remove <npcId>` | OP | 删除 NPC 及其绑定 |
| `/mnpc reload` | OP | 重读 `npc_menus.json` 并重建 NPC |

---

## 7. RemoteCall 导出（命名空间 `MeowMenu`）

| 导出 | 签名 | 说明 |
|---|---|---|
| `setMenuNpcMode` | `(menuId, enabled, npcName, dialogue) -> bool` | **设置菜单的 NPC 对话模式**（话语里用反斜杠 n 表示换行） |
| `getMenuNpcMode` | `(menuId) -> string` | 读模式配置 JSON（含 `clickableButtons` / `maxButtons: 6`） |
| `isNpcMenuAvailable` | `() -> bool` | 前置是否已连接且版本达标 |
| `getNpcMenuStatus` | `() -> string` | 状态文本 |
| `openNpcMenuDialog` | `(playerName, menuId) -> bool` | 以对话打开菜单（menuId 空 = 主菜单） |
| `closeNpcMenuDialog` | `(playerName) -> bool` | 关闭对话 |

附（可选能力，默认用不到）：`spawnNpcMenuAt` / `bindNpcMenu` / `removeNpcMenu` / `getNpcMenus` /
`reloadNpcMenus` —— 生成一只"点击即打开菜单"的 NPC 实体并与菜单绑定；菜单模式本身不需要它。

LSE 示例：

```js
const status  = RemoteCall.importAs("MeowMenu", "getNpcMenuStatus");
const open    = RemoteCall.importAs("MeowMenu", "openNpcMenuDialog");
logger.info(status());
open(player.realName, "npc_test_main");
```

---

## 8. 工作原理

```
打开菜单（任何入口: /mmenu、子菜单按钮、触发物品、renderMenu API）
   └─ MenuRenderer::renderMenu
        └─ menu.isNpcMode() && 前置可用 → NpcMenuBridge::openDialogue
             └─ 菜单 → NpcDialogSpec
                  · NPC 名 = npc.name（空 = 菜单标题）, 话语 = npc.dialogue（空 = content）
                  · 两者都过 MeowPAPI 占位符 + {player} 替换, 再剥离 § 颜色码（原版对话框不解析）
                  · 按钮 = 可点击按钮前 6 个, actionId = "menu:<菜单id>#<按钮下标>"
                  · 有空槽时追加「<< 返回上一级」
             └─ INpcDialogue::open(realName, spec)
           └─ 否则 → 普通表单（前置缺失时也走这里, 菜单不会打不开）

（可选）玩家右键 NPC
   └─ HologramLib ghost 交互回调（domain="npc", action=1）
        └─ 按库内 id 查绑定表 → openDialogue(player, menuId, NPC名)

玩家点对话按钮
   └─ HologramLib 点击回调（带 sceneName / buttonIndex / actionId）
        └─ NpcMenuBridge::onDialogClick
             ├─ actionId == "@close" → closeDialogue
             ├─ actionId == "@back"  → 弹出上级栈并重新打开上一层
             └─ "menu:<id>#<index>"  → executeMenuButton
                  ├─ submenu / folder / admin_submenu
                  │     ├─ 目标菜单也是 NPC 模式 → 入栈 + 同一对话内推进一层（admin_* 先查 OP）
                  │     └─ 否则 → MenuRenderer 打开普通表单
                  └─ command / admin_command 等 → ButtonHandler::handleButton（原有执行链）
                        · 执行后重推当前层, 保证菜单继续可用
```

- **持久化**：`Meowdata/MeowMenu/npc_menus.json` 记录每个菜单 NPC 的 id / 名称 / 绑定菜单 /
  坐标 / 维度 / 皮肤快照（base64）。HologramLib 明确不落盘（"持久化由消费方负责"），
  所以皮肤快照也由 MeowMenu 保存，重启后 `registerSkinFromBlob` 恢复并 `createWithId` 重建。
- **重复打开**：同一玩家重复打开会由库先关掉上一层界面；MeowMenu 侧的上级栈只在点击子菜单时入栈。
- **点按钮后为什么要等 1 tick 再开下一层**（重要, 排查时先看这条）：HologramLib 处理点击回传的顺序是
  `dispatch(回调)` → **回调返回后**才按玩家拆掉"当前对话记录 + 客户端载体实体"。如果在回调里直接
  `open` 下一层, 它会把我刚开的那层当成旧对话一起删掉 —— 现象就是**界面直接关闭、新的一层没出来**。
  因此 MeowMenu 把所有"点按钮触发的显示"（进入子菜单 / 返回上一级 / 命令执行后重推当前层）
  统一延迟一个 tick（50ms）执行；表现为界面短暂闪一下再出现，属于库当前时序下的必然现象。
- **线程**：HologramLib 的 ghost / 点击回调发生在主线程网络处理路径，与命令执行同线程，
  因此直接在回调里执行命令（不额外跳线程）。

---

## 9. 已知限制与排错

| 现象 | 原因 / 处理 |
|---|---|
| 菜单打开还是普通表单, 没有 NPC 对话 | 该菜单没开 NPC 对话模式：`/mmenu admin → 选菜单 → NPC 对话模式 → 启用`；也确认下日志有没有 `HologramLib 已连接` |
| 对话里只显示 6 个按钮, 后面的看不到 | 原版对话框只有 6 个槽位；把多余按钮移入子菜单（编辑器会显示 `N/6`，日志也有提示） |
| 点按钮后界面直接关闭, 没有进入下一层 | 早期版本会在库拆对话之前开下一层, 导致新层被误删；已通过"延迟 1 tick"修复。若仍复现, 看日志有没有 `NPC 菜单进入子菜单(对话): ...` 紧跟 `NPC 对话菜单已打开`（两条都在说明服务端两次都开了） |
| 日志说"未检测到 HologramLib" | 服务器没装；装好重启即可，无需改配置；配了模式的菜单会自动回退表单 |
| 日志说"API 版本低于 ..." | HologramLib 太旧（或换成了 `HologramLib-SkinFix` 这个 `0x011A00` 旧线, 它没有 NPC 对话域），升级/换回 26.40.2+ 主库即可 |
| 日志有 `写入 npc_menus.json 失败: invalid UTF-8` | 旧版本的 `§` 处理 bug（UTF-8 里 § 是两字节）；已修，用新 DLL 重新生成一次即可 |
| NPC 外观是默认皮肤 | HologramLib 已知问题（`NPC 皮肤暂无效`），与菜单模式无关 |
| 点了按钮没反应 | 看日志有没有 `NPC 对话点击: ...`：<br>· 没有 → 客户端没回传点击（把该现象和 `rawActionJson` 一起反馈给 HologramLib 侧调字段）<br>· 有 → 看紧随其后的 `NPC 菜单按钮执行: ...` 判断命令是否执行 |
| 重启后（可选能力里的）NPC 没回来 | 看日志 `菜单 NPC 重建完成: 成功 N / 失败 M`；失败通常是皮肤快照缺失，重新生成一次即可 |
| 想彻底关掉聊天里的乱码提示 | 关掉对应菜单的 NPC 对话模式即可；要整体停用就移除 HologramLib |

---

## 10. 本次交付的验证情况

已验证（可复现）：

- `xmake build` 编译链接通过；`dumpbin /dependents` 确认导入表无 HologramLib
- `tests\npc_abi\run.bat` 三层校验全部通过：
  编译期结构体尺寸/偏移与全部 override 签名、vtable 槽位顺序与结构体字段顺序文本比对、
  部署 DLL 的导出符号
- 测试菜单数据校验：五份 JSON 合法、三层嵌套引用闭环、所有图标存在于内置图标库（5603 条）

实机已验证（2026-09-16 服务器现场日志）：

```
[INFO][MeowMenu] HologramLib 已连接 (ABI 0x011B00): NPC 对话菜单可用; 点击监听=1, NPC 域=可用, ghost 监听=1
[INFO][MeowMenu] NPC 对话菜单已打开: BeetleCellar915 → npc_test_main (dialog=1, 按钮=8)
[INFO][MeowMenu] NPC 对话点击: player=BeetleCellar915 scene=npc_test_main index=0 action=menu:npc_test_main#0
[INFO][MeowMenu] NPC 菜单按钮执行: menu=npc_test_main index=0 type=npc_spawn value=npc_test_main
[INFO][MeowMenu] 点击 NPC 生成菜单NPC (测试)(4) → 打开菜单 npc_test_main (player=BeetleCellar915)
[INFO][MeowMenu] NPC 菜单进入子菜单(对话): npc_test_main -[1]-> npc_test_fun
```

即：对话能开、**客户端确实回传 `actionId` + `buttonIndex`**、按钮执行链通、嵌套推进通。
那次现场暴露的两个问题都已修复：按钮数超过 6 个槽位（现在按 6 截断并在编辑器/日志提示）、
UTF-8 下 `§` 是两字节导致的非法 UTF-8 与乱码（现在按 UTF-8 剥离并按字符边界截断）。
