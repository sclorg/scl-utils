/* Copyright (C) 2011 Red Hat, Inc.

   Written by Jindrich Novy <jnovy@redhat.com>.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SCL_CONF_DIR "/etc/scl/conf/"
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

static void check_asprintf( char **strp, const char *fmt, ... ) {
	va_list args;

	va_start(args, fmt);

	if (vasprintf(strp, fmt, args) == -1 || !*strp) {
		fprintf(stderr, "Allocation failed.\n");
		exit(EXIT_FAILURE);
	}

	va_end(args);
}

static void write_script( int tfd, char *s ) {
	if (write(tfd, s, strlen(s)) == -1) {
		fprintf(stderr, "Error writing to temporary file\n");
		exit(EXIT_FAILURE);
	}
}

static void print_usage( const char *name ) {
	fprintf(stderr, "usage: %s <action> [<collection>...] <command>\n", name);
	fprintf(stderr, "   or: %s -l|--list [<collection>...]\n", name);
	fprintf(stderr, "   or: %s register <path>\n", name);
	fprintf(stderr, "   or: %s deregister <collection> [--force]\n", name);

	fprintf(stderr, "\nOptions:\n"
				 "    -l, --list            list installed Software Collections or packages\n"
				 "                          that belong to them\n"
				 "    -h, --help            display this help and exit\n"
				 "\nActions:\n"
				 "    enable                calls enable script from Software Collection\n"
				 "                          (enables a Software Collection)\n"
				 "    <SCL script name>     calls arbitrary script from a Software Collection\n"
				 "\nUse '-' as <command> to read the command from standard input.\n");
}

static int check_directory(const char *dir_name, struct stat *sb, int *count, struct dirent ***nl) {
    if (stat(dir_name, sb) == -1) {
        fprintf(stderr, "%s does not exist\n", dir_name);
        return EXIT_FAILURE;
    }

    if (!S_ISDIR(sb->st_mode)) {
        fprintf(stderr, "%s is not a directory\n", dir_name);
        return EXIT_FAILURE;
    }

    if ((*count = scandir(dir_name, nl, 0, alphasort)) < 0) {
        perror("scandir");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int get_collection_dir_path(char *col_name, char **_col_dir) {
    int i;
    int fd = -1;
    char *file_path = NULL;
    char *col_dir = NULL;
    struct stat st;
    int ret = EXIT_FAILURE;
    int col_name_len = strlen(col_name);
    int col_dir_len;

    file_path = (char *)malloc(sizeof(SCL_CONF_DIR) + col_name_len + 1);
    if (file_path == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        return EXIT_FAILURE;
    }
    sprintf(file_path, "%s%s", SCL_CONF_DIR, col_name);

    if (stat(file_path, &st) != 0) {
        perror("Unable to get file status");
        goto done;
    }

    fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("Unable to open file");
        goto done;
    }

	/* One for slash, one for terminating zero*/
    col_dir = (char *)calloc(st.st_size + col_name_len + 2, 1);
    if (col_dir == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        goto done;
    }
    if ((col_dir_len = read(fd, col_dir, st.st_size)) < 0) {
        fprintf(stderr, "Unable to read from file.\n");
        goto done;
    }
    for (i = col_dir_len-1; i > 0; i--) {
        if (isspace(col_dir[i]) || col_dir[i] == '/') {
            col_dir[i] = '\0';
        } else {
            break;
        }
    }
    col_dir[i+1] = '/';
    memcpy(col_dir + i + 2, col_name, col_name_len + 1);

    *_col_dir = col_dir;

    ret = EXIT_SUCCESS;
done:
    if (fd > 0) {
        close(fd);
    }
    if (ret != EXIT_SUCCESS) {
        free(col_dir);
    }
    free(file_path);
    return ret;
}

static int col_available(char *col_name) {
    char *col_dir = NULL;
    int ret = 0;

    if (get_collection_dir_path(col_name, &col_dir)) {
        return EXIT_FAILURE;
    }
    ret = access(col_dir, F_OK);
    free(col_dir);
    return ret;
}

