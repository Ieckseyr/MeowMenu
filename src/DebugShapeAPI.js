/**
 * DebugShape API 完整封装模块
 * 基于 debugshape-lseAPI 插件，提供面向对象的形状管理
 * 
 * 特性：
 * - 支持针对单个玩家渲染（不影响其他玩家）
 * - 支持所有形状类型：文本、线段、方框、圆形、球体、箭头、填充面
 * - 支持多行悬浮字（FloatingText）：多行文本、渐变色、彩虹效果、滚动动画
 * - 支持渐变线段（GradientLine）：渐变色、彩虹效果
 * - 兼容 GMLIB StaticFloatingText 接口
 * - 支持批量操作
 * - 内置持久化存储，服务器重启后自动恢复悬浮字
 * - 支持固定朝向模式（不跟随玩家视角）
 * 
 * @author 伊希娅
 * @version 2.0.0
 */

const fs = require('fs');
const path = require('path');

// ==================== 持久化存储配置 ====================
const DATA_DIR = './Meowdata/FloatingText';
const DATA_FILE = 'floating_texts.json';

/** 悬浮字数据缓存 */
let _storageCache = null;

/** 已创建的持久化悬浮字实例映射 (id -> shape) */
const _persistedShapes = new Map();

// ==================== 导入 DebugShape 原生函数 ====================
let DebugShape = null;
let DebugShapeNative = null;
let FloatingText = null;
let GradientLine = null;
let hasAPI = false;
let hasNativeAPI = false;
let hasFloatingTextAPI = false;
let hasGradientLineAPI = false;

/** API 就绪回调列表 */
const _onReadyCallbacks = [];

/** 是否已触发过就绪回调 */
let _readyCallbacksFired = false;

/** API 检查定时器 */
let _apiCheckTimer = null;

/** API 检查最大次数 */
const API_CHECK_MAX_ATTEMPTS = 30;

/** API 检查间隔（毫秒） */
const API_CHECK_INTERVAL = 1000;

/** 当前检查次数 */
let _apiCheckAttempts = 0;

// ==================== 动态刷新配置 ====================
const _refreshConfig = {
    refreshInterval: 0.05,
    durationMultiplier: 3.0,
    minDuration: 0.2,
    extraOverlap: 0.1
};

function getRefreshInterval() {
    return _refreshConfig.refreshInterval;
}

function setRefreshInterval(interval) {
    if (typeof interval === 'number' && interval > 0) {
        _refreshConfig.refreshInterval = interval;
    }
}

function getDurationMultiplier() {
    return _refreshConfig.durationMultiplier;
}

function setDurationMultiplier(multiplier) {
    if (typeof multiplier === 'number' && multiplier >= 1) {
        _refreshConfig.durationMultiplier = multiplier;
    }
}

function setExtraOverlap(overlap) {
    if (typeof overlap === 'number' && overlap >= 0) {
        _refreshConfig.extraOverlap = overlap;
    }
}

function setMinDuration(minDuration) {
    if (typeof minDuration === 'number' && minDuration > 0) {
        _refreshConfig.minDuration = minDuration;
    }
}

function calculateDuration(interval) {
    const refreshInterval = interval || _refreshConfig.refreshInterval;
    const baseDuration = refreshInterval * _refreshConfig.durationMultiplier;
    const duration = baseDuration + _refreshConfig.extraOverlap;
    return Math.max(duration, _refreshConfig.minDuration);
}

function getRefreshConfig() {
    return {
        refreshInterval: _refreshConfig.refreshInterval,
        durationMultiplier: _refreshConfig.durationMultiplier,
        minDuration: _refreshConfig.minDuration,
        extraOverlap: _refreshConfig.extraOverlap,
        calculatedDuration: calculateDuration()
    };
}

/**
 * 安全导入函数
 */
function safeImport(namespace, funcName) {
    try {
        if (!ll.hasExported(namespace, funcName)) {
            return null;
        }
        const fn = ll.import(namespace, funcName);
        return typeof fn === 'function' ? fn : null;
    } catch (e) {
        return null;
    }
}

/**
 * 检查 DebugShape API 是否可用
 */
function checkAPIAvailable() {
    try {
        return ll.hasExported("DebugShape", "createText") && 
               ll.hasExported("DebugShape", "draw");
    } catch (e) {
        return false;
    }
}

/**
 * 初始化 DebugShape API
 */
function initializeAPI() {
    if (hasAPI) return true;
    
    if (!checkAPIAvailable()) return false;
    
    try {
        // ==================== DebugShape 命名空间 ====================
        DebugShape = {
            // 创建函数
            createText: safeImport("DebugShape", "createText"),
            createLine: safeImport("DebugShape", "createLine"),
            createBox: safeImport("DebugShape", "createBox"),
            createCircle: safeImport("DebugShape", "createCircle"),
            createSphere: safeImport("DebugShape", "createSphere"),
            createArrow: safeImport("DebugShape", "createArrow"),
            createFilledQuad: safeImport("DebugShape", "createFilledQuad"),
            createFilledQuadBatch: safeImport("DebugShape", "createFilledQuadBatch"),
            
            // 属性设置
            setText: safeImport("DebugShape", "setText"),
            setLocation: safeImport("DebugShape", "setLocation"),
            setColor: safeImport("DebugShape", "setColor"),
            setScale: safeImport("DebugShape", "setScale"),
            setDuration: safeImport("DebugShape", "setDuration"),
            setRotation: safeImport("DebugShape", "setRotation"),
            clearRotation: safeImport("DebugShape", "clearRotation"),
            
            // 属性获取
            getText: safeImport("DebugShape", "getText"),
            getLocation: safeImport("DebugShape", "getLocation"),
            getColor: safeImport("DebugShape", "getColor"),
            getRotation: safeImport("DebugShape", "getRotation"),
            getShapeType: safeImport("DebugShape", "getShapeType"),
            
            // 显示控制 - 渲染
            draw: safeImport("DebugShape", "draw"),
            drawToPlayer: safeImport("DebugShape", "drawToPlayer"),
            drawToDimension: safeImport("DebugShape", "drawToDimension"),
            
            // 显示控制 - 移除
            remove: safeImport("DebugShape", "remove"),
            removeToPlayer: safeImport("DebugShape", "removeToPlayer"),
            removeToDimension: safeImport("DebugShape", "removeToDimension"),
            
            // 显示控制 - 更新
            update: safeImport("DebugShape", "update"),
            updateToPlayer: safeImport("DebugShape", "updateToPlayer"),
            updateToDimension: safeImport("DebugShape", "updateToDimension"),
            
            // 批量操作
            drawBatch: safeImport("DebugShape", "drawBatch"),
            destroyBatch: safeImport("DebugShape", "destroyBatch"),
            
            // 查询功能
            findTextByLocation: safeImport("DebugShape", "findTextByLocation"),
            findTextByLocationAndContent: safeImport("DebugShape", "findTextByLocationAndContent"),
            getAllShapeIds: safeImport("DebugShape", "getAllShapeIds"),
            exists: safeImport("DebugShape", "exists"),
            
            // 生命周期
            destroy: safeImport("DebugShape", "destroy"),
            destroyAll: safeImport("DebugShape", "destroyAll"),
        };
        
        hasAPI = DebugShape.createText !== null && DebugShape.draw !== null;
        
        // ==================== DebugShapeNative 命名空间 ====================
        DebugShapeNative = {
            createFilledQuad: safeImport("DebugShapeNative", "createFilledQuad"),
            createFilledQuadBatch: safeImport("DebugShapeNative", "createFilledQuadBatch"),
            setColor: safeImport("DebugShapeNative", "setColor"),
            setLocation: safeImport("DebugShapeNative", "setLocation"),
            draw: safeImport("DebugShapeNative", "draw"),
            drawToDimension: safeImport("DebugShapeNative", "drawToDimension"),
            drawBatch: safeImport("DebugShapeNative", "drawBatch"),
            remove: safeImport("DebugShapeNative", "remove"),
            destroy: safeImport("DebugShapeNative", "destroy"),
            destroyBatch: safeImport("DebugShapeNative", "destroyBatch"),
            destroyAll: safeImport("DebugShapeNative", "destroyAll"),
        };
        
        hasNativeAPI = DebugShapeNative.createFilledQuad !== null;
        
        // ==================== FloatingText 命名空间 ====================
        FloatingText = {
            // 创建/销毁
            create: safeImport("FloatingText", "create"),
            destroy: safeImport("FloatingText", "destroy"),
            destroyAll: safeImport("FloatingText", "destroyAll"),
            
            // 行操作
            addLine: safeImport("FloatingText", "addLine"),
            setLineText: safeImport("FloatingText", "setLineText"),
            setLineScale: safeImport("FloatingText", "setLineScale"),
            removeLine: safeImport("FloatingText", "removeLine"),
            clearLines: safeImport("FloatingText", "clearLines"),
            getLineCount: safeImport("FloatingText", "getLineCount"),
            
            // 颜色
            setColor: safeImport("FloatingText", "setColor"),
            setLineColor: safeImport("FloatingText", "setLineColor"),
            setLineGradient: safeImport("FloatingText", "setLineGradient"),
            setLineRainbow: safeImport("FloatingText", "setLineRainbow"),
            
            // 动画
            setLineScroll: safeImport("FloatingText", "setLineScroll"),
            setVerticalAnimation: safeImport("FloatingText", "setVerticalAnimation"),
            setLineSpacing: safeImport("FloatingText", "setLineSpacing"),
            setLocation: safeImport("FloatingText", "setLocation"),
            setFollowPlayer: safeImport("FloatingText", "setFollowPlayer"),
            clearFollowPlayer: safeImport("FloatingText", "clearFollowPlayer"),
            tick: safeImport("FloatingText", "tick"),
            
            // 显示
            draw: safeImport("FloatingText", "draw"),
            drawToDimension: safeImport("FloatingText", "drawToDimension"),
            drawToPlayer: safeImport("FloatingText", "drawToPlayer"),
            remove: safeImport("FloatingText", "remove"),
            refresh: safeImport("FloatingText", "refresh"),
        };
        
        hasFloatingTextAPI = FloatingText.create !== null;
        
        // ==================== GradientLine 命名空间 ====================
        GradientLine = {
            create: safeImport("GradientLine", "create"),
            setGradient: safeImport("GradientLine", "setGradient"),
            setRainbow: safeImport("GradientLine", "setRainbow"),
            setColor: safeImport("GradientLine", "setColor"),
            setEndpoints: safeImport("GradientLine", "setEndpoints"),
            draw: safeImport("GradientLine", "draw"),
            drawToDimension: safeImport("GradientLine", "drawToDimension"),
            remove: safeImport("GradientLine", "remove"),
            destroy: safeImport("GradientLine", "destroy"),
            destroyAll: safeImport("GradientLine", "destroyAll"),
            tick: safeImport("GradientLine", "tick"),
        };
        
        hasGradientLineAPI = GradientLine.create !== null;
        
        if (hasAPI) {
            stopAPICheck();
            fireReadyCallbacks();
        }
        
        return hasAPI;
    } catch (e) {
        hasAPI = false;
        DebugShape = null;
        return false;
    }
}

