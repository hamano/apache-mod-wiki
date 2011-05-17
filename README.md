mod_wiki - git based wiki system for Apache HTTPD Server
========================================================

# I think it's the best wiki system

mod_wiki is fast, lightweight and CLI friendly Git based Wiki System.

![architecture](raw/master/architecture.png)

# Feature

* Controll changes by git repository
* Markdown syntax
* Pure C (apache module)

# Recommended for people who like

* I want to use favorite editor like emacs, vim insted WEB Browser.
* I don't want to use RDB
* I want to management document by SCM, and I love git.

# Source Code

https://github.com/hamano/apache-mod-wiki

# Build

    % ./autogen.sh
    % ./configure --with-apache=<APACHE_DIR> \
        --with-discount=<DISCOUNT_BUILD_DIR> \
        --with-libgit2=<LIBGIT2_DIR>
    % make
    # make install

## Dependencies

* discount
    - http://www.pell.portland.or.us/~orc/Code/discount/
* libgit2
    - http://libgit2.github.com/

# Configration

httpd.conf

    LoadModule wiki_module modules/mod_wiki.so
    <Location />
      SetHandler wiki
      WikiRepository /path/to/repository.git
      WikiName "My Wiki"
      WikiCss /style.css
    </Location>

# Usage

## Search

`grep <KEYWORD> *.md`

or `git clone` and `git grep <KEYWORD>`

or google search

## History

`git log`

# TODO

* WEB interface editor
* template system
* cache system

# Author

Tsukasa Hamano <http://twitter.com/hamano>