static void list_collections() {
	struct stat sb;
	struct dirent **nl;
	int n, i;

	if (check_directory(SCL_CONF_DIR, &sb, &n, &nl)) {
		exit(EXIT_FAILURE);
	}

	for (i=0; i<n; i++) {
		if (*nl[i]->d_name != '.') {
			printf("%s\n", nl[i]->d_name);
		}
	}

	free(nl);
}

static char **read_script_output( char *ori_cmd ) {
	struct stat sb;
	char tmp[] = "/var/tmp/sclXXXXXX";
	char *m, **lines, *mp, *cmd;
	int lp = 0, ls, tfd, i;
	FILE *f;

	tfd = mkstemp(tmp);
	if (tfd < 0) {
		fprintf(stderr, "Cannot create a temporary file: %s\n", tmp);
		exit(EXIT_FAILURE);
	}
	check_asprintf(&cmd, "%s > %s", ori_cmd, tmp);
	i = system(cmd);
	free(cmd);

	if (WEXITSTATUS(i) != 0) {
		fprintf(stderr, "Command execution failed: %s\n", ori_cmd);
		exit(EXIT_FAILURE);
	}
	free(ori_cmd);

	if (stat(tmp, &sb) == -1) {
		fprintf(stderr, "%s does not exist\n", tmp);
		exit(EXIT_FAILURE);
	}

	if ((m = malloc(sb.st_size)) == NULL) {
		fprintf(stderr, "Can't allocate memory.\n");
		exit(EXIT_FAILURE);
	}

	if ((f=fopen(tmp, "r")) == NULL || fread(m, 1, sb.st_size, f) < 1) {
		fprintf(stderr, "Unable to read contents of temporary file.\n");
		exit(EXIT_FAILURE);
	}

	fclose(f);
	close(tfd);
	unlink(tmp);

	ls = 0x100;
	lines = malloc(ls*sizeof(char*));
	*lines = NULL;

	for (mp=m; mp && mp < &m[sb.st_size];) {
		lines[lp++] = mp;
		if (lp == ls-1 ) {
			ls += 0x100;
			lines = realloc(lines, ls*sizeof(char*));
		}
		mp = strchr(mp, '\n');
		*mp = '\0';
		mp++;
	}

	lines[lp] = NULL;
	return lines;
}

static int list_packages_in_collection(const char *colname) {
	struct stat sb;
	struct dirent **nl;
	int i, n, found, smax, ss;
	char *cmd, **lines;
	char **srpms = NULL;
	size_t cns;

	if (check_directory(SCL_CONF_DIR, &sb, &n, &nl)) {
		exit(EXIT_FAILURE);
	}

	for (found=i=0; i<n; i++) {
		if (*nl[i]->d_name != '.') {
			if (!strcmp(nl[i]->d_name, colname)) {
				found = 1;
			}
		}
		free(nl[i]);
	}

	free(nl);

	if (!found) {
		fprintf(stderr, "warning: collection \"%s\" doesn't seem to be installed, checking anyway...\n", colname);
	}

	check_asprintf(&cmd, "rpm -qa --qf=\"#%%{name}-%%{version}-%%{release}.%%{arch}\n%%{sourcerpm}\n[%%{provides}\n]\"");
	lines = read_script_output(cmd);
	if (!lines[0]) {
		fprintf(stderr, "No package list from RPM received.\n");
		exit(EXIT_FAILURE);
	}

	cns = strlen(colname);

	for (smax=ss=i=0; lines[i];) {
		char *srpm = lines[i+1];
		i += 2;
		for (;lines[i] && lines[i][0] != '#'; i++) {
			if (!strncmp(lines[i], "scl-package(", 12) && !strncmp(&lines[i][12], colname, cns) && lines[i][12+cns] == ')') {
				for (found=n=0; n<ss; n++) {
					if (!strcmp(srpms[n], srpm)) {
						found = 1;
						break;
					}
				}
				if (!found) {
					if (ss == smax) {
						smax += 0x100;
						if (!(srpms=realloc(srpms, smax*sizeof(char*)))) {
							fprintf(stderr, "Can't allocate memory.\n");
							exit(EXIT_FAILURE);
						}
					}
					srpms[ss++] = srpm;
				}
			}
		}
	}

	for (i=0; lines[i]; i++) {
		if (lines[i][0] == '#') {
			for (n=0; n<ss; n++) {
				if (!strcmp(lines[i+1], srpms[n])) {
					printf("%s\n", &lines[i][1]);
				}
			}
		}
	}

	free(srpms);
	free(*lines);
	free(lines);

	return 0;
}