function fireReadyCallbacks() {
    if (_readyCallbacksFired) return;
    _readyCallbacksFired = true;
    
    for (const callback of _onReadyCallbacks) {
        try {
            callback();
        } catch (e) {}
    }
}

function startAPICheck() {
    if (_apiCheckTimer || hasAPI) return;
    
    _apiCheckAttempts = 0;
    _apiCheckTimer = setInterval(() => {
        _apiCheckAttempts++;
        
        if (initializeAPI()) {
            return;
        }
        
        if (_apiCheckAttempts >= API_CHECK_MAX_ATTEMPTS) {
            stopAPICheck();
        }
    }, API_CHECK_INTERVAL);
}

function stopAPICheck() {
    if (_apiCheckTimer) {
        clearInterval(_apiCheckTimer);
        _apiCheckTimer = null;
    }
}

function onReady(callback) {
    if (typeof callback !== 'function') return;
    
    if (hasAPI) {
        try {
            callback();
        } catch (e) {}
    } else {
        _onReadyCallbacks.push(callback);
        startAPICheck();
    }
}

function waitForReady(timeout = 30000) {
    return new Promise((resolve) => {
        if (hasAPI) {
            resolve(true);
            return;
        }
        
        let resolved = false;
        const timeoutId = setTimeout(() => {
            if (!resolved) {
                resolved = true;
                resolve(false);
            }
        }, timeout);
        
        onReady(() => {
            if (!resolved) {
                resolved = true;
                clearTimeout(timeoutId);
                resolve(true);
            }
        });
    });
}

// 尝试立即初始化
initializeAPI();

if (!hasAPI) {
    try {
        const data = loadStorageData();
        if (data.floatingTexts && data.floatingTexts.length > 0) {
            startAPICheck();
        }
    } catch (e) {}
}

// ==================== 形状类型枚举 ====================
const ShapeType = {
    Line: 0,
    Box: 1,
    Sphere: 2,
    Circle: 3,
    Text: 4,
    Arrow: 5,
    FilledQuad: 6
};

// ==================== 平面类型枚举 ====================
const PlaneType = {
    XY: 0,
    XZ: 1,
    YZ: 2
};

// ==================== 滚动方向枚举 ====================
const ScrollDirection = {
    None: 0,
    Left: 1,
    Right: 2
};

// ==================== 垂直动画类型枚举 ====================
const VerticalAnimationType = {
    None: 0,
    Bounce: 1,
    Scroll: 2
};

// ==================== 持久化存储功能 ====================
function ensureDataDir() {
    if (!fs.existsSync(DATA_DIR)) {
        fs.mkdirSync(DATA_DIR, { recursive: true });
    }
}

function getDataFilePath() {
    return path.join(DATA_DIR, DATA_FILE);
}

function loadStorageData() {
    if (_storageCache !== null) {
        return _storageCache;
    }
    
    const filePath = getDataFilePath();
    
    try {
        if (fs.existsSync(filePath)) {
            const content = fs.readFileSync(filePath, 'utf8');
            _storageCache = JSON.parse(content);
            if (!_storageCache.floatingTexts) {
                _storageCache.floatingTexts = [];
            }
        } else {
            _storageCache = { floatingTexts: [] };
        }
    } catch (e) {
        _storageCache = { floatingTexts: [] };
    }
    
    return _storageCache;
}

function saveStorageData() {
    if (_storageCache === null) return false;
    
    try {
        ensureDataDir();
        const filePath = getDataFilePath();
        fs.writeFileSync(filePath, JSON.stringify(_storageCache, null, 2), 'utf8');
        return true;
    } catch (e) {
        return false;
    }
}

function generateStorageId(category) {
    return `${category}_${Date.now()}_${Math.random().toString(36).substring(2, 8)}`;
}


// ==================== 基础形状类 ====================
class BaseShape {
    constructor(shapeId, type) {
        this.shapeId = shapeId;
        this.type = type;
        this.isDestroyed = false;
    }
    
    isValid() {
        return !this.isDestroyed && this.shapeId >= 0 && hasAPI;
    }
    
    setLocation(x, y, z) {
        if (!this.isValid()) return false;
        return DebugShape.setLocation(this.shapeId, x, y, z);
    }
    
    getLocation() {
        if (!this.isValid()) return null;
        const loc = DebugShape.getLocation(this.shapeId);
        return loc && loc.length === 3 ? { x: loc[0], y: loc[1], z: loc[2] } : null;
    }
    
    setColor(r, g, b, a = 1.0) {
        if (!this.isValid()) return false;
        return DebugShape.setColor(this.shapeId, r, g, b, a);
    }
    
    getColor() {
        if (!this.isValid()) return null;
        const c = DebugShape.getColor(this.shapeId);
        return c && c.length === 4 ? { r: c[0], g: c[1], b: c[2], a: c[3] } : null;
    }
    
    setScale(scale) {
        if (!this.isValid()) return false;
        return DebugShape.setScale(this.shapeId, scale);
    }
    
    setDuration(seconds) {
        if (!this.isValid()) return false;
        if (!DebugShape.setDuration) return false;
        return DebugShape.setDuration(this.shapeId, seconds);
    }
    
    setRotation(pitch, yaw, roll) {
        if (!this.isValid()) return false;
        if (!DebugShape.setRotation) return false;
        return DebugShape.setRotation(this.shapeId, pitch, yaw, roll);
    }
    
    clearRotation() {
        if (!this.isValid()) return false;
        if (!DebugShape.clearRotation) return false;
        return DebugShape.clearRotation(this.shapeId);
    }
    
    getRotation() {
        if (!this.isValid()) return null;
        if (!DebugShape.getRotation) return null;
        const rot = DebugShape.getRotation(this.shapeId);
        if (!rot || rot.length !== 3) return null;
        return { pitch: rot[0], yaw: rot[1], roll: rot[2] };
    }
    
    draw() {
        if (!this.isValid()) return false;
        return DebugShape.draw(this.shapeId);
    }
    
    drawToPlayer(playerName) {
        if (!this.isValid()) return false;
        return DebugShape.drawToPlayer(this.shapeId, playerName);
    }
    
    drawTo(player) {
        if (!this.isValid() || !player) return false;
        return DebugShape.drawToPlayer(this.shapeId, player.name);
    }
    
    drawToDimension(dimId) {
        if (!this.isValid()) return false;
        return DebugShape.drawToDimension(this.shapeId, dimId);
    }
    
