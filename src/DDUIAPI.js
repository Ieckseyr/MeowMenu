/**
 * DDUI API - LSE 封装模块
 * 支持:
 *   - SimpleForm (按钮菜单, 对应 DDUI CustomForm 纯按钮模式)
 *   - CustomForm (输入表单, 对应 DDUI CustomForm 输入模式)
 *   - ModalForm  (双键对话, 对应 DDUI MessageBox)
 *   - sendUpdate 动态刷新 (DDUI 核心特性)
 *   - Observable 反应式数据绑定 (JS 侧实现)
 *   - InterceptedForm 按钮点击不关闭表单 (C++ hook 拦截)
 *
 * 用法:
 *   const DDUI = require('../src/DDUIAPI');
 *   // 或
 *   const { Observable, SimpleForm, CustomForm, MessageBox } = require('../src/DDUIAPI');
 *
 * @author 伊希娅
 * @version 1.0.0
 */

// ==================== 导入原生函数 ====================
let _Simple = null;
let _Custom = null;
let _Modal  = null;
let _Intercept = null;
let _Raw    = null;
let hasAPI  = false;

let _cbCounter = 0;
const CB_NS = "DDUI_LSE_CB";

function safeImport(ns, fn) {
    try {
        if (!ll.hasExported(ns, fn)) return null;
        const f = ll.import(ns, fn);
        return typeof f === 'function' ? f : null;
    } catch (e) { return null; }
}

function initAPI() {
    if (hasAPI) return true;
    try {
        _Simple = {
            create:        safeImport("DDUI_Simple", "create"),
            setTitle:      safeImport("DDUI_Simple", "setTitle"),
            setContent:    safeImport("DDUI_Simple", "setContent"),
            appendHeader:  safeImport("DDUI_Simple", "appendHeader"),
            appendLabel:   safeImport("DDUI_Simple", "appendLabel"),
            appendDivider: safeImport("DDUI_Simple", "appendDivider"),
            appendButton:  safeImport("DDUI_Simple", "appendButton"),
            sendTo:        safeImport("DDUI_Simple", "sendTo"),
            sendUpdate:    safeImport("DDUI_Simple", "sendUpdate"),
            destroy:       safeImport("DDUI_Simple", "destroy"),
        };
        _Custom = {
            create:           safeImport("DDUI_Custom", "create"),
            setTitle:         safeImport("DDUI_Custom", "setTitle"),
            setSubmitButton:  safeImport("DDUI_Custom", "setSubmitButton"),
            appendHeader:     safeImport("DDUI_Custom", "appendHeader"),
            appendLabel:      safeImport("DDUI_Custom", "appendLabel"),
            appendDivider:    safeImport("DDUI_Custom", "appendDivider"),
            appendInput:      safeImport("DDUI_Custom", "appendInput"),
            appendToggle:     safeImport("DDUI_Custom", "appendToggle"),
            appendDropdown:   safeImport("DDUI_Custom", "appendDropdown"),
            appendSlider:     safeImport("DDUI_Custom", "appendSlider"),
            appendStepSlider: safeImport("DDUI_Custom", "appendStepSlider"),
            sendTo:           safeImport("DDUI_Custom", "sendTo"),
            sendUpdate:       safeImport("DDUI_Custom", "sendUpdate"),
            getFormData:      safeImport("DDUI_Custom", "getFormData"),
            destroy:          safeImport("DDUI_Custom", "destroy"),
        };
        _Modal = {
            create:         safeImport("DDUI_Modal", "create"),
            setTitle:       safeImport("DDUI_Modal", "setTitle"),
            setContent:     safeImport("DDUI_Modal", "setContent"),
            setUpperButton: safeImport("DDUI_Modal", "setUpperButton"),
            setLowerButton: safeImport("DDUI_Modal", "setLowerButton"),
            sendTo:         safeImport("DDUI_Modal", "sendTo"),
            sendUpdate:     safeImport("DDUI_Modal", "sendUpdate"),
            destroy:        safeImport("DDUI_Modal", "destroy"),
        };
        _Intercept = {
            sendIntercepted:     safeImport("DDUI_Intercept", "sendIntercepted"),
            updateIntercepted:   safeImport("DDUI_Intercept", "updateIntercepted"),
            closeIntercepted:    safeImport("DDUI_Intercept", "closeIntercepted"),
            closeAllIntercepted: safeImport("DDUI_Intercept", "closeAllIntercepted"),
        };
        // Raw API - 直接操作原始表单 JSON
        _Raw = {
            sendRawTo:     safeImport("DDUI_Raw", "sendRawTo"),
            sendRawUpdate: safeImport("DDUI_Raw", "sendRawUpdate"),
        };
        hasAPI = _Simple.create !== null && _Custom.create !== null && _Modal.create !== null;
        return hasAPI;
    } catch (e) { return false; }
}

