# `/mmenu admin` 菜单编辑器 — 严格说明

本文是 MeowMenu **游戏内菜单编辑功能**的完整规格：页面地图、字段与校验、图标规则、保存时机、
硬性约束、容量分页，以及**当前尚不支持、建议补齐**的项。目标是让"游戏内改菜单"这件事有据可依、
可逐项验收。

对应实现：`src/MenuEditor.cpp` / `MenuEditor.h`（界面）、`src/MenuData.*`（数据与序列化）、
`src/MenuStore.cpp`（落盘）、`src/IconValidator.*`（图标校验与修复）。

---

## 0. 入口、权限、落盘

| 项 | 说明 |
|---|---|
| 入口 | `/mmenu admin`，需要 **OP（权限等级 ≥ 1）**；也可由菜单按钮触发（`admin_command`，值 `mmenu admin`） |
| 界面形态 | 编辑器各页是**直接构造的普通表单**，不经过菜单样式通道（表单/NPC/箱子）—— 它们永远是 JSON UI |
| 落盘 | 每次"保存"类操作**立即写盘**：`Meowdata/MeowMenu/data/<菜单ID>.json`，并在聊天栏回 `菜单 X 已保存` |
| 目标 | 编辑器直接读写与其他功能**同一份**菜单数据：`/mmenu`、物品触发、NPC/箱子样式、LSE API 用的都是这份 |
| 依赖 | 编辑器本身**无前置**；只有"显示模式"里选 NPC 对话/箱子时才需要 HologramLib（缺前置会标注并回退） |

---

## 1. 页面地图

```
/mmenu admin
└─ 编辑器主界面 (showEditorMain)
   ├─ 新建菜单 (showCreateMenuForm)   ID / 标题 / 内容 → 创建 → 直接进编辑页
   ├─ 菜单列表 (showMenuList)  ── 每个菜单一项 → 菜单编辑页
   │                             └─ 返回主菜单
   ├─ 物品设置 (showItemSettings)      字段: 物品类型ID / 显示名称 / 物品SNBT(留空用默认)
   └─ 从手持物品设置 (setItemFromHand) 把手中物品设为"触发物品"（写 config.triggerItem.type）

菜单编辑页 (showEditMenu: <菜单ID>)
   ├─ 编辑标题和内容 (showEditTitleContent)   字段: 标题 / 内容
   ├─ 添加按钮 (showAddButton)
   │   ├─ 普通按钮 (showAddButtonForm)        字段: 文本 / 图标路径 / 图标物品ID / 类型(下拉) / 按钮值 / 箱子UI物品
   │   ├─ NPC 菜单按钮 (showAddNpcButtonType)
   │   │   ├─ 以 NPC 对话打开 (showAddNpcButtonForm → type=npc_menu)
   │   │   └─ 生成菜单 NPC   (showAddNpcButtonForm → type=npc_spawn)
   │   ├─ 大字体标题行 header (showAddDisplayItemForm)
   │   ├─ 文本行 label        (showAddDisplayItemForm)
   │   └─ 分隔线 divider      (showAddDisplayItemForm)
   ├─ 从手持物品设置图标 (showIconFromHandSelector → setButtonIconFromHand)  选按钮 → 写入它的 iconItem
   ├─ 显示模式 (showUiModeSettings)           普通表单 / NPC 对话 / 箱子 + 固有约束入口
   │   └─ 固有约束 (页内) 切换"允许 NPC 对话 / 允许箱子"（表单恒允许）
   ├─ NPC 对话设置 (showNpcSettings)          开关 / NPC 名 / 话语正文(用 \n 换行)
   ├─ [按钮 i] → 按钮操作页 (showButtonActions)
   │   ├─ 编辑内容 (showEditButton)          同"普通按钮"字段
   │   ├─ ↑ 上移一位 / ↓ 下移一位            顺序 = 箱子占格 / NPC 占槽顺序
   │   ├─ 移动到指定位置 (showMoveButtonTo)  滑块选目标序号（按钮多时更快）
   │   ├─ 从图标库挑图标 (showIconPickerCategories → showIconPicker)  分类 + 搜索 + 分页, 候选按真实纹理显示
   │   ├─ 用手中物品作图标 (setButtonIconFromHand)
   │   └─ 删除此按钮（弹确认框，删除不可恢复）
   ├─ 删除按钮 (showDeleteButtonSelector)    列表选择 → 二次确认
   ├─ DogeUI 设置 (showDogeUISettings)       每菜单开关 + 标识符前缀（默认 `/D `）
   └─ 删除菜单 (showDeleteMenuConfirm)       二次确认；同时删除 `<ID>.json`
```

