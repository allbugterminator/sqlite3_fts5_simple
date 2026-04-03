#!/bin/bash
set -e

echo "编译极简版 SQLite with 自定义中文分词器（修正版）..."
echo "=================================================="

# 检查必要文件
if [ ! -f "src/sqlite3.c" ]; then
    echo "错误: 找不到 src/sqlite3.c"
    echo "请从 https://sqlite.org/download.html 下载 SQLite 合并源码"
    echo "例如:"
    echo "  wget https://sqlite.org/2024/sqlite-amalgamation-3450100.zip"
    echo "  unzip sqlite-amalgamation-3450100.zip"
    echo "  mv sqlite-amalgamation-3450100/* src/"
    exit 1
fi

# 检查分词器源码
if [ ! -f "src/fts5_simple.c" ]; then
    echo "错误: 找不到 src/fts5_simple.c"
    echo "请确保 fts5_simple.c 在 src/ 目录中"
    exit 1
fi

# 创建目录
mkdir -p bin lib

# 编译配置
CC=gcc
CFLAGS="-O2 -fPIC -DSQLITE_CORE -DSQLITE_ENABLE_FTS5"
CFLAGS="$CFLAGS -DSQLITE_THREADSAFE=1 -DSQLITE_ENABLE_JSON1"
CFLAGS="$CFLAGS -I./src"

echo "1. 编译 SQLite 核心..."
$CC $CFLAGS -c src/sqlite3.c -o bin/sqlite3.o

echo "2. 编译极简分词器..."
$CC $CFLAGS -c src/fts5_simple.c -o bin/fts5_simple.o

echo "3. 创建静态库..."
ar rcs lib/libsqlite3_fts5_simple.a bin/sqlite3.o bin/fts5_simple.o

echo "4. 创建动态库..."
$CC -shared $CFLAGS -o lib/libsqlite3_fts5_simple.so bin/sqlite3.o bin/fts5_simple.o

echo "5. 编译测试程序..."
$CC $CFLAGS -o bin/test_simple test/test_simple.c \
    lib/libsqlite3_fts5_simple.a -lpthread -ldl

echo ""
echo "✅ 编译完成！"
echo "📁 生成的文件:"
ls -la lib/*.a lib/*.so 2>/dev/null || echo "   (库文件可能未生成)"
ls -la bin/test_simple 2>/dev/null || echo "   (测试程序可能未生成)"
echo ""
echo "运行测试: ./bin/test_simple"