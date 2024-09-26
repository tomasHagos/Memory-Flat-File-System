CC = clang 
CCFLAGS = -Wall -DNDEBUG  #-g
all: a5_tests_mm a5_main  a5_imffs_tests
a5_main: a5_main.o a5_imffs.o a5_multimap.o 
a5_tests_mm: a5_tests.o a5_multimap.o a5_tests_mm.o
a5_imffs_tests: a5_imffs_tests.o a5_multimap.o a5_tests.o a5_imffs.o
a5_imffs_tests.o: a5_imffs_tests.c a5_imffs_helpers.h a5_imffs.h a5_multimap.h a5_tests.h
a5_imffs.o: a5_imffs.c a5_multimap.h a5_imffs.h
a5_tests_mm.o : a5_tests_mm.c a5_tests.h a5_multimap.h
a5_multimap.o: a5_multimap.c a5_multimap.h 
a5_main.o: a5_main.c a5_imffs.h
a5_tests.o: a5_tests.c a5_tests.h

clean:
	rm -f *.o a5_tests_mm a5_main a5_imffs_tests