**新建菜单**：编辑器主界面 → 「§a新建菜单」（填 ID / 标题 / 内容 → 校验 ID → 创建 → 直接进入编辑页）。
> 早期版本没有独立入口，只能靠 submenu 目标自动创建；现已补齐。
旧的隐式做法仍然有效。实际做法是"由按钮目标自动创建"：
给任一菜单加 `submenu` / `admin_submenu` / `folder` 按钮，`value` 填一个新 ID，保存时会自动
`createMenu(id, text, "")` 建出空菜单；随后它出现在菜单列表里可继续编辑。（`folder` 且 `value`
留空时会自动生成 `<父菜单>_folder_<时间戳>` 作为 ID。）
> 建议补齐项见 §8-1。

---

## 2. 数据模型（菜单与按钮）

### 2.1 菜单


| 字段 | 类型 | 编辑器可否改 | 说明 |
|---|---|---|---|
| `id` | string | ✗（= 文件名） | 唯一标识；改名需新建+删除 |
| `title` | string | ✓（编辑标题和内容） | 支持 § 颜色码、MeowPAPI 变量 |
| `content` | string | ✓ | 表单正文；NPC 样式的兜底话语 |
| `renderMode` | string | ✓（显示模式） | `default`(表单) / `npc` / `chest` |
| `buttons` | array | ✓ | 见 §2.2；顺序即渲染顺序 |
| `dogeui.enabled` / `dogeui.prefix` | bool / string | ✓（DogeUI 设置） | 启用后标题加前缀，并触发**固定表单**硬规则 |
| `npc.enabled` / `npc.name` / `npc.dialogue` | bool / string / string | ✓（NPC 对话设置） | NPC 对话样式的内容 |
| `uiAllow` | string[] | ✓（显示模式 → 固有约束） | 该菜单允许的样式白名单；`form` 恒允许 |
| `chestUI` / `infoBar` / `carousel` / `video` | object | ✗ | 历史字段，解析后原样回写，不使用 |

### 2.2 按钮

| 字段 | 说明 |
|---|---|
| `text` | 显示文本（§ 颜色码；`\n` 在表单/NPC 里换行；箱子样式显示为物品名，故会被压成单行） |
| `icon` | **纹理路径**（必须在图标库内，如 `textures/ui/gear`） |
| `iconItem` | **物品 ID**（如 `minecraft:dirt`）；表单里 `icon` 优先，其次 `iconItem`；箱子样式会用它反查纹理 |
| `type` | 见 §3 |
| `value` | 命令字符串（command/admin_command）或目标菜单 ID（submenu/folder/npc_*） |
| `item` | **箱子样式的格子物品**（如 `minecraft:diamond`）；空则依次退回 `iconItem`、`paper` |

---

## 3. 按钮类型全表

| type | `value` 语义 | 表单 | NPC 对话 | 箱子 |
|---|---|---|---|---|
| `command` | 以玩家身份执行命令（支持 `{player}` 与 PAPI） | 可点 | 可点 | 可点 |
| `admin_command` | OP 校验后执行命令 | 可点 | 可点（标签前缀 `[管理]`） | 可点（说明行标"需要管理员"） |
| `submenu` | 目标菜单 ID，不存在则**自动创建** | 开表单 | 目标也是 NPC 样式 → 对话内推进；否则开表单 | 目标也是箱子样式 → 换箱；否则开表单 |
| `admin_submenu` | 同上，先 OP 校验 | 可点 | 可点 | 可点 |
| `folder` | 同 submenu；`value` 留空时自动生成 ID | 可点 | 可点 | 可点 |
| `npc_menu` | 目标菜单 ID（空 = 主菜单），**强制以 NPC 对话打开** | 可点（需 HologramLib，缺则提示） | 可点 | 可点 |
| `npc_spawn` | 目标菜单 ID，**在身前生成一只绑定该菜单的 NPC** | 可点（需 HologramLib） | 可点 | 可点 |
| `header` | 无（用 `text`） | 表单大标题行 | 并入正文文本 | 不显示 |
| `label` | 无（用 `text`） | 表单文本行 | 并入正文文本 | 不显示 |
| `divider` | 无 | 分隔线 | 不显示 | 不显示 |
| `spacer` | 无 | 跳过（不显示） | 不显示 | 不显示 |
| 未知类型 | — | 点击时提示可用类型列表（不再静默无反应） | 同左 | 同左 |

**添加按钮的类型选择是下拉菜单**（`command / admin_command / submenu / admin_submenu / folder /
npc_menu / npc_spawn`），编辑已有按钮时下拉里会保留并显示原类型（含显示项类型），避免手打类型名出错。

---

## 4. 图标规则（严格）

三处图标来源，各有明确的类型要求：

