// LiteLoader-AIDS automatic generated
/// <reference path="d:\lse/dts/helperlib/src/index.d.ts"/>
const bn = require('bytenode');
const JavaScriptObfuscator = require('javascript-obfuscator');
const ProgressBar = require('progress');
const path = require('path');
const fs = require('fs');
const { EventEmitter } = require('events');

const version = [1, 0, 8];
const pluginName = 'yoyo-plugin-template';

ll.registerPlugin(
  /* name */ pluginName,
  /* introduction */ "一款支持多个插件的模板-可以对子插件进行、混淆、转jsc等加密措施。",
  /* version */version,
  /* otherInformation */ {
    author: "Yoyo",
    qq: '1294858802'
  }
);
const isLoadLLplugin = true;//是否加载ll插件目录的jsc子扩展？
const llPluginDir = path.join(process.cwd(), 'plugins');
const subDir = path.join(__dirname, '../plugins');
const mixDir = path.join(__dirname, '../mix');
const jscDir = path.join(__dirname, '../jsc');

// Wine 兼容性处理：检测 stdout 是否可用
const isWineEnvironment = (() => {
  try {
    // 尝试访问 stdout，如果失败说明在 Wine 环境下
    process.stdout;
    return false;
  } catch (e) {
    return e.code === 'EBADF';
  }
})();

// 安全的日志输出函数，兼容 Wine 环境
const safeLog = (originalFn, ...args) => {
  try {
    originalFn.apply(console, args);
  } catch (e) {
    if (e.code === 'EBADF') {
      // Wine 环境下使用 ll.log 作为备选
      if (typeof ll !== 'undefined' && ll.log) {
        ll.log(args.map(a => a instanceof Error ? a.stack : String(a)).join(' '));
      }
    } else {
      throw e;
    }
  }
};

const originalLog = console.log.bind(console);
const originalWarn = console.warn.bind(console);
const originalError = console.error.bind(console);

console.success = function(...args) {
  const formatted = args.map(a => `${a instanceof Error ? a.stack : a}`);
  if (isWineEnvironment) {
    safeLog(originalLog, '✓ SUCCESS:', ...formatted);
  } else {
    safeLog(originalLog, '\x1b[48;5;194m\x1b[38;5;0m✓ SUCCESS:\x1b[0m', ...formatted.map(a => `\x1b[48;5;194m\x1b[38;5;0m${a}\x1b[0m`));
  }
};

console.warn = function(...args) {
  const formatted = args.map(a => `${a instanceof Error ? a.stack : a}`);
  if (isWineEnvironment) {
    safeLog(originalWarn, '⚠️ WARNING:', ...formatted);
  } else {
    safeLog(originalWarn, '\x1b[48;5;229m\x1b[38;5;0m⚠️ WARNING:\x1b[0m', ...formatted.map(a => `\x1b[48;5;229m\x1b[38;5;0m${a}\x1b[0m`));
  }
};

console.error = function(...args) {
  const formatted = args.map(a => `${a instanceof Error ? a.stack : a}`);
  if (isWineEnvironment) {
    safeLog(originalError, '❌ ERROR:', ...formatted);
  } else {
    safeLog(originalError, '\x1b[48;5;224m\x1b[38;5;0m❌ ERROR:\x1b[0m', ...formatted.map(a => `\x1b[48;5;224m\x1b[38;5;0m${a}\x1b[0m`));
  }
};
const gl = {
  dirname: subDir
};


// 初始化全局(方法)
Object.defineProperty(global, '$Y', {
  value: gl,
  writable: false,   // 不可修改
  enumerable: false,  // 不可枚举
  configurable: false // 不可删除或重新定义
});


// 初始化插件文件夹
fs.mkdirSync(subDir, { recursive: true });

// 封装类


class PluginsManage {
  /**
   * 插件储存 
   */
  _items = [];
  _plugins = new Map();
  _EventEmitter = new EventEmitter();
  /**
   * 返回数组长度作为插件数量 
   */
  get count() {
    return this._items.length;
  }

  constructor() {
    this._items = [];//清空全部插件缓存
    // 获取ll目录的jsc插件列表
    const llPluginJscList = this.getLlPluginJscList();

    //获取所有插件文件名列表
    const pluginList = this.getPluginList();

    const list = [...llPluginJscList, ...pluginList];// 插件列表

    // 为插件注册全局api
    this.regPluginFun();
    console.success(`[Y]开始加载子插件：`)
    // 加载所有插件(先用filterDirents筛选重复的)
    this.loadPlugin(filterDirents(list));

    // 全部加载完成需要做些事情
    // 调用指定方法
    const convey = {
      count: this.count,
      getPluginFileName: this.getPluginFileName,//通过扩展名找到文件名
    };//需要传递给子扩展插件的

    // 向所有子插件调用它们的onAllLoaded方法以system等级
    this.callPluginFun(this._items.map(v => v.fileName), 'onAllLoaded', [convey], 'system');
  }

  /**
   * 获取插件目录的文件列表 
   * @returns 
   */
  getPluginList() {
    const fileList = fs.readdirSync(subDir, { recursive: false, withFileTypes: true });
    return fileList.filter((file) => (String(file.name).endsWith('.js') || String(file.name).endsWith('.jsc')) && file.isFile());
  }

