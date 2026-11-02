// check_abi.js - 校验 MeowMenu 的 HologramLib ABI 镜像与真实头文件是否一致
//
// 比对内容:
//   1. 接口的**虚函数声明顺序**（= vtable 槽位顺序, 顺序漂移会导致运行时按错槽位调用 → 崩溃）
//   2. 结构体的**字段顺序**（跨 ABI 按值传递的结构必须逐字段对齐）
//   3. 镜像里写死的最低 API 版本 vs 真实头文件的 HOLOGLIB_API_VERSION
//   4. 运行时 GetProcAddress 用的导出符号串是否仍与真实类名/命名空间相符
//
// 用法: node check_abi.js [真实头文件路径] [镜像头文件路径]
// 退出码: 0 = 一致, 1 = 有差异（差异会逐条打印）
const fs = require('fs');
const path = require('path');

const realPath =
    process.argv[2] || path.resolve(__dirname, '../../../HologramLib/include/hologramlib/HologramLib.h');
const mirrorPath = process.argv[3] || path.resolve(__dirname, '../../src/HologramLibAbi.h');

const INTERFACES = ['INpcDialogue', 'IContainerMenu', 'IPlayerNpc', 'IHologramLib'];
const STRUCTS = [
    'GhostInteractEvent',
    'PlayerNpcSkin',
    'PlayerNpcConfig',
    'NpcDialogButton',
    'NpcDialogSpec',
    'NpcDialogClickEvent',
    'ContainerMenuItem',
    'ContainerMenuSpec',
    'ContainerClickEvent',
];

