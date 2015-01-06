#ifndef __SCLLIB_H__
#define __SCLLIB_H__

#include <stdbool.h>
#include "errors.h"

/*
 * Runs specified command.
 * @param[in] colnames      Collections in which will be the command run.
 * @param[in] cmd           Command to run.
 * @return                  EOK on succes otherwise err code
 */
scl_rc run_command(char *const colname[], const char *cmd, bool exec);

/*
 * Created array of installed collections.
 * @param[out] _collnames   NULL-terminated array of char*
 * @return                  EOK on succes otherwise err code
 */
scl_rc get_installed_collections(char *const **colnames);

/*
 * Creates array of package names.
 * @param[in] colname       Name of inspected collection.
 * @param[out] pkgnames     NULL-terminated array of char*
 * @return                  EOK on succes otherwise err code
 */
scl_rc list_packages_in_collection(const char *colname, char ***pkgnames);

/*
 * Register a new collection.
 * @param[in] colpath       Path to a valid collection directory structure.
 * @return                  EOK on succes otherwise err code
 */
scl_rc register_collection(const char *colpath);

/*
 * Deregister a collection.
 * @param[in] colname       Name of collection to deregister.
 * @param[in] force         Force deregistration in case that collection
 *                          was installed as a package
 * @return                  EOK on succes otherwise err code
 */
scl_rc deregister_collection(const char *colname, bool force);

/*
 * Show manual page about collection.
 * @param[in] colname       Name of collection.
 * @return                  EOK on succes otherwise err code
 */
scl_rc show_man(const char *colname);

/*
 * Get path where collection is located.
 * @param[in] colname       Name of collection.
 * @param[out] _colpath     Path where collection is located.
 * @return                  EOK on succes otherwise err code
 */
scl_rc get_collection_path(const char *colname, char **_colpath);

/*
 * Release scllib cache. It has to be called after work with scllib is done.
 */
void release_scllib_cache();

#endif