  /**
   * 获取LL插件目录的jsc文件列表 
   * @returns 
   */
  getLlPluginJscList() {
    if(!isLoadLLplugin) return [];// 不加载ll插件目录的jsc子扩展直接拦截返回空数组即可
    const fileList = fs.readdirSync(llPluginDir, { recursive: false, withFileTypes: true });
    return fileList.filter((file) => String(file.name).endsWith('.jsc') && file.isFile());
  }

  /**
   * 为扩展插件注册方法
   */
  regPluginFun() {
    // 注册一个 i 用于插件之间的Api调用
    Object.defineProperty(gl, 'i', {
      get: () => new Proxy({}, {
        get: (target, pluginName) => new Proxy({}, {
          get: (target, apiName) => {
            const pluginFileName = this.getPluginFileName(pluginName);
            // 帮助插件调用插件的api
            if (pluginFileName) return (...param) => this.callPluginFun(pluginFileName, apiName, param, 'plugin');
            return () => {
              console.warn(`[Y]"${pluginName}" 插件不存在！所以 "${apiName}" 无法调用！请检查: "$Y.i.${pluginName}.${apiName}"`);
              return undefined;
            };
          }
        })
      })
    });

    /**
     * 返回是Promise 
     *  - 如果值是false说明拦截了
     *  - true不拦截也没有修改 
     *  - 如果返回是Array说明是修改了事件值
     */
    Object.defineProperty(gl, 'emit', {
      get: () => {
        return async (event, ...args) => {
          const listeners = this._EventEmitter.listeners(event);
          if (listeners.length <= 0) return true;//没有注册的
          // 手动帮忙注册
          let isIntercept = undefined;//默认不拦截(true是放行)
          let same = args;// 得到修改后的值(根据多个监听最终返回的为准)
          listeners.forEach(async (fun) => {
            let result = fun.apply(fun, args);
            if (result instanceof Promise) result = await result;//把异步参数提取出来
            if (result === false) isIntercept = false;//遇到拦截的修改状态
            if (Array.isArray(result) && this.areBasicTypesEqual(same, result)) {
              // 修改了值那么isIntercept需要设置true才能返回值
              same = result;
              isIntercept = true;
            }
          });
          if (isIntercept === true) return same;//已数组方式返回回去
          return isIntercept === undefined ? true : false;//返回拦截状态 undefined也放行并且没有修改值
        }
      }
    });

    // 注册监听的
    Object.defineProperty(gl, 'on', {
      get: () => {
        return (event, callblock) => this._EventEmitter.on(event, callblock)
      }
    });

    Object.defineProperty(gl, 'off', {
      get: () => {
        return (event, callblock) => this._EventEmitter.off(event, callblock)
      }
    });

  }

  // 判断两个数组元素的类型是否一致
  areBasicTypesEqual(arr1, arr2) {
    if (arr1.length !== arr2.length) return false;
    const getType = (data) => Object.prototype.toString.call(data).replace(/^\[object (.+)\]$/, '$1').toLowerCase();
    return arr1.every((element, index) => {
      return getType(element) === getType(arr2[index]);
    });
  }

  /**
   * 加载插件
   * @param pluginNameList 插件名字数组
   */
  loadPlugin(pluginNameList) {
    pluginNameList.forEach((Dirent, index) => {
      try {
        const loadHint = `✓[Y] 加载扩展插件 "${Dirent.name}" 耗时`;
        console.time(loadHint);
        const pluginPath = path.join(Dirent.parentPath, Dirent.name);
        const plugin = require(pluginPath);
        // 初始化插件的信息
        const pluginInfo = plugin.INFO ?? {};
        this._items.push({
          name: pluginInfo.name ?? String(Dirent.name).replace(/^(.*)\.(js|jsc)$/, '$1'),
          fileName: Dirent.name,
          path: pluginPath,
          version: pluginInfo.version ?? [0, 0, 1],
          author: pluginInfo.author ?? `未知`
        });
        this._plugins.set(Dirent.name, plugin);
        console.timeEnd(loadHint);
      } catch (error) {
        console.error(`[Y]插件 "${Dirent.name}" 加载失败 原因: `,error);
      }
    });

  }

  /**
   * 获取所有已加载的插件
   * @returns 
   */
  getRamPluginList() {
    return this._items.slice();
  }

  //调用扩展插件内导出的方法

  callPluginFun(pluginOrName, apiName, param, tag) {
    if (Array.isArray(pluginOrName)) {
      // 处理插件名数组的情况

      pluginOrName.forEach(fileName => this.callPluginFun(fileName, apiName, param, tag));
    } else {
      // 处理单个插件名字符串的情况
      const pluginFileName = this.getPluginFileName(pluginOrName);//找到正确的文件名称
      if (!pluginFileName) return;
      pluginOrName = pluginFileName;//替换为正确的文件名称
      const pluginNode = this._plugins.get(pluginOrName);
      if (pluginNode) {
        if (!apiName) return new Error(`你应该提供 ApiName 否则无法调用。`);
        try {
          if (!Array.isArray(param)) param = [param];

          if (tag == 'system') {
            if (typeof pluginNode[apiName] != 'function') return;//目标插件未实现该API方法 系统调用函数可以无视
            try {
              return pluginNode[apiName].call(pluginNode[apiName], ...[pluginOrName, ...param]);
            } catch (error) {
              console.warn(`[Y]扩展 "${pluginOrName}" 的 "${apiName}" 接口 出现问题: `,error);
            }
            return;
          }

          if (tag == 'plugin') {
            if (!pluginNode.API) throw new Error(`目标插件未定义API列表`);
            if (typeof pluginNode.API[apiName] != 'function') throw new Error(`"${pluginOrName}" 插件未声明 "${apiName}" 方法 所以无法调用！`);
            return pluginNode.API[apiName].call(pluginNode[apiName], ...param);
          }
          throw new Error(`Tag 参数错误！`);

        } catch (error) {
          console.warn(`[Y]调用扩展 "${pluginOrName}" 的 "${apiName}" 接口 出现问题: "${error.message}"`);
          return undefined;
        }
      }
      console.error(`[Y]"${pluginOrName}" 插件不存在！所以 "${apiName}" 无法调用！`);
      return undefined;
    }
  }

