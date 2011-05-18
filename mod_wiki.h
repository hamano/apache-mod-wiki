/*
 * mod_wiki.c - Git based wiki system for Apache module
 * Copyright (C) 2011 Tsukasa Hamano <code@cuspy.org>
 */
#define WIKI_ERROR -1
#define WIKI_NOTFOUND 0
#define WIKI_FOUND 1
#define WIKI_DIR 2
#define WIKI_MARKDOWN 3

typedef struct {
    const void *data;
    struct list_t *next;
} list_t;

typedef struct {
    const char *repo;
    const char *name;
    const char *basepath;
    list_t *css;
    const char *header;
    const char *footer;
} wiki_conf;

#define P(s) ap_rputs(s, r)
