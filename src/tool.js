/**
 * @author Yoyo
 * 其它工具类
 * date 2025-05-04 04:20:09
 * 好累呀，但是生活还要继续！
 */

const { EventEmitter } = require('events');

class TypeConverter extends EventEmitter {


  /**​
 * 确定性概率判断（结果可复现）
 * @param {number} percent - 成功概率 (0.01~100)
 * @param {number} [seed=Date.now()] - 随机数种子（默认当前时间戳）
 * @returns {boolean} 是否成功
 */
  seededProbabilityCheck(percent, seed = Date.now()) {
    // 参数校验
    if (percent < 0.01 || percent > 100) {
      throw new Error('概率必须在 0.01~100 之间');
    }
    // 线性同余生成器 (LCG) 参数
    const m = 0x80000000; // 模数 (2^31)
    const a = 1103515245;  // 乘数
    const c = 12345;       // 增量
    // 生成伪随机数 (范围 [0,1))
    seed = (a * seed + c) % m; // 更新种子
    const randomValue = seed / m; // 归一化
    return randomValue < (percent / 100);
  }

  /**
 * 智能随机数生成器（支持 0.01~100 或更大范围）
 * @param {Object} options - 配置项
 * @param {number} options.min - 最小值
 * @param {number} options.max - 最大值
 * @param {boolean} [options.isFloat=true] - 是否强制浮点数
 * @param {number} [options.precision] - 小数位数（默认自动计算）
 * @returns {number} 随机数
 */
  getSmartRandom(options) {
    let { min, max, isFloat = true, precision } = options;
    // 自动计算合理的小数位数（避免 0.001 被截断）
    if (precision === undefined) {
      const minDecimals = min.toString().split('.')[1]?.length || 0;
      const maxDecimals = max.toString().split('.')[1]?.length || 0;
      precision = Math.max(minDecimals, maxDecimals, 2); // 至少保留 2 位
    }
    // 生成随机数
    const value = Math.random() * (max - min) + min;
    return isFloat ? parseFloat(value.toFixed(precision)) : Math.round(value);
  }

  /**
   * 检查并转换为数字
   * @param {any} input - 输入值
   * @param {boolean} isErr - 是否返回错误对象
   * @param {string} errString - 自定义错误信息
   * @returns {number|null} 转换后的数字或 null
   */
  isNumber(input, isErr = false, errString = '请输入合适的数字格式') {
    // 处理空值
    if (input === null || input === undefined || input === '') {
      if (isErr) throw new Error(errString)
      return null;
    }

    // 如果是数字类型直接返回
    if (typeof input === 'number') {
      if (isNaN(input)) {
        if (isErr) throw new Error(errString)
        return null;
      }
      return input;
    }

    // 处理字符串数字
    const num = Number(input);
    if (!isNaN(num)) return num;
    if (isErr) throw new Error(errString)
    return null;
  }

  /**
   * 检查并转换为数组
   * @param {any} input - 输入值
   * @param {boolean} isErr - 是否返回错误对象
   * @param {string} errString - 自定义错误信息
   * @returns {Array|null} 转换后的数组或 null
   */
  isArray(input, isErr = false, errString = '请输入合适的数组格式') {
    // 处理空值
    if (input === null || input === undefined) {
      if (isErr) throw new Error(errString)
      return null;
    }

    // 如果已经是数组直接返回
    if (Array.isArray(input)) return input;

    // 处理字符串分割
    if (typeof input === 'string') {
      try {
        return input.split(/[,，|]/).map(s => s.trim()).filter(Boolean);
      } catch {
        if (isErr) throw new Error(errString)
        return null;
      }
    }

    // 其他类型无法转换
    if (isErr) throw new Error(errString)
    return null;
  }


  /**
 * 将json字符串转换为对象
 * @param {string} json 待转换的json字符串
 * @returns {object|null}
 */
  jsonToObject(json) {
    try {
      return JSON.parse(json);
    } catch (e) {
      return null;
    }
  }

  /**生成随机唯一ID
   * @param {number} length - 所需ID的长度
   * @returns {string} 随机生成的唯一ID
   */
  generateRandomId(length = 16) {
    const safeLength = Math.max(8, length);
    const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789';
    let result = Date.now().toString(36).slice(-4);
    for (let i = 0; i < Math.floor(safeLength * 0.6) - 4; i++) {
      result += chars.charAt(Math.floor(Math.random() * chars.length));
    }
    result += performance.now().toString(36).replace('.', '').slice(-3);
    while (result.length < safeLength) {
      result += chars.charAt(Math.floor(Math.random() * chars.length));
    }
    return result.slice(0, safeLength);
  }

