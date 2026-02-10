// yoyoSqlite.d.ts



// 新建字段的类型
interface tableObjectInfo {
  /** 字段类型 */
  type?: string;
  /** 默认值 */
  default?: string;
  /** 是否保持唯一值 */
  unique?: boolean;
  /** 是否为主键 */
  key?: boolean;
  /** 是否自增 */
  auto?: boolean;
}

/**
 * 数据库连接配置
 */
type dbParams = {
  /** 数据库文件路径 */
  path: string,
  /** 不存在是否自动创建 */
  create: boolean,
  /** 以读写模式打开 */
  readwrite: boolean
}


type tableObject = {
  [key: string]: any
} | Array<any[] | { [key: string]: any }>;


declare class sqlite {

  /** 表的前缀 */
  static table_prefix: string;
  /** 是否打印显示生成sql */
  static isShowSql: boolean;

  /**
   * 连接数据库并配置
   * @param {string|dbParams} 数据库路径或配置
   */
  static connectDb(dbPath: string | dbParams): boolean;
  static execute(sql: string, bind?: Array<number | string>): (0 | { insertId: number, affectedRows: number });
  static query(sql: string, bind?: Array<number | string>): { data: Array<any>, field: Array<string>, total: number, get: (index: number, field?: string) => any };
  static createTable(table: string, fields: { [key: string]: tableObjectInfo | string }): boolean;
  /**
   * 选中表名(每条操作的起步)
   * @param table 需要操作的表名(完整 表名)
   */
  static table(table: string): sqlite;
  /**
   * 判断表是否存在
   * @param table 表名
   */
  static isTable(table: string): boolean;
  /**
   * 选中表名(每条操作的起步)(自动组合前缀)
   * @param table 需要操作的表名(会组合前缀)
   */
  name(table: string): sqlite;
  /**
   * 添加条件(字符串会自动加单引号)
   * @param args 
   */
  where(...args: tableObject[]): sqlite;
  whereRaw(str: string, bind?: Array<number | string>): sqlite;
  /**
   * 设置字段
   * @param {Array|string} field 字段数组或者字符串
   */
  field(field: Array<string> | string): sqlite;
  /**
   * 给表取别名 就是 as
   * @param name 别名
   */
  alias(name: string): sqlite;
  /**
    * 限制条数
    * @param  {Number} 起始位置  
    * @param  ?{Number} 显示的条数
    */
  limit(limit: number, offset?: number): sqlite;
  /**
   * 排序
   * @param field 字段名
   * @param type asc|desc
   */
  order(field: string, type?: ('asc' | 'desc')): sqlite;
  /**
   * 分组
   * @param {string} fields 字段多个逗号
   */
  group(fields: string): sqlite;
  having(str: string): sqlite;
  /**
   * 多表联查[等值查询](INNER JOIN)
   * @param table 表名
   * @param where 条件
   */
  join(table: string, where: string): sqlite;
  /**
   * 多表联查[左查询](LEFT JOIN)
   * @param table 表
   * @param where 条件
   */
  leftJoin(table: string, where: string): sqlite;
  /**
   * 多表联查[右查询](RIGHT JOIN)
   * @param table 表
   * @param where 条件
   */
  rightJoin(table: string, where: string): sqlite;
  /**
   * 多表联查[一个匹配填充null返回](FULL JOIN)
   * @param table 表
   * @param where 条件
   */
  fullJoin(table: string, where: string): sqlite;
  /**
   * 添加数据
   * @param {object|Array} data 数据(单条或者多条)
   */
  insert(data: object | Array<object>): (0 | { insertId: number, affectedRows: number });
  /**
   * 更新数据
   * @param data 对象数据
   */
  update(data: { [key: string]: any }); olean;
  /**
   * 删除指定条件的数据
   */
  delete(): boolean;
  /**
   * 查询单个数据
   */
  find(): { [key: string]: any };
  /**
   * 查询多条数据
   */
  select(): { data: Array<any>, field: Array<string>, total: number, get: (index: number, field?: string) => any };
  pages(pages: number, limit?: number): any;
  count(field?: string): number;
  max(field: string): number;
  min(field: string): number;
  avg(field: string): number;
  sum(field: string): number;

}



export const sqlite;