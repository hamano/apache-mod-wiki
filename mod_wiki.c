/*
**  mod_wiki.c -- Git-based wiki for Apache module
**  [Autogenerated via ``apxs -n wiki -g'']
**
**  To play with this sample module first compile it into a
**  DSO file and install it into Apache's modules directory
**  by running:
**
**    $ apxs -c -i mod_wiki.c
**
**  Then activate it in Apache's httpd.conf file for instance
**  for the URL /wiki in as follows:
**
**    #   httpd.conf
**    LoadModule wiki_module modules/mod_wiki.so
**    <Location />
**      SetHandler wiki
**      WikiRepository /path/to/repository.git
**      WikiName "My Wiki"
**      WikiCss style.css
**    </Location>
**
**  Then after restarting Apache via
**
**    $ apachectl restart
**
**  you immediately can request the URL /wiki and watch for the
**  output of this module. This can be achieved for instance via:
**
**    $ lynx -mime_header http://localhost/
**
**  The output should be similar to the following one:
**
**    HTTP/1.1 200 OK
**    Date: Tue, 31 Mar 1998 14:42:22 GMT
**    Server: Apache/1.3.4 (Unix)
**    Connection: close
**    Content-Type: text/html
**
**    The sample page from mod_wiki.c
*/

#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "http_log.h"
#include "ap_config.h"
#include "apr_strings.h"

#include "git2.h"
#include "markdown.h"

module AP_MODULE_DECLARE_DATA wiki_module;

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

extern char *mkd_doc_title(Document *);

#define P(s) ap_rputs(s, r)

static void markdown_output(Document * doc, request_rec * r)
{
    char *title;
    int ret;
    int size;
    char *p;
    wiki_conf *conf;
    list_t *css;

    conf =
        (wiki_conf *) ap_get_module_config(r->per_dir_config,
                                           &wiki_module);

    ret = mkd_compile(doc, MKD_TOC | MKD_AUTOLINK);
    ap_rputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", r);
    ap_rputs("<!DOCTYPE html PUBLIC \n"
             "          \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n"
             "          \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n",
             r);
    ap_rputs("<html xmlns=\"http://www.w3.org/1999/xhtml\">\n", r);
    ap_rputs("<head>\n", r);

    if (conf->css) {
        ap_rputs("<meta http-equiv=\"Content-Type\""
                 " content=\"text/html; charset=UTF-8\" />\n", r);
        ap_rputs("<meta http-equiv=\"Content-Style-Type\""
                 " content=\"text/css\" />\n", r);
        css = conf->css;
        do {
            ap_rprintf(r,
                       "<link rel=\"stylesheet\" href=\"%s\""
                       " type=\"text/css\" />\n", (char *)css->data);
            css = (list_t *) css->next;
        }
        while (css);
    }
    title = mkd_doc_title(doc);
    if (title) {
        if (conf->name) {
            ap_rprintf(r, "<title>%s - %s</title>\n", title, conf->name);
        } else {
            ap_rprintf(r, "<title>%s</title>\n", title);
        }
    }
    ap_rputs("</head>\n", r);
    ap_rputs("<body>\n", r);
    if (title) {
        ap_rprintf(r, "<h1 class=\"title\">%s</h1>\n", title);
    }
    if (conf->name) {
        ap_rprintf(r,
                   "<div class=\"subtitle\">" "From %s" "</div>\n",
                   conf->name);
    }
    if ((size = mkd_document(doc, &p)) != EOF) {
        ap_rwrite(p, size, r);
    }
    ap_rputc('\n', r);
    ap_rputs("</body>\n", r);
    ap_rputs("</html>\n", r);
    mkd_cleanup(doc);
}

void raw_output(FILE * fp, request_rec * r)
{
    char buf[1024];
    size_t len;
    while (1) {
        len = fread(buf, 1, 1024, fp);
        if (len <= 0) {
            break;
        }
        ap_rwrite(buf, len, r);
    }
}

git_tree *lookup_master_tree(git_repository * repo)
{
    int ret;
    git_tree *tree;
    git_reference *ref;
    const git_oid *commit_oid;
    git_commit *commit;

    ret = git_reference_lookup(&ref, repo, "refs/heads/master");
    if (ret) {
        printf("git_reference_lookup() error: %s\n", git_strerror(ret));
        return NULL;
    }
    if (git_reference_type(ref) != GIT_REF_OID) {
        perror("reference_type is not oid reference.");
        return NULL;
    }

    commit_oid = git_reference_oid(ref);

    ret = git_commit_lookup(&commit, repo, commit_oid);

    ret = git_commit_tree(&tree, commit);
    git_commit_close(commit);
    return tree;
}

const git_oid *lookup_object_oid(git_repository * repo, git_tree * master,
                                 const char *path)
{
    const git_oid *object_oid;
    const git_tree_entry *entry;
    git_object *object;
    git_otype type;
    int ret;

    entry = git_tree_entry_byname(master, path);
    if (!entry) {
        return NULL;
    }
    ret = git_tree_entry_2object(&object, repo, entry);

    type = git_object_type(object);
    printf("type: %s\n", git_object_type2string(type));
    object_oid = git_object_id(object);
    git_object_close(object);
    return object_oid;
}

char *ltrim(const char *base, const char *path)
{
    int i = 0;
    size_t base_len = strlen(base);
    //size_t path_len = strlen(path);

    for (i = 0; i < base_len; i++) {
        if (base[i] != path[i]) {
            continue;
        }
    }
    while (path[i] && path[i] == '/') {
        i++;
    }
    return (char *)(path + i);
}