  /**
   * 通过插件名称获取插件的文件名字带后缀
   * @param PluginName 插件名称
   * @returns 
   */
  getPluginFileName(PluginName) {
    const fileName = path.parse(PluginName);
    const inPlugin = this._items.find(plugin => plugin.name === PluginName || [`${fileName.name}.js`, `${fileName.name}.jsc`].includes(plugin.fileName));
    if (inPlugin) return inPlugin.fileName;
    return undefined;
  }
}


const PluginsClass = new PluginsManage();
// 在插件对象实例后导出接口
exportApi();

// 导入DebugShapeAPI用于悬浮字管理
let DebugShapeAPI = null;
try {
  DebugShapeAPI = require('./DebugShapeAPI');
} catch (e) {
  // 忽略加载错误
}

mc.listen("onServerStarted", () => {
  regCmd();
  
  // 向所有子插件调用它们的onServerStarted方法以system等级
  PluginsClass.callPluginFun(PluginsClass.getRamPluginList().map(v => v.fileName), 'onServerStarted', undefined, 'system');
});

/**
 * 注册命令(控制台的)
 */
function regCmd() {
  const cmd = mc.newCommand("yjsc", "用于混淆和编译jsc的文件", PermType.Console);
  cmd.setAlias("yj");
  cmd.setEnum("types", ["mix", "jsc", "add"]);
  cmd.setEnum("list", ["list"]);
  cmd.mandatory("types", ParamType.Enum, "types", 1);
  cmd.mandatory("list", ParamType.Enum, "list", 1);
  cmd.optional("plugin", ParamType.String);
  cmd.overload(["types", "plugin"]);
  cmd.overload(["list"]);
  cmd.overload([]);
  cmd.setCallback(async (_cmd, _ori, out, res) => {
    const resCount = Object.keys(res).length;
    if (resCount === 0) {
      console.log('命令 yjsc  别名 yj （作者：Yoyo）v ' + version.join('.') + '\n例：\n    yjsc mix test - 混淆test\n    yjsc jsc test - 编译test成jsc\n\n所有命令：\n   yjsc - 显示帮助信息\n   yjsc mix [插件名]- 混淆指定子插件\n   yjsc jsc [插件名] - 编译指定子插件成jsc\n   yjsc add <插件名> - 新建一个子扩展模板\n   yjsc list - 列出所有子插件\n说明：\n    plugins 子插件目录出现 a.js和a.jsc那么优先级是.js > .jsc\n    如果LL目录的plugins的a.js和插件内置的a.js那么后者不会加载\n   混淆和编译的子插件会放在插件根目录的 mix和jsc文件夹下');
      return;
    }



    if (res.list === 'list') {
      const entries = fs.readdirSync(subDir, { withFileTypes: true });
      const fileListName = entries.filter(f => f.isFile() && (f.name.endsWith('.js') || f.name.endsWith('.jsc')))
        .map(f => ({ name: f.name, size: fs.statSync(path.join(subDir, f.name)).size }))
        .sort((a, b) => b.size - a.size)
        .map((f, i) => `\x1B[32m${i + 1}\x1B[0m、${f.name} [${formatFileSize(f.size)}]`);
      console.log(`[Y]子插件目录列表：(只有.js才能混淆和编译jsc)\n${fileListName.join('\n')}`);
      return;
    }

    if (res.types === 'add') {
      if (!res.plugin) return out.error('请输入要添加的子插件名');
      const newSubPlugin = path.join(subDir, `${res.plugin}.js`);
      if (fs.existsSync(newSubPlugin)) return out.error(`子插件“${res.plugin}”已存在了。`);
      const headerStr = `/** \n * 文档地址：https://www.minebbs.com/resources/yoyo-plugin-template-an-quan-de-zi-kuo-zhan-kai-fa-mo-ban-bi-kan.11037/\n * 该插件是Node.js开发所以你任然可以用require去引入Node的内置模块使用\n * 注意：Node.js的模块是单线程的，所以不要在Node.js模块中执行耗时操作，否则可能会导致其他模块无法正常执行，这边建议使用 worker-loader 来实现多线程\n * @author Yoyo QQ:1294858802\n * @description 这是一个子插件模板\n * @version ${version.join('.')}\n **/`;
      fs.writeFileSync(newSubPlugin, `${headerStr}\nexports.INFO = {\n  name: '${res.plugin}',\n  version: [0, 0, 1],\n  introduction: '这是一个子插件模板',\n  author: '作者名称'\n};\n\n// 内部导出的api和配置其它子扩展可以通过 $Y.i.<插件名|文件名>.api(参数) 调用执行\nexports.API = {\n  yoyo: () => {}\n};\n// 全部子插件加载完成\nexports.onAllLoaded = () => {\n\n\n};\n// 服务器启动完成\nexports.onServerStarted = () => {\n\n\n};`);
      return out.success(`子插件“${res.plugin}”添加成功。`);
    }


    const pluginArr = [];
    const errorArr = [];

    if (res.plugin) {
      pluginArr.push(res.plugin);
    } else {
      const entries = fs.readdirSync(subDir, { withFileTypes: true });
      const fileListName = entries.filter(f => f.isFile() && f.name.endsWith('.js')).map(f => f.name.replace(/\.js$/i, ''));
      pluginArr.push(...fileListName);
    }

    // 创建进度条
    const bar = new ProgressBar('[:bar] 进度:percent :current/:total \x1B[33m耗时:elapseds\x1B[0m :msg', {
      total: pluginArr.length,
      width: 40,
      complete: '\x1B[32m■\x1B[0m',
      incomplete: '\x1B[33m□\x1B[0m'
    });



    for (const p of pluginArr) {
      await sleep(100);// 延迟100ms 故意加的不然进度条就看不到了
      const plugin = path.parse(p);// 子插件名
      const filename = path.join(subDir, `${plugin.base.replace(/\.js/i, '')}.js`);
      const result = mixJsc(filename, res.types, bar);
      bar.tick({
        msg: `\x1B[33m处理：${plugin.base}\x1B[0m`
      });
      if (result) continue;
      errorArr.push(plugin.name);
    }

    console.log(`共 ${pluginArr.length} 个子插件，成功 \x1B[32m${pluginArr.length - errorArr.length}\x1B[0m 个，失败 \x1B[31m${errorArr.length}\x1B[0m 个`)

  });
  cmd.setup();

  // 注册 mst 命令 - 收费插件模板生成器
  regMstCmd();
  
  // 注册悬浮字刷新命令
  regFloatingTextCmd();
}

