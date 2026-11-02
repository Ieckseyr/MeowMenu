add_rules("mode.debug", "mode.release")

-- 优先使用本地缓存的 liteldev-repo，回退到 GitHub 远程
-- 依次尝试：MeowMenu 自身、兄弟插件、根目录的本地仓库
local function find_local_repo()
    local candidates = {
        -- 服务器根目录的离线仓库（26.60 SDK，预构建 levilamina 26.60.0）
        path.join(os.projectdir(), "..", "..", ".llsdk", "liteldev-repo"),
        path.join(os.projectdir(), ".xmake", os.host(), os.arch(), "repositories", "liteldev-repo"),
        path.join(os.projectdir(), "..", "RecipeExporter", ".xmake", os.host(), os.arch(), "repositories", "liteldev-repo"),
        path.join(os.projectdir(), "..", "MeowSidebar", ".xmake", os.host(), os.arch(), "repositories", "liteldev-repo"),
        path.join(os.projectdir(), "..", "MeowEntityModel", ".xmake", os.host(), os.arch(), "repositories", "liteldev-repo"),
        path.join(os.projectdir(), "..", ".xmake", os.host(), os.arch(), "repositories", "liteldev-repo"),
    }
    for _, p in ipairs(candidates) do
        if os.exists(path.join(p, "packages", "l", "levilamina", "versions", "26_60_0.lua")) then return p end
    end
    return nil
end
local local_repo = find_local_repo()
if local_repo then
    add_repositories("liteldev-repo " .. local_repo)
else
    add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")
end

if is_config("target_type", "server") then
    add_requires("levilamina 26.60.0", {configs = {target_type = "server"}})
else
    add_requires("levilamina 26.60.0", {configs = {target_type = "client"}})
end

-- 插件统一用 clang-cl（与 LeviLamina 本体一致；缓存里的依赖包也是这套工具链）
if is_plat("windows") then
    set_toolchains("clang-cl")
end

add_requires("levibuildscript")
-- legacyremotecall 不再需要：MeowPAPI 静态库通过 DLL 转发所有 RemoteCall 调用
add_requires("magic_enum v0.9.7")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

-- 包含 MeowPAPI 静态库项目（使其成为可被 add_deps 引用的 target）
includes("../MeowPAPI/xmake.lua")

target("MeowMenu")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_cxflags("/EHa", "/utf-8", "/W4", "/Zm2000", "/wd4100", {force = true})
    add_defines("NOMINMAX", "UNICODE", "_AMD64_", "LL_MEMORY_OPERATORS")
    add_packages("levilamina")
    add_packages("magic_enum")
    set_exceptions("none")
    set_kind("shared")
    set_languages("c++23")

    -- 依赖 MeowPAPI 静态库（PAPI 变量翻译）
    add_deps("MeowPAPI")
    add_includedirs("../MeowPAPI/include")

    -- 插件源文件
    add_headerfiles("src/MeowMenu.h")
    add_headerfiles("src/MenuData.h")
    add_headerfiles("src/Config.h")
    add_headerfiles("src/MenuStore.h")
    add_headerfiles("src/ButtonHandler.h")
    add_headerfiles("src/MenuRenderer.h")
    add_headerfiles("src/TriggerItem.h")
    add_headerfiles("src/MenuEditor.h")
    add_headerfiles("src/EventRegistration.h")
    add_headerfiles("src/IconValidator.h")
    add_headerfiles("src/ItemTrigger.h")
    add_headerfiles("src/TriggerRouter.h")
    add_headerfiles("src/ItemUseHook.h")
    add_headerfiles("src/MigrationManager.h")
    add_headerfiles("src/ExportApi.h")
    -- HologramLib 可选前置: 仅镜像头文件, 不链接、不引用其 include 目录
    add_headerfiles("src/HologramLibAbi.h")
    add_headerfiles("src/NpcMenuBridge.h")
    add_headerfiles("src/PlayerUiPref.h")
    add_files("src/MeowMenu.cpp")
    add_files("src/MenuData.cpp")
    add_files("src/Config.cpp")
    add_files("src/MenuStore.cpp")
    add_files("src/ButtonHandler.cpp")
    add_files("src/MenuRenderer.cpp")
    add_files("src/TriggerItem.cpp")
    add_files("src/MenuEditor.cpp")
    add_files("src/MenuEditorTools.cpp")
    add_files("src/CommandRegistration.cpp")
    add_files("src/EventRegistration.cpp")
    add_files("src/IconValidator.cpp")
    add_files("src/ItemTrigger.cpp")
    add_files("src/TriggerRouter.cpp")
    add_files("src/ItemUseHook.cpp")
    add_files("src/MigrationManager.cpp")
    add_files("src/NpcMenuBridge.cpp")
    add_files("src/PlayerUiPref.cpp")
    add_files("src/ExportApi.cpp")
    add_files("src/MemoryOperators.cpp")

    add_includedirs("src")
    set_symbols("hidden")
    add_ldflags("/OPT:REF", "/OPT:ICF", "/DEBUG:NONE")

    if is_config("target_type", "server") then
        add_defines("LL_PLAT_S")
    else
        add_defines("LL_PLAT_C")
    end

    after_build(function (target)
        local projectdir = os.projectdir()
        -- 部署单个文件：直接 cp 失败（目标被正在跑的服务端加载占用）就改名旧文件再拷。
        -- 注意：xmake 回调沙箱里没有 pcall，用 try/catch（在回调环境里才有）。
        local function deploy(src, dst)
            if not os.isfile(src) then
                return false
            end
            local ok = false
            try {
                function ()
                    os.cp(src, dst, {force = true})
                    ok = true
                end,
                catch {
                    function (e)
                    end
                }
            }
            if ok then
                return true
            end
            local backup = dst .. ".old"
            try {
                function ()
                    os.rm(backup)
                end,
                catch {
                    function (e)
                    end
                }
            }
            try {
                function ()
                    os.mv(dst, backup)
                end,
                catch {
                    function (e)
                    end
                }
            }
            try {
                function ()
                    os.cp(src, dst, {force = true})
                    ok = true
                end,
                catch {
                    function (e)
                    end
                }
            }
            if ok then
                cprint("${yellow}[deploy]: ${reset}目标被占用，旧文件改名为 " .. backup)
            else
                print("warn: 部署失败（文件被占用且改名失败）: " .. dst)
            end
            return ok
        end

        local dll = target:targetfile()
        local pdb = path.join(path.directory(dll), "MeowMenu.pdb")
        deploy(dll, path.join(projectdir, "MeowMenu.dll"))
        deploy(pdb, path.join(projectdir, "MeowMenu.pdb"))
    end)