function registerCb(callback) {
    if (typeof callback !== 'function') return ["", ""];
    const fn = `cb_${++_cbCounter}_${Date.now()}`;
    ll.export(callback, CB_NS, fn);
    return [CB_NS, fn];
}

// ==================== Observable ====================

class Observable {
    constructor(value) {
        this._value = value;
        this._subs = [];
    }

    static create(defaultValue) {
        return new Observable(defaultValue);
    }

    getData() { return this._value; }

    setData(value) {
        this._value = value;
        for (const cb of this._subs) {
            try { cb(value); } catch (e) {}
        }
    }

    subscribe(callback) {
        this._subs.push(callback);
        return this._subs.length - 1;
    }

    unsubscribe(index) {
        if (index >= 0 && index < this._subs.length) {
            this._subs.splice(index, 1);
        }
    }
}

// ==================== SimpleForm ====================

class SimpleFormBuilder {
    constructor(formId) {
        this._id = formId;
        this._buttons = [];
        // 同时维护原始数据，用于 sendRawUpdate 构建 JSON
        this._title = "";
        this._content = "";
        this._rawButtons = [];
    }

    static create(title) {
        if (!hasAPI && !initAPI()) return null;
        const id = _Simple.create(String(title));
        if (id < 0) return null;
        const builder = new SimpleFormBuilder(id);
        builder._title = String(title);
        return builder;
    }

    setTitle(t) { this._title = String(t); _Simple.setTitle(this._id, this._title); return this; }
    setContent(c) { this._content = String(c); _Simple.setContent(this._id, this._content); return this; }
    header(t) { _Simple.appendHeader(this._id, String(t)); return this; }
    label(t) {
        _Simple.appendLabel(this._id, String(t instanceof Observable ? t.getData() : t));
        return this;
    }
    divider() { _Simple.appendDivider(this._id); return this; }
    spacer() { _Simple.appendLabel(this._id, " "); return this; }

    button(text, callback, options = {}) {
        this._buttons.push(callback || null);
        const btnData = { text: String(text) };
        if (options.icon) {
            btnData.image = { type: options.iconType || "path", data: options.icon };
        }
        this._rawButtons.push(btnData);
        _Simple.appendButton(this._id, String(text), options.icon || "", options.iconType || "");
        return this;
    }

    // 构建 BDS SimpleForm JSON（用于 sendRawUpdate）
    _buildJSON() {
        return JSON.stringify({
            type: "form",
            title: this._title,
            content: this._content,
            buttons: this._rawButtons.map(b => {
                const btn = { text: b.text };
                if (b.image) btn.image = b.image;
                return btn;
            })
        });
    }

    sendTo(player) {
        const pName = typeof player === 'string' ? player : player.realName;
        const buttons = this._buttons;
        return new Promise((resolve, reject) => {
            const [ns, fn] = registerCb((name, sel) => {
                if (sel < 0) {
                    resolve({ canceled: true, selection: -1 });
                } else {
                    if (sel < buttons.length && buttons[sel]) {
                        try { buttons[sel](); } catch (e) {}
                    }
                    resolve({ canceled: false, selection: sel });
                }
            });
            if (!_Simple.sendTo(this._id, pName, ns, fn)) reject(new Error("发送失败"));
        });
    }

    sendUpdate(player) {
        const pName = typeof player === 'string' ? player : player.realName;
        const buttons = this._buttons;
        // 优先使用 sendRawUpdate（保持滚动位置）
        if (_Raw && _Raw.sendRawUpdate) {
            const formJSON = this._buildJSON();
            return new Promise((resolve, reject) => {
                const [ns, fn] = registerCb((name, resp) => {
                    if (!resp) {
                        resolve({ canceled: true, selection: -1 });
                    } else {
                        let sel = -1;
                        try { sel = parseInt(resp, 10); } catch (e) {}
                        if (isNaN(sel)) sel = -1;
                        if (sel >= 0 && sel < buttons.length && buttons[sel]) {
                            try { buttons[sel](); } catch (e) {}
                        }
                        resolve({ canceled: false, selection: sel });
                    }
                });
                if (!_Raw.sendRawUpdate(pName, formJSON, ns, fn)) reject(new Error("更新失败"));
            });
        }
        // 回退到 LL 的 sendUpdate
        return new Promise((resolve, reject) => {
            const [ns, fn] = registerCb((name, sel) => {
                if (sel < 0) resolve({ canceled: true, selection: -1 });
                else {
                    if (sel < buttons.length && buttons[sel]) try { buttons[sel](); } catch (e) {}
                    resolve({ canceled: false, selection: sel });
                }
            });
            if (!_Simple.sendUpdate(this._id, pName, ns, fn)) reject(new Error("更新失败"));
        });
    }