/**
 * 注册悬浮字刷新命令
 */
function regFloatingTextCmd() {
  const ftCmd = mc.newCommand("ftrefresh", "刷新所有悬浮字（用于reload后刷新）", PermType.GameMasters);
  ftCmd.setAlias("ftr");
  ftCmd.setEnum("action", ["all", "list", "category"]);
  ftCmd.mandatory("action", ParamType.Enum, "action", 1);
  ftCmd.optional("categoryName", ParamType.String);
  ftCmd.overload(["action", "categoryName"]);
  ftCmd.overload(["action"]);
  ftCmd.overload([]);
  ftCmd.setCallback((_cmd, _ori, out, res) => {
    if (!DebugShapeAPI) {
      return out.error('DebugShapeAPI 未加载');
    }
    
    if (!DebugShapeAPI.isAvailable()) {
      return out.error('DebugShape API 不可用');
    }
    
    const resCount = Object.keys(res).length;
    if (resCount === 0 || res.action === 'all') {
      // reload刷新所有悬浮字
      DebugShapeAPI.reloadRefreshAll();
      return out.success('已触发reload刷新所有悬浮字');
    }
    
    if (res.action === 'list') {
      // 列出所有已注册的分类
      const stats = DebugShapeAPI.getFloatingTextStats();
      let msg = '悬浮字管理系统状态:\n';
      msg += `  API可用: ${stats.apiAvailable ? '是' : '否'}\n`;
      msg += `  总悬浮字数: ${stats.totalTexts}\n`;
      msg += '已注册分类:\n';
      for (const cat of stats.categories) {
        msg += `  - ${cat.name}: ${cat.count} 个悬浮字\n`;
      }
      console.log(msg);
      return out.success('已输出悬浮字状态到控制台');
    }
    
    if (res.action === 'category') {
      if (!res.categoryName) {
        return out.error('请指定要刷新的分类名称');
      }
      DebugShapeAPI.reloadRefreshCategory(res.categoryName);
      return out.success(`已触发reload刷新分类: ${res.categoryName}`);
    }
  });
  ftCmd.setup();
}

/**
 * 注册 mst 命令 - 收费插件模板生成器（带 serverTelemetry 授权验证）
 */
function regMstCmd() {
  const mstCmd = mc.newCommand("mst", "创建带授权验证的收费插件模板或编译为收费jsc", PermType.Console);
  mstCmd.setEnum("types", ["add", "jsc"]);
  mstCmd.mandatory("types", ParamType.Enum, "types", 1);
  mstCmd.optional("plugin", ParamType.String);
  mstCmd.overload(["types", "plugin"]);
  mstCmd.overload([]);
  mstCmd.setCallback(async (_cmd, _ori, out, res) => {
    const resCount = Object.keys(res).length;
    if (resCount === 0) {
      console.log('命令 mst （作者：伊希娅）\n用于创建带 serverTelemetry 授权验证的收费插件\n\n用法：\n   mst add <插件名> - 创建一个带授权验证的收费插件模板\n   mst jsc <插件名> - 将插件编译为带授权验证的jsc（文件名带[mst]前缀）\n\n说明：\n   mst add: 生成的插件模板包含完整的授权验证逻辑\n   mst jsc: 编译后的jsc文件会注入授权验证代码，未授权无法加载');
      return;
    }

    if (res.types === 'add') {
      if (!res.plugin) return out.error('请输入要创建的插件名');
      
      const newPluginPath = path.join(subDir, `${res.plugin}.js`);
      if (fs.existsSync(newPluginPath)) return out.error(`插件"${res.plugin}"已存在了。`);
      
      // 生成收费插件模板
      const template = generatePaidPluginTemplate(res.plugin);
      fs.writeFileSync(newPluginPath, template);
      return out.success(`收费插件模板"${res.plugin}"创建成功！\n请在 serverTelemetry 后台添加该插件的授权配置。`);
    }

    if (res.types === 'jsc') {
      if (!res.plugin) return out.error('请输入要编译的插件名');
      
      const pluginParse = path.parse(res.plugin);
      const pluginName = pluginParse.name;
      const sourcePath = path.join(subDir, `${pluginName}.js`);
      
      if (!fs.existsSync(sourcePath)) {
        return out.error(`插件"${pluginName}.js"不存在！`);
      }
      
      console.log(`[mst] 正在编译收费插件: ${pluginName}`);
      
      try {
        // 编译为带授权验证的 jsc
        const result = await compilePaidJsc(sourcePath, pluginName);
        if (result.success) {
          if (result.skipped) {
            return out.success(`收费插件编译成功！（已检测到授权验证代码，跳过注入）\n输出文件: ${result.outputPath}\n请将此文件放入 plugins 目录使用。`);
          }
          return out.success(`收费插件编译成功！（已注入授权验证代码）\n输出文件: ${result.outputPath}\n请将此文件放入 plugins 目录使用。`);
        } else {
          return out.error(`编译失败: ${result.error}`);
        }
      } catch (error) {
        return out.error(`编译异常: ${error.message}`);
      }
    }
  });
  mstCmd.setup();
}