| 场景 | 字段 | 取值 | 说明 |
|---|---|---|---|
| 表单按钮图标 | `icon` → `iconItem` | 纹理路径 / 物品 ID | `icon` 非空优先；两者都空则无图标 |
| 箱子格子物品 | `item` → `iconItem` → `paper` | 物品 ID | `item` 空则用 `iconItem` 反查纹理当图标；仍无则用纸 |
| NPC 对话 / 原生表单 | 只用纹理路径 | 纹理路径 | 对话框不显示图标；原生表单按钮图标只吃纹理 |

**保存时 `icon` 一定过 `IconValidator::normalizeIcon`**，结果只有四种：

1. 合法纹理路径（在图标库内）→ 原样保留
2. 合法物品 ID → **转成**对应纹理路径
3. 以 `textures/` 开头但不在库内 → 原样保留（视为自定义纹理，允许）
4. 其它非法值 → **清空**（不会留下无效图标）

**图标库**：5603 条，编译期嵌入插件（`src/embedded_texture_path.h`），**不读**
`plugins/Meow/textures/texture_path.json`；刷新数据用 `gen_texture_path_embed.ps1`。

### 4.1 本轮修正的无效图标（编辑器 UI 自身）

审计发现编辑器 UI 里有 6 个图标**不在图标库内**（表单里会显示成空白/破图），已全部替换：

| 旧（无效） | 新（已在库内） | 用在哪 |
|---|---|---|
| `textures/ui/icon_text_edit` | `textures/ui/book_addtextpage_default` | 编辑标题和内容 / 编辑按钮内容 |
| `textures/ui/icon_plus` | `textures/ui/plus` | 添加按钮 |
| `textures/ui/icon_hand` | `textures/ui/subcategory_icons/hands` | 从手持物品设置图标（3 处） |
| `textures/ui/icon_spring` | `textures/ui/icons/icon_spring` | DogeUI 设置 |
| `textures/ui/icon_border` | `textures/ui/divider` | 分隔线显示项 |
| `textures/ui/book_writable` | `textures/ui/icon_book_writable` | 大字体标题行 / 菜单列表项 |

### 4.2 自检方法（改完菜单或改代码后跑一次）

```powershell
# 全工程 textures/ 字符串对账：任一不在图标库内即为破图风险
node -e "const fs=require('fs');const lib=new Set(Object.values(JSON.parse(fs.readFileSync('plugins/Meow/textures/texture_path.json','utf8'))));let m=0;for(const f of fs.readdirSync('plugins/MeowMenu/src').filter(f=>/\.(cpp|h)$/.test(f))){const s=fs.readFileSync('plugins/MeowMenu/src/'+f,'utf8');for(const u of new Set([...s.matchAll(/\"(textures\/[A-Za-z0-9_\/\-.]+)\"/g)].map(x=>x[1])))if(u.endsWith('/'))continue;if(!lib.has(u)){console.log('✗',f,u);m++}}console.log(m?m+' 个无效图标':'图标审计通过')"
```

### 4.3 常用合法图标速查

| 用途 | 路径 |
|---|---|
| 设置 / 齿轮 | `textures/ui/gear`、`textures/ui/icon_setting` |
| 添加 | `textures/ui/plus`、`textures/ui/color_plus` |
| 书 / 文本编辑 | `textures/ui/icon_book_writable`、`textures/ui/book_addtextpage_default` |
| 标题行 / 文本行 | `textures/ui/icon_book_writable`、`textures/ui/icon_sign` |
| 分隔线 | `textures/ui/divider`、`textures/ui/horizontal_divider` |
| 返回 / 取消 | `textures/ui/arrow_left`、`textures/ui/cancel` |
| 玩家 / NPC | `textures/ui/icon_steve` |
| 世界 / 地图 | `textures/ui/world_glyph_color`、`textures/ui/icon_map` |
| 手持物品 | `textures/ui/subcategory_icons/hands` |
| 物品图标 | `textures/items/<物品名>`（如 `textures/items/diamond`） |

---

## 5. 保存与校验流程

```
玩家提交表单
 ├─ 取字段（输入项为 string；开关为 0/1；下拉为下标 → 映射回类型名）
 ├─ icon 非空 → IconValidator::normalizeIcon()（四种结果见 §4）
 ├─ 类型为 submenu / admin_submenu / npc_menu / npc_spawn 且 value 非空 → 目标不存在则 createMenu 建空菜单
 ├─ MenuStore::saveMenu() 写 Meowdata/MeowMenu/data/<ID>.json（写盘即生效，无需重启）
 └─ 聊天栏反馈：菜单 X 已保存 / 保存失败；随后回到上一级页面
```

- 排序操作（上移/下移）会**交换并立即保存**，然后停在按钮的新位置，方便连续移动
- 删除按钮有确认框；删除菜单有二次确认
- 重载与外部改动：菜单文件手改后 `/mmenu reload` 即生效（`MenuStore::loadAll()`）