    remove() {
        if (!this.isValid()) return false;
        return DebugShape.remove(this.shapeId);
    }
    
    removeToPlayer(playerName) {
        if (!this.isValid()) return false;
        return DebugShape.removeToPlayer(this.shapeId, playerName);
    }
    
    removeFrom(player) {
        if (!this.isValid() || !player) return false;
        return DebugShape.removeToPlayer(this.shapeId, player.name);
    }
    
    removeToDimension(dimId) {
        if (!this.isValid()) return false;
        return DebugShape.removeToDimension(this.shapeId, dimId);
    }
    
    update() {
        if (!this.isValid()) return false;
        return DebugShape.update(this.shapeId);
    }
    
    updateToPlayer(playerName) {
        if (!this.isValid()) return false;
        return DebugShape.updateToPlayer(this.shapeId, playerName);
    }
    
    updateTo(player) {
        if (!this.isValid() || !player) return false;
        return DebugShape.updateToPlayer(this.shapeId, player.name);
    }
    
    updateToDimension(dimId) {
        if (!this.isValid()) return false;
        return DebugShape.updateToDimension(this.shapeId, dimId);
    }
    
    destroy() {
        if (this.isDestroyed) return true;
        this.isDestroyed = true;
        if (this.shapeId >= 0 && hasAPI && DebugShape && DebugShape.destroy) {
            try {
                return DebugShape.destroy(this.shapeId);
            } catch (e) {
                return true;
            }
        }
        return true;
    }
    
    getId() {
        return this.shapeId;
    }
}

// ==================== 文本形状类 ====================
class TextShape extends BaseShape {
    constructor(shapeId) {
        super(shapeId, ShapeType.Text);
    }
    
    setText(text) {
        if (!this.isValid()) return false;
        return DebugShape.setText(this.shapeId, text);
    }
    
    getText() {
        if (!this.isValid()) return "";
        return DebugShape.getText(this.shapeId) || "";
    }
}

// ==================== 多行悬浮字类 ====================
class MultiLineFloatingText {
    constructor(floatingTextId) {
        this.ftId = floatingTextId;
        this.isDestroyed = false;
    }
    
    isValid() {
        return !this.isDestroyed && this.ftId >= 0 && hasFloatingTextAPI;
    }
    
    // 行操作
    addLine(text) {
        if (!this.isValid()) return false;
        return FloatingText.addLine(this.ftId, text);
    }
    
    setLineText(lineIndex, text) {
        if (!this.isValid()) return false;
        return FloatingText.setLineText(this.ftId, lineIndex, text);
    }
    
    setLineScale(lineIndex, scale) {
        if (!this.isValid()) return false;
        return FloatingText.setLineScale(this.ftId, lineIndex, scale);
    }
    
    removeLine(lineIndex) {
        if (!this.isValid()) return false;
        return FloatingText.removeLine(this.ftId, lineIndex);
    }
    
    clearLines() {
        if (!this.isValid()) return false;
        return FloatingText.clearLines(this.ftId);
    }
    
    getLineCount() {
        if (!this.isValid()) return 0;
        return FloatingText.getLineCount(this.ftId);
    }
    
    // 颜色
    setColor(r, g, b, a = 1.0) {
        if (!this.isValid()) return false;
        return FloatingText.setColor(this.ftId, r, g, b, a);
    }
    
    setLineColor(lineIndex, r, g, b, a = 1.0) {
        if (!this.isValid()) return false;
        return FloatingText.setLineColor(this.ftId, lineIndex, r, g, b, a);
    }
    
    setLineGradient(lineIndex, r1, g1, b1, r2, g2, b2) {
        if (!this.isValid()) return false;
        return FloatingText.setLineGradient(this.ftId, lineIndex, r1, g1, b1, r2, g2, b2);
    }
    
    setLineRainbow(lineIndex, speed = 1.0) {
        if (!this.isValid()) return false;
        return FloatingText.setLineRainbow(this.ftId, lineIndex, speed);
    }
    
    // 动画
    setLineScroll(lineIndex, direction, speed = 1.0) {
        if (!this.isValid()) return false;
        return FloatingText.setLineScroll(this.ftId, lineIndex, direction, speed);
    }
    
    setVerticalAnimation(type, speed = 1.0, range = 0.5) {
        if (!this.isValid()) return false;
        return FloatingText.setVerticalAnimation(this.ftId, type, speed, range);
    }
    
    setLineSpacing(spacing) {
        if (!this.isValid()) return false;
        return FloatingText.setLineSpacing(this.ftId, spacing);
    }
    
    setLocation(x, y, z) {
        if (!this.isValid()) return false;
        return FloatingText.setLocation(this.ftId, x, y, z);
    }
    
    setFollowPlayer(playerName, offsetY = 2.0) {
        if (!this.isValid()) return false;
        return FloatingText.setFollowPlayer(this.ftId, playerName, offsetY);
    }
    
    clearFollowPlayer() {
        if (!this.isValid()) return false;
        return FloatingText.clearFollowPlayer(this.ftId);
    }
    
    // 显示
    draw() {
        if (!this.isValid()) return false;
        return FloatingText.draw(this.ftId);
    }
    
    drawToDimension(dimId) {
        if (!this.isValid()) return false;
        return FloatingText.drawToDimension(this.ftId, dimId);
    }
    
    drawToPlayer(playerName) {
        if (!this.isValid()) return false;
        return FloatingText.drawToPlayer(this.ftId, playerName);
    }
    
    drawTo(player) {
        if (!this.isValid() || !player) return false;
        return FloatingText.drawToPlayer(this.ftId, player.name);
    }
    
    remove() {
        if (!this.isValid()) return false;
        return FloatingText.remove(this.ftId);
    }
    
    refresh() {
        if (!this.isValid()) return false;
        return FloatingText.refresh(this.ftId);
    }
    
    destroy() {
        if (this.isDestroyed) return true;
        this.isDestroyed = true;
        if (this.ftId >= 0 && hasFloatingTextAPI && FloatingText && FloatingText.destroy) {
            try {
                return FloatingText.destroy(this.ftId);
            } catch (e) {
                return true;
            }
        }
        return true;
    }
    
    getId() {
        return this.ftId;
    }
}

// ==================== 渐变线段类 ====================
class GradientLineShape {
    constructor(lineId) {
        this.lineId = lineId;
        this.isDestroyed = false;
    }
    
    isValid() {
        return !this.isDestroyed && this.lineId >= 0 && hasGradientLineAPI;
    }
    
    setGradient(r1, g1, b1, r2, g2, b2) {
        if (!this.isValid()) return false;
        return GradientLine.setGradient(this.lineId, r1, g1, b1, r2, g2, b2);
    }
    
    setRainbow(speed = 1.0) {
        if (!this.isValid()) return false;
        return GradientLine.setRainbow(this.lineId, speed);
    }
    
    setColor(r, g, b, a = 1.0) {
        if (!this.isValid()) return false;
        return GradientLine.setColor(this.lineId, r, g, b, a);
    }
    
    setEndpoints(x1, y1, z1, x2, y2, z2) {
        if (!this.isValid()) return false;
        return GradientLine.setEndpoints(this.lineId, x1, y1, z1, x2, y2, z2);
    }
    
    draw() {
        if (!this.isValid()) return false;
        return GradientLine.draw(this.lineId);
    }
    
    drawToDimension(dimId) {
        if (!this.isValid()) return false;
        return GradientLine.drawToDimension(this.lineId, dimId);
    }
    
    remove() {
        if (!this.isValid()) return false;
        return GradientLine.remove(this.lineId);
    }
    
    destroy() {
        if (this.isDestroyed) return true;
        this.isDestroyed = true;
        if (this.lineId >= 0 && hasGradientLineAPI && GradientLine && GradientLine.destroy) {
            try {
                return GradientLine.destroy(this.lineId);
            } catch (e) {
                return true;
            }
        }
        return true;
    }
    
    getId() {
        return this.lineId;
    }
}


