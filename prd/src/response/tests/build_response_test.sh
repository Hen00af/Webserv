#!/bin/sh
set -e

cd "$(dirname "$0")"

# NOTE: ../../persing/persing_conf.{cpp,hpp} は parseConf/ にリネーム移行中で未完。
# 動作確認には parseConf/parseConf.cpp 等への置換が別途必要。
c++ -Wall -Wextra -std=c++98 \
    response_test.cpp \
    ../builder.cpp \
    ../builder_util.cpp \
    ../serializer.cpp \
    ../../parseConf/parseConf.cpp \
    ../../parseConf/parseConfUtil.cpp \
    -o response_test

./response_test
