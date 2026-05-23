#!/bin/bash
# ============================================================
#  サーバー統合テストランナー
#  実行: bash src/server/run_test.sh
#  出力先: prd/log/server.log
# ============================================================

set -u

# プロジェクトルートに移動 (このスクリプトは src/server/ にある想定)
cd "$(dirname "$0")/../.."

# ───────── 設定 ─────────
TEST_BIN=/tmp/server_test
CONF_FILE=conf/nginx.conf
LOG_FILE=log/server.log

PORTS=(4040 4041 4042)
WAIT_AFTER_BOOT=1
WAIT_AFTER_REQ=0.3
TIMEOUT_WAIT=7       # TIMEOUT_SEC + 2 程度

# 集計用
TOTAL=0
PASS=0
FAIL=0

# ───────── ビルド ─────────
echo "[ビルド] ソースをコンパイル中..."

c++ -Wall -Wextra -Werror -std=c++98 -DTEST_BUILD \
    src/server/_server_test.cpp \
    src/server/server_multi_io.cpp \
    src/server/client_state.cpp \
    src/persing/persing.cpp \
    src/persing/persing_conf.cpp \
    src/persing/persing_request.cpp \
    src/test/test_handler.cpp \
    -o "$TEST_BIN"

if [ $? -ne 0 ]; then
    echo "[エラー] ビルド失敗。コンパイルエラーを確認してください。"
    exit 1
fi

echo "[ビルド] 完了: $TEST_BIN"

# ───────── 既存サーバーの掃除 ─────────
pkill -f "$(basename $TEST_BIN)" 2>/dev/null
sleep 1

# ───────── 初期化 ─────────
mkdir -p log
> "$LOG_FILE"

cat >> "$LOG_FILE" << EOF
================================================================================
  サーバー統合テスト
================================================================================
  実行日時:   $(date '+%Y-%m-%d %H:%M:%S')
  設定ファイル: $CONF_FILE
  実行バイナリ: $TEST_BIN
  ポート:     ${PORTS[@]}
================================================================================

EOF

# ───────── サーバー起動 ─────────
echo "[起動] サーバーを起動しています..." | tee -a "$LOG_FILE"
"$TEST_BIN" "$CONF_FILE" >> "$LOG_FILE" 2>&1 &
SERVER_PID=$!
sleep $WAIT_AFTER_BOOT

# プロセス生きてるか確認
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "[エラー] サーバー起動失敗。ログを確認してください: $LOG_FILE"
    exit 1
fi

echo "[起動] サーバー PID = $SERVER_PID で起動完了" | tee -a "$LOG_FILE"

# ───────── テストヘルパー ─────────
# $1: テスト番号
# $2: 何をテストするか (日本語)
# $3: 期待される動作
# $4: 実行コマンド
# $5: 成功判定パターン (出力中に含まれてればPASS)
run_test() {
    local num="$1"
    local what="$2"
    local expected="$3"
    local cmd="$4"
    local pattern="$5"

    TOTAL=$((TOTAL + 1))

    cat >> "$LOG_FILE" << EOF


================================================================================
 テスト #${num}: ${what}
================================================================================
[内容]    ${what}
[期待値]  ${expected}
[コマンド] ${cmd}
--------------------------------------------------------------------------------
[サーバー側ログ + クライアント応答]
EOF

    local output
    output=$(eval "$cmd" 2>&1)
    echo "$output" >> "$LOG_FILE"

    # 成功判定
    if echo "$output" | grep -qE "$pattern"; then
        echo "[判定]    ✅ PASS" >> "$LOG_FILE"
        echo "  [PASS] #${num} ${what}"
        PASS=$((PASS + 1))
    else
        echo "[判定]    ❌ FAIL (期待パターン: $pattern)" >> "$LOG_FILE"
        echo "  [FAIL] #${num} ${what}"
        FAIL=$((FAIL + 1))
    fi

    sleep $WAIT_AFTER_REQ
}

echo ""
echo "============================================================"
echo " テスト開始"
echo "============================================================"

# ───────── テストケース ─────────
PORT=${PORTS[0]}

# 【1】正常系: GET
run_test "01" \
    "ルート(/)へのGETリクエスト" \
    "200 OK が返ってくる" \
    "curl -s -i http://localhost:$PORT/" \
    "HTTP/1.1 200"

# 【2】正常系: GET (パス指定)
run_test "02" \
    "/index.html へのGETリクエスト" \
    "200 OK + パース結果に target=/index.html" \
    "curl -s -i http://localhost:$PORT/index.html" \
    "HTTP/1.1 200"

# 【3】POST (body 付き)
run_test "03" \
    "POSTリクエスト(form-encoded body)" \
    "200 OK + body が正しくHandlerに渡る" \
    "curl -s -i -X POST -d 'name=hello&value=world' http://localhost:$PORT/upload" \
    "HTTP/1.1 200"

# 【4】POST (JSON body)
run_test "04" \
    "POSTリクエスト(JSON body)" \
    "200 OK + Content-Type: application/json が認識される" \
    "curl -s -i -X POST -H 'Content-Type: application/json' -d '{\"k\":\"v\"}' http://localhost:$PORT/api" \
    "HTTP/1.1 200"

# 【5】DELETE
run_test "05" \
    "DELETEリクエスト" \
    "200 OK + method=DELETE が認識される" \
    "curl -s -i -X DELETE http://localhost:$PORT/file" \
    "HTTP/1.1 200"

