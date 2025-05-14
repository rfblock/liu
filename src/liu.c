#define _GNU_SOURCE

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
	#include <io.h>
	#include <windows.h>
	#define F_OK 0
	#define access _access
#else
	#include <unistd.h>
	#include <errno.h>
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
	#define TEXT_WARNING ANSI_YELLOW ANSI_BOLD "warning" ANSI_RESET " "
	#define TEXT_FATAL ANSI_RED ANSI_BOLD "fatal" ANSI_RESET " "
	#define TEXT_TODO ANSI_BLUE ANSI_BOLD "todo" ANSI_RESET " "
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
	#define TEXT_WARNING "[warning] "
	#define TEXT_FATAL "[fatal] "
	#define TEXT_TODO "[todo] "
#endif

#define LIU_HIGHLIGHT(x) ANSI_BOLD ANSI_UNDERLINE x ANSI_RESET

#define PRINTF_WARNING(fmt, ...) printf(TEXT_WARNING fmt, ##__VA_ARGS__)
#define PRINTLN_WARNING(x) PRINTF_WARNING("%s\n", x);

#define PRINTF_FATAL(fmt, ...) do { printf(TEXT_FATAL fmt, ##__VA_ARGS__); exit(-1); } while (0)
#define PRINTLN_FATAL(x) PRINTF_FATAL("%s\n", x);

#define LIU_ASSERT(cond, msg) do { \
	if (!(cond)) { \
		PRINTLN_FATAL(msg)	\
	} \
} while (0)

#define LIU_ASSERTF(cond, msg, ...) do { \
	if (!(cond)) { \
		PRINTF_FATAL(msg, ##__VA_ARGS__); \
	} \
} while (0)

#define LIU_PRINTF_TRACELOG(fmt, ...) do { \
	if (liu_config_enable_tracelog) { \
		printf(fmt "\n", ##__VA_ARGS__); \
	} \
} while (0)

char *liu_config_source_dir = NULL;
char *liu_config_object_dir = NULL;
char *liu_config_binary_name = NULL;
char *liu_config_compiler_flags = NULL;
char *liu_config_compiler_binary = NULL;
char *liu_config_enable_tracelog = NULL;
char *liu_config_replace_binary = NULL;

char object_paths[BUFSIZ];
size_t object_paths_size = 0;

void liu_show_help(char *arg0)
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

void liu_generate_liu_file(void) {
	char file[256];
	
	if (access(".liu", F_OK) == 0) {
		PRINTLN_WARNING(".liu file already exists, this will overwrite it");
	}

	printf("Enter output binary name (eg. my_program): " ANSI_GREEN);
	scanf("%256[^\n]%*c", file);

	#ifdef ENABLE_COLORS
	printf(ANSI_RESET);
	#endif

	FILE *fp = fopen(".liu", "w");
	LIU_ASSERT(fp != NULL, "Could not create .liu file");

	char *format=(
		"BINARY_NAME %s\n"
		"COMPILER_FLAGS -Wall -Werror -Wextra\n"
		"CC gcc"
	);

	int size = snprintf(NULL, 0, format, file);
	char *out = malloc(size + 1);

	LIU_ASSERT(out != NULL, "Could not generate .liu file content");

	snprintf(out, size + 1, format, file);

	fwrite(out, 1, size, fp);
	fflush(fp);

	free(out);
	fclose(fp);
}

