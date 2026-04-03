#include "fts5_simple.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * 由于 SQLite FTS5 的分词器接口比较特殊，我们直接使用 FTS5 提供的接口
 * 而不是传统的 sqlite3_tokenizer
 */

/* 简单的中文词典（可扩展）*/
static const char *g_szDict[] = {
    /* 常用中文词 */
    "中国", "国家", "人民", "政府", "发展", "经济", "社会", 
    "技术", "科学", "教育", "文化", "工作", "企业", "公司",
    "大学", "学生", "学校", "学习", "研究", "开发", "系统",
    "管理", "服务", "信息", "数据", "网络", "软件", "硬件",
    "程序", "代码", "算法", "计算", "智能", "人工", "机器",
    "语言", "中文", "英语", "编程", "编译", "运行", "测试",
    "项目", "产品", "市场", "用户", "客户", "需求", "设计",
    
    /* 单个中文字符（作为fallback）*/
    "我", "你", "他", "她", "它", "的", "是", "了", "在", 
    "和", "与", "及", "或", "不", "有", "没", "好", "坏",
    "大", "小", "多", "少", "上", "下", "左", "右", "中",
    
    /* 示例结束，实际可扩展更多词 */
};
static const int g_nDictSize = sizeof(g_szDict) / sizeof(g_szDict[0]);

/* 工具函数：UTF-8 字符长度 */
static int utf8_char_len(unsigned char c) {
    if (c < 0x80) return 1;          /* ASCII */
    else if ((c & 0xE0) == 0xC0) return 2;
    else if ((c & 0xF0) == 0xE0) return 3;
    else if ((c & 0xF8) == 0xF0) return 4;
    return 1;  /* 无效 UTF-8，按单字节处理 */
}

/* 工具函数：是否为中文字符（简单判断）*/
static int is_chinese_char(const unsigned char *p, int n) {
    if (n < 3) return 0;
    /* UTF-8 中文编码范围: E4 B8 80 - E9 BE A5 (粗略判断) */
    if (p[0] == 0xE4 && p[1] == 0xB8 && p[2] >= 0x80) return 1;
    if (p[0] >= 0xE5 && p[0] <= 0xE9) return 1;
    return 0;
}

/* 检查字符串是否在词典中 */
static int in_dict(const char *word, int len) {
    for (int i = 0; i < g_nDictSize; i++) {
        int dict_len = (int)strlen(g_szDict[i]);
        if (dict_len == len && strncmp(g_szDict[i], word, len) == 0) {
            return 1;
        }
    }
    return 0;
}

/* 分词器配置结构 */
typedef struct SimpleTokenizerConfig {
    int nMinToken;      /* 最小词长（字节数）*/
    int nMaxToken;      /* 最大词长（字节数）*/
    int bDiscardAscii;  /* 是否丢弃 ASCII 字符 */
} SimpleTokenizerConfig;

/* 分词器实例结构 */
typedef struct SimpleTokenizer {
    SimpleTokenizerConfig config; /* 配置 */
} SimpleTokenizer;

/* 分词器游标结构 */
typedef struct SimpleTokenizerCursor {
    const char *pInput;    /* 输入文本 */
    int nInput;            /* 输入长度 */
    int iOffset;           /* 当前位置 */
    int iToken;            /* Token 索引 */
} SimpleTokenizerCursor;

/*******************************
** 分词器核心实现
*******************************/

