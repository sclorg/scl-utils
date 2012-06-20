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
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

static void list_collections() {
	struct stat sb;
	struct dirent **nl;
	int n, i;
        const char prefix[] = "/etc/scl/prefixes/";

	if (stat(prefix, &sb) == -1) {
		fprintf(stderr, "%s does not exist\n", prefix);
		exit(EXIT_FAILURE);
	}

	if (!S_ISDIR(sb.st_mode)) {
		fprintf(stderr, "%s is not a directory\n", prefix);
		exit(EXIT_FAILURE);
	}


	if ((n = scandir(prefix, &nl, 0, alphasort)) < 0) {
		perror("scandir");
		exit(EXIT_FAILURE);
	}

	for (i=0; i<n; i++) {
		if (*nl[i]->d_name != '.') {
			printf("%s\n", nl[i]->d_name);
		}
	}

	free(nl);
}

int main(int argc, char **argv) {
	struct stat st;
	char *path, *enablepath;
	char tmp[] = "/var/tmp/sclXXXXXX";
	char *cmd = NULL, *bash_cmd, *echo, *enabled;
	int i, tfd, ffd, stdin_read = 0;

	if (argc == 2 && (!strcmp(argv[1],"--list") || !strcmp(argv[1],"-l"))) {
		list_collections();
		return 0;
	}

	if (!strcmp(argv[argc-1], "-")) {	/* reading command from stdin */
		size_t r;

		if (argc < 4) {
			fprintf(stderr, "Need at least 3 arguments.\nRun %s without arguments to get help.\n", argv[0]);
			exit(EXIT_FAILURE);
		}

		cmd = malloc(BUFSIZ);

		if (!cmd) {
			fprintf(stderr, "Can't allocate memory.\n");
			exit(EXIT_FAILURE);
		}

		for (r=0; (r += fread(cmd+r, 1, BUFSIZ, stdin));) {
			if (feof(stdin)) break;
			cmd = realloc(cmd, r+BUFSIZ);
			if (!cmd) {
				fprintf(stderr, "Can't reallocate memory.\n");
				exit(EXIT_FAILURE);
			}
		}
		if (!r) {
			fprintf(stderr, "Error reading command from stdin.\n");
			exit(EXIT_FAILURE);
		}
		stdin_read = 1;
	}

	if (!stdin_read) {
		if (argc < 4) {
			fprintf(stderr, "Usage: %s <action> [<collection1>, <collection2> ...] <command>\n"
					"If <command> is '-' then the command will be read from standard input.\n", argv[0]);
			exit(EXIT_FAILURE);
		}
		cmd = strdup(argv[argc-1]);
	}

	tfd = mkstemp(tmp);

	check_asprintf(&enabled, "scl_enabled %s\nif [ $? != 0 ]; then\n"
				 "  eval \"SCLS=( ${x_scls[*]} )\"\n"
				 "  SCLS+=(%s)\n"
				 "  export X_SCLS=$(printf '%%q ' \"${SCLS[@]}\")\nfi\n", argv[2], argv[2]);
	write_script(tfd, enabled);
	free(enabled);

	for (i=2; i<argc-1; i++) {
		FILE *f;
		size_t r;
		char scl_dir[BUFSIZ];

		check_asprintf(&path, "/etc/scl/prefixes/%s", argv[i]);
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
			fprintf(stderr, "WARNING: %s scriptlet does not exist!\n", enablepath);
			unlink(tmp);
			exit(EXIT_FAILURE);
		}

		close(ffd);
		free(echo);
		free(enablepath);
		free(path);
	}

	write_script(tfd, cmd);
	write_script(tfd, "\n");
	free(cmd);
	close(tfd);

	check_asprintf(&bash_cmd, "/bin/bash %s", tmp);
	i = system(bash_cmd);
	free(bash_cmd);
	unlink(tmp);

	return WEXITSTATUS(i);
}
