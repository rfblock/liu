#define _GNU_SOURCE

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
	// TODO: Test this on a windows machine
	#include <io.h>
	#define F_OK 0
	#define access _access
#else
	#include <unistd.h>
#endif

#ifdef ENABLE_COLORS
	#define ANSI_RESET   "\x1B[0m"
	#define ANSI_BOLD    "\x1b[1m"
	#define ANSI_ITALIC  "\x1b[3m"
	#define ANSI_UNDERLINE "\x1b[4m"
	#define ANSI_INVERT  "\x1b[7m"
	#define ANSI_RED     "\x1B[31m"
	#define ANSI_GREEN   "\x1B[32m"
	#define ANSI_YELLOW  "\x1B[33m"
	#define ANSI_BLUE    "\x1B[34m"
	#define ANSI_MAGENTA "\x1B[35m"
	#define ANSI_CYAN    "\x1B[36m"
#else
	#define ANSI_RESET   ""
	#define ANSI_BOLD    ""
	#define ANSI_ITALIC  ""
	#define ANSI_UNDERLINE ""
	#define ANSI_INVERT  ""
	#define ANSI_RED     ""
	#define ANSI_GREEN   ""
	#define ANSI_YELLOW  ""
	#define ANSI_BLUE    ""
	#define ANSI_MAGENTA ""
	#define ANSI_CYAN    ""
#endif

#define TEXT_WARNING ANSI_YELLOW ANSI_BOLD "warning" ANSI_RESET " "
#define TEXT_FATAL ANSI_RED ANSI_BOLD "fatal" ANSI_RESET " "
#define TEXT_TODO ANSI_BLUE ANSI_BOLD "todo" ANSI_RESET " "

char *source_dir = NULL;
char *object_dir = NULL;
char *binary_name = NULL;
char *compiler_flags = NULL;
char *compiler_binary = NULL;
char *enable_tracelog = NULL;

char object_paths[BUFSIZ];
size_t object_paths_size = 0;

void help(char *arg0)
{
	printf(
		"Usage: %s command\n"
		"Commands:\n"
		"\tg, generate\tGenerates a .liu file (interactive)\n"
		"\n"
		"\tb, build\tBuilds and tests the project\n"
		"\n"
		"\td, debug\tBuilds and opens in a debugger\n"
		"\n"
		"\tr, run  \tBuilds, tests, and runs the project\n"
		"\n"
		"\tc, clean\tClears all temporary/object files\n"
		"\n"
		"\th, help \tOpens the help page (this)\n",
		arg0
	);
}

void generate_liu_file(void) {
	char file[256];
	
	if (access(".liu", F_OK) == 0) {
		printf(TEXT_WARNING ".liu file already exists, this will overwrite it\n");
	}

	printf("Enter output binary name (eg. my_program): " ANSI_GREEN);
	scanf("%256[^\n]%*c", file);

	#ifdef ENABLE_COLORS
	printf(ANSI_RESET);
	#endif

	if (access(file, F_OK) != 0) {
		printf(TEXT_WARNING "Specified file " ANSI_RED ANSI_BOLD ANSI_UNDERLINE "%s" ANSI_RESET " does not exist, continuing\n", file);
		// TODO: confirm [y/n]
	}

	FILE *fp = fopen(".liu", "w");
	if (fp == NULL) {
		printf(TEXT_FATAL " Could not create .liu file\n");
		exit(-1);
	}

	char *format=(
		"BINARY_NAME %s\n"
		"COMPILER_FLAGS -Wall -Werror -Wextra\n"
		"CC gcc"
	);

	int size = snprintf(NULL, 0, format, file);
	char *out = malloc(size + 1);

	if (out == NULL) {
		printf(TEXT_FATAL " Could not generate .liu file content\n");
		exit(-1);
	}

	snprintf(out, size + 1, format, file);

	fwrite(out, 1, size, fp);
	fflush(fp);

	free(out);
	fclose(fp);
}

void scan_liu_file(void)
{
	FILE *fp = fopen(".liu", "r");
	if (fp == NULL) {
		printf(TEXT_FATAL "Could not read .liu file\n");
		exit(-1);
	}

	while (1) {
		int size;
		if(fscanf(fp, "%*s%n", &size) == EOF) {
			break;
		}
		if (size > 255) {
			fscanf(fp, "%*[^\n]");
			continue;
		}

		char *key = malloc(size + 1);
		if (key == NULL) {
			printf(TEXT_FATAL "Unable to allocate key\n");
			exit(-1);
		}

		if (fseek(fp, -size, SEEK_CUR)) {
			printf(TEXT_FATAL "Could not seek to scan key\n");
			exit(-1);
		}
		if (fscanf(fp, "%s", key) == EOF) {
			printf(TEXT_WARNING "Sanity check failed, could not scan key\n");
			free(key);
			break;
		}

		char **value = NULL;
		if (!strcmp(key, "SOURCE_DIR")) {
			value = &source_dir;
		} else if (!strcmp(key, "OBJECT_DIR")) {
			value = &object_dir;
		} else if (!strcmp(key, "BINARY_NAME")) {
			value = &binary_name;
		} else if (!strcmp(key, "COMPILER_FLAGS")) {
			value = &compiler_flags;
		} else if (!strcmp(key, "CC")) {
			value = &compiler_binary;
		} else if (!strcmp(key, "TRACELOG")) {
			value = &enable_tracelog;
		}

		if (value == NULL) {
			continue;
		}

		if (*value != NULL) {
			printf(TEXT_FATAL "Redefinition of " ANSI_RED ANSI_BOLD ANSI_UNDERLINE "%s" ANSI_RESET "\n", key);
			exit(-1);
		}

		free(key);
		key = NULL;

		if(fscanf(fp, " %*[^\n]%n", &size) == EOF) {
			break;
		}

		*value = malloc(size + 1);
		if (*value == NULL) {
			printf(TEXT_FATAL "Unable to allocate value\n");
			exit(-1);
		}

		fseek(fp, -size, SEEK_CUR);
		if(fscanf(fp, " %[^\n]", *value) == EOF) {
			printf(TEXT_WARNING "Sanity check failed, unable to scan value\n");
			break;
		}
	}

	if (source_dir == NULL) {
		source_dir = "src/";
	}

	if (object_dir == NULL) {
		object_dir = "obj/";
	}

	if (binary_name == NULL) {
		binary_name = "main";
	}

	// Clear boolean values if set to "off"
	if (enable_tracelog && !strcmp(enable_tracelog, "off")) {
		free(enable_tracelog);
		enable_tracelog = NULL;
	}

	fclose(fp);
}