---

## 6. 硬性约束（样式层面，编辑器内可见）

| 规则 | 判定 | 效果 |
|---|---|---|
| 启用 DogeUI | `dogeui.enabled = true` | **固定表单**（前缀只在表单有意义） |
| 含编辑器入口 | 菜单内有 `command`/`admin_command` 且值形如 `<命令名> admin`（`mmenu` 或其别名） | **固定表单**（管理入口必须稳定可辨） |
| 固有约束 | `uiAllow`（空 = 全部允许；`form` 恒允许） | 玩家 `/mmenu ui` 的选择若不在白名单内，对该菜单不生效 |
| 玩家自选样式 | `/mmenu ui`（按 xuid 存 `player_ui.json`） | 优先级高于菜单自身 `renderMode`，但**不得越过**上面三条 |

编辑器"显示模式"页会实时显示当前模式、可点击按钮数，并在被硬规则固定时给出提示。

---

## 7. 容量与分页（影响按钮该怎么放）

| 样式 | 容量 | 超出时 |
|---|---|---|
| 表单 | 不限 | — |
| NPC 对话 | **6 个槽位** | >6 自动分页：普通菜单每页 **4** 项（首格「← 上一页」+ 4 + 末格「下一页 →」）；主菜单每页 **5** 项（余 1 格导航：优先「下一页」，末页显示「上一页」）。分页时**不追加**"返回上一级"，请在菜单里自带返回按钮 |
| 箱子 | 3 行 27 格 / 6 行 54 格 | 自动升到 6 行；仍超出则分页（导航格固定占本页末尾），翻页为**就地刷新**（0 延迟 0 闪烁） |

`header` / `label` / `divider` / `spacer` 是显示项，**不占**按钮槽位（NPC 模式并入正文、箱子模式不显示）。

---

## 8. 现有限制与建议完善项（按优先级）

1. ✅ **已实现**：编辑器主界面「新建菜单」（ID 校验：非空/≤48/无 `\/:*?"<>| ` 与空格/不重名）
2. **复制**：菜单级"以此为基础新建"、按钮级"复制此按钮"（避免手工重填）。
3. ✅ **已实现**：按钮操作页「移动到指定位置」（滑块选目标序号，1 起）
4. **跨菜单移动按钮**：目前不能把按钮搬到另一个菜单。
5. ✅ **已实现**：按钮操作页「从图标库挑图标」——分类浏览（全部/ui/items/blocks/entity/gui/editor）+ 关键字搜索 + 每页 24 个分页；候选按钮**按真实纹理显示**，选中即写入 `icon`（库内路径必然合法）。另：新增按钮若未填图标，保存后**直接进选择器**。
6. **预览**：编辑器里看不到实际渲染效果（表单/NPC/箱子三种形态差异大）。建议加「预览本菜单」按钮，按当前样式打开一次。
7. **多行文本**：`title`/`content`/`npc.dialogue` 是单行输入（话语里用 `\n` 表示换行）。可考虑用"逐行编辑 + 拼接"的方式支持真多行。
8. **菜单 ID 重命名**：需要"新建新 ID + 迁移按钮 + 删除旧 ID"的封装。
9. **全局设置页**：`useDogeUI` / `mainMenuId` / `triggerItem.*` 目前只能靠 `/mmenu`、`config.json` 或 API；建议在编辑器里加一页。
10. **编辑器自身的无缝翻页**：编辑器页是表单，翻页仍是普通表单切换（原版表单固有行为）。若要无缝，需要把编辑器页也做成 NPC/箱子形态 —— 与"编辑器必须稳定可辨"的取向冲突，暂不建议。
11. **批量与撤销**：批量删除/排序、撤销上一步，暂无。

---

## 9. 验收清单（改完菜单过一遍）

- [ ] 每个按钮的 `icon` / `iconItem` / `item` 三选一是否填对（表单看 icon，箱子看 item）？跑一次 §4.2 的审计
- [ ] 目标菜单 ID 是否已存在或会被自动创建（submenu/npc_* 的 value）？
- [ ] 若是 NPC 样式，可点击按钮数是否 ≤6（否则确认分页与自带返回按钮）？
- [ ] 若是箱子样式，是否 ≤54（超过会分页，确认导航格可用）？
- [ ] 该菜单是否被硬规则固定为表单（DogeUI 开 / 含编辑器入口）？预期一致吗？
- [ ] `uiAllow` 是否需要收紧（例如依赖 § 颜色码排版的菜单应 `["form"]`）？
- [ ] 命令类按钮的权限：`command` 以玩家权限执行，`admin_command` 才做 OP 校验
- [ ] 改完 `/mmenu reload`，再用 `/mmenu` 与玩家自己的 `/mmenu ui` 各验一次
