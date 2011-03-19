#/bin/sh
valgrind --suppressions=tools/valgrind/memcheck/suppressions.txt --leak-check=full --track-origins=yes --num-callers=30 --log-file=valgrind-%p.log "$@"