    destroy() { _Simple.destroy(this._id); }
}

// ==================== CustomForm ====================

class CustomFormBuilder {
    constructor(formId) {
        this._id = formId;
        this._observables = {};
    }

    static create(title) {
        if (!hasAPI && !initAPI()) return null;
        const id = _Custom.create(String(title));
        return id >= 0 ? new CustomFormBuilder(id) : null;
    }

    setTitle(t) { _Custom.setTitle(this._id, String(t)); return this; }
    submitButton(t) { _Custom.setSubmitButton(this._id, String(t)); return this; }
    header(t) { _Custom.appendHeader(this._id, String(t)); return this; }
    label(t) {
        _Custom.appendLabel(this._id, String(t instanceof Observable ? t.getData() : t));
        return this;
    }
    divider() { _Custom.appendDivider(this._id); return this; }
    spacer() { _Custom.appendLabel(this._id, " "); return this; }

    textField(name, label, observable, options = {}) {
        if (observable instanceof Observable) this._observables[name] = observable;
        const def = observable instanceof Observable ? String(observable.getData()) : "";
        _Custom.appendInput(this._id, name, String(label), options.placeholder || "", def, options.tooltip || "");
        return this;
    }

    toggle(name, label, observable, options = {}) {
        if (observable instanceof Observable) this._observables[name] = observable;
        const def = observable instanceof Observable ? !!observable.getData() : false;
        _Custom.appendToggle(this._id, name, String(label), def, options.tooltip || "");
        return this;
    }

    dropdown(name, label, observable, options, opts = {}) {
        if (observable instanceof Observable) this._observables[name] = observable;
        const def = observable instanceof Observable ? Number(observable.getData()) : 0;
        _Custom.appendDropdown(this._id, name, String(label), options, def, opts.tooltip || "");
        return this;
    }

    slider(name, label, observable, min, max, options = {}) {
        if (observable instanceof Observable) this._observables[name] = observable;
        const def = observable instanceof Observable ? Number(observable.getData()) : min;
        _Custom.appendSlider(this._id, name, String(label), min, max, options.step || 0, def, options.tooltip || "");
        return this;
    }

    stepSlider(name, label, steps, defaultIdx = 0, options = {}) {
        _Custom.appendStepSlider(this._id, name, String(label), steps, defaultIdx, options.tooltip || "");
        return this;
    }

    sendTo(player) {
        const pName = typeof player === 'string' ? player : player.realName;
        const obs = this._observables;
        return new Promise((resolve, reject) => {
            const [ns, fn] = registerCb((name, json) => {
                if (!json) { resolve({ canceled: true }); return; }
                try {
                    const data = JSON.parse(json);
                    for (const [k, o] of Object.entries(obs)) {
                        if (k in data) o.setData(data[k]);
                    }
                    resolve({ canceled: false, ...data });
                } catch (e) { resolve({ canceled: true }); }
            });
            if (!_Custom.sendTo(this._id, pName, ns, fn)) reject(new Error("发送失败"));
        });
    }

    sendUpdate(player) {
        const pName = typeof player === 'string' ? player : player.realName;
        const obs = this._observables;
        return new Promise((resolve, reject) => {
            const [ns, fn] = registerCb((name, json) => {
                if (!json) { resolve({ canceled: true }); return; }
                try {
                    const data = JSON.parse(json);
                    for (const [k, o] of Object.entries(obs)) {
                        if (k in data) o.setData(data[k]);
                    }
                    resolve({ canceled: false, ...data });
                } catch (e) { resolve({ canceled: true }); }
            });
            if (!_Custom.sendUpdate(this._id, pName, ns, fn)) reject(new Error("更新失败"));
        });
    }

    getFormData() { return _Custom.getFormData(this._id); }
    destroy() { _Custom.destroy(this._id); }
}

// ==================== MessageBox ====================

class MessageBoxBuilder {
    constructor(formId) { this._id = formId; }

    static create(title, content, button1, button2) {
        if (!hasAPI && !initAPI()) return null;
        const id = _Modal.create(String(title || ""), String(content || ""),
            String(button1 || "确认"), String(button2 || "取消"));
        return id >= 0 ? new MessageBoxBuilder(id) : null;
    }