/**
 * 检测代码是否已包含 mst 授权验证
 * @param {string} code 源代码
 * @returns {boolean} 是否已包含授权验证
 */
function hasMstLicenseCheck(code) {
  // 检测特征标记
  const markers = [
    '__MST_PLUGIN_NAME__',
    '__MST_LICENSE_VERIFIED__',
    '__mst_do_verify__',
    '[mst] 授权验证模块',
    'mst jsc 命令自动注入'
  ];
  
  return markers.some(marker => code.includes(marker));
}

/**
 * 编译插件为带授权验证的 jsc 文件
 * @param {string} sourcePath 源文件路径
 * @param {string} pluginName 插件名称
 * @returns {Promise<{success: boolean, outputPath?: string, error?: string, skipped?: boolean}>}
 */
async function compilePaidJsc(sourcePath, pluginName) {
  try {
    // 确保 jsc 目录存在
    if (!fs.existsSync(jscDir)) {
      fs.mkdirSync(jscDir, { recursive: true });
    }
    
    // 读取原始插件代码
    const originalCode = fs.readFileSync(sourcePath, 'utf8');
    
    // 检测是否已包含授权验证代码
    if (hasMstLicenseCheck(originalCode)) {
      console.log(`[mst] 插件 "${pluginName}" 已包含授权验证代码，跳过注入`);
      
      // 直接编译，不再注入
      const outputFileName = `[mst]${pluginName}.jsc`;
      const outputPath = path.join(jscDir, outputFileName);
      
      bn.compileFile({
        filename: sourcePath,
        output: outputPath,
        compileAsModule: true
      });
      
      console.log(`[mst] 编译完成: ${outputFileName}`);
      
      return {
        success: true,
        outputPath: outputPath,
        skipped: true
      };
    }
    
    // 生成授权验证包装代码
    const wrappedCode = generateLicenseWrapper(pluginName, originalCode);
    
    // 创建临时文件
    const tempDir = path.join(jscDir, '.temp');
    if (!fs.existsSync(tempDir)) {
      fs.mkdirSync(tempDir, { recursive: true });
    }
    const tempFilePath = path.join(tempDir, `${pluginName}_wrapped.js`);
    fs.writeFileSync(tempFilePath, wrappedCode, 'utf8');
    
    // 输出文件名带 [mst] 前缀
    const outputFileName = `[mst]${pluginName}.jsc`;
    const outputPath = path.join(jscDir, outputFileName);
    
    // 编译为 jsc
    bn.compileFile({
      filename: tempFilePath,
      output: outputPath,
      compileAsModule: true
    });
    
    // 清理临时文件
    try {
      fs.unlinkSync(tempFilePath);
      fs.rmdirSync(tempDir);
    } catch (e) {
      // 忽略清理错误
    }
    
    console.log(`[mst] 编译完成: ${outputFileName}`);
    
    return {
      success: true,
      outputPath: outputPath
    };
  } catch (error) {
    console.error(`[mst] 编译失败: ${error.message}`);
    return {
      success: false,
      error: error.message
    };
  }
}

/**
 * 生成授权验证包装代码
 * @param {string} pluginName 插件名称
 * @param {string} originalCode 原始插件代码
 * @returns {string} 包装后的代码
 */