// ==================== 形状工厂函数 ====================
const ShapeFactory = {
    createText(x, y, z, text) {
        if (!hasAPI) return null;
        const id = DebugShape.createText(x, y, z, text);
        return id >= 0 ? new TextShape(id) : null;
    },
    
    createLine(x1, y1, z1, x2, y2, z2) {
        if (!hasAPI) return null;
        const id = DebugShape.createLine(x1, y1, z1, x2, y2, z2);
        return id >= 0 ? new BaseShape(id, ShapeType.Line) : null;
    },
    
    createBox(x1, y1, z1, x2, y2, z2) {
        if (!hasAPI) return null;
        const id = DebugShape.createBox(x1, y1, z1, x2, y2, z2);
        return id >= 0 ? new BaseShape(id, ShapeType.Box) : null;
    },
    
    createCircle(x, y, z, radius = 1.0) {
        if (!hasAPI) return null;
        const id = DebugShape.createCircle(x, y, z, radius);
        return id >= 0 ? new BaseShape(id, ShapeType.Circle) : null;
    },
    
    createSphere(x, y, z, radius = 1.0) {
        if (!hasAPI) return null;
        const id = DebugShape.createSphere(x, y, z, radius);
        return id >= 0 ? new BaseShape(id, ShapeType.Sphere) : null;
    },
    
    createArrow(x1, y1, z1, x2, y2, z2) {
        if (!hasAPI) return null;
        const id = DebugShape.createArrow(x1, y1, z1, x2, y2, z2);
        return id >= 0 ? new BaseShape(id, ShapeType.Arrow) : null;
    },
    
    /**
     * 创建填充面
     * @param {number} x - 中心X坐标
     * @param {number} y - 中心Y坐标
     * @param {number} z - 中心Z坐标
     * @param {number} width - 宽度
     * @param {number} height - 高度
     * @param {number} plane - 平面类型 (PlaneType.XY/XZ/YZ)
     */
    createFilledQuad(x, y, z, width, height, plane = PlaneType.XY) {
        if (!hasAPI || !DebugShape.createFilledQuad) return null;
        const id = DebugShape.createFilledQuad(x, y, z, width, height, plane);
        return id >= 0 ? new BaseShape(id, ShapeType.FilledQuad) : null;
    },
    
    /**
     * 批量创建填充面
     * @param {number[]} positions - 位置数组 [x1,y1,z1, x2,y2,z2, ...]
     * @param {number[]} colors - 颜色数组 [r1,g1,b1,a1, r2,g2,b2,a2, ...]
     * @param {number} width - 宽度
     * @param {number} height - 高度
     * @param {number} plane - 平面类型
     * @returns {BaseShape[]}
     */
    createFilledQuadBatch(positions, colors, width, height, plane = PlaneType.XY) {
        if (!hasAPI || !DebugShape.createFilledQuadBatch) return [];
        const ids = DebugShape.createFilledQuadBatch(positions, colors, width, height, plane);
        return ids.map(id => id >= 0 ? new BaseShape(id, ShapeType.FilledQuad) : null).filter(s => s !== null);
    },
    
    /**
     * 创建多行悬浮字
     * @param {number} x - X坐标
     * @param {number} y - Y坐标
     * @param {number} z - Z坐标
     * @returns {MultiLineFloatingText|null}
     */
    createFloatingText(x, y, z) {
        if (!hasFloatingTextAPI) return null;
        const id = FloatingText.create(x, y, z);
        return id >= 0 ? new MultiLineFloatingText(id) : null;
    },
    
    /**
     * 创建渐变线段
     * @param {number} x1 - 起点X
     * @param {number} y1 - 起点Y
     * @param {number} z1 - 起点Z
     * @param {number} x2 - 终点X
     * @param {number} y2 - 终点Y
     * @param {number} z2 - 终点Z
     * @param {number} segments - 分段数（越多越平滑）
     * @returns {GradientLineShape|null}
     */
    createGradientLine(x1, y1, z1, x2, y2, z2, segments = 10) {
        if (!hasGradientLineAPI) return null;
        const id = GradientLine.create(x1, y1, z1, x2, y2, z2, segments);
        return id >= 0 ? new GradientLineShape(id) : null;
    }
};

// ==================== 原生渲染器工厂 ====================
const NativeFactory = {
    /**
     * 创建填充面（原生渲染器）
     */
    createFilledQuad(x, y, z, width, height, plane = PlaneType.XY) {
        if (!hasNativeAPI) return null;
        const id = DebugShapeNative.createFilledQuad(x, y, z, width, height, plane);
        return id >= 0 ? { id, type: 'native_quad' } : null;
    },
    
    /**
     * 批量创建填充面（原生渲染器）
     */
    createFilledQuadBatch(positions, colors, width, height, plane = PlaneType.XY) {
        if (!hasNativeAPI) return [];
        return DebugShapeNative.createFilledQuadBatch(positions, colors, width, height, plane);
    },
    
    setColor(id, r, g, b, a = 1.0) {
        if (!hasNativeAPI) return false;
        return DebugShapeNative.setColor(id, r, g, b, a);
    },
    
    setLocation(id, x, y, z) {
        if (!hasNativeAPI) return false;
        return DebugShapeNative.setLocation(id, x, y, z);
    },
    
    draw(id) {
        if (!hasNativeAPI) return false;
        return DebugShapeNative.draw(id);
    },
    
    drawToDimension(id, dimId) {
        if (!hasNativeAPI) return false;
        return DebugShapeNative.drawToDimension(id, dimId);
    },
    
    drawBatch(ids) {
        if (!hasNativeAPI) return false;
        return DebugShapeNative.drawBatch(ids);
    },
    
    remove(id) {
        if (!hasNativeAPI) return false;
        return DebugShapeNative.remove(id);
    },
    
    destroy(id) {
        if (!hasNativeAPI) return false;
        return DebugShapeNative.destroy(id);
    },
    
    destroyBatch(ids) {
        if (!hasNativeAPI) return false;
        return DebugShapeNative.destroyBatch(ids);
    },
    
    destroyAll() {
        if (!hasNativeAPI) return;
        DebugShapeNative.destroyAll();
    }
};

// ==================== FloatingText 动画 Tick ====================
/**
 * 更新 FloatingText 动画
 * 应在游戏 tick 中调用此函数
 * @param {number} deltaTime - 时间增量（秒）
 */
function tickFloatingText(deltaTime) {
    if (!hasFloatingTextAPI || !FloatingText.tick) return;
    FloatingText.tick(deltaTime);
}

/**
 * 更新 GradientLine 动画（彩虹效果）
 * 应在游戏 tick 中调用此函数
 * @param {number} deltaTime - 时间增量（秒）
 */
function tickGradientLine(deltaTime) {
    if (!hasGradientLineAPI || !GradientLine.tick) return;
    GradientLine.tick(deltaTime);
}

/**
 * 更新所有动画（FloatingText + GradientLine）
 * @param {number} deltaTime - 时间增量（秒）
 */
function tickAll(deltaTime) {
    tickFloatingText(deltaTime);
    tickGradientLine(deltaTime);
}

// ==================== 持久化存储管理 ====================
const PersistentStorage = {
    loadData: loadStorageData,
    saveData: saveStorageData,
    generateId: generateStorageId,
    
    restoreAll() {
        if (!hasAPI) return 0;
        
        const data = loadStorageData();
        let count = 0;
        
        for (const ft of data.floatingTexts) {
            try {
                if (_persistedShapes.has(ft.id)) continue;
                
                const shapeId = DebugShape.createText(ft.x, ft.y, ft.z, ft.text);
                if (shapeId < 0) continue;
                
                const shape = new TextShape(shapeId);
                
                if (ft.color) {
                    shape.setColor(ft.color.r || 1, ft.color.g || 1, ft.color.b || 1, ft.color.a || 1);
                }
                
                if (ft.rotation) {
                    shape.setRotation(ft.rotation.pitch || 0, ft.rotation.yaw || 0, ft.rotation.roll || 0);
                }
                
                shape.draw();
                
                shape.persistId = ft.id;
                shape.category = ft.category;
                shape.pos = { x: ft.x, y: ft.y, z: ft.z };
                shape.dimId = ft.dimId;
                shape.metadata = ft.metadata || {};
                shape.rotation = ft.rotation || null;
                
                _persistedShapes.set(ft.id, shape);
                count++;
            } catch (e) {}
        }
        
        return count;
    },
    
    restoreByCategory(category) {
        if (!hasAPI) return 0;
        
        const data = loadStorageData();
        const categoryTexts = data.floatingTexts.filter(ft => ft.category === category);
        let count = 0;
        
        for (const ft of categoryTexts) {
            try {
                if (_persistedShapes.has(ft.id)) continue;
                
                const shapeId = DebugShape.createText(ft.x, ft.y, ft.z, ft.text);
                if (shapeId < 0) continue;
                
                const shape = new TextShape(shapeId);
                if (ft.color) {
                    shape.setColor(ft.color.r || 1, ft.color.g || 1, ft.color.b || 1, ft.color.a || 1);
                }
                
                if (ft.rotation) {
                    shape.setRotation(ft.rotation.pitch || 0, ft.rotation.yaw || 0, ft.rotation.roll || 0);
                }
                
                shape.draw();
                
                shape.persistId = ft.id;
                shape.category = ft.category;
                shape.pos = { x: ft.x, y: ft.y, z: ft.z };
                shape.dimId = ft.dimId;
                shape.metadata = ft.metadata || {};
                shape.rotation = ft.rotation || null;
                
                _persistedShapes.set(ft.id, shape);
                count++;
            } catch (e) {}
        }
        
        return count;
    },
    
    getShape(id) {
        return _persistedShapes.get(id) || null;
    },
    
    getByCategory(category) {
        const data = loadStorageData();
        return data.floatingTexts.filter(ft => ft.category === category);
    },
    
    getAll() {
        const data = loadStorageData();
        return [...data.floatingTexts];
    },
    
    remove(id) {
        const shape = _persistedShapes.get(id);
        if (shape) {
            shape.destroy();
        }
        
        const data = loadStorageData();
        const index = data.floatingTexts.findIndex(ft => ft.id === id);
        if (index !== -1) {
            data.floatingTexts.splice(index, 1);
            saveStorageData();
        }
        
        _persistedShapes.delete(id);
        return true;
    },
    
    removeByCategory(category) {
        const data = loadStorageData();
        const toRemove = data.floatingTexts.filter(ft => ft.category === category);
        
        for (const ft of toRemove) {
            const shape = _persistedShapes.get(ft.id);
            if (shape && typeof shape.destroy === 'function') {
                try { shape.destroy(); } catch (e) {}
            }
            _persistedShapes.delete(ft.id);
        }
        
        data.floatingTexts = data.floatingTexts.filter(ft => ft.category !== category);
        saveStorageData();
        
        return toRemove.length;
    },
    
    updateText(id, newText) {
        const shape = _persistedShapes.get(id);
        if (shape && shape.isValid()) {
            shape.setText(newText);
        }
        
        const data = loadStorageData();
        const ft = data.floatingTexts.find(f => f.id === id);
        if (ft) {
            ft.text = newText;
            ft.updatedAt = Date.now();
            saveStorageData();
            return true;
        }
        return false;
    },
    
    sendToPlayer(player, category = null) {
        const dimId = player.pos.dimid;
        
        for (const [id, shape] of _persistedShapes) {
            if (!shape.isValid()) continue;
            
            if (category) {
                const data = loadStorageData();
                const ft = data.floatingTexts.find(f => f.id === id);
                if (!ft || ft.category !== category) continue;
                if (ft.dimId !== dimId) continue;
            }
            
            try {
                shape.drawTo(player);
            } catch (e) {}
        }
    },
    
    clearDisplays() {
        for (const [, shape] of _persistedShapes) {
            if (shape && typeof shape.destroy === 'function') {
                try { shape.destroy(); } catch (e) {}
            }
        }
        _persistedShapes.clear();
    },
    
    clearAll() {
        for (const [, shape] of _persistedShapes) {
            if (shape && typeof shape.destroy === 'function') {
                try { shape.destroy(); } catch (e) {}
            }
        }
        _persistedShapes.clear();
        _storageCache = { floatingTexts: [] };
        saveStorageData();
    }
};