/* The wiki handler */
static int wiki_handler(request_rec * r)
{
    wiki_conf *conf;
    git_repository *repo;
    git_tree *tree;
    char *path;

    const git_oid *oid;
    char sha1[41];
    sha1[40] = '\0';
    int ret;

    int markdown_flag = 0;
    Document *doc;

    if (strcmp(r->handler, "wiki")) {
        return DECLINED;
    }

    if (r->header_only) {
        return OK;
    }

    conf =
        (wiki_conf *) ap_get_module_config(r->per_dir_config,
                                           &wiki_module);

    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "filename=%s",
                  r->filename);
    if (conf->basepath) {
        path = ltrim(conf->basepath, r->parsed_uri.path);
    } else {
        path = r->parsed_uri.path + 1;
    }

    //ext= ap_getword(r->pool, &r->filename, '.');

    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "path=%s", path);
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                  "r->content_typ=%s", r->content_type);

    if (conf->repo == NULL) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                      "WikiRepository is not set.");
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    ret = git_repository_open(&repo, conf->repo);
    if (ret) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                      "Cannot open git repository.(%s)", conf->repo);
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    tree = lookup_master_tree(repo);
    if (tree == NULL) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                      "Not found master tree.");
        git_repository_free(repo);
    }

    oid = lookup_object_oid(repo, tree, path);
    if (oid == NULL && strchr(path, '.') == NULL) {
        path = apr_pstrcat(r->pool, path, ".md", NULL);
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                      "object is not found, tring %s", path);
        oid = lookup_object_oid(repo, tree, path);
        markdown_flag = 1;
    }

    git_tree_close(tree);

    if (oid == NULL) {
        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                      "object is not found.");
        git_repository_free(repo);
        return HTTP_NOT_FOUND;
    }

    git_oid_fmt(sha1, oid);
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "sha1=%s", sha1);

    const char *data;
    size_t size;

    git_odb *odb;
    odb = git_repository_database(repo);
    git_odb_object *odb_object;
    ret = git_odb_read(&odb_object, odb, oid);
    data = git_odb_object_data(odb_object);
    size = git_odb_object_size(odb_object);

    git_odb_object_close(odb_object);	// safe?

    if (!markdown_flag) {
        ap_rwrite(data, size, r);
    } else {
        r->content_type = "text/html";
        doc = mkd_string(data, size, 0);
        if (doc == NULL) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                          "mkd_string() returned NULL\n");
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        markdown_output(doc, r);
    }

    git_repository_free(repo);
    return OK;
}

static void *wiki_config(apr_pool_t * p, char *dummy)
{
    wiki_conf *c = (wiki_conf *) apr_pcalloc(p, sizeof(wiki_conf));
    memset(c, 0, sizeof(wiki_conf));
    return (void *)c;
}

static const char *set_wiki_repo(cmd_parms * cmd, void *conf,
                                 const char *arg)
{
    wiki_conf *c = (wiki_conf *) conf;
    c->repo = arg;
    return NULL;
}

static const char *set_wiki_name(cmd_parms * cmd, void *conf,
                                 const char *arg)
{
    wiki_conf *c = (wiki_conf *) conf;
    c->name = arg;
    return NULL;
}

static const char *set_wiki_basepath(cmd_parms * cmd, void *conf,
                                     const char *arg)
{
    wiki_conf *c = (wiki_conf *) conf;
    c->basepath = arg;
    return NULL;
}

static const char *set_wiki_css(cmd_parms * cmd, void *conf,
                                const char *arg)
{
    wiki_conf *c = (wiki_conf *) conf;
    list_t *item = (list_t *) malloc(sizeof(list_t));
    item->data = arg;
    item->next = NULL;

    list_t *tail;
    if (c->css) {
        tail = c->css;
        while (tail->next)
            tail = (list_t *) tail->next;
        tail->next = (struct list_t *)item;
    } else {
        c->css = item;
    }
    return NULL;
}

static const char *set_wiki_header(cmd_parms * cmd, void *conf,
                                   const char *arg)
{
    wiki_conf *c = (wiki_conf *) conf;
    c->header = arg;
    return NULL;
}

static const char *set_wiki_footer(cmd_parms * cmd, void *conf,
                                   const char *arg)
{
    wiki_conf *c = (wiki_conf *) conf;
    c->footer = arg;
    return NULL;
}

static const command_rec wiki_cmds[] = {
    AP_INIT_TAKE1("WikiRepository", set_wiki_repo, NULL, OR_ALL,
                  "set Repository"),
    AP_INIT_TAKE1("WikiName", set_wiki_name, NULL, OR_ALL,
                  "set WikiName"),
    AP_INIT_TAKE1("WikiBasePath", set_wiki_basepath, NULL, OR_ALL,
                  "set Base Path"),

    AP_INIT_TAKE1("WikiCSS", set_wiki_css, NULL, OR_ALL,
                  "set CSS"),
    AP_INIT_TAKE1("WikiHeaderHtml", set_wiki_header, NULL, OR_ALL,
                  "set Header HTML"),
    AP_INIT_TAKE1("WikiFooterHtml", set_wiki_footer, NULL, OR_ALL,
                  "set Footer HTML"),
    {NULL}
};

static void wiki_register_hooks(apr_pool_t * p)
{
    ap_hook_handler(wiki_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

/* Dispatch list for API hooks */
module AP_MODULE_DECLARE_DATA wiki_module = {
    STANDARD20_MODULE_STUFF,
    wiki_config,                /* create per-dir    config structures */
    NULL,                       /* merge  per-dir    config structures */
    NULL,                       /* create per-server config structures */
    NULL,                       /* merge  per-server config structures */
    wiki_cmds,                  /* table of config file commands       */
    wiki_register_hooks         /* register hooks                      */
};

/*
 * Local Variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 * vim: sw=4 ts=4 sts=4 et
 */
