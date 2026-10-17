-- ============================================================
-- AIPet 桌宠 数据导出 SQL 脚本（SQLite）
-- 来源：%APPDATA%\AIPet\AIPet1.ini
-- 注意：本脚本为结构示例，API Key 已脱敏（api/key 为占位符）；
--       使用时请将 api/key 替换为你自己的 Key，勿提交真实密钥。
-- 生成时间：2026-10-16
-- ============================================================

PRAGMA foreign_keys = OFF;
BEGIN TRANSACTION;

-- 1. 应用配置
CREATE TABLE IF NOT EXISTS app_config (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
INSERT OR REPLACE INTO app_config (key, value) VALUES ('api/key', 'sk-REPLACE_WITH_YOUR_OWN_KEY');

-- 2. 任务清单
CREATE TABLE IF NOT EXISTS tasks (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    content TEXT NOT NULL,
    done    INTEGER NOT NULL DEFAULT 0
);
INSERT INTO tasks (id, content, done) VALUES (1, '明天要收衣服', 1);
INSERT INTO tasks (id, content, done) VALUES (2, '明天努力学习', 1);

-- 3. 课表
CREATE TABLE IF NOT EXISTS schedule (
    id   INTEGER PRIMARY KEY AUTOINCREMENT,
    item TEXT NOT NULL
);

-- 4. 专注时长按日统计
CREATE TABLE IF NOT EXISTS focus_daily (
    date    TEXT PRIMARY KEY,   -- yyyyMMdd
    seconds INTEGER NOT NULL DEFAULT 0
);
INSERT OR REPLACE INTO focus_daily (date, seconds) VALUES ('20261015', 125);

-- 5. 应用状态（今日专注累计等）
CREATE TABLE IF NOT EXISTS app_state (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
INSERT OR REPLACE INTO app_state (key, value) VALUES ('focus/date', '20261015');
INSERT OR REPLACE INTO app_state (key, value) VALUES ('focus/todaySeconds', '125');

COMMIT;
