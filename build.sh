#!/bin/bash
clang -O3 main.c -static -o wdd_converter && strip wdd_converter
echo "Download Themes From https://windd.info/themes/free.html"
./wdd_converter
