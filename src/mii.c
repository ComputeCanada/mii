#include "mii.h"
#include "util.h"
#include "log.h"
#include "analysis.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

/* options */
static char* _mii_modulepath = NULL;
static char* _mii_datadir    = NULL;

/* state */
static char* _mii_datafile = NULL;

void mii_option_modulepath(const char* modulepath) {
    if (modulepath) _mii_modulepath = mii_strdup(modulepath);
}

void mii_option_datadir(const char* datadir) {
    if (datadir) _mii_datadir = mii_strdup(datadir);
}

int mii_init() {
    if (!_mii_modulepath) {
        char* env_modulepath = getenv("MODULEPATH");

        if (env_modulepath) {
            _mii_modulepath = mii_strdup(env_modulepath);
        } else {
            mii_error("MODULEPATH is not set!");
            return -1;
        }
    }

    if (!_mii_datadir) {
        char* home = getenv("HOME");

        if (!home) {
            mii_error("Cannot compute default data dir: HOME variable is not set!");
            return -1;
        }

        _mii_datadir = mii_join_path(home, ".mii");
    }

    int res = mkdir(_mii_datadir, 0755);

    if (res && (errno != EEXIST)) {
        mii_error("Error initializing data directory: %s", strerror(errno));
        return -1;
    }

    _mii_datafile = mii_join_path(_mii_datadir, "index");

    mii_debug("Initialized mii with cache path %s", _mii_datafile);
    return 0;
}

void mii_free() {
    if (_mii_modulepath) free(_mii_modulepath);
    if (_mii_datadir) free(_mii_datadir);
    if (_mii_datafile) free(_mii_datafile);
}

int mii_build() {
    /*
     * BUILD: rebuild the index from the modules on the disk
     * this is equivalent to a sync, but without the import/merge step
     */

    mii_modtable index;
    mii_modtable_init(&index);

    /* initialize analysis regular expressions */
    if (mii_analysis_init()) {
        mii_error("Unexpected failure initializing analysis functions!");
        return -1;
    }

    /* generate a partial index from the disk */
    if (mii_modtable_gen(&index, _mii_modulepath)) {
        mii_error("Error occurred during index generation, terminating!");
        return -1;
    }

    /* perform analysis over the entire index */
    int count;

    if (mii_modtable_analysis(&index, &count)) {
        mii_error("Error occurred during index analysis, terminating!");
        return -1;
    }

    if (count) {
        mii_info("Finished analysis on %d modules", count);
    } else {
        mii_warn("Didn't analyze any modules. Is the MODULEPATH correct?");
    }

    /* export back to the disk */
    if (mii_modtable_export(&index, _mii_datafile)) {
        mii_error("Error occurred during index write, terminating!");
        return -1;
    }

    /* cleanup */
    mii_modtable_free(&index);
    mii_analysis_free();

    return 0;
}

int mii_sync() {
    /*
     * SYNC: sychronize the index if necessary
     */

    mii_modtable index;
    mii_modtable_init(&index);

    /* initialize analysis regular expressions */
    if (mii_analysis_init()) {
        mii_error("Unexpected failure initializing analysis functions!");
        return -1;
    }

    /* generate a partial index from the disk */
    if (mii_modtable_gen(&index, _mii_modulepath)) {
        mii_error("Error occurred during index generation, terminating!");
        return -1;
    }

    /* try and import up-to-date modules from the cache */
    if (mii_modtable_preanalysis(&index, _mii_datafile)) {
        mii_warn("Error occurred during index preanalysis, will rebuild the whole cache!");
    }

    /* perform analysis over any remaining modules */
    int count;

    if (mii_modtable_analysis(&index, &count)) {
        mii_error("Error occurred during index analysis, terminating!");
        return -1;
    }


    /* export back to the disk only if modules were analyzed */
    if (count) {
        mii_info("Finished analysis on %d modules", count);

        if (mii_modtable_export(&index, _mii_datafile)) {
            mii_error("Error occurred during index write, terminating!");
            return -1;
        }
    } else {
        mii_info("All modules up to date :)");
    }

    /* cleanup */
    mii_modtable_free(&index);
    mii_analysis_free();

    return 0;
}

int mii_search_exact(mii_search_result* res, const char* cmd) {
    mii_modtable index;
    mii_modtable_init(&index);

    /* try and import the cache from the disk */
    if (mii_modtable_import(&index, _mii_datafile)) {
        mii_warn("Couldn't import module index, will try and build one now.");

        if (mii_build()) return -1;

        mii_info("Trying to import new index..");

        if (mii_modtable_import(&index, _mii_datafile)) {
            mii_error("Failed to import again, giving up..");
            return -1;
        }
    }

    /* perform the search */
    if (mii_modtable_search_exact(&index, cmd, res)) {
        mii_error("Error occurred during search, terminating!");
        return -1;
    }

    /* cleanup */
    mii_modtable_free(&index);

    return 0;
}

int mii_search_fuzzy(mii_search_result* res, const char* cmd) {
    mii_modtable index;
    mii_modtable_init(&index);

    /* try and import the cache from the disk */
    if (mii_modtable_import(&index, _mii_datafile)) {
        mii_warn("Couldn't import module index, will try and build one now.");

        if (mii_build()) return -1;

        mii_info("Trying to import new index..");

        if (mii_modtable_import(&index, _mii_datafile)) {
            mii_error("Failed to import again, giving up..");
            return -1;
        }
    }

    /* perform the search */
    if (mii_modtable_search_similar(&index, cmd, res)) {
        mii_error("Error occurred during search, terminating!");
        return -1;
    }

    /* cleanup */
    mii_modtable_free(&index);

    return 0;
}

int mii_search_info(mii_search_result* res, const char* cmd) {
    mii_modtable index;
    mii_modtable_init(&index);

    /* try and import the cache from the disk */
    if (mii_modtable_import(&index, _mii_datafile)) {
        mii_warn("Couldn't import module index, will try and build one now.");

        if (mii_build()) return -1;

        mii_info("Trying to import new index..");

        if (mii_modtable_import(&index, _mii_datafile)) {
            mii_error("Failed to import again, giving up..");
            return -1;
        }
    }

    /* perform the search */
    if (mii_modtable_search_info(&index, cmd, res)) {
        mii_error("Error occurred during search, terminating!");
        return -1;
    }

    /* cleanup */
    mii_modtable_free(&index);

    return 0;
}