function generateLicenseWrapper(pluginName, originalCode) {
  // 授权验证前置代码
  const licenseCheckCode = `
// ==================== [mst] 授权验证模块 ====================
// 此代码由 mst jsc 命令自动注入，用于收费插件授权验证
// 未经授权的服务器无法加载此插件

const __MST_PLUGIN_NAME__ = '${pluginName}';
let __MST_LICENSE_VERIFIED__ = false;
let __MST_ORIGINAL_EXPORTS__ = {};

// 延迟验证标记
let __MST_VERIFICATION_PENDING__ = true;

// 保存原始的生命周期函数
let __MST_ORIGINAL_ON_ALL_LOADED__ = null;
let __MST_ORIGINAL_ON_SERVER_STARTED__ = null;

/**
 * 执行授权验证
 */
async function __mst_do_verify__() {
    if (!__MST_VERIFICATION_PENDING__) return __MST_LICENSE_VERIFIED__;
    
    try {
        // 检查 serverTelemetry 是否可用
        if (typeof $Y === 'undefined' || !$Y.i || !$Y.i.serverTelemetry) {
            console.error('[' + __MST_PLUGIN_NAME__ + '] serverTelemetry 插件未加载，无法验证授权');
            __MST_VERIFICATION_PENDING__ = false;
            return false;
        }
        
        // 调用异步验证
        const result = await $Y.i.serverTelemetry.verifyLicense(__MST_PLUGIN_NAME__, {
            allowDegradedMode: false
        });
        
        __MST_LICENSE_VERIFIED__ = result && result.authorized === true;
        __MST_VERIFICATION_PENDING__ = false;
        
        if (__MST_LICENSE_VERIFIED__) {
            console.log('[' + __MST_PLUGIN_NAME__ + '] 授权验证通过');
            if (result.expiresAt) {
                const daysLeft = Math.ceil((result.expiresAt - Date.now()) / 86400000);
                console.log('[' + __MST_PLUGIN_NAME__ + '] 授权剩余: ' + daysLeft + ' 天');
            }
        } else {
            console.error('[' + __MST_PLUGIN_NAME__ + '] 授权验证失败: ' + (result ? result.message : '未知错误'));
            console.error('[' + __MST_PLUGIN_NAME__ + '] 此插件需要有效授权才能使用，请联系作者获取授权');
        }
        
        return __MST_LICENSE_VERIFIED__;
    } catch (error) {
        console.error('[' + __MST_PLUGIN_NAME__ + '] 授权验证异常: ' + error.message);
        __MST_VERIFICATION_PENDING__ = false;
        return false;
    }
}

// 初始加载时的快速检查（非阻塞）
(function __mst_initial_check__() {
    // 检查 serverTelemetry 是否存在
    if (typeof $Y === 'undefined' || !$Y.i || !$Y.i.serverTelemetry) {
        console.warn('[' + __MST_PLUGIN_NAME__ + '] serverTelemetry 未就绪，将在 onAllLoaded 时验证授权');
        return;
    }
    
    // 尝试获取缓存的授权状态
    try {
        const telemetry = $Y.i.serverTelemetry;
        if (typeof telemetry.getLicenseStatus === 'function') {
            const status = telemetry.getLicenseStatus();
            if (status && status.licensedPlugins && Array.isArray(status.licensedPlugins)) {
                // 检查插件是否在授权列表中
                const isLicensed = status.licensedPlugins.some(p => 
                    p.name === __MST_PLUGIN_NAME__ || p.pluginName === __MST_PLUGIN_NAME__
                );
                if (isLicensed) {
                    console.log('[' + __MST_PLUGIN_NAME__ + '] 检测到授权缓存，将在 onAllLoaded 时完成验证');
                }
            }
        }
    } catch (e) {
        // 忽略初始检查错误
    }
})();

// ==================== 原始插件代码开始 ====================

`;

  // 后置代码 - 拦截生命周期函数
  const postCode = `

// ==================== [mst] 生命周期拦截 ====================

// 保存原始导出
__MST_ORIGINAL_ON_ALL_LOADED__ = exports.onAllLoaded;
__MST_ORIGINAL_ON_SERVER_STARTED__ = exports.onServerStarted;
__MST_ORIGINAL_EXPORTS__ = { ...exports };

// 重写 onAllLoaded - 在此进行授权验证
exports.onAllLoaded = async function(...args) {
    // 等待 serverTelemetry 初始化
    let waitCount = 0;
    const maxWait = 100; // 最多等待10秒
    while ((!$Y || !$Y.i || !$Y.i.serverTelemetry) && waitCount < maxWait) {
        await new Promise(resolve => setTimeout(resolve, 100));
        waitCount++;
    }
    
    // 执行授权验证
    const isAuthorized = await __mst_do_verify__();
    
    if (!isAuthorized) {
        console.error('[' + __MST_PLUGIN_NAME__ + '] 插件功能已禁用，请联系作者获取授权');
        return;
    }
    
    // 授权通过，调用原始的 onAllLoaded
    if (typeof __MST_ORIGINAL_ON_ALL_LOADED__ === 'function') {
        return __MST_ORIGINAL_ON_ALL_LOADED__.apply(this, args);
    }
};

// 重写 onServerStarted - 检查授权状态
exports.onServerStarted = function(...args) {
    if (!__MST_LICENSE_VERIFIED__) {
        // 未授权，不执行
        return;
    }
    
    // 授权通过，调用原始的 onServerStarted
    if (typeof __MST_ORIGINAL_ON_SERVER_STARTED__ === 'function') {
        return __MST_ORIGINAL_ON_SERVER_STARTED__.apply(this, args);
    }
};

// 包装 API - 所有 API 调用都需要检查授权
if (exports.API && typeof exports.API === 'object') {
    const originalAPI = { ...exports.API };
    exports.API = new Proxy(originalAPI, {
        get(target, prop) {
            if (!__MST_LICENSE_VERIFIED__ && __MST_VERIFICATION_PENDING__ === false) {
                return function() {
                    console.warn('[' + __MST_PLUGIN_NAME__ + '] 未授权，API 调用被拒绝: ' + String(prop));
                    return undefined;
                };
            }
            return target[prop];
        }
    });
    
    // 添加授权状态查询 API
    exports.API.__mst_isLicensed__ = () => __MST_LICENSE_VERIFIED__;
    exports.API.__mst_getLicenseStatus__ = () => ({
        verified: __MST_LICENSE_VERIFIED__,
        pending: __MST_VERIFICATION_PENDING__,
        pluginName: __MST_PLUGIN_NAME__
    });
}

// ==================== [mst] 授权验证模块结束 ====================
`;

  return licenseCheckCode + originalCode + postCode;
}