const stripComments = (text) =>
    text.replace(/\/\*[\s\S]*?\*\//g, ' ').replace(/\/\/[^\n]*/g, ' ');

// 取出 "class Name { ... };" 或 "struct Name { ... };" 的函数体（按花括号配对）
function extractBody(text, name) {
    const re = new RegExp(`(?:class|struct)\\s+${name}\\s*(?::[^{]*)?\\{`);
    const m = re.exec(text);
    if (!m) return null;
    let i = m.index + m[0].length;
    let depth = 1;
    const start = i;
    for (; i < text.length && depth > 0; ++i) {
        if (text[i] === '{') depth++;
        else if (text[i] === '}') depth--;
    }
    return text.slice(start, i - 1);
}

// 按顶层 ';' 切分声明（忽略括号内的分号）
function splitStatements(body) {
    const out = [];
    let depth = 0;
    let cur = '';
    for (const ch of body) {
        if (ch === '(') depth++;
        else if (ch === ')') depth--;
        if (ch === ';' && depth === 0) {
            out.push(cur);
            cur = '';
            continue;
        }
        cur += ch;
    }
    if (cur.trim()) out.push(cur);
    return out;
}

// 归一化: 去掉所有空白（类型/名称/参数顺序的差异都会保留下来, 便于精确比对）
// 同时剥离访问修饰符 —— 它不影响 vtable 槽位顺序, 且镜像有意省略了静态 getInstance
// （镜像里只留 GetProcAddress 解析, 直接调用会变成加载期硬依赖）
const norm = (s) =>
    s
        .replace(/\b(public|private|protected)\s*:/g, '')
        .replace(/\s+/g, '');

// 接口: 取所有 virtual 声明（含虚析构）, 顺序即 vtable 槽位顺序
function interfaceSlots(headerText, name) {
    const body = extractBody(stripComments(headerText), name);
    if (body === null) return null;
    return splitStatements(body)
        .map((s) => norm(s))
        .filter((s) => s.includes('virtual'))
        // 静态成员不进 vtable
        .filter((s) => !s.includes('static'));
}

// 结构体: 取所有数据字段（不含函数声明的语句）
function structFields(headerText, name) {
    const body = extractBody(stripComments(headerText), name);
    if (body === null) return null;
    return splitStatements(body)
        .map((s) => norm(s))
        .filter((s) => s.length > 0 && !s.includes('(') && !s.startsWith('//'));
}

function printDiff(label, realList, mirrorList) {
    if (!realList) {
        console.log(`  [跳过] ${label}: 真实头文件里找不到该类型（可能已改名/移除）`);
        return 0;
    }
    if (!mirrorList) {
        console.log(`  [失败] ${label}: 镜像里找不到该类型`);
        return 1;
    }
    let bad = 0;
    const max = Math.max(realList.length, mirrorList.length);
    for (let i = 0; i < max; ++i) {
        const a = realList[i];
        const b = mirrorList[i];
        if (a === b) continue;
        bad++;
        if (i >= realList.length) {
            console.log(`  [失败] ${label} 槽位 ${i}: 镜像多出 "${b}"`);
        } else if (i >= mirrorList.length) {
            console.log(`  [失败] ${label} 槽位 ${i}: 镜像缺少 "${a}"`);
        } else {
            console.log(`  [失败] ${label} 槽位 ${i} 不一致:`);
            console.log(`         真实: ${a}`);
            console.log(`         镜像: ${b}`);
        }
    }
    if (!bad) console.log(`  [通过] ${label}: ${realList.length} 项, 顺序与签名完全一致`);
    return bad;
}

function main() {
    if (!fs.existsSync(realPath)) {
        console.error(`[ERROR] 找不到真实头文件: ${realPath}`);
        console.error('        用法: node check_abi.js [真实头文件] [镜像头文件]');
        process.exit(2);
    }
    if (!fs.existsSync(mirrorPath)) {
        console.error(`[ERROR] 找不到镜像头文件: ${mirrorPath}`);
        process.exit(2);
    }

    const real = fs.readFileSync(realPath, 'utf8');
    const mirror = fs.readFileSync(mirrorPath, 'utf8');

    console.log(`真实头文件: ${realPath}`);
    console.log(`ABI 镜像  : ${mirrorPath}\n`);

    let bad = 0;

    console.log('── 接口 vtable 槽位（顺序敏感）──');
    for (const name of INTERFACES) {
        bad += printDiff(name, interfaceSlots(real, name), interfaceSlots(mirror, name));
    }

    console.log('\n── 结构体字段（跨 ABI 传递, 逐字段对齐）──');
    for (const name of STRUCTS) {
        bad += printDiff(name, structFields(real, name), structFields(mirror, name));
    }

    console.log('\n── 版本与导出符号 ──');
    const realVersion = /#define\s+HOLOGLIB_API_VERSION\s+(0x[0-9A-Fa-f]+)/.exec(real);
    const mirrorMin = /kMinApiVersionForNpcDialog\s*=\s*(0x[0-9A-Fa-f]+)/.exec(mirror);
    if (!realVersion) {
        console.log('  [警告] 真实头文件里没找到 HOLOGLIB_API_VERSION');
    } else if (!mirrorMin) {
        console.log('  [失败] 镜像里没找到 kMinApiVersionForNpcDialog');
        bad++;
    } else {
        const rv = parseInt(realVersion[1], 16);
        const mv = parseInt(mirrorMin[1], 16);
        if (mv <= rv) {
            console.log(
                `  [通过] HOLOGLIB_API_VERSION=${realVersion[1]}, 镜像要求最低 ${mirrorMin[1]}（满足）`
            );
        } else {
            console.log(
                `  [失败] 镜像要求最低 ${mirrorMin[1]} 高于真实库版本 ${realVersion[1]} —— NPC 对话会一直判定为不可用`
            );
            bad++;
        }
    }

    const symbol = /kGetInstanceSymbol\s*=\s*"([^"]+)"/.exec(mirror);
    if (!symbol) {
        console.log('  [失败] 镜像里没找到 kGetInstanceSymbol');
        bad++;
    } else {
        const s = symbol[1];
        const ok = s.includes('getInstance') && s.includes('IHologramLib') && s.includes('hologramlib');
        console.log(`  ${ok ? '[通过]' : '[失败]'} 导出符号: ${s}`);
        if (!ok) bad++;
    }

    console.log(`\n=== ${bad === 0 ? 'ABI 镜像一致' : bad + ' 处不一致'} ===`);
    process.exit(bad === 0 ? 0 : 1);
}

main();