  /**
 * 判断坐标是否在区域内
 * @param {IntPos} startPoint 区域开始坐标
 * @param {IntPos} endPoint 区域结束坐标
 * @param {int} dimid 区域的维度id
 * @param {FloatPos} point 指定坐标
 * @returns 
 */
  isPointInRegion(startPoint, endPoint, dimid = 0, point) {
    if (dimid !== point.dimid) return false; // 不在同一维度

    const adjustedStartX = Math.min(startPoint.x, endPoint.x);
    const adjustedEndX = Math.max(startPoint.x, endPoint.x);
    const adjustedStartY = Math.min(startPoint.y, endPoint.y);
    const adjustedEndY = Math.max(startPoint.y, endPoint.y);
    const adjustedStartZ = Math.min(startPoint.z, endPoint.z);
    const adjustedEndZ = Math.max(startPoint.z, endPoint.z);

    const inRangeX = point.x >= adjustedStartX && point.x <= adjustedEndX;
    const inRangeY = point.y >= adjustedStartY && point.y <= adjustedEndY;
    const inRangeZ = point.z >= adjustedStartZ && point.z <= adjustedEndZ;

    return inRangeX && inRangeY && inRangeZ;
  }


  /**
    * 获取两个坐标直线距离
    * @param {IntPos} posA 
    * @param {IntPos} posB 
    * @returns {number} 距离
    */
  getPosDistance(posA, posB) {
    if (posA.dimid == posB.dimid) {
      let dx = posA.x - posB.x;
      let dy = posA.y - posB.y
      let dz = posA.z - posB.z;
      let ams = (dx * dx) + (dy * dy) + (dz * dz);
      return Math.sqrt(ams);
    }
    return Infinity;
  }

  /**​
   * 根据掩码数组筛选对象数组并添加mask字段（增强容错版）
   * @param {Array} objectsArray 对象数组，每个对象包含item和index属性
   * @param {Array} maskArray 掩码数组（0视为false，其他值视为true）
   * @param {boolean} isErr 是否返回错误对象
   * @returns {Array|null} 筛选后的新对象数组，包含原字段和mask字段
   */
  filterByMask(objectsArray, maskArray, isErr = false) {
    // 参数顺序智能判断（根据第一个元素的类型自动交换）
    const isFirstParamMask = typeof objectsArray?.[0] === 'number';
    const isSecondParamObject = typeof maskArray?.[0] === 'object';
    if (isFirstParamMask && isSecondParamObject) {
      [objectsArray, maskArray] = [maskArray, objectsArray];
      if (isErr) console.warn('参数顺序已自动交换');
    }

    // 严格长度校验
    if (objectsArray.length !== maskArray.length) {
      const errorMsg = `数组长度不匹配 (对象数组:${objectsArray.length} 掩码数组:${maskArray.length})`;
      if (isErr) throw new Error(errorMsg);
      console.error(errorMsg);
      return null;
    }

    // 使用reduce同时完成过滤和字段添加
    return objectsArray.reduce((result, current, index) => {
      const maskValue = maskArray[index];

      // 排除undefined和无效值
      if (maskValue === undefined) return result;

      // 类型转换判断
      if (Boolean(Number(maskValue))) {
        // 创建新对象并添加mask字段
        result.push({
          ...current,
          mask: maskValue
        });
      }
      return result;
    }, []);
  }

  /**
   * 延迟
   * @param {int} ms 毫秒
   * @returns Promise
   */
  sleep(ms = 1000) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  /**​
 * 判断两个由任意顺序的 vec3 点定义的立方体是否重叠
 * @param {Object} box1 第一个立方体 {p1: vec3, p2: vec3}
 * @param {Object} box2 第二个立方体 {p1: vec3, p2: vec3}
 * @returns {boolean} 如果重叠返回 true，否则返回 false
 */
  isOverlapping3D(box1, box2) {
    if (box1.p1?.dimid != box2.p1?.dimid) return false;// 判断两个立方体是否在同一个维度
    // 计算 box1 的实际 min 和 max
    const box1Min = {
      x: Math.min(box1.p1.x, box1.p2.x),
      y: Math.min(box1.p1.y, box1.p2.y),
      z: Math.min(box1.p1.z, box1.p2.z)
    };
    const box1Max = {
      x: Math.max(box1.p1.x, box1.p2.x),
      y: Math.max(box1.p1.y, box1.p2.y),
      z: Math.max(box1.p1.z, box1.p2.z)
    };

    // 计算 box2 的实际 min 和 max
    const box2Min = {
      x: Math.min(box2.p1.x, box2.p2.x),
      y: Math.min(box2.p1.y, box2.p2.y),
      z: Math.min(box2.p1.z, box2.p2.z)
    };
    const box2Max = {
      x: Math.max(box2.p1.x, box2.p2.x),
      y: Math.max(box2.p1.y, box2.p2.y),
      z: Math.max(box2.p1.z, box2.p2.z)
    };

    // 检查是否不重叠
    if (
      box1Max.x < box2Min.x || // box1 在 box2 左侧
      box1Min.x > box2Max.x || // box1 在 box2 右侧
      box1Max.y < box2Min.y || // box1 在 box2 上方
      box1Min.y > box2Max.y || // box1 在 box2 下方
      box1Max.z < box2Min.z || // box1 在 box2 前面
      box1Min.z > box2Max.z    // box1 在 box2 后面
    ) {
      return false;
    }

    // 否则重叠
    return true;
  }

