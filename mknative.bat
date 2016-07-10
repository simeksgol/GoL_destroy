gcc destroy.c -lm -o destroy128.exe -std=c99 -O3 -Wall -Wextra -Wno-unused-function -Werror -fno-tree-loop-distribute-patterns -march=native -D __NO_AVX2
gcc destroy.c -lm -o destroy256.exe -std=c99 -O3 -Wall -Wextra -Wno-unused-function -Werror -fno-tree-loop-distribute-patterns -march=native
