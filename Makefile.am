## This is the shared library to be built                                       
lib_LTLIBRARIES = libmodwiki.la

## Define the source file for the module
libmodwiki_la_SOURCES = mod_wiki.c
libmodwiki_la_CFLAGS = -Wall -DAPACHE_DIR=\"@apache_dir@\"
libmodwiki_la_LDFLAGS = -L@discount_dir@/lib -lmarkdown -L@libgit2_dir@/lib -Wl,-rpath,@libgit2_dir@/lib -lgit2 -lz -lcrypto

## Define that an include directory is required.
INCLUDES = -I@apache_dir@/include -I@discount_dir@/include -I@libgit2_dir@/include -D_LARGEFILE64_SOURCE
#DEFS =

install: .libs/libmodwiki.so
	install $< @apache_dir@/modules/mod_wiki.so

