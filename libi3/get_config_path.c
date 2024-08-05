/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Get the path of the first configuration file found. If override_configpath is
 * specified, that path is returned and saved for further calls. Otherwise,
 * checks the home directory first, then the system directory, always taking
 * into account the XDG Base Directory Specification ($XDG_CONFIG_HOME,
 * $XDG_CONFIG_DIRS).
 *
 */
char *get_config_path(const char *override_configpath, bool use_system_paths) {
    char *xdg_config_home, *xdg_config_dirs, *config_path;

    static const char *saved_configpath = NULL;

    if (override_configpath != NULL) {
        saved_configpath = override_configpath;
        return sstrdup(saved_configpath);
    }

    if (saved_configpath != NULL) {
        return sstrdup(saved_configpath);
    }

    /* 1: check for $XDG_CONFIG_HOME/i3/config */
    if ((xdg_config_home = getenv("XDG_CONFIG_HOME")) == NULL) {
        xdg_config_home = "~/.config";
    }

    xdg_config_home = resolve_tilde(xdg_config_home);
    sasprintf(&config_path, "%s/i3/config", xdg_config_home);
    free(xdg_config_home);

    if (path_exists(config_path)) {
        return config_path;
    }
    free(config_path);

    /* 2: check the traditional path under the home directory */
    config_path = resolve_tilde("~/.i3/config");
    if (path_exists(config_path)) {
        return config_path;
    }
    free(config_path);

    /* The below paths are considered system-level, and can be skipped if the
     * caller only wants user-level configs. */
    if (!use_system_paths) {
        return NULL;
    }

    /* 3: check for $XDG_CONFIG_DIRS/i3/config */
    if ((xdg_config_dirs = getenv("XDG_CONFIG_DIRS")) == NULL) {
        xdg_config_dirs = SYSCONFDIR "/xdg";
    }

    char *buf = sstrdup(xdg_config_dirs);
    char *tok = strtok(buf, ":");
    while (tok != NULL) {
        tok = resolve_tilde(tok);
        sasprintf(&config_path, "%s/i3/config", tok);
        free(tok);
        if (path_exists(config_path)) {
            free(buf);
            return config_path;
        }
        free(config_path);
        tok = strtok(NULL, ":");
    }
    free(buf);

    /* 4: check the traditional path under /etc */
    config_path = SYSCONFDIR "/i3/config";
    if (path_exists(config_path)) {
        return sstrdup(config_path);
    }

    return NULL;
}