/**
 * 生成带授权验证的收费插件模板
 * @param {string} pluginName 插件名称
 * @returns {string} 插件模板代码
 */
function generatePaidPluginTemplate(pluginName) {
  const template = `// LiteLoader-AIDS automatic generated
/// <reference path="d:\\lse/dts/helperlib/src/index.d.ts"/>

/**
 * ${pluginName} - 收费插件
 * 需要 serverTelemetry 授权验证
 * 
 * @author 你的名字
 * @version 1.0.0
 */

// ==================== 插件信息 ====================
const PLUGIN_NAME = '${pluginName}';
const PLUGIN_VERSION = [1, 0, 0];

// ==================== 插件信息导出 ====================
exports.INFO = {
    name: PLUGIN_NAME,
    version: PLUGIN_VERSION,
    introduction: '${pluginName} 插件',
    author: '你的名字',
    dependency: ['serverTelemetry']  // 声明依赖 serverTelemetry
};

// ==================== 授权状态 ====================
let licenseStatus = {
    authorized: false,
    degradedMode: false,
    message: '未验证',
    checked: false
};

// 插件是否已初始化
let isInitialized = false;

/**
 * 验证插件授权
 * @returns {Promise<Object>} 授权状态
 */
async function verifyPluginLicense() {
    try {
        // 检查 serverTelemetry 是否可用
        if (typeof $Y === 'undefined' || !$Y.i || !$Y.i.serverTelemetry) {
            logger.error(\`[\${PLUGIN_NAME}] serverTelemetry 插件未加载，无法验证授权\`);
            licenseStatus = {
                authorized: false,
                degradedMode: false,
                message: 'serverTelemetry 插件未加载',
                checked: true
            };
            return licenseStatus;
        }

        // 调用 serverTelemetry 的授权验证 API
        const result = await $Y.i.serverTelemetry.verifyLicense(PLUGIN_NAME, {
            allowDegradedMode: false  // 不允许降级模式，必须有授权才能使用
        });

        licenseStatus = {
            authorized: result.authorized,
            degradedMode: result.degradedMode || false,
            message: result.message || '验证完成',
            checked: true,
            expiresAt: result.expiresAt || null
        };

        if (licenseStatus.authorized) {
            logger.info(\`[\${PLUGIN_NAME}] 授权验证成功\`);
            if (licenseStatus.expiresAt) {
                const daysLeft = Math.ceil((licenseStatus.expiresAt - Date.now()) / 86400000);
                logger.info(\`[\${PLUGIN_NAME}] 授权剩余: \${daysLeft} 天\`);
            }
        } else {
            logger.error(\`[\${PLUGIN_NAME}] 授权验证失败: \${licenseStatus.message}\`);
        }

        return licenseStatus;
    } catch (error) {
        logger.error(\`[\${PLUGIN_NAME}] 授权验证异常: \${error.message}\`);
        licenseStatus = {
            authorized: false,
            degradedMode: false,
            message: \`验证异常: \${error.message}\`,
            checked: true
        };
        return licenseStatus;
    }
}

/**
 * 检查是否已授权
 * @returns {boolean} 是否已授权
 */
function isLicensed() {
    return licenseStatus.authorized === true;
}

/**
 * 获取授权状态
 * @returns {Object} 授权状态对象
 */
function getLicenseStatus() {
    return { ...licenseStatus };
}

// ==================== 插件主逻辑 ====================

/**
 * 初始化插件功能
 * 在授权验证通过后调用
 */
function initializePlugin() {
    if (isInitialized) return;
    
    logger.info(\`[\${PLUGIN_NAME}] 正在初始化插件功能...\`);
    
    // TODO: 在这里添加你的插件初始化逻辑
    // 例如：注册命令、监听事件、加载配置等
    
    isInitialized = true;
    logger.info(\`[\${PLUGIN_NAME}] 插件初始化完成\`);
}

// ==================== API 导出 ====================

/**
 * 对外导出的 API
 * 其他插件可以通过 $Y.i.${pluginName}.xxx() 调用
 */
exports.API = {
    /**
     * 检查插件是否已授权
     * @returns {boolean} 是否已授权
     */
    isLicensed: () => isLicensed(),
    
    /**
     * 获取授权状态
     * @returns {Object} 授权状态对象
     */
    getLicenseStatus: () => getLicenseStatus(),
    
    /**
     * 刷新授权状态
     * @returns {Promise<Object>} 授权状态
     */
    refreshLicense: async () => {
        if (typeof $Y !== 'undefined' && $Y.i && $Y.i.serverTelemetry) {
            $Y.i.serverTelemetry.clearLicenseCache(PLUGIN_NAME);
        }
        return await verifyPluginLicense();
    },
    
    /**
     * 检查插件是否已初始化
     * @returns {boolean} 是否已初始化
     */
    isInitialized: () => isInitialized,
    
    /**
     * 获取插件版本
     * @returns {string} 版本号
     */
    getVersion: () => PLUGIN_VERSION.join('.')
    
    // TODO: 在这里添加你的插件 API
};

// ==================== 生命周期钩子 ====================

/**
 * 全部子插件加载完成
 */
exports.onAllLoaded = async () => {
    // 等待 serverTelemetry 初始化
    let waitCount = 0;
    const maxWait = 100; // 最多等待10秒
    while ((!$Y || !$Y.i || !$Y.i.serverTelemetry || !$Y.i.serverTelemetry.getToken()) && waitCount < maxWait) {
        await new Promise(resolve => setTimeout(resolve, 100));
        waitCount++;
    }
    
    if (waitCount >= maxWait) {
        logger.error(\`[\${PLUGIN_NAME}] 等待 serverTelemetry 初始化超时，插件无法启动\`);
        return;
    }
    
    // 验证授权
    logger.info(\`[\${PLUGIN_NAME}] 正在验证授权...\`);
    const authResult = await verifyPluginLicense();
    
    if (!authResult.authorized) {
        logger.error(\`[\${PLUGIN_NAME}] 授权验证失败: \${authResult.message}\`);
        logger.error(\`[\${PLUGIN_NAME}] 插件功能已禁用，请联系作者获取授权\`);
        return;
    }
    
    // 授权通过，初始化插件
    initializePlugin();
};

/**
 * 服务器启动完成
 */
exports.onServerStarted = () => {
    if (!isLicensed()) {
        // 未授权，不执行任何操作
        return;
    }
    
    // TODO: 在这里添加服务器启动后需要执行的逻辑
    logger.info(\`[\${PLUGIN_NAME}] 服务器已启动，插件就绪\`);
};
`;

  return template;
}