// ==================== 全局悬浮字管理器注册系统 ====================
const _registeredManagers = new Map();

function registerManager(category, manager, refreshCallback) {
    if (!category || !manager) return false;
    _registeredManagers.set(category, { manager, refreshCallback });
    return true;
}

function unregisterManager(category) {
    _registeredManagers.delete(category);
}

function getRegisteredManager(category) {
    const entry = _registeredManagers.get(category);
    return entry ? entry.manager : null;
}

function getRegisteredCategories() {
    return Array.from(_registeredManagers.keys());
}

function reloadRefreshAll() {
    if (!hasAPI) return;
    
    logger.info('[DebugShapeAPI] 开始reload刷新所有悬浮字...');
    
    for (const [category, entry] of _registeredManagers) {
        try {
            if (entry.manager && typeof entry.manager.destroyDisplays === 'function') {
                entry.manager.destroyDisplays();
                logger.info(`[DebugShapeAPI] 已销毁 ${category} 的悬浮字显示`);
            }
        } catch (e) {
            logger.warn(`[DebugShapeAPI] 销毁 ${category} 显示时出错: ${e.message}`);
        }
    }
    
    setTimeout(() => {
        for (const [category, entry] of _registeredManagers) {
            try {
                if (typeof entry.refreshCallback === 'function') {
                    entry.refreshCallback();
                    logger.info(`[DebugShapeAPI] 已刷新 ${category} 的悬浮字`);
                }
            } catch (e) {
                logger.warn(`[DebugShapeAPI] 刷新 ${category} 时出错: ${e.message}`);
            }
        }
        
        setTimeout(() => {
            const players = mc.getOnlinePlayers();
            for (const player of players) {
                for (const [, entry] of _registeredManagers) {
                    try {
                        if (entry.manager && typeof entry.manager.sendToPlayer === 'function') {
                            entry.manager.sendToPlayer(player);
                        }
                    } catch (e) {}
                }
            }
            logger.info('[DebugShapeAPI] reload刷新完成');
        }, 500);
    }, 200);
}

function reloadRefreshCategory(category) {
    const entry = _registeredManagers.get(category);
    if (!entry) {
        logger.warn(`[DebugShapeAPI] 未找到分类: ${category}`);
        return;
    }
    
    try {
        if (entry.manager && typeof entry.manager.destroyDisplays === 'function') {
            entry.manager.destroyDisplays();
        }
        
        setTimeout(() => {
            if (typeof entry.refreshCallback === 'function') {
                entry.refreshCallback();
            }
            
            setTimeout(() => {
                const players = mc.getOnlinePlayers();
                for (const player of players) {
                    if (entry.manager && typeof entry.manager.sendToPlayer === 'function') {
                        entry.manager.sendToPlayer(player);
                    }
                }
            }, 300);
        }, 100);
    } catch (e) {
        logger.warn(`[DebugShapeAPI] 刷新 ${category} 时出错: ${e.message}`);
    }
}

function sendAllToPlayer(player) {
    for (const [, entry] of _registeredManagers) {
        try {
            if (entry.manager && typeof entry.manager.sendToPlayer === 'function') {
                entry.manager.sendToPlayer(player);
            }
        } catch (e) {}
    }
}

function getFloatingTextStats() {
    const stats = {
        apiAvailable: hasAPI,
        nativeApiAvailable: hasNativeAPI,
        floatingTextApiAvailable: hasFloatingTextAPI,
        gradientLineApiAvailable: hasGradientLineAPI,
        categories: [],
        totalTexts: 0
    };
    
    for (const [category, entry] of _registeredManagers) {
        let count = 0;
        if (entry.manager && typeof entry.manager.getAllIds === 'function') {
            count = entry.manager.getAllIds().length;
        }
        stats.categories.push({ name: category, count });
        stats.totalTexts += count;
    }
    
    return stats;
}

// ==================== 颜色工具函数 ====================

/**
 * 解析十六进制颜色为 RGBA
 * @param {string} hex - 十六进制颜色 (#RGB, #RGBA, #RRGGBB, #RRGGBBAA)
 * @returns {{r: number, g: number, b: number, a: number}|null}
 */