# 【6】並列リクエスト (多重化確認)
run_test "06" \
    "並列3クライアント (I/O多重化確認)" \
    "3つ全部に 200 OK が返る" \
    "(curl -s http://localhost:$PORT/p1 & curl -s http://localhost:$PORT/p2 & curl -s http://localhost:$PORT/p3 & wait)" \
    "received GET"

# 【7】巨大ヘッダー (DoS対策確認)
run_test "07" \
    "巨大ヘッダーを送信 (DoS対策)" \
    "431 Request Header Fields Too Large が返る" \
    "curl -s -i -H 'X-Huge: $(printf 'A%.0s' {1..10000})' http://localhost:$PORT/" \
    "HTTP/1.1 431"

# 【8】巨大Content-Length (DoS対策)
run_test "08" \
    "巨大Content-Length宣言 (DoS対策)" \
    "413 Payload Too Large が返る" \
    "curl -s -i -X POST -H 'Content-Length: 99999999' -H 'Content-Type: text/plain' --data-binary '' http://localhost:$PORT/" \
    "HTTP/1.1 413"

# 【9】不正リクエスト (400)
run_test "09" \
    "HTTPプロトコル違反のリクエスト" \
    "400 Bad Request が返る" \
    "printf 'INVALID_REQUEST\r\n\r\n' | nc -w 1 localhost $PORT" \
    "HTTP/1.1 400"

# 【10】タイムアウト
run_test "10" \
    "ヘッダー未完成のまま放置 (タイムアウト)" \
    "408 Request Timeout が返る (TIMEOUT_SEC秒後)" \
    "(printf 'GET / HTTP/1.1\r\nHost: localhost\r\n'; sleep $TIMEOUT_WAIT) | nc localhost $PORT" \
    "HTTP/1.1 408"

# 【11-13】複数ポート
for i in "${!PORTS[@]}"; do
    P=${PORTS[$i]}
    num=$((11 + i))
    run_test "$num" \
        "ポート $P での待ち受け確認" \
        "200 OK が返る (複数ポート対応)" \
        "curl -s -i http://localhost:$P/" \
        "HTTP/1.1 200"
done

# ─────────────────────────────────────────────────
# エッジケース
# ─────────────────────────────────────────────────

# 【14】HTTP/1.0送信
run_test "14" \
    "HTTP/1.0でリクエスト送信" \
    "505 HTTP Version Not Supported が返る" \
    "printf 'GET / HTTP/1.0\r\nHost: localhost\r\n\r\n' | nc -w 2 localhost $PORT" \
    "HTTP/1.1 505"

# 【15】Hostヘッダーなし
run_test "15" \
    "Hostヘッダーなしのリクエスト" \
    "400 Bad Request が返る (RFC 7230でHost必須)" \
    "printf 'GET / HTTP/1.1\r\nUser-Agent: test\r\n\r\n' | nc -w 2 localhost $PORT" \
    "HTTP/1.1 400"

# 【16】非対応メソッド (PATCH)
run_test "16" \
    "PATCHメソッド (非対応)" \
    "400 Bad Request が返る (GET/POST/DELETE以外)" \
    "printf 'PATCH / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc -w 2 localhost $PORT" \
    "HTTP/1.1 400"

# 【17】空ボディPOST
run_test "17" \
    "空ボディのPOST (Content-Length: 0)" \
    "200 OK が返る + body size: 0" \
    "curl -s -i -X POST -H 'Content-Length: 0' http://localhost:$PORT/upload" \
    "HTTP/1.1 200"

# 【18】GET にbodyあり
run_test "18" \
    "GETリクエストにbody付与" \
    "200 OK が返る (Content-Length分のbodyを読む)" \
    "curl -s -i -X GET -H 'Content-Length: 5' --data-binary 'hello' http://localhost:$PORT/" \
    "HTTP/1.1 200"

# 【19】不正なHTTPバージョン文字列
run_test "19" \
    "不正なHTTPバージョン文字列" \
    "400 か 505 のエラーが返る" \
    "printf 'GET / HTTP/9.9\r\nHost: localhost\r\n\r\n' | nc -w 2 localhost $PORT" \
    "HTTP/1.1 (400|505)"

# 【20】リクエストラインなし
run_test "20" \
    "リクエストラインがない不正リクエスト" \
    "400 Bad Request が返る" \
    "printf 'Host: localhost\r\n\r\n' | nc -w 2 localhost $PORT" \
    "HTTP/1.1 400"

# ───────── サーバー停止 & ビルドファイル削除 ─────────
echo ""
echo "[停止] サーバーを停止します..." | tee -a "$LOG_FILE"
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo "[掃除] ビルドファイルを削除します: $TEST_BIN" | tee -a "$LOG_FILE"
rm -f "$TEST_BIN"

# ───────── サマリー ─────────
SUMMARY=$(cat << EOF


================================================================================
  テスト結果サマリー
================================================================================
  実行日時:   $(date '+%Y-%m-%d %H:%M:%S')
  総テスト数: ${TOTAL}
  成功:       ${PASS} ✅
  失敗:       ${FAIL} ❌
================================================================================
EOF
)

echo "$SUMMARY" >> "$LOG_FILE"
echo ""
echo "============================================================"
echo " 結果"
echo "============================================================"
echo "  総テスト数: ${TOTAL}"
echo "  成功:       ${PASS}"
echo "  失敗:       ${FAIL}"
echo ""
echo "詳細ログ: $LOG_FILE"
echo ""

if [ $FAIL -eq 0 ]; then
    echo "🎉 すべてのテストが成功しました!"
    exit 0
else
    echo "⚠️  $FAIL 件のテストが失敗しました。ログを確認してください。"
    exit 1
fi