    title(t) { _Modal.setTitle(this._id, String(t)); return this; }
    body(t) { _Modal.setContent(this._id, String(t)); return this; }
    button1(t) { _Modal.setUpperButton(this._id, String(t)); return this; }
    button2(t) { _Modal.setLowerButton(this._id, String(t)); return this; }

    sendTo(player) {
        const pName = typeof player === 'string' ? player : player.realName;
        return new Promise((resolve, reject) => {
            const [ns, fn] = registerCb((name, sel) => {
                resolve({ canceled: sel < 0, selection: sel });
            });
            if (!_Modal.sendTo(this._id, pName, ns, fn)) reject(new Error("发送失败"));
        });
    }

    sendUpdate(player) {
        const pName = typeof player === 'string' ? player : player.realName;
        return new Promise((resolve, reject) => {
            const [ns, fn] = registerCb((name, sel) => {
                resolve({ canceled: sel < 0, selection: sel });
            });
            if (!_Modal.sendUpdate(this._id, pName, ns, fn)) reject(new Error("更新失败"));
        });
    }

    destroy() { _Modal.destroy(this._id); }
}

// ==================== InterceptedForm (按钮点击回弹表单并更新内容) ====================

class InterceptedFormBuilder {
    constructor() {
        this._type = "form";
        this._title = "";
        this._content = "";
        this._buttons = [];
        this._formId = -1;
        this._playerName = "";
        this._closed = false;
    }

    static createSimple(title) {
        if (!hasAPI && !initAPI()) return null;
        if (!_Intercept || !_Intercept.sendIntercepted) return null;
        const builder = new InterceptedFormBuilder();
        builder._title = String(title);
        return builder;
    }

    setTitle(t) { this._title = String(t); return this; }
    setContent(c) { this._content = String(c); return this; }

    button(text, callback, options = {}) {
        const btn = { text: String(text), callback: callback || null };
        if (options.icon) {
            btn.image = { type: options.iconType || "path", data: options.icon };
        }
        this._buttons.push(btn);
        return this;
    }

    _buildJSON() {
        const data = {
            type: "form",
            title: this._title,
            content: this._content,
            buttons: this._buttons.map(b => {
                const btn = { text: b.text };
                if (b.image) btn.image = b.image;
                return btn;
            })
        };
        return JSON.stringify(data);
    }

    sendTo(player) {
        const pName = typeof player === 'string' ? player : player.realName;
        this._playerName = pName;
        const self = this;

        const formJSON = this._buildJSON();

        // 注册回调
        const cbFn = `icb_${++_cbCounter}_${Date.now()}`;
        ll.export((playerName, formId, responseJson) => {
            if (responseJson === "__canceled__") {
                self._closed = true;
                if (self._onCancel) {
                    try { self._onCancel(); } catch (e) {}
                }
                return;
            }

            let sel = -1;
            try { sel = parseInt(responseJson, 10); } catch (e) {}
            const currentButtons = self._buttons;
            if (sel >= 0 && sel < currentButtons.length && currentButtons[sel].callback) {
                const ctx = {
                    formId: formId,
                    playerName: playerName,
                    selection: sel,
                    close: () => {
                        self._closed = true;
                        if (_Intercept.closeIntercepted) {
                            _Intercept.closeIntercepted(playerName, formId);
                        }
                    },
                    // 动态更新表单内容
                    update: (newBuilder) => {
                        if (newBuilder && _Intercept.updateIntercepted) {
                            const newJSON = newBuilder._buildJSON();
                            _Intercept.updateIntercepted(playerName, formId, newJSON);
                            self._buttons = newBuilder._buttons;
                        }
                    }
                };
                try { currentButtons[sel].callback(ctx); } catch (e) {}
            }
        }, CB_NS, cbFn);

        this._formId = _Intercept.sendIntercepted(pName, formJSON, CB_NS, cbFn);
        return this;
    }

    onCancel(callback) {
        this._onCancel = callback;
        return this;
    }

    close() {
        if (!this._closed && this._formId >= 0) {
            this._closed = true;
            _Intercept.closeIntercepted(this._playerName, this._formId);
        }
    }
}

// ==================== 初始化 ====================
initAPI();

// ==================== 导出 ====================
module.exports = {
    Observable,
    SimpleForm: SimpleFormBuilder,
    CustomForm: CustomFormBuilder,
    MessageBox: MessageBoxBuilder,
    InterceptedForm: InterceptedFormBuilder,
    isAvailable: () => hasAPI,
    initialize: initAPI,
};