void liu_scan_liu_file(void)
{
	FILE *fp = fopen(".liu", "rb");
	LIU_ASSERT(fp != NULL, "Could not read .liu file");

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
		LIU_ASSERT(key != NULL, "Unable to allocate key");

		LIU_ASSERT(!fseek(fp, -size, SEEK_CUR), "Could not seek to scan key");
		if (fscanf(fp, "%s", key) == EOF) {
			PRINTLN_WARNING("Sanity check failed, could not scan key");
			free(key);
			break;
		}

		char **value = NULL;
		if (!strcmp(key, "SOURCE_DIR")) {
			value = &liu_config_source_dir;
		} else if (!strcmp(key, "OBJECT_DIR")) {
			value = &liu_config_object_dir;
		} else if (!strcmp(key, "BINARY_NAME")) {
			value = &liu_config_binary_name;
		} else if (!strcmp(key, "COMPILER_FLAGS")) {
			value = &liu_config_compiler_flags;
		} else if (!strcmp(key, "CC")) {
			value = &liu_config_compiler_binary;
		} else if (!strcmp(key, "TRACELOG")) {
			value = &liu_config_enable_tracelog;
		} else if (!strcmp(key, "REPLACE_BINARY")) {
			value = &liu_config_replace_binary;
		}

		if (value == NULL) {
			continue;
		}

		LIU_ASSERTF(*value == NULL, "Redefinition of " ANSI_RED LIU_HIGHLIGHT("%s") "\n", key);

		free(key);
		key = NULL;

		if(fscanf(fp, " %*[^\n]%n", &size) == EOF) {
			break;
		}

		*value = malloc(size + 1);
		LIU_ASSERT(*value != NULL, "Unable to allocate value\n");

		fseek(fp, -size, SEEK_CUR);
		if(fscanf(fp, " %[^\n]", *value) == EOF) {
			PRINTLN_WARNING("Sanity check failed, unable to scan value");
			break;
		}
	}

	if (liu_config_source_dir == NULL) {
		liu_config_source_dir = "src";
	}

	if (liu_config_object_dir == NULL) {
		liu_config_object_dir = "obj";
	}

	if (liu_config_binary_name == NULL) {
		liu_config_binary_name = "main";
	}

	// Clear boolean values if set to "off"
	if (liu_config_enable_tracelog && !strcmp(liu_config_enable_tracelog, "off")) {
		free(liu_config_enable_tracelog);
		liu_config_enable_tracelog = NULL;
	}

	if (liu_config_replace_binary && !strcmp(liu_config_replace_binary, "off")) {
		free(liu_config_replace_binary);
		liu_config_replace_binary = NULL;
	}

	fclose(fp);
}

void liu_run_tests(void)
{
	printf(TEXT_TODO "liu_run_tests\n");
}

void liu_add_object_path(const char *path)
{
	size_t len = strlen(path);
	LIU_ASSERT(object_paths_size + len + 2 < sizeof object_paths, "Length of object paths exceeds buffer size");

	if (object_paths_size == 0) {
		object_paths[0] = '\0';
	} else {
		strcat(object_paths + object_paths_size, " ");
		object_paths_size++;
	}
	strcat(object_paths + object_paths_size, liu_config_object_dir);
	object_paths_size += strlen(liu_config_object_dir) + 1;
	object_paths[object_paths_size - 1] = '/';
	strcat(object_paths + object_paths_size, path);
	object_paths_size += len;
}

void liu_mkdir_silent(const char *path)
{
	int print;
#ifdef _WIN32
	mkdir(path);
	print = true; // TODO: Print only when created
#else
	mkdir(path, 755);
	print = errno != EEXIST;
#endif
	if (print) {
		LIU_PRINTF_TRACELOG("mkdir %s", path);
	}
}

void liu_rename_file(const char *from, const char *to)
{
	LIU_PRINTF_TRACELOG("rename %s %s", from, to);
	rename(from, to);
}

void liu_remove_file(const char *path)
{
	LIU_PRINTF_TRACELOG("remove %s", path);
	remove(path);
}

void liu_remove_dir(const char *path)
{
	LIU_PRINTF_TRACELOG("remove %s", path);
	rmdir(path);
}

int liu_remove_recursive__(const char *path, const struct stat *, int typeflag, struct FTW *)
{
	switch (typeflag) {
	case FTW_F:
		liu_remove_file(path);
		break;
	case FTW_DP:
		liu_remove_dir(path);
		break;
	}

	return 0;
}

void liu_remove_recursive(const char *path)
{
	nftw(path, &liu_remove_recursive__, 8, FTW_DEPTH | FTW_PHYS);
}