/**
 * 编译mix或jsc
 * @param {string} filePath 文件路径
 * @param {mix|jsc} type 编译类型
 * @returns 
 */
function mixJsc(filePath, type = 'mix', bar = null) {
  const pathParse = path.parse(filePath);
  if (!fs.existsSync(filePath)) {
    if (bar) bar.interrupt(`\x1B[31m[Y - ${type}]子插件 “${pathParse.name}” 不存在 × 位置：${filePath}\x1B[0m`);
    return false;
  }

  if (type === 'mix') {
    if (!fs.existsSync(mixDir)) fs.mkdirSync(mixDir, { recursive: true });
    const outPath = path.join(mixDir, pathParse.base);
    const obfuscatedCode = JavaScriptObfuscator.obfuscate(fs.readFileSync(filePath, 'utf8'), {
      compact: true,
      controlFlowFlattening: true
    }).getObfuscatedCode();
    fs.writeFileSync(outPath, obfuscatedCode);
    return true;

  }
  if (type === 'jsc') {
    if (!fs.existsSync(jscDir)) fs.mkdirSync(jscDir, { recursive: true });
    const outPath = path.join(jscDir, `${pathParse.name}.jsc`);
    bn.compileFile({
      filename: filePath,
      output: outPath,
      compileAsModule: true
    });
    return true;
  }
  if (bar) bar.interrupt(`\x1B[31m[Y - ${type}]子插件 “${pathParse.name}” 操作失败 × 位置：${outPath}\x1B[0m`);
  return false;
}

function sleep(time = 1000) {
  return new Promise(resolve => setTimeout(resolve, time));
}
function formatFileSize(bytes) {
  if (typeof bytes !== 'number' || bytes < 0) return '0 B';

  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let unitIndex = 0;
  let sizeInKB = bytes / 1024; // Convert to KB for color decision

  while (bytes >= 1024 && unitIndex < units.length - 1) {
    bytes /= 1024;
    unitIndex++;
  }

  // Determine color based on size in KB
  let color;
  if (sizeInKB < 5) {
    color = '\x1B[32m'; // Green (<5KB)
  } else if (sizeInKB < 50) {
    color = '\x1B[33m'; // Yellow (5KB-50KB)
  } else {
    color = '\x1B[31m'; // Red (>50KB)
  }

  const value = unitIndex === 0 ? bytes : bytes.toFixed(2);
  return `${color}${value} ${units[unitIndex]}\x1B[0m`;
}


/**
 * 筛选重复文件以及优先级
 * @param {Array<fs.Dirent>} dirents 
 * @returns {Array<fs.Dirent>}
 */
function filterDirents(dirents) {
  const PRIORITY_EXT = ['js', 'jsc']; // 优先级：js > jsc
  const baseMap = new Map();

  for (const dirent of dirents) {
    const filename = dirent.name;
    const lastDot = filename.lastIndexOf('.');
    const [base, ext] = lastDot === -1
      ? [filename, '']
      : [filename.slice(0, lastDot), filename.slice(lastDot + 1)];

    if (!baseMap.has(base)) {
      baseMap.set(base, new Map());
    }

    const extMap = baseMap.get(base);
    if (!extMap.has(ext)) {
      extMap.set(ext, dirent);
    }
  }

  const result = [];
  for (const [base, extMap] of baseMap) {
    let selected;
    for (const targetExt of PRIORITY_EXT) {
      if (extMap.has(targetExt)) {
        selected = extMap.get(targetExt);
        break;
      }
    }
    if (selected) {
      result.push(selected);
    } else {
      extMap.forEach(dirent => result.push(dirent));
    }
  }

  return result;
}

/**
 * 导出外部api
 */
function exportApi() {
  ll.exports((pluginName, funcName, param) => $Y.i[pluginName][funcName](...param), pluginName, '$Y.i');
}