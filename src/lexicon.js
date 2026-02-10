/**
 * 作者：Yoyo
 * 违禁词库AC自动机
 * QQ: 1294858802
 */
module.exports = class Lexicon {
  constructor(words) {
    this.words = new Set();
    this.trie = { children: {}, fail: null, output: [] };
    this.add(...words);
  }

  // 添加新词条到违禁词库
  add(...words) {
    let rebuildNeeded = false;
    for (const word of words) {
      if (!this.words.has(word)) {
        this.words.add(word);
        rebuildNeeded = true;
      }
    }
    if (rebuildNeeded) {
      this._buildTrie();
    }
  }

  // 从违禁词库移除词条
  remove(...words) {
    let rebuildNeeded = false;
    for (const word of words) {
      if (this.words.has(word)) {
        this.words.delete(word);
        rebuildNeeded = true;
      }
    }
    if (rebuildNeeded) {
      this._buildTrie();
    }
  }

  // 检测并替换文本中的违禁词
  check(text, replaceStr = '***') {
    const matches = this._findMatches(text);
    const intervals = this._mergeIntervals(matches);
    const replacedText = this._replaceIntervals(text, intervals, replaceStr);

    return {
      words: [...new Set(matches.map(match => match.pattern))],
      text: replacedText
    };
  }

  // 构建AC自动机核心结构
  _buildTrie() {
    // 初始化根节点
    this.trie = { children: {}, fail: null, output: [] };

    // 构建基础Trie树
    for (const word of this.words) {
      let node = this.trie;
      for (const char of word) {
        if (!node.children[char]) {
          node.children[char] = { children: {}, fail: null, output: [] };
        }
        node = node.children[char];
      }
      node.output.push(word);
    }

    // 使用BFS构建fail指针
    const queue = [];
    this.trie.fail = this.trie;  // 根节点的fail指向自己

    // 第一层节点的fail指向根节点
    for (const [char, child] of Object.entries(this.trie.children)) {
      child.fail = this.trie;
      queue.push(child);
    }

    while (queue.length) {
      const current = queue.shift();

      for (const [char, child] of Object.entries(current.children)) {
        let failNode = current.fail;

        // 沿着fail链查找匹配的子节点
        while (failNode !== this.trie && !failNode.children[char]) {
          failNode = failNode.fail;
        }

        // 设置fail指针并合并output
        child.fail = failNode.children[char] || this.trie;
        child.output = [...child.output, ...(child.fail.output || [])];
        queue.push(child);
      }
    }
  }

  // 识别文本中的违禁词（含变体）
  _findMatches(text) {
    const matches = [];
    let state = this.trie;
    let cleanIndex = 0;
    const positions = [];

    for (let i = 0; i < text.length; i++) {
      const char = text[i];

      // 跳过非中文字符（保留位置信息）
      if (!this._isChinese(char)) {
        continue;
      }

      positions[cleanIndex] = i;

      // 状态转移（沿fail链回溯）
      while (state !== this.trie && !state.children[char]) {
        state = state.fail;
      }

      if (state.children[char]) {
        state = state.children[char];
      } else {
        state = this.trie;
      }

      // 收集匹配结果
      if (state.output.length > 0) {
        for (const pattern of state.output) {
          const start = positions[cleanIndex - pattern.length + 1];
          matches.push({
            pattern,
            start,
            end: i
          });
        }
      }

      cleanIndex++;
    }

    return matches;
  }

  // 合并重叠的违禁词区间
  _mergeIntervals(matches) {
    if (matches.length === 0) return [];

    // 按起始位置排序
    matches.sort((a, b) => a.start - b.start);

    const merged = [matches[0]];

    for (let i = 1; i < matches.length; i++) {
      const last = merged[merged.length - 1];
      const current = matches[i];

      if (current.start <= last.end) {
        // 扩展区间覆盖范围
        last.end = Math.max(last.end, current.end);
      } else {
        merged.push(current);
      }
    }

    // 按结束位置降序排列（便于后续替换）
    return merged.sort((a, b) => b.start - a.start);
  }

  // 执行违禁词替换
  _replaceIntervals(text, intervals, replaceStr) {
    let result = text;

    for (const interval of intervals) {
      const before = result.substring(0, interval.start);
      const after = result.substring(interval.end + 1);
      result = before + replaceStr + after;
    }

    return result;
  }

  // 中文字符检测
  _isChinese(char) {
    return /[\u4e00-\u9fa5]/.test(char);
  }


  /**
   * 分隔字符串成数组
   * @param {string} str 待分割的字符串
   * @param {string} [separator=','] 分割符
   * @returns {Array<string>}
   */
  static fastSplit(str,separator = ',') {
    let start = 0, index, result = [];
    while ((index = str.indexOf(separator, start)) !== -1) {
      // 直接截取并跳过空白，避免生成中间数组
      let item = str.slice(start, index).trim();
      if (item) result.push(item);
      start = index + 1;
    }
    // 处理最后一个元素
    let last = str.slice(start).trim();
    if (last) result.push(last);
    return result;
  }
}

// // 使用示例
// const le = new Lexicon(['傻逼','狗日的']);
// le.add('暴力');  // 动态添加
// le.remove('暴力');  // 动态移除

// const testText = '你是傻逼，玩意狗日的，干傻1122逼的事情，狗11日asdsad的吊毛，傻 逼东西';
// const result = le.check(testText, '***');

// console.log(result.words); // ["傻逼", "狗日的"]
// console.log(result.text);
// // "你是***，玩意***，干***的事情，***吊毛，***东西"