static int split_path(char *col_path, char **_col, char **_fname) {
    char *name_start = NULL;
    char *name_end = NULL;
    char *col = NULL;
    int col_path_len = strlen(col_path);

    col = (char *)malloc(strlen(col_path) + 1);
    if (col == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        return EXIT_FAILURE;
    }
    memcpy(col, col_path, col_path_len + 1);

    name_end = col + col_path_len - 1;
    while (name_end > col && *name_end == '/') {
        *name_end = '\0';
        name_end--;
    }

    name_start = strrchr(col, '/');
    if (name_start == NULL) {
        free(col);
        return EXIT_FAILURE;
    } else {
        *name_start = '\0';
        name_start++;
    }

    *_fname = name_start;
    *_col = col;
    return EXIT_SUCCESS;
}

static int get_collection_conf_path(char *col_name, char **_col_path) {
    char *col_path = (char *)malloc(sizeof(SCL_CONF_DIR) + strlen(col_name) + 1);
    if (col_path == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        return EXIT_FAILURE;
    }
    sprintf(col_path, "%s%s", SCL_CONF_DIR, col_name);
    *_col_path = col_path;
    return EXIT_SUCCESS;
}

static int check_valid_collection(char *col_dir) {
    struct stat sb;
    struct dirent **nl;
    int n, i;
    bool missing_root = true;
    bool missing_enable = true;

    if (check_directory(col_dir, &sb, &n, &nl)) {
        exit(EXIT_FAILURE);
    }

    for (i=0; i<n; i++) {
        if (*nl[i]->d_name != '.') {
            if (!strcmp(nl[i]->d_name, "root")) {
                missing_root = false;
            } else if (!strcmp(nl[i]->d_name, "enable")) {
                missing_enable = false;
            }
        }
        free(nl[i]);
    }
    free(nl);

    return missing_root || missing_enable;
}

static int register_collection(char *col_path) {
    FILE *f;
    char *col = NULL;
    char *name = NULL;
    char *new_file = NULL;

    if (col_path == NULL || col_path[0] != '/') {
        fprintf(stderr, "Collection must be specified as an absolute path!\n");
        return EXIT_FAILURE;
    }

    if (access(col_path, F_OK)) {
        perror("Unable to register collection");
        return EXIT_FAILURE;
    }

    if (check_valid_collection(col_path)) {
        fprintf(stderr, "Unable to register collection: %s is not a valid collection\n", col_path);
        return EXIT_FAILURE;
    }

    if (split_path(col_path, &col, &name)) {
        return EXIT_FAILURE;
    }

    if (get_collection_conf_path(name, &new_file)) {
        free(col);
        return EXIT_FAILURE;
    }

    if (access(new_file, F_OK) == 0) {
        fprintf(stderr, "Unable to register collection: Collection with the same name is already registered\n");
        free(new_file);
        free(col);
        return EXIT_FAILURE;
    }

    f = fopen(new_file, "w+");
    if (f == NULL) {
        perror("Unable to open file");
        free(col);
        free(new_file);
        return EXIT_FAILURE;
    }

    fprintf(f, "%s\n", col);
    printf("Collection succesfully registered.\n"
           "The collection can now be enabled using 'scl enable %s <command>'\n", name);
    fclose(f);
    free(new_file);
    free(col);

    return EXIT_SUCCESS;
}

static int check_package(char *file_path, int *_status) {
    char *cmd = NULL;
    int path_len = strlen(file_path);
    char rpm_query[] = "rpm -qf %s > /dev/null 2> /dev/null";

    cmd  = (char *)malloc(path_len + sizeof(rpm_query) - 1);
    if (cmd == NULL) {
        fprintf(stderr, "Can't allocate memory.\n");
        return EXIT_FAILURE;
    }
    sprintf(cmd, rpm_query, file_path);
    *_status = system(cmd);
    free(cmd);

    return EXIT_SUCCESS;
}