void liu_replace_binary(void)
{
	char current_binary[BUFSIZ];
	char dead_binary[BUFSIZ];
	char new_binary[BUFSIZ];


	// On windows and the file is bare (gcc automatically adds ".exe")
#ifdef _WIN32
	if (!strchr(liu_config_binary_name, '.')) {
		snprintf(current_binary, sizeof current_binary, "%s.exe", liu_config_binary_name);
		snprintf(dead_binary, sizeof dead_binary, "%s.exe.bak", liu_config_binary_name);
		snprintf(new_binary, sizeof current_binary, "~%s.exe", liu_config_binary_name);
	} else
#endif
	// On linux or the file has an extension
	{
		snprintf(current_binary, sizeof current_binary, "%s", liu_config_binary_name);
		snprintf(dead_binary, sizeof dead_binary, "%s.bak", liu_config_binary_name);
		snprintf(new_binary, sizeof current_binary, "~%s", liu_config_binary_name);
	}

	liu_remove_file(dead_binary);
	liu_rename_file(current_binary, dead_binary);
	liu_rename_file(new_binary, current_binary);
}

int liu_build_file(const char *path, const struct stat *, int typeflag, struct FTW *)
{
	if (typeflag == FTW_D) {
		char buf[BUFSIZ];
		snprintf(buf, sizeof buf, "%s/%s", liu_config_object_dir, path);
		liu_mkdir_silent(buf);
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
	liu_add_object_path(obj_name);
	
	// {cc} -o obj/{path} -c {flags} {path}
	// TODO: multi-core this
	char buf[BUFSIZ];
	int ret = snprintf(buf, sizeof buf, "%s -o %s/%s -c %s %s", liu_config_compiler_binary, liu_config_object_dir, obj_name, liu_config_compiler_flags, path);
	LIU_ASSERT(ret >= 0, "Could not compile object");
	LIU_PRINTF_TRACELOG("%s", buf);
	system(buf);

	free(obj_name);

	return 0;
}

void liu_build_all(char *dirname)
{
	liu_mkdir_silent(liu_config_object_dir);
	
	nftw(dirname, &liu_build_file, 8, FTW_PHYS);
	
	char *binary_prefix = liu_config_replace_binary ? "~" : "";

	char buf[BUFSIZ];
	int ret = snprintf(buf, sizeof buf, "%s -o %s%s %s", liu_config_compiler_binary, binary_prefix, liu_config_binary_name, object_paths);
	LIU_PRINTF_TRACELOG("%s", buf);
	LIU_ASSERT(ret >= 0, "Could not link files");
	system(buf);

	if (liu_config_replace_binary) {
		liu_replace_binary();
	}
}

// void run(void)
// {
// 	execl()
// }

void clean(void)
{
	PRINTLN_WARNING("Delete object files?");

	PRINTF_WARNING("exec: " LIU_HIGHLIGHT("remove %s") " [Y/n] ", liu_config_object_dir);

	int in = getc(stdin);
	if (in == '\n' || in == 'y') {
		liu_remove_recursive(liu_config_object_dir);
	}
	while (in != '\n' && in != EOF) {
		in = getc(stdin);
	}

	PRINTLN_WARNING("Delete generated binaries?");
	PRINTF_WARNING("exec: " LIU_HIGHLIGHT("remove %s") " [y/N] ", liu_config_binary_name);
	in = getc(stdin);
	if (in == 'y') {
		remove(liu_config_binary_name);
	}
}

int main(int argc, char** argv)
{
	if (argc == 1) {
		printf(ANSI_RED LIU_HIGHLIGHT("No liu command") "\n\n");
		liu_show_help(argv[0]);
		return 0;
	}

	char *cmd = argv[1];
	if (!strcmp(cmd, "h") || !strcmp(cmd, "help")) {
		liu_show_help(argv[0]);
		return 0;
	}

	if (!strcmp(cmd, "g") || !strcmp(cmd, "generate")) {
		liu_generate_liu_file();
		return 0;
	}
	
	liu_scan_liu_file();

	if (!strcmp(cmd, "b") || !strcmp(cmd, "build")) {
		liu_build_all(liu_config_source_dir);
		liu_run_tests();
		return 0;
	}

	if (!strcmp(cmd, "c") || !strcmp(cmd, "clean")) {
		clean();
		return 0;
	}

	PRINTF_FATAL("unknown command " ANSI_RED LIU_HIGHLIGHT("%s") "\n", cmd);
	return -1;
}