void run_tests(void)
{
	printf(TEXT_TODO "run_tests\n");
}

void add_object_path(const char *path)
{
	size_t len = strlen(path);
	if (object_paths_size + len + 1 >= sizeof object_paths) {
		printf(TEXT_FATAL "Length of object paths exceeds buffer size\n");
		exit(-1);
	}

	if (object_paths_size == 0) {
		object_paths[0] = '\0';
	} else {
		strcat(object_paths + object_paths_size, " ");
		object_paths_size++;
	}
	strcat(object_paths + object_paths_size, object_dir);
	object_paths_size += strlen(object_dir);
	strcat(object_paths + object_paths_size, path);
	object_paths_size += len;
}

int build_file(const char *path, const struct stat *, int typeflag, struct FTW *)
{
	if (typeflag == FTW_D) {
		char buf[BUFSIZ];
		snprintf(buf, sizeof buf, "mkdir -p %s/%s", object_dir, path);
		if (enable_tracelog) { printf("%s\n", buf); }
		system(buf);
	}

	if (typeflag != FTW_F) { return 0; }

	const char *ext = strrchr(path, '.');

	if (ext == NULL) { return 0; }
	if (strcmp(ext, ".c") && strcmp(ext, ".cpp")) {
		return 0;
	}

	char *obj_name = malloc(strlen(path) + 1);
	strcpy(obj_name, path);
	strcpy(strrchr(obj_name, '.'), ".o");
	add_object_path(obj_name);
	
	// cc -o obj/{path} -c {path}
	// TODO: multi-core this
	char buf[BUFSIZ];
	int ret = snprintf(buf, sizeof buf, "%s -o %s/%s -c %s %s", compiler_binary, object_dir, obj_name, compiler_flags, path);
	if (ret < 0) {
		printf(TEXT_FATAL "Could not compile object\n");
		exit(-1);
	}
	if (enable_tracelog) { printf("%s\n", buf); }
	system(buf);

	free(obj_name);

	return 0;
}

void build(char *dirname)
{
	nftw(dirname, &build_file, 8, FTW_PHYS);
	
	char buf[BUFSIZ];
	snprintf(buf, sizeof buf, "mkdir -p %s", object_dir); // TODO: turn `mkdir -p` into a liu var
	if (enable_tracelog) { printf("%s\n", buf); }
	system(buf);

	int ret = snprintf(buf, sizeof buf, "%s -o %s %s", compiler_binary, binary_name, object_paths);
	if (ret < 0) {
		printf(TEXT_FATAL "Could not link files\n");
		exit(-1);
	}
	if (enable_tracelog) { printf("%s\n", buf); }
	system(buf);
}

void clean(void)
{
	printf(TEXT_WARNING "This will delete all object files and generated binaries\n");

	char buf[BUFSIZ];
	// TODO: make `rm` a .liu variable
	snprintf(buf, sizeof buf, "rm -rf %s", object_dir);
	printf(TEXT_WARNING "exec: " ANSI_BOLD ANSI_UNDERLINE "%s" ANSI_RESET " [y/N] ", buf);

	int in = getc(stdin);
	if (in == 'y') {
		system(buf);
	}
	while (in != '\n' && in != EOF) {
		in = getc(stdin);
	}

	snprintf(buf, sizeof buf, "rm %s", binary_name);
	printf(TEXT_WARNING "exec: " ANSI_BOLD ANSI_UNDERLINE "%s" ANSI_RESET " [y/N] ", buf);
	in = getc(stdin);
	if (in == 'y') {
		system(buf);
	}
}

int main(int argc, char** argv)
{
	if (argc == 1) {
		printf(ANSI_RED ANSI_BOLD ANSI_UNDERLINE "No liu command" ANSI_RESET "\n\n");
		help(argv[0]);
		return 0;
	}

	char *cmd = argv[1];
	if (!strcmp(cmd, "h") || !strcmp(cmd, "help")) {
		help(argv[0]);
		return 0;
	}

	if (!strcmp(cmd, "g") || !strcmp(cmd, "generate")) {
		generate_liu_file();
		return 0;
	}
	
	scan_liu_file();

	if (!strcmp(cmd, "b") || !strcmp(cmd, "build")) {
		build(source_dir);
		run_tests();
		return 0;
	}

	if (!strcmp(cmd, "c") || !strcmp(cmd, "clean")) {
		clean();
		return 0;
	}

	printf(TEXT_FATAL "unknown command " ANSI_RED ANSI_BOLD ANSI_UNDERLINE "%s" ANSI_RESET "\n", cmd);
	return -1;
}