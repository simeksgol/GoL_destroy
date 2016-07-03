gcc.exe destroy.c -o destroy256.exe -O3 -Wall -Wextra -Wno-unused-function -fno-tree-loop-distribute-patterns -march=haswell
gcc.exe destroy.c -o destroy128.exe -O3 -Wall -Wextra -Wno-unused-function -fno-tree-loop-distribute-patterns -march=core2 -mtune=sandybridge -D __NO_AVX2
