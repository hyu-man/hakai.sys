# -*- coding: utf-8 -*-
"""
hakai.sys の商品画像（BOOTH / リポジトリ用アイコン、2026-10-24 ω）。
koe.sys のアイコン（茶色のドットの顔・クリーム地・ローズの枠）と同じ家族で、
この子は「こわれている」：顔の帯が2段ずれて（スタッターの輪切り）、
右へ欠片が飛び、片方の頬もずれて、口は欠けている。
手で置いた升目だけ。乱数なし。二度描けば同じ絵。512×512（16×16升を32倍）。
実行: python icon.py → icon.png
"""
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
BG = (247, 240, 228)
BORDER = (214, 112, 112)
FACE = (138, 58, 46)
PINK = (255, 107, 138)
G = 16; S = 32

# 16×16 の升目。'.'=地, '#'=顔, 'o'=クリーム(目・口), 'p'=頬, '*'=欠片(顔色), '+'=欠片(ピンク)
# koe.sys の顔の升目そのまま。壊しは輪切り2本：頬と口の帯が右へ2、その下の帯が左へ1。欠片が3つ飛ぶ。
ROWS = [
    "................",
    "................",
    ".............+..",
    "...#########....",
    "...#########..*.",
    "..###########...",
    "..##..###..##...",
    "..##..###..##...",
    "....#pp######pp.",
    "....#pp##..##pp.",
    ".#####..####....",
    "...#########....",
    "............*...",
    "................",
    "................",
    "................",
]
COL = {'.': BG, '#': FACE, 'o': (247, 240, 228), 'p': PINK, '*': FACE, '+': PINK}

img = Image.new('RGB', (G * S, G * S), BG)
px = img.load()
for gy, row in enumerate(ROWS):
    for gx, ch in enumerate(row):
        c = COL[ch]
        if ch == '.':
            continue
        for y in range(gy * S, gy * S + S):
            for x in range(gx * S, gx * S + S):
                px[x, y] = c
# 枠（koe.sys と同じ太さ 12px）
for t in range(12):
    for x in range(G * S):
        px[x, t] = BORDER; px[x, G * S - 1 - t] = BORDER
    for y in range(G * S):
        px[t, y] = BORDER; px[G * S - 1 - t, y] = BORDER

img.save(os.path.join(HERE, 'icon.png'))
print('saved', img.size)
