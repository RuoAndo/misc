# around-AFL

<img src="afl.png">

# CVE-2014-9330

bmp2tiff(画像変換アプリ）の脆弱性をAFLでファジングし、bmp2tiffをクラッシュさせる57個の画像を生成して入力してみる。

<pre>
# wget http://download.osgeo.org/libtiff/old/tiff-3.8.2.tar.gz
# tar xvzf tiff-3.8.2.tar.gz
# cd tiff-3.8.2
# export CC=afl-gcc
# export CXX=afl-g++
# ./configure --disable-shared
# make
# ls ./tools
# cp ./tools/bmp2tiff .
</pre>

<pre>
# ./gen.sh list-crash 2>&1 | tee output
</pre>

<pre>
[0]
./gen.sh: line 8: 16758 Segmentation fault      ./bmp2tiff 0.bmp out.tiff
[1]
./gen.sh: line 8: 16762 Segmentation fault      ./bmp2tiff 1.bmp out.tiff
[2]
./gen.sh: line 8: 16766 Segmentation fault      ./bmp2tiff 2.bmp out.tiff
[3]
./gen.sh: line 8: 16770 Segmentation fault      ./bmp2tiff 3.bmp out.tiff
[4]
./gen.sh: line 8: 16774 Segmentation fault      ./bmp2tiff 4.bmp out.tiff
[5]
./gen.sh: line 8: 16778 Segmentation fault      ./bmp2tiff 5.bmp out.tiff
[6]
./gen.sh: line 8: 16782 Segmentation fault      ./bmp2tiff 6.bmp out.tiff
[7]
./gen.sh: line 8: 16786 Segmentation fault      ./bmp2tiff 7.bmp out.tiff
[8]
./gen.sh: line 8: 16790 Segmentation fault      ./bmp2tiff 8.bmp out.tiff
[9]
./gen.sh: line 8: 16794 Segmentation fault      ./bmp2tiff 9.bmp out.tiff
[10]
./gen.sh: line 8: 16798 Segmentation fault      ./bmp2tiff 10.bmp out.tiff
[11]
./gen.sh: line 8: 16802 Segmentation fault      ./bmp2tiff 11.bmp out.tiff
[12]
./gen.sh: line 8: 16806 Segmentation fault      ./bmp2tiff 12.bmp out.tiff
[13]
./gen.sh: line 8: 16810 Segmentation fault      ./bmp2tiff 13.bmp out.tiff
[14]
./gen.sh: line 8: 16814 Segmentation fault      ./bmp2tiff 14.bmp out.tiff
[15]
./gen.sh: line 8: 16818 Segmentation fault      ./bmp2tiff 15.bmp out.tiff
[16]
free(): invalid pointer
./gen.sh: line 8: 16822 Aborted                 ./bmp2tiff 16.bmp out.tiff
[17]
free(): invalid pointer
./gen.sh: line 8: 16826 Aborted                 ./bmp2tiff 17.bmp out.tiff
[18]
free(): invalid pointer
./gen.sh: line 8: 16830 Aborted                 ./bmp2tiff 18.bmp out.tiff
[19]
./gen.sh: line 8: 16834 Segmentation fault      ./bmp2tiff 19.bmp out.tiff
[20]
./gen.sh: line 8: 16838 Segmentation fault      ./bmp2tiff 20.bmp out.tiff
[21]
./gen.sh: line 8: 16842 Segmentation fault      ./bmp2tiff 21.bmp out.tiff
[22]
./gen.sh: line 8: 16846 Segmentation fault      ./bmp2tiff 22.bmp out.tiff
[23]
./gen.sh: line 8: 16850 Segmentation fault      ./bmp2tiff 23.bmp out.tiff
[24]
./gen.sh: line 8: 16854 Segmentation fault      ./bmp2tiff 24.bmp out.tiff
[25]
./gen.sh: line 8: 16858 Segmentation fault      ./bmp2tiff 25.bmp out.tiff
[26]
./gen.sh: line 8: 16862 Segmentation fault      ./bmp2tiff 26.bmp out.tiff
[27]
./gen.sh: line 8: 16866 Segmentation fault      ./bmp2tiff 27.bmp out.tiff
[28]
./gen.sh: line 8: 16870 Segmentation fault      ./bmp2tiff 28.bmp out.tiff
[29]
./gen.sh: line 8: 16874 Segmentation fault      ./bmp2tiff 29.bmp out.tiff
[30]
./gen.sh: line 8: 16878 Segmentation fault      ./bmp2tiff 30.bmp out.tiff
[31]
free(): invalid pointer
./gen.sh: line 8: 16882 Aborted                 ./bmp2tiff 31.bmp out.tiff
[32]
./gen.sh: line 8: 16886 Segmentation fault      ./bmp2tiff 32.bmp out.tiff
[33]
./gen.sh: line 8: 16890 Segmentation fault      ./bmp2tiff 33.bmp out.tiff
[34]
./gen.sh: line 8: 16894 Segmentation fault      ./bmp2tiff 34.bmp out.tiff
[35]
./gen.sh: line 8: 16898 Segmentation fault      ./bmp2tiff 35.bmp out.tiff
[36]
./gen.sh: line 8: 16902 Segmentation fault      ./bmp2tiff 36.bmp out.tiff
[37]
./gen.sh: line 8: 16906 Segmentation fault      ./bmp2tiff 37.bmp out.tiff
[38]
./gen.sh: line 8: 16910 Segmentation fault      ./bmp2tiff 38.bmp out.tiff
[39]
./gen.sh: line 8: 16914 Segmentation fault      ./bmp2tiff 39.bmp out.tiff
[40]
./gen.sh: line 8: 16918 Segmentation fault      ./bmp2tiff 40.bmp out.tiff
[41]
./gen.sh: line 8: 16922 Segmentation fault      ./bmp2tiff 41.bmp out.tiff
[42]
./gen.sh: line 8: 16926 Segmentation fault      ./bmp2tiff 42.bmp out.tiff
[43]
./gen.sh: line 8: 16930 Segmentation fault      ./bmp2tiff 43.bmp out.tiff
[44]
free(): invalid pointer
./gen.sh: line 8: 16934 Aborted                 ./bmp2tiff 44.bmp out.tiff
[45]
./gen.sh: line 8: 16938 Segmentation fault      ./bmp2tiff 45.bmp out.tiff
[46]
./gen.sh: line 8: 16942 Segmentation fault      ./bmp2tiff 46.bmp out.tiff
[47]
free(): invalid pointer
./gen.sh: line 8: 16946 Aborted                 ./bmp2tiff 47.bmp out.tiff
[48]
free(): invalid pointer
./gen.sh: line 8: 16950 Aborted                 ./bmp2tiff 48.bmp out.tiff
[49]
./gen.sh: line 8: 16954 Segmentation fault      ./bmp2tiff 49.bmp out.tiff
[50]
./gen.sh: line 8: 16958 Segmentation fault      ./bmp2tiff 50.bmp out.tiff
[51]
./gen.sh: line 8: 16962 Segmentation fault      ./bmp2tiff 51.bmp out.tiff
[52]
./gen.sh: line 8: 16966 Segmentation fault      ./bmp2tiff 52.bmp out.tiff
[53]
./gen.sh: line 8: 16970 Segmentation fault      ./bmp2tiff 53.bmp out.tiff
[54]
free(): invalid pointer
./gen.sh: line 8: 16974 Aborted                 ./bmp2tiff 54.bmp out.tiff
[55]
free(): invalid pointer
./gen.sh: line 8: 16978 Aborted                 ./bmp2tiff 55.bmp out.tiff
[56]
free(): invalid pointer
./gen.sh: line 8: 16982 Aborted                 ./bmp2tiff 56.bmp out.tiff
[57]
free(): invalid pointer
./gen.sh: line 8: 16986 Aborted                 ./bmp2tiff 57.bmp out.tiff

</pre>
