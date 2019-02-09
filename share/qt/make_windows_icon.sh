#!/bin/bash
# create multiresolution windows icon
#mainnet
ICON_SRC=../../src/qt/res/icons/pivx.png
ICON_DST=../../src/qt/res/icons/pivx.ico
convert ${ICON_SRC} -resize 16x16 pivx-16.png
convert ${ICON_SRC} -resize 32x32 pivx-32.png
convert ${ICON_SRC} -resize 48x48 pivx-48.png
convert pivx-16.png pivx-32.png pivx-48.png ${ICON_DST}
#testnet
ICON_SRC=../../src/qt/res/icons/pivx_testnet.png
ICON_DST=../../src/qt/res/icons/pivx_testnet.ico
convert ${ICON_SRC} -resize 16x16 pivx-16.png
convert ${ICON_SRC} -resize 32x32 pivx-32.png
convert ${ICON_SRC} -resize 48x48 pivx-48.png
convert pivx-16.png pivx-32.png pivx-48.png ${ICON_DST}
rm pivx-16.png pivx-32.png pivx-48.png
#regtest
ICON_SRC=../../src/qt/res/icons/pivx_regtest.png
ICON_DST=../../src/qt/res/icons/pivx_regtest.ico
convert ${ICON_SRC} -resize 16x16 pivx-16.png
convert ${ICON_SRC} -resize 32x32 pivx-32.png
convert ${ICON_SRC} -resize 48x48 pivx-48.png
convert bitcoin-16.png bitcoin-32.png pivx-48.png ${ICON_DST}
rm pivx-16.png pivx-32.png pivx-48.png