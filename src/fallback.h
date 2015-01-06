#ifndef __FALLBACK_H__
#define __FALLBACK_H__

bool has_old_collection(char * const colnames[]);
bool fallback_is_collection_enabled(const char *colname);
scl_rc fallback_run_command(char * const colnames[], const char *cmd);
scl_rc fallback_get_installed_collections(char ***_colnames);
scl_rc fallback_collection_exists(const char *colname, bool *_exists);

#endif
