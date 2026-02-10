# MeowMenu

MeowMenu 是一个基于 Meow 插件模板的可视化菜单管理插件，为 Minecraft 基岩版服务器（LeviLamina/LiteLoaderBDS）提供完整的菜单系统。

支持 DDUI 动态表单与 LSE 静态表单双模式渲染、游戏内可视化菜单编辑器、BetterSidebar 变量集成、轮播文字动画、视频帧动画播放、自定义触发物品、入服自动给予物品等功能。

- 作者：伊希娅
- 版本：1.0.0

## 功能特性

- DDUI 动态表单 + LSE 静态表单双模式渲染（自动回退）
- 游戏内可视化菜单编辑器（`/mmenu admin`）
- 多级子菜单 / 文件夹结构
- 信息栏实时刷新（在线人数、延迟、坐标等）
- 轮播文字动画
- 视频帧动画播放（需 DDUI）
- BetterSidebar 变量集成 + PAPI 变量注册
- 自定义触发物品 + 入服自动给予
- 从 CD / PMenu 格式自动迁移菜单
- 配置文件热重载

## 前置依赖

### 必需

- [Meow 插件模板](https://github.com/Ieckseyr) — 插件运行框架
- LeviLamina / LiteLoaderBDS — 基岩版服务器模组加载器

### 可选

| 依赖 | 说明 |
|------|------|
| [DDUI-lseAPI](https://github.com/Ieckseyr/DDUI-lseAPI) | 动态表单能力（C++ 插件），未安装时自动回退静态模式 |
| [DogeUI](https://github.com/DogeLakeDev/DogeUI) | 表单 UI 美化资源包（GPL-3.0） |
| BetterSidebar | 变量系统，菜单内容可引用 `{yoyo.tps}` 等动态变量 |
| BEPlaceholderAPI (PAPI) | 通过 BetterSidebar 注册 PAPI 变量 |

## 安装

1. 将 `Meow` 文件夹放入服务器的 `plugins/` 目录
2. 启动服务器，插件自动生成配置文件到 `./Meowdata/MeowMenu/`
3. （可选）安装 DDUI-lseAPI 以启用动态表单特性
4. （可选）安装 DogeUI 资源包并在配置中开启

## 指令

| 指令 | 权限 | 说明 |
|------|------|------|
| `/mmenu` | 所有玩家 | 打开主菜单 |
| `/mmenu admin` | OP | 打开菜单编辑器 |
| `/clearmmenu` | 所有玩家 | 清除触发物品 |

## 触发方式

1. 执行 `/mmenu` 命令
2. 右键使用触发物品（默认为钟 `minecraft:clock`）

## 详细文档

完整的配置说明、数据结构、API 文档请参阅 [MeowMenu.md](plugins/MeowMenu.md)

## 导出 API

其他 Meow 插件可通过 `$Y.i.MeowMenu` 调用接口：

```javascript
$Y.i.MeowMenu.renderMenu(player, menuId)  // 渲染菜单
$Y.i.MeowMenu.getConfig()                  // 获取配置
$Y.i.MeowMenu.getMenu(menuId)              // 获取菜单数据
$Y.i.MeowMenu.createMenu(menuId, title, content)  // 创建菜单
$Y.i.MeowMenu.showEditor(player)           // 打开编辑器
$Y.i.MeowMenu.registerInfoItem(id, name, getter)  // 注册信息栏项
```