static int deregister_collection(char *col_path, bool force) {
    char *col = NULL;
    char *col_name = NULL;

    if (get_collection_conf_path(col_path, &col_name)) {
        free(col);
        return EXIT_FAILURE;
    }

    if (!force) {
        int status;
        if (check_package(col_name, &status)) {
            free(col_name);
            free(col);
            return EXIT_FAILURE;
        }

        if (status == 0) {
            fprintf(stderr, "Unable to deregister collection: "
                    "Collection was installed as a package, please use --force to deregister it.\n");
            free(col);
            free(col_name);
            return EXIT_FAILURE;
        }
    }

    if (remove(col_name)) {
        perror("Unable to deregister collection");
        free(col_name);
        free(col);
        return EXIT_FAILURE;
    }
    printf("Collection successfully deregistered.\n");
    free(col_name);
    free(col);
    return EXIT_SUCCESS;
}




int main(int argc, char **argv) {
	struct stat st;
	char *path, *enablepath;
	char tmp[] = "/var/tmp/sclXXXXXX";
	char *bash_cmd, *echo, *enabled;
	int i, tfd, ffd;
	int separator_pos = 0;
	char *command = NULL;
	int failed = 0;

	if (argc == 2 && (!strcmp(argv[1],"--help") || !strcmp(argv[1],"-h"))) {
		print_usage(argv[0]);
		exit(EXIT_SUCCESS);
	}

	if (argc >= 2 && (!strcmp(argv[1],"--list") || !strcmp(argv[1],"-l"))) {
		if (argc == 2) {
			list_collections();
		} else {
			for (i=2; i<argc; i++)
				list_packages_in_collection(argv[i]);
		}
		exit(EXIT_SUCCESS);
	}

	if (argc > 2 && (!strcmp(argv[1], "register"))) {
		failed = 0;
		for (i = 2; i < argc; i++) {
			if (register_collection(argv[i]) != 0) {
				failed++;
			}
		}
		if (failed > 0) {
			fprintf(stderr, "Registration of %d collections failed!\n", failed);
			exit(EXIT_FAILURE);
		} else {
			exit(EXIT_SUCCESS);
		}
	}
	if (argc > 2 && (!(strcmp(argv[1], "deregister")))) {
		bool force = false;
		for (i = 2; i < argc; i++) {
			if (!strcmp(argv[i], "--force")) {
				force = true;
				break;
			}
		}
		for (i = 2; i < argc; i++) {
			if (strcmp(argv[i], "--force") != 0) {
				failed = 0;
				if (deregister_collection(argv[i], force) != 0) {
					failed++;
				}
			}
		}
		if (failed > 0) {
			fprintf(stderr, "Deregistration of %d collections failed!\n", failed);
			exit(EXIT_FAILURE);
		} else {
			exit(EXIT_SUCCESS);
		}
	}

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--") == 0) {
			break;
		}
	}
	separator_pos = i;

	if (separator_pos == argc) {
		/* Separator not found */
		if (argc < 4) {
			fprintf(stderr, "Need at least 3 arguments.\nRun %s --help to get help.\n", argv[0]);
			exit(EXIT_FAILURE);
		}

		command = strdup(argv[argc-1]);
		if (command == NULL) {
			fprintf(stderr, "Can't duplicate string.\n");
		}
	} else if (separator_pos == argc-1) {
		command = "-";
	} else if (separator_pos <= 2) {
		fprintf(stderr, "Need at least 2 arguments before command is specified.\nRun %s --help to get help.\n", argv[0]);
		exit(EXIT_FAILURE);
	} else {
		command = NULL;
	}

	if ((command == NULL && !strcmp(argv[separator_pos+1], "-")) ||
	    (command != NULL && !strcmp(command, "-"))) {	/* reading command from stdin */
		size_t r;


		command = malloc(BUFSIZ+1);
		if (!command) {
			fprintf(stderr, "Can't allocate memory.\n");
			exit(EXIT_FAILURE);
		}

		for (r=0; (r += fread(command+r, 1, BUFSIZ, stdin));) {
			if (feof(stdin)) {
				if (r % BUFSIZ == 0) {
					command = realloc(command, r+1);
					if (!command) {
						fprintf(stderr, "Can't reallocate memory.\n");
						exit(EXIT_FAILURE);
					}
				}
				command[r] = '\0';
				break;
			}
			command = realloc(command, r+BUFSIZ+1);
			if (!command) {
				fprintf(stderr, "Can't reallocate memory.\n");
				exit(EXIT_FAILURE);
			}
		}
		if (!r) {
			fprintf(stderr, "Error reading command from stdin.\n");
			exit(EXIT_FAILURE);
		}
	} else if (command == NULL) {
		int len = 0;
		for (i = separator_pos+1; i < argc; i++) {
			len += strlen(argv[i])+3; /* +1 for additional space, +2 for additional quotes */
		}

		command = malloc((len+1)*sizeof(char));
		if (command == NULL) {
			fprintf(stderr, "Can't allocate memory.\n");
			exit(EXIT_FAILURE);
		}

		len = 0;
		for (i = separator_pos+1; i < argc; i++) {
			command[len++] = '"';
			strcpy(command+len, argv[i]);
			len += strlen(argv[i]);
			command[len++] = '"';
			command[len++] = ' ';
		}
		command[len] = '\0';
	}

	tfd = mkstemp(tmp);
	if (tfd < 0) {
		fprintf(stderr, "Cannot create a temporary file: %s\n", tmp);
		exit(EXIT_FAILURE);
	}

	check_asprintf(&enabled, "eval \"SCLS=( ${X_SCLS[*]} )\"\n");
	write_script(tfd, enabled);
	free(enabled);

	for (i=2; i<MIN(separator_pos, argc-1); i++) {
		FILE *f;
		size_t r;
		char scl_dir[BUFSIZ];

		check_asprintf(&enabled, "/usr/bin/scl_enabled %s\nif [ $? != 0 ]; then\n"
					 "  SCLS+=(%s)\n"
					 "  export X_SCLS=$(printf '%%q ' \"${SCLS[@]}\")\n", argv[i], argv[i]);
		write_script(tfd, enabled);
		free(enabled);
		check_asprintf(&path, SCL_CONF_DIR "%s", argv[i]);
		if (!(f=fopen(path,"r"))) {
			fprintf(stderr, "Unable to open %s!\n", path);
			unlink(tmp);
			exit(EXIT_FAILURE);
		}
		r = fread(scl_dir, 1, BUFSIZ, f);
		if (!r) {
			fprintf(stderr, "Unable to read or file empty %s!\n", path);
			unlink(tmp);
			exit(EXIT_FAILURE);
		}
		scl_dir[r-1] = '\0';
		strncat(scl_dir, "/", BUFSIZ-1);
		strncat(scl_dir, argv[i], BUFSIZ-1);
		strncat(scl_dir, "/", BUFSIZ-1);
		fclose(f);
		free(path);

		check_asprintf(&path, "%s", scl_dir);
		if (lstat(path, &st)) {
			fprintf(stderr, "%s doesn't exist\n", path);
			unlink(tmp);
			exit(EXIT_FAILURE);
		}
		if (!S_ISDIR(st.st_mode)) {
			fprintf(stderr, "%s is not a directory\n", path);
			unlink(tmp);
			exit(EXIT_FAILURE);
		}
		check_asprintf(&enablepath, "%s/%s", path, argv[1]);
		check_asprintf(&echo, ". %s\n", enablepath);

		ffd = open(enablepath, O_RDONLY);

		if (ffd != -1) {
			write_script(tfd, echo);
		} else {
			fprintf(stderr, "warning: %s scriptlet does not exist!\n", enablepath);
			unlink(tmp);
			exit(EXIT_FAILURE);
		}
		write_script(tfd, "fi\n");

		close(ffd);
		free(echo);
		free(enablepath);
		free(path);
	}

	write_script(tfd, command);
	write_script(tfd, "\n");
	free(command);
	close(tfd);

	check_asprintf(&bash_cmd, "/bin/bash %s", tmp);
	i = system(bash_cmd);
	free(bash_cmd);
	unlink(tmp);

	return WEXITSTATUS(i);
}
