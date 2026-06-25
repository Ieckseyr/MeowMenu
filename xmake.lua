add_rules("mode.debug", "mode.release")

-- 使用本地缓存的 liteldev-repo（避免 GitHub 网络访问）
add_repositories("liteldev-repo ../MeowEntityModel/.xmake/windows/x64/repositories/liteldev-repo")

if is_config("target_type", "server") then
    add_requires("levilamina 26.10.3", {configs = {target_type = "server"}})
else
    add_requires("levilamina 26.10.3", {configs = {target_type = "client"}})
end

add_requires("levibuildscript")
add_requires("magic_enum v0.9.7")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

option("target_type")
    set_default("server")
    set_showmenu(true)
    set_values("server", "client")
option_end()

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

    -- Protocol v944 静态库
    add_includedirs("../Protocol-v944/install/include")
    add_linkdirs("../Protocol-v944/install/lib")
    add_links("Protocol")

    -- 插件源文件
    add_headerfiles("src/MeowMenu.h")
    add_headerfiles("src/MenuData.h")
    add_headerfiles("src/Config.h")
    add_headerfiles("src/MenuStore.h")
    add_headerfiles("src/FormManager.h")
    add_headerfiles("src/FormSender.h")
    add_headerfiles("src/FormResponseHook.h")
    add_headerfiles("src/ButtonHandler.h")
    add_headerfiles("src/MenuRenderer.h")
    add_headerfiles("src/TriggerItem.h")
    add_headerfiles("src/MenuEditor.h")
    add_headerfiles("src/EventRegistration.h")
    add_files("src/MeowMenu.cpp")
    add_files("src/MenuData.cpp")
    add_files("src/Config.cpp")
    add_files("src/MenuStore.cpp")
    add_files("src/FormManager.cpp")
    add_files("src/FormSender.cpp")
    add_files("src/FormResponseHook.cpp")
    add_files("src/ButtonHandler.cpp")
    add_files("src/MenuRenderer.cpp")
    add_files("src/TriggerItem.cpp")
    add_files("src/MenuEditor.cpp")
    add_files("src/CommandRegistration.cpp")
    add_files("src/EventRegistration.cpp")
    add_files("src/MemoryOperators.cpp")

    add_includedirs("src")
    set_symbols("hidden")
    add_ldflags("/OPT:REF", "/OPT:ICF", "/DEBUG:NONE")

    if is_config("target_type", "server") then
        add_defines("LL_PLAT_S")
    else
        add_defines("LL_PLAT_C")
    end