function parseHexColor(hex) {
    if (!hex || typeof hex !== 'string') return null;
    
    // 移除 # 前缀
    hex = hex.replace(/^#/, '');
    
    let r, g, b, a = 1.0;
    
    if (hex.length === 3) {
        // #RGB
        r = parseInt(hex[0] + hex[0], 16) / 255;
        g = parseInt(hex[1] + hex[1], 16) / 255;
        b = parseInt(hex[2] + hex[2], 16) / 255;
    } else if (hex.length === 4) {
        // #RGBA
        r = parseInt(hex[0] + hex[0], 16) / 255;
        g = parseInt(hex[1] + hex[1], 16) / 255;
        b = parseInt(hex[2] + hex[2], 16) / 255;
        a = parseInt(hex[3] + hex[3], 16) / 255;
    } else if (hex.length === 6) {
        // #RRGGBB
        r = parseInt(hex.substring(0, 2), 16) / 255;
        g = parseInt(hex.substring(2, 4), 16) / 255;
        b = parseInt(hex.substring(4, 6), 16) / 255;
    } else if (hex.length === 8) {
        // #RRGGBBAA
        r = parseInt(hex.substring(0, 2), 16) / 255;
        g = parseInt(hex.substring(2, 4), 16) / 255;
        b = parseInt(hex.substring(4, 6), 16) / 255;
        a = parseInt(hex.substring(6, 8), 16) / 255;
    } else {
        return null;
    }
    
    if (isNaN(r) || isNaN(g) || isNaN(b) || isNaN(a)) return null;
    
    return { r, g, b, a };
}

/**
 * RGBA 转十六进制
 * @param {number} r - 红色 (0-1)
 * @param {number} g - 绿色 (0-1)
 * @param {number} b - 蓝色 (0-1)
 * @param {number} a - 透明度 (0-1)
 * @returns {string}
 */
function toHexColor(r, g, b, a = 1.0) {
    const toHex = (v) => Math.round(v * 255).toString(16).padStart(2, '0');
    if (a < 1.0) {
        return `#${toHex(r)}${toHex(g)}${toHex(b)}${toHex(a)}`;
    }
    return `#${toHex(r)}${toHex(g)}${toHex(b)}`;
}

/**
 * HSV 转 RGB
 * @param {number} h - 色相 (0-360)
 * @param {number} s - 饱和度 (0-1)
 * @param {number} v - 明度 (0-1)
 * @returns {{r: number, g: number, b: number}}
 */
function hsvToRgb(h, s, v) {
    const c = v * s;
    const x = c * (1 - Math.abs((h / 60) % 2 - 1));
    const m = v - c;
    
    let r, g, b;
    if (h < 60) { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    
    return { r: r + m, g: g + m, b: b + m };
}

/**
 * 颜色线性插值
 * @param {Object} c1 - 起始颜色 {r, g, b, a}
 * @param {Object} c2 - 结束颜色 {r, g, b, a}
 * @param {number} t - 插值因子 (0-1)
 * @returns {{r: number, g: number, b: number, a: number}}
 */
function lerpColor(c1, c2, t) {
    return {
        r: c1.r + (c2.r - c1.r) * t,
        g: c1.g + (c2.g - c1.g) * t,
        b: c1.b + (c2.b - c1.b) * t,
        a: (c1.a || 1) + ((c2.a || 1) - (c1.a || 1)) * t
    };
}

/**
 * 解析带十六进制颜色标记的文本
 * 格式: <#RRGGBB>文本</> 或 <#RGB>文本</>
 * @param {string} text - 带颜色标记的文本
 * @returns {Array<{text: string, color: Object|null}>} 解析后的片段
 */
function parseColoredText(text) {
    const segments = [];
    const regex = /<#([0-9A-Fa-f]{3,8})>(.*?)<\/>/g;
    let lastIndex = 0;
    let match;
    
    while ((match = regex.exec(text)) !== null) {
        // 添加标记前的普通文本
        if (match.index > lastIndex) {
            segments.push({
                text: text.substring(lastIndex, match.index),
                color: null
            });
        }
        
        // 添加带颜色的文本
        const color = parseHexColor(match[1]);
        segments.push({
            text: match[2],
            color: color
        });
        
        lastIndex = regex.lastIndex;
    }
    
    // 添加剩余的普通文本
    if (lastIndex < text.length) {
        segments.push({
            text: text.substring(lastIndex),
            color: null
        });
    }
    
    return segments;
}

// ==================== 工具函数 ====================
function isAvailable() {
    return hasAPI;
}

function isNativeAvailable() {
    return hasNativeAPI;
}

function isFloatingTextAvailable() {
    return hasFloatingTextAPI;
}

function isGradientLineAvailable() {
    return hasGradientLineAPI;
}

function getRawAPI() {
    return hasAPI ? DebugShape : null;
}

function getRawNativeAPI() {
    return hasNativeAPI ? DebugShapeNative : null;
}

function getRawFloatingTextAPI() {
    return hasFloatingTextAPI ? FloatingText : null;
}

function getRawGradientLineAPI() {
    return hasGradientLineAPI ? GradientLine : null;
}

function destroyAll() {
    if (hasAPI) {
        DebugShape.destroyAll();
    }
}

function destroyAllNative() {
    if (hasNativeAPI) {
        DebugShapeNative.destroyAll();
    }
}

function destroyAllFloatingText() {
    if (hasFloatingTextAPI) {
        FloatingText.destroyAll();
    }
}

function destroyAllGradientLine() {
    if (hasGradientLineAPI) {
        GradientLine.destroyAll();
    }
}

function findTextByLocation(x, y, z, radius = 0.5) {
    if (!hasAPI || !DebugShape.findTextByLocation) return [];
    try {
        return DebugShape.findTextByLocation(x, y, z, radius) || [];
    } catch (e) {
        return [];
    }
}

function findTextByLocationAndContent(x, y, z, text, radius = 0.5) {
    if (!hasAPI || !DebugShape.findTextByLocationAndContent) return -1;
    try {
        return DebugShape.findTextByLocationAndContent(x, y, z, radius, text);
    } catch (e) {
        return -1;
    }
}

function createTextWithCleanup(x, y, z, text, radius = 0.5) {
    if (!hasAPI) return null;
    
    if (DebugShape.findTextByLocationAndContent) {
        try {
            const existingId = DebugShape.findTextByLocationAndContent(x, y, z, radius, text);
            if (existingId >= 0 && DebugShape.destroy) {
                DebugShape.destroy(existingId);
            }
        } catch (e) {}
    }
    
    return ShapeFactory.createText(x, y, z, text);
}

function cleanupTextAtLocation(x, y, z, radius = 0.5) {
    if (!hasAPI || !DebugShape.findTextByLocation) return 0;
    
    try {
        const ids = DebugShape.findTextByLocation(x, y, z, radius) || [];
        let count = 0;
        
        for (const id of ids) {
            if (DebugShape.destroy && DebugShape.destroy(id)) {
                count++;
            }
        }
        
        return count;
    } catch (e) {
        return 0;
    }
}

function getAllShapeIds() {
    if (!hasAPI || !DebugShape.getAllShapeIds) return [];
    try {
        return DebugShape.getAllShapeIds() || [];
    } catch (e) {
        return [];
    }
}

function shapeExists(id) {
    if (!hasAPI || !DebugShape.exists) return false;
    try {
        return DebugShape.exists(id);
    } catch (e) {
        return false;
    }
}

// ==================== MC 颜色代码工具 ====================
/**
 * MC 16色调色板
 * 注意：这里的颜色值经过调整，使 hexToMcColor 能更好地匹配常见颜色
 * 例如 #FF0000 应该匹配到 §c (红色) 而不是 §4 (深红)
 */
const MC_COLORS = {
    '0': { r: 0, g: 0, b: 0 },         // 黑色
    '1': { r: 0, g: 0, b: 170 },       // 深蓝
    '2': { r: 0, g: 170, b: 0 },       // 深绿
    '3': { r: 0, g: 170, b: 170 },     // 深青
    '4': { r: 128, g: 0, b: 0 },       // 深红 (调整为更暗的红色)
    '5': { r: 170, g: 0, b: 170 },     // 紫色
    '6': { r: 255, g: 170, b: 0 },     // 金色
    '7': { r: 170, g: 170, b: 170 },   // 灰色
    '8': { r: 85, g: 85, b: 85 },      // 深灰
    '9': { r: 85, g: 85, b: 255 },     // 蓝色
    'a': { r: 85, g: 255, b: 85 },     // 绿色
    'b': { r: 85, g: 255, b: 255 },    // 青色
    'c': { r: 255, g: 0, b: 0 },       // 红色 (调整为纯红色，匹配 #FF0000)
    'd': { r: 255, g: 85, b: 255 },    // 粉色
    'e': { r: 255, g: 255, b: 85 },    // 黄色
    'f': { r: 255, g: 255, b: 255 }    // 白色
};

/**
 * 将十六进制颜色转换为最接近的 MC 颜色代码
 * @param {string} hex - 十六进制颜色 (#RGB 或 #RRGGBB)
 * @returns {string} MC 颜色代码 (如 '§c')
 */
function hexToMcColor(hex) {
    const color = parseHexColor(hex);
    if (!color) return '§f';
    
    const r = Math.round(color.r * 255);
    const g = Math.round(color.g * 255);
    const b = Math.round(color.b * 255);
    
    let bestCode = 'f';
    let bestDist = Infinity;
    
    for (const [code, mc] of Object.entries(MC_COLORS)) {
        const dist = (r - mc.r) ** 2 + (g - mc.g) ** 2 + (b - mc.b) ** 2;
        if (dist < bestDist) {
            bestDist = dist;
            bestCode = code;
        }
    }
    
    return '§' + bestCode;
}

/**
 * 将带十六进制颜色标记的文本转换为 MC 颜色代码格式
 * 输入格式: <#RRGGBB>文本</> 或 <#RGB>文本</>
 * 输出格式: §x文本
 * @param {string} text - 带颜色标记的文本
 * @returns {string} MC 颜色代码格式的文本
 */
function convertHexToMcColors(text) {
    return text.replace(/<#([0-9A-Fa-f]{3,6})>(.*?)<\/>/g, (match, hex, content) => {
        return hexToMcColor('#' + hex) + content;
    });
}

// ==================== 简单悬浮字管理器（单文本框，不闪烁）====================
/**
 * SimpleTextManager - 简单悬浮字管理器
 * 使用单个文本框，不会闪烁
 * 支持 MC 颜色代码 (§x) 实现多色文字
 */
class SimpleTextManager {
    constructor(category) {
        this.category = category;
        this.texts = new Map(); // id -> { shape, data }
        this._initialized = false;
    }
    
    /**
     * 初始化并恢复持久化的悬浮字
     */
    initialize() {
        if (this._initialized) return 0;
        this._initialized = true;
        
        if (!hasAPI) return 0;
        
        const data = loadStorageData();
        const categoryTexts = data.floatingTexts.filter(ft => ft.category === this.category);
        let count = 0;
        
        for (const ft of categoryTexts) {
            try {
                if (this.texts.has(ft.id)) continue;
                
                const shape = ShapeFactory.createText(ft.x, ft.y, ft.z, ft.text);
                if (!shape) continue;
                
                if (ft.color) {
                    shape.setColor(ft.color.r || 1, ft.color.g || 1, ft.color.b || 1, ft.color.a || 1);
                }
                
                // 设置长持续时间，避免消失
                shape.setDuration(3600);
                
                if (ft.rotation) {
                    shape.setRotation(ft.rotation.pitch || 0, ft.rotation.yaw || 0, ft.rotation.roll || 0);
                }
                
                shape.draw();
                
                this.texts.set(ft.id, {
                    shape,
                    data: ft
                });
                count++;
            } catch (e) {}
        }
        
        return count;
    }
    
    /**
     * 创建悬浮字
     * @param {string} id - 唯一标识符
     * @param {Object} pos - 位置 {x, y, z}
     * @param {number} dimId - 维度ID
     * @param {string} text - 文本内容（支持 §x 颜色代码）
     * @param {Object} options - 选项 { color, offsetY, rotation }
     */
    create(id, pos, dimId, text, options = {}) {
        if (!hasAPI) return null;
        
        // 先删除旧的
        this.remove(id);
        
        const x = pos.x;
        const y = pos.y + (options.offsetY || 0);
        const z = pos.z;
        
        const shape = ShapeFactory.createText(x, y, z, text);
        if (!shape) return null;
        
        const color = options.color || { r: 1, g: 1, b: 1, a: 1 };
        shape.setColor(color.r, color.g, color.b, color.a);
        
        // 设置长持续时间
        shape.setDuration(3600);
        
        if (options.rotation) {
            shape.setRotation(options.rotation.pitch || 0, options.rotation.yaw || 0, options.rotation.roll || 0);
        }
        
        shape.draw();
        
        const data = {
            id,
            category: this.category,
            x, y, z,
            dimId,
            text,
            color,
            rotation: options.rotation || null,
            createdAt: Date.now(),
            updatedAt: Date.now()
        };
        
        this.texts.set(id, { shape, data });
        
        // 保存到持久化存储
        const storage = loadStorageData();
        const existingIndex = storage.floatingTexts.findIndex(ft => ft.id === id);
        if (existingIndex !== -1) {
            storage.floatingTexts[existingIndex] = data;
        } else {
            storage.floatingTexts.push(data);
        }
        saveStorageData();
        
        return shape;
    }
    
    /**
     * 更新悬浮字文本
     */
    updateText(id, newText) {
        const entry = this.texts.get(id);
        if (!entry || !entry.shape.isValid()) return false;
        
        entry.shape.setText(newText);
        entry.data.text = newText;
        entry.data.updatedAt = Date.now();
        
        // 更新持久化存储
        const storage = loadStorageData();
        const ft = storage.floatingTexts.find(f => f.id === id);
        if (ft) {
            ft.text = newText;
            ft.updatedAt = Date.now();
            saveStorageData();
        }
        
        return true;
    }
    
    /**
     * 删除悬浮字
     */
    remove(id) {
        const entry = this.texts.get(id);
        if (entry) {
            try { entry.shape.destroy(); } catch (e) {}
            this.texts.delete(id);
        }
        
        // 从持久化存储中删除
        const storage = loadStorageData();
        const index = storage.floatingTexts.findIndex(ft => ft.id === id);
        if (index !== -1) {
            storage.floatingTexts.splice(index, 1);
            saveStorageData();
        }
        
        return true;
    }
    
    /**
     * 获取悬浮字
     */
    get(id) {
        const entry = this.texts.get(id);
        return entry ? entry.shape : null;
    }
    
    /**
     * 检查是否存在
     */
    has(id) {
        return this.texts.has(id);
    }
    
    /**
     * 获取所有ID
     */
    getAllIds() {
        return Array.from(this.texts.keys());
    }
    
    /**
     * 发送给指定玩家
     */
    sendToPlayer(player) {
        const dimId = player.pos.dimid;
        for (const [id, entry] of this.texts) {
            if (!entry.shape.isValid()) continue;
            if (entry.data.dimId !== undefined && entry.data.dimId !== dimId) continue;
            try {
                entry.shape.drawTo(player);
            } catch (e) {}
        }
    }
    
    /**
     * 刷新所有（重新发送 draw）
     */
    refreshAll() {
        for (const [, entry] of this.texts) {
            if (entry.shape.isValid()) {
                entry.shape.draw();
            }
        }
    }
    
    /**
     * 清除所有
     */
    clearAll() {
        for (const [id, entry] of this.texts) {
            try { entry.shape.destroy(); } catch (e) {}
        }
        this.texts.clear();
        
        const storage = loadStorageData();
        storage.floatingTexts = storage.floatingTexts.filter(ft => ft.category !== this.category);
        saveStorageData();
    }
    
    /**
     * 定期刷新 duration（防止超时消失）
     * 建议每 30 分钟调用一次
     */
    refreshDuration() {
        for (const [, entry] of this.texts) {
            if (entry.shape.isValid()) {
                entry.shape.setDuration(3600);
            }
        }
    }
}


// ==================== 悬浮字管理器（供插件使用）====================
class FloatingTextManager {
    constructor(category) {
        this.category = category;
        this.texts = new Map();
        this._initialized = false;
    }
    
    initialize() {
        if (this._initialized) return 0;
        this._initialized = true;
        
        if (!hasAPI) return 0;
        
        const data = loadStorageData();
        const categoryTexts = data.floatingTexts.filter(ft => ft.category === this.category);
        let count = 0;
        
        for (const ft of categoryTexts) {
            try {
                if (this.texts.has(ft.id)) continue;
                
                const shapeId = DebugShape.createText(ft.x, ft.y, ft.z, ft.text);
                if (shapeId < 0) continue;
                
                const shape = new TextShape(shapeId);
                if (ft.color) {
                    shape.setColor(ft.color.r || 1, ft.color.g || 1, ft.color.b || 1, ft.color.a || 1);
                }
                
                if (ft.rotation) {
                    shape.setRotation(ft.rotation.pitch || 0, ft.rotation.yaw || 0, ft.rotation.roll || 0);
                }
                
                shape.draw();
                
                shape.persistId = ft.id;
                shape.pos = { x: ft.x, y: ft.y, z: ft.z };
                shape.dimId = ft.dimId;
                shape.metadata = ft.metadata || {};
                shape.rotation = ft.rotation || null;
                
                this.texts.set(ft.id, shape);
                _persistedShapes.set(ft.id, shape);
                count++;
            } catch (e) {}
        }
        
        return count;
    }
    
    create(id, pos, dimId, text, options = {}) {
        if (!hasAPI) return null;
        
        this.remove(id);
        
        const x = pos.x;
        const y = pos.y + (options.offsetY || 0);
        const z = pos.z;
        
        const shapeId = DebugShape.createText(x, y, z, text);
        if (shapeId < 0) return null;
        
        const shape = new TextShape(shapeId);
        const color = options.color || { r: 1, g: 1, b: 1, a: 1 };
        shape.setColor(color.r, color.g, color.b, color.a);
        
        if (options.rotation) {
            const rot = options.rotation;
            shape.setRotation(rot.pitch || 0, rot.yaw || 0, rot.roll || 0);
        }
        
        shape.draw();
        
        shape.persistId = id;
        shape.pos = { x, y, z };
        shape.dimId = dimId;
        shape.metadata = options.metadata || {};
        shape.rotation = options.rotation || null;
        
        this.texts.set(id, shape);
        _persistedShapes.set(id, shape);
        
        const data = loadStorageData();
        const existingIndex = data.floatingTexts.findIndex(ft => ft.id === id);
        const now = Date.now();
        
        const ftData = {
            id: id,
            category: this.category,
            x: x,
            y: y,
            z: z,
            dimId: dimId,
            text: text,
            color: color,
            rotation: options.rotation || null,
            createdAt: existingIndex !== -1 ? data.floatingTexts[existingIndex].createdAt : now,
            updatedAt: now,
            metadata: options.metadata || {}
        };
        
        if (existingIndex !== -1) {
            data.floatingTexts[existingIndex] = ftData;
        } else {
            data.floatingTexts.push(ftData);
        }
        saveStorageData();
        
        return shape;
    }
    
    remove(id) {
        const shape = this.texts.get(id);
        if (shape) {
            try { shape.destroy(); } catch (e) {}
            this.texts.delete(id);
        }
        _persistedShapes.delete(id);
        
        const data = loadStorageData();
        const index = data.floatingTexts.findIndex(ft => ft.id === id);
        if (index !== -1) {
            data.floatingTexts.splice(index, 1);
            saveStorageData();
        }
        
        return true;
    }
    
    updateText(id, newText) {
        const shape = this.texts.get(id);
        if (shape && shape.isValid()) {
            shape.setText(newText);
            shape.update();
        }
        
        const data = loadStorageData();
        const ft = data.floatingTexts.find(f => f.id === id);
        if (ft) {
            ft.text = newText;
            ft.updatedAt = Date.now();
            saveStorageData();
            return true;
        }
        return false;
    }
    
    setRotation(id, pitch, yaw, roll) {
        const shape = this.texts.get(id);
        if (shape && shape.isValid()) {
            shape.setRotation(pitch, yaw, roll);
            shape.rotation = { pitch, yaw, roll };
            shape.update();
        }
        
        const data = loadStorageData();
        const ft = data.floatingTexts.find(f => f.id === id);
        if (ft) {
            ft.rotation = { pitch, yaw, roll };
            ft.updatedAt = Date.now();
            saveStorageData();
            return true;
        }
        return false;
    }
    
    clearRotation(id) {
        const shape = this.texts.get(id);
        if (shape && shape.isValid()) {
            shape.clearRotation();
            shape.rotation = null;
            shape.update();
        }
        
        const data = loadStorageData();
        const ft = data.floatingTexts.find(f => f.id === id);
        if (ft) {
            ft.rotation = null;
            ft.updatedAt = Date.now();
            saveStorageData();
            return true;
        }
        return false;
    }
    
    get(id) {
        return this.texts.get(id) || null;
    }
    
    has(id) {
        return this.texts.has(id);
    }
    
    getAllIds() {
        return Array.from(this.texts.keys());
    }
    
    sendToPlayer(player) {
        const dimId = player.pos.dimid;
        for (const [id, shape] of this.texts) {
            if (!shape.isValid()) continue;
            if (shape.dimId !== undefined && shape.dimId !== dimId) continue;
            try {
                shape.drawTo(player);
            } catch (e) {}
        }
    }
    
    refreshAll() {
        for (const [, shape] of this.texts) {
            if (shape.isValid()) {
                shape.draw();
            }
        }
    }
    
    clearAll() {
        for (const [id, shape] of this.texts) {
            try {
                if (shape && typeof shape.destroy === 'function') {
                    shape.destroy();
                }
            } catch (e) {}
            _persistedShapes.delete(id);
        }
        this.texts.clear();
        
        const data = loadStorageData();
        data.floatingTexts = data.floatingTexts.filter(ft => ft.category !== this.category);
        saveStorageData();
    }
    
    destroyDisplays() {
        for (const [id, shape] of this.texts) {
            try {
                if (shape && typeof shape.destroy === 'function') {
                    shape.destroy();
                }
            } catch (e) {}
            _persistedShapes.delete(id);
        }
        this.texts.clear();
        this._initialized = false;
    }
}

// ==================== 富文本悬浮字类 ====================
/**
 * 支持十六进制颜色的富文本悬浮字
 * 每个字符可以有独立的颜色
 */
class RichTextShape {
    constructor() {
        this.charShapes = []; // 每个字符的 TextShape
        this.x = 0;
        this.y = 0;
        this.z = 0;
        this.isDestroyed = false;
    }
    
    isValid() {
        return !this.isDestroyed && this.charShapes.length > 0 && hasAPI;
    }
    
    /**
     * 设置富文本内容
     * @param {string} text - 带颜色标记的文本，格式: <#RRGGBB>文本</>
     * @param {Object} defaultColor - 默认颜色 {r, g, b, a}
     */
    setText(text, defaultColor = { r: 1, g: 1, b: 1, a: 1 }) {
        // 先销毁旧的
        this.destroy();
        this.isDestroyed = false;
        this.charShapes = [];
        
        const segments = parseColoredText(text);
        let charX = this.x;
        const charWidth = 0.12; // 字符宽度
        
        // 计算总宽度用于居中
        let totalChars = 0;
        for (const seg of segments) {
            totalChars += [...seg.text].length;
        }
        charX = this.x - (totalChars * charWidth) / 2;
        
        for (const seg of segments) {
            const color = seg.color || defaultColor;
            const chars = [...seg.text]; // 支持 Unicode
            
            for (const char of chars) {
                const shape = ShapeFactory.createText(charX, this.y, this.z, char);
                if (shape) {
                    shape.setColor(color.r, color.g, color.b, color.a || 1);
                    this.charShapes.push(shape);
                }
                charX += charWidth;
            }
        }
    }
    
    /**
     * 设置位置
     */
    setLocation(x, y, z) {
        const dx = x - this.x;
        const dy = y - this.y;
        const dz = z - this.z;
        
        this.x = x;
        this.y = y;
        this.z = z;
        
        for (const shape of this.charShapes) {
            if (shape.isValid()) {
                const loc = shape.getLocation();
                if (loc) {
                    shape.setLocation(loc.x + dx, loc.y + dy, loc.z + dz);
                }
            }
        }
    }
    
    draw() {
        for (const shape of this.charShapes) {
            if (shape.isValid()) {
                shape.draw();
            }
        }
    }
    
    drawToPlayer(playerName) {
        for (const shape of this.charShapes) {
            if (shape.isValid()) {
                shape.drawToPlayer(playerName);
            }
        }
    }
    
    drawTo(player) {
        if (!player) return;
        this.drawToPlayer(player.name);
    }
    
    update() {
        for (const shape of this.charShapes) {
            if (shape.isValid()) {
                shape.update();
            }
        }
    }
    
    remove() {
        for (const shape of this.charShapes) {
            if (shape.isValid()) {
                shape.remove();
            }
        }
    }
    
    destroy() {
        for (const shape of this.charShapes) {
            if (shape && typeof shape.destroy === 'function') {
                try { shape.destroy(); } catch (e) {}
            }
        }
        this.charShapes = [];
        this.isDestroyed = true;
    }
}

/**
 * 创建富文本悬浮字
 * @param {number} x 
 * @param {number} y 
 * @param {number} z 
 * @param {string} text - 带颜色标记的文本
 * @param {Object} defaultColor - 默认颜色
 * @returns {RichTextShape}
 */
function createRichText(x, y, z, text, defaultColor = { r: 1, g: 1, b: 1, a: 1 }) {
    const rt = new RichTextShape();
    rt.x = x;
    rt.y = y;
    rt.z = z;
    rt.setText(text, defaultColor);
    return rt;
}

// ==================== 导出 ====================
module.exports = {
    // API 状态
    isAvailable,
    isNativeAvailable,
    isFloatingTextAvailable,
    isGradientLineAvailable,
    getRawAPI,
    getRawNativeAPI,
    getRawFloatingTextAPI,
    getRawGradientLineAPI,
    destroyAll,
    destroyAllNative,
    destroyAllFloatingText,
    destroyAllGradientLine,
    
    // API 就绪回调
    onReady,
    waitForReady,
    initializeAPI,
    
    // 动态刷新配置
    getRefreshInterval,
    setRefreshInterval,
    getDurationMultiplier,
    setDurationMultiplier,
    setExtraOverlap,
    setMinDuration,
    calculateDuration,
    getRefreshConfig,
    
    // 查询功能
    findTextByLocation,
    findTextByLocationAndContent,
    createTextWithCleanup,
    cleanupTextAtLocation,
    getAllShapeIds,
    shapeExists,
    
    // 全局悬浮字管理
    registerManager,
    unregisterManager,
    getRegisteredManager,
    getRegisteredCategories,
    reloadRefreshAll,
    reloadRefreshCategory,
    sendAllToPlayer,
    getFloatingTextStats,
    
    // FloatingText 动画
    tickFloatingText,
    tickGradientLine,
    tickAll,
    
    // 枚举
    ShapeType,
    PlaneType,
    ScrollDirection,
    VerticalAnimationType,
    
    // 类
    BaseShape,
    TextShape,
    MultiLineFloatingText,
    GradientLineShape,
    
    // 工厂
    ShapeFactory,
    NativeFactory,
    
    // 持久化存储
    PersistentStorage,
    
    // 管理器
    FloatingTextManager,
    SimpleTextManager,  // 简单悬浮字管理器
    
    // 颜色工具
    parseHexColor,
    toHexColor,
    hsvToRgb,
    lerpColor,
    parseColoredText,
    hexToMcColor,
    convertHexToMcColors,
    MC_COLORS,
    
    // 富文本
    RichTextShape,
    createRichText,
    
    // 原始 API（动态获取）
    get DebugShape() {
        return hasAPI ? DebugShape : null;
    },
    get DebugShapeNative() {
        return hasNativeAPI ? DebugShapeNative : null;
    },
    get FloatingText() {
        return hasFloatingTextAPI ? FloatingText : null;
    },
    get GradientLine() {
        return hasGradientLineAPI ? GradientLine : null;
    }
};