  /**
 * 设置物品锁定和死亡不掉落
 * @param {Item} item 
 * @param {string} name 
 * @param {Array<string>} lore 
 * @returns {boolean}
 */
  setItemLockOrKeep(item, name = null, lore = []) {
    if (!item) return false;
    if (item?.isNull()) return false;
    name = name || item.name;
    const itemNbt = item.getNbt();
    itemNbt.setTag("tag", new NbtCompound({
      "minecraft:item_lock": new NbtByte(1),//锁定物品不能移动（红色） 2是可以移动不能丢和合成黄色
      "minecraft:keep_on_death": new NbtByte(1),//死亡不掉落(黄色)
      "display": new NbtCompound({
        "Name": new NbtString(name)
      }),
      "Unbreakable": new NbtByte(1)// 是否可损坏
    }));
    itemNbt.setByte("Count", 1);
    item.setNbt(itemNbt);
    item.setLore(lore);
  }

  /**
 * 设置物品死亡不掉落(不能丢弃)
 * @param {Item} item 
 * @param {string} name 
 * @param {Array<string>} lore 
 * @returns {boolean}
 */
  setItemKeep(item, name = null, lore = []) {
    if (!item) return false;
    if (item?.isNull()) return false;
    name = name || item.name;
    const itemNbt = item.getNbt();
    itemNbt.setTag("tag", new NbtCompound({
      "minecraft:item_lock": new NbtByte(2),
      "minecraft:keep_on_death": new NbtByte(1),//死亡不掉落(黄色)
      "display": new NbtCompound({
        "Name": new NbtString(name)
      }),
      "Unbreakable": new NbtByte(1)// 是否可损坏
    }));
    itemNbt.setByte("Count", 1);
    item.setNbt(itemNbt);
    item.setLore(lore);
  }

  /**
   * 判断物品是否死亡不掉落
   * @param {Item} item 
   */
  isItemKeep(item) {
    if (!item) return false;
    if (item?.isNull()) return false;
    const itemNbt = item.getNbt();
    const tag = itemNbt.getTag("tag");
    if (!tag) return false;// 不存在tag标签
    return Boolean(tag.getData('minecraft:keep_on_death'));
  }

  /**
   * 判断物品是否锁定
   * @param {Item} item 
   */
  isItemLock(item) {
    if (!item) return false;
    if (item?.isNull()) return false;
    const itemNbt = item.getNbt();
    const tag = itemNbt.getTag("tag");
    if (!tag) return false;// 不存在tag标签
    return Boolean(tag.getData('minecraft:item_lock'))
  }

  /**
   * 判断物品是否不会损坏
   * @param {Item} item 
   */
  isItemUnbreakable(item) {
    if (!item) return false;
    if (item?.isNull()) return false;
    const itemNbt = item.getNbt();
    const tag = itemNbt.getTag("tag");
    if (!tag) return false;// 不存在tag标签
    return Boolean(tag.getData('Unbreakable'))
  }

  /**
 * 通用排序函数（支持嵌套路径、中英文、数字、对象数组）
 * @param {Array} arr - 要排序的数组
 * @param {string} [order='asc'] - 排序方式：'asc'（升序）或 'desc'（降序）
 * @param {string} [key] - 排序字段（支持嵌套路径，如 'a.name'）
 * @returns {Array} - 排序后的数组
 */
  smartSort(arr, order = 'asc', key) {
    const sortedArray = [...arr];
    sortedArray.sort((a, b) => {
      // 获取嵌套字段的值（支持 'a.name' 这种路径）
      const getValue = (obj, path) => {
        return path.split('.').reduce((o, k) => (o ? o[k] : undefined), obj);
      };
      const valueA = key ? getValue(a, key) : a;
      const valueB = key ? getValue(b, key) : b;

      // 如果是字符串，用 localeCompare 排序（中文按拼音，英文按字母）
      if (typeof valueA === 'string' && typeof valueB === 'string') {
        return order === 'asc'
          ? valueA.localeCompare(valueB, 'zh-CN')
          : valueB.localeCompare(valueA, 'zh-CN');
      } else {
        // 数字或其他类型
        return order === 'asc' ? valueA - valueB : valueB - valueA;
      }
    });
    return sortedArray;
  }
}


module.exports = new TypeConverter;