/*
** 创建分词器实例
*/
static int fts5SimpleCreate(
    void *pCtx,                 /* 上下文（未使用） */
    const char **azArg,         /* 参数数组 */
    int nArg,                   /* 参数数量 */
    Fts5Tokenizer **ppOut       /* 输出分词器 */
){
    SimpleTokenizer *pNew;
    
    (void)pCtx;  /* 未使用参数 */
    
    pNew = (SimpleTokenizer *)sqlite3_malloc(sizeof(SimpleTokenizer));
    if (pNew == NULL) return SQLITE_NOMEM;
    
    memset(pNew, 0, sizeof(SimpleTokenizer));
    
    /* 默认配置 */
    pNew->config.nMinToken = 3;  /* 最小3字节（一个中文字符）*/
    pNew->config.nMaxToken = 18; /* 最大18字节（6个中文字符）*/
    pNew->config.bDiscardAscii = 0;
    
    /* 解析配置参数 */
    for (int i = 0; i < nArg; i++) {
        if (strncmp(azArg[i], "min=", 4) == 0) {
            pNew->config.nMinToken = atoi(azArg[i] + 4);
        } else if (strncmp(azArg[i], "max=", 4) == 0) {
            pNew->config.nMaxToken = atoi(azArg[i] + 4);
        } else if (strcmp(azArg[i], "noascii") == 0) {
            pNew->config.bDiscardAscii = 1;
        }
    }
    
    *ppOut = (Fts5Tokenizer *)pNew;
    return SQLITE_OK;
}

/*
** 销毁分词器实例
*/
static void fts5SimpleDelete(Fts5Tokenizer *p) {
    sqlite3_free(p);
}

/*
** 分词函数 - 核心分词逻辑
*/
static int simpleTokenize(
    SimpleTokenizer *pTokenizer,  /* 分词器 */
    void *pCtx,                   /* 上下文 */
    int flags,                    /* 标志位 */
    const char *pText, int nText, /* 输入文本 */
    SimpleTokenizerCursor *pCursor, /* 游标 */
    int (*xToken)(                /* Token 回调 */
        void *pCtx, 
        int tflags,
        const char *pToken, 
        int nToken,
        int iStart, 
        int iEnd
    )
){
    int rc = SQLITE_OK;
    
    /* 跳过空白字符 */
    while (pCursor->iOffset < nText && isspace((unsigned char)pText[pCursor->iOffset])) {
        pCursor->iOffset++;
    }
    
    if (pCursor->iOffset >= nText) {
        return SQLITE_DONE;
    }
    
    int iStart = pCursor->iOffset;
    
    /* 处理 ASCII 字符 */
    if ((unsigned char)pText[pCursor->iOffset] < 0x80) {
        if (pTokenizer->config.bDiscardAscii) {
            /* 跳过 ASCII 字符 */
            while (pCursor->iOffset < nText && (unsigned char)pText[pCursor->iOffset] < 0x80) {
                pCursor->iOffset++;
            }
            /* 递归调用，跳过 ASCII 后继续处理 */
            return simpleTokenize(pTokenizer, pCtx, flags, pText, nText, pCursor, xToken);
        }
        
        /* 提取 ASCII 单词 */
        int start = pCursor->iOffset;
        while (pCursor->iOffset < nText && (unsigned char)pText[pCursor->iOffset] < 0x80 && 
               !isspace((unsigned char)pText[pCursor->iOffset])) {
            pCursor->iOffset++;
        }
        
        int iEnd = pCursor->iOffset;
        int nToken = iEnd - start;
        
        /* 调用回调函数返回 token */
        rc = xToken(pCtx, 0, pText + start, nToken, start, iEnd);
        
        return rc;
    }
    
    /* 处理中文文本：最大正向匹配 */
    int max_len = pTokenizer->config.nMaxToken;
    int remaining = nText - pCursor->iOffset;
    if (max_len > remaining) max_len = remaining;
    
    /* 确保 max_len 是完整字符的边界 */
    while (max_len > 0 && (pText[pCursor->iOffset + max_len] & 0xC0) == 0x80) {
        max_len--;
    }
    
    /* 从最大长度开始尝试匹配 */
    int found_len = 0;
    for (int try_len = max_len; try_len >= pTokenizer->config.nMinToken; ) {
        /* 检查是否是词典中的词 */
        if (in_dict(pText + pCursor->iOffset, try_len)) {
            found_len = try_len;
            break;
        }
        
        /* 减少一个完整字符的长度 */
        int char_len = utf8_char_len((unsigned char)pText[pCursor->iOffset + try_len - 1]);
        try_len -= char_len;
        
        /* 调整到字符边界 */
        while (try_len > 0 && (pText[pCursor->iOffset + try_len] & 0xC0) == 0x80) {
            try_len--;
        }
    }
    
    /* 如果找到匹配，返回该词 */
    if (found_len > 0) {
        int iEnd = pCursor->iOffset + found_len;
        rc = xToken(pCtx, 0, pText + pCursor->iOffset, found_len, pCursor->iOffset, iEnd);
        pCursor->iOffset += found_len;
        return rc;
    }
    
    /* 未找到匹配，按单个字符处理 */
    int char_len = utf8_char_len((unsigned char)pText[pCursor->iOffset]);
    if (char_len > remaining) char_len = remaining;
    
    int iEnd = pCursor->iOffset + char_len;
    rc = xToken(pCtx, 0, pText + pCursor->iOffset, char_len, pCursor->iOffset, iEnd);
    pCursor->iOffset += char_len;
    
    return rc;
}

/*
** FTS5 分词函数
*/
static int fts5SimpleTokenize(
    Fts5Tokenizer *pTokenizer,  /* 分词器 */
    void *pCtx,                 /* 上下文 */
    int flags,                  /* 标志位 */
    const char *pText, int nText, /* 输入文本 */
    int (*xToken)(              /* Token 回调 */
        void *pCtx, 
        int tflags,
        const char *pToken, 
        int nToken,
        int iStart, 
        int iEnd
    )
){
    SimpleTokenizer *p = (SimpleTokenizer *)pTokenizer;
    SimpleTokenizerCursor cursor;
    int rc = SQLITE_OK;
    
    memset(&cursor, 0, sizeof(cursor));
    cursor.pInput = pText;
    cursor.nInput = nText;
    cursor.iOffset = 0;
    cursor.iToken = 0;
    
    /* 循环分词直到处理完所有文本 */
    while (cursor.iOffset < nText && rc == SQLITE_OK) {
        rc = simpleTokenize(p, pCtx, flags, pText, nText, &cursor, xToken);
        if (rc == SQLITE_DONE) {
            rc = SQLITE_OK;
            break;
        }
    }
    
    return rc;
}

/*
** FTS5 分词器接口结构
*/
static const fts5_tokenizer fts5SimpleTokenizer = {
    fts5SimpleCreate,
    fts5SimpleDelete,
    fts5SimpleTokenize
};

/*******************************
** 扩展初始化函数
*******************************/

/*
** 注册 Simple 分词器到 FTS5
*/
int sqlite3Fts5SimpleInit(
    sqlite3 *db, 
    char **pzErrMsg, 
    const sqlite3_api_routines *pApi
){
    SQLITE_EXTENSION_INIT2(pApi);
    int rc = SQLITE_OK;
    fts5_api *pFts5 = NULL;
    
    (void)pApi;  /* 未使用参数 */
    
    /* 检查 FTS5 是否可用，并获取 FTS5 API */
    sqlite3_stmt *pStmt = NULL;
    rc = sqlite3_prepare_v2(db, "SELECT fts5()", -1, &pStmt, 0);
    if (rc != SQLITE_OK) {
        if (pzErrMsg) {
            *pzErrMsg = sqlite3_mprintf("FTS5 not available");
        }
        return rc;
    }
    
    /* 从 sqlite3_stmt 中提取 FTS5 API */
    pFts5 = (fts5_api *)sqlite3_column_blob(pStmt, 0);
    sqlite3_finalize(pStmt);
    
    if (pFts5 == NULL) {
        if (pzErrMsg) {
            *pzErrMsg = sqlite3_mprintf("FTS5 API not found");
        }
        return SQLITE_ERROR;
    }
    
    rc = pFts5->xCreateTokenizer(pFts5,
        "simple",               /* 分词器名称 */
        (void *)db,             /* 上下文 */
        (fts5_tokenizer *)&fts5SimpleTokenizer, /* 分词器接口 */
        0                       /* 析构函数 */
    );
    
    if (rc != SQLITE_OK && pzErrMsg) {
        *pzErrMsg = sqlite3_mprintf("Failed to register simple tokenizer");
    }
    
    return rc;
}