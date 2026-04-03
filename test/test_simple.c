#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite3.h"
#include "fts5_simple.h"

/* 自定义分词器初始化函数声明 */
int sqlite3Fts5SimpleInit(sqlite3 *db, char **pzErrMsg, 
                          const sqlite3_api_routines *pApi);

/* 简单的测试程序 */
int main() {
    sqlite3 *db = NULL;
    char *err_msg = NULL;
    int rc;
    
    printf("测试极简中文分词器 - SQLite FTS5\n");
    printf("===================================\n\n");
    fflush(stdout);
    
    /* 1. 打开磁盘数据库 */
    rc = sqlite3_open("test.db", &db);
    printf("sqlite3_open returned: %d\n", rc);
    fflush(stdout);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    printf("打开数据库成功\n");
    fflush(stdout);
    
    /* 2. 启用扩展加载 */
    rc = sqlite3_enable_load_extension(db, 1);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "无法启用扩展加载\n");
        fflush(stderr);
    } else {
        printf("扩展加载已启用\n");
        fflush(stdout);
    }
    
    /* 3. 直接调用分词器初始化函数 */
    rc = sqlite3Fts5SimpleInit(db, &err_msg, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "加载分词器失败: %s\n", err_msg);
        if (err_msg) sqlite3_free(err_msg);
    } else {
        printf("加载极简中文分词器成功\n");
    }
    
    /* 4. 创建使用自定义分词器的 FTS5 表 */
    const char *create_table = 
        "CREATE VIRTUAL TABLE IF NOT EXISTS documents USING fts5("
        "  title, "
        "  content, "
        "  tokenize = 'simple'"
        ");";
    
    rc = sqlite3_exec(db, create_table, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "创建 FTS5 表失败: %s\n", err_msg);
        if (err_msg) sqlite3_free(err_msg);
    } else {
        printf("创建 FTS5 虚拟表成功\n");
    }
    
    /* 5. 插入中文测试数据 */
    const char *insert_data[] = {
        "INSERT INTO documents(title, content) VALUES ('人工智能', '人工智能是计算机科学的一个分支')",
        "INSERT INTO documents(title, content) VALUES ('机器学习', '机器学习是一门人工智能的科学')",
        "INSERT INTO documents(title, content) VALUES ('深度学习', '深度学习是机器学习领域中新的研究方向')",
        NULL
    };
    
    for (int i = 0; insert_data[i] != NULL; i++) {
        rc = sqlite3_exec(db, insert_data[i], 0, 0, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "插入数据失败: %s\n", err_msg);
            if (err_msg) sqlite3_free(err_msg);
        } else {
            printf("插入数据: %s\n", strstr(insert_data[i], "VALUES") + 7);
        }
    }
    
    /* 6. 测试搜索 */
    printf("\n测试中文搜索：\n");
    printf("================\n");
    
    sqlite3_stmt *stmt;
    
    /* 搜索单个词 */
    const char *search = "SELECT title FROM documents WHERE documents MATCH '人工'";
    rc = sqlite3_prepare_v2(db, search, -1, &stmt, 0);
    if (rc == SQLITE_OK) {
        printf("搜索 '人工' 的结果:\n");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *title = (const char *)sqlite3_column_text(stmt, 0);
            printf("  - %s\n", title);
        }
        sqlite3_finalize(stmt);
    } else {
        fprintf(stderr, "搜索失败: %s\n", sqlite3_errmsg(db));
    }
    
    /* 清理 */
    sqlite3_close(db);
    
    printf("\n测试完成！\n");
    
    fflush(stdout);
    return 0;
}
