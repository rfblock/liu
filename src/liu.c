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
		"Usages: %s command\n"
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
		printf(TEXT_WARNING ".liu file already exists, this will overwrite it\n");
	}

	printf("Enter output binary name (eg. my_program): " ANSI_GREEN);
	scanf("%256[^\n]%*c", file);

	#ifdef ENABLE_COLORS
	printf(ANSI_RESET);
	#endif

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

void liu_scan_liu_file(void)
{
	FILE *fp = fopen(".liu", "rb");
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
	if (object_paths_size + len + 2 >= sizeof object_paths) {
		printf(TEXT_FATAL "Length of object paths exceeds buffer size\n");
		exit(-1);
	}

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
#ifdef _WIN32
	mkdir(path);
#else
	mkdir(path, 755);
#endif
	if (liu_config_enable_tracelog) {
		printf("mkdir %s\n", path);
	}
}

void liu_rename_file(const char *from, const char *to)
{
	if (liu_config_enable_tracelog) {
		printf("rename %s %s\n", from, to);
	}
	rename(from, to);
}

void liu_remove(const char *path)
{
	if (liu_config_enable_tracelog) {
		printf("remove %s\n", path);
	}
	remove(path);
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

	liu_remove(dead_binary);
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
	if (ret < 0) {
		printf(TEXT_FATAL "Could not compile object\n");
		exit(-1);
	}
	if (liu_config_enable_tracelog) { printf("%s\n", buf); }
	system(buf);

	free(obj_name);

	return 0;
}

void liu_build_all(char *dirname)
{
	printf("BEGIN BUILD\n");
	liu_mkdir_silent(liu_config_object_dir);
	
	nftw(dirname, &liu_build_file, 8, FTW_PHYS);
	
	char *binary_prefix = liu_config_replace_binary ? "~" : "";

	char buf[BUFSIZ];
	int ret = snprintf(buf, sizeof buf, "%s -o %s%s %s", liu_config_compiler_binary, binary_prefix, liu_config_binary_name, object_paths);
	if (liu_config_enable_tracelog) { printf("%s\n", buf); }
	if (ret < 0) {
		printf(TEXT_FATAL "Could not link files\n");
		exit(-1);
	}
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
	printf(TEXT_WARNING "This will delete all object files and generated binaries\n");

	char buf[BUFSIZ];
	// TODO: make `rm` a .liu variable
	snprintf(buf, sizeof buf, "rm -rf %s", liu_config_object_dir);
	printf(TEXT_WARNING "exec: " ANSI_BOLD ANSI_UNDERLINE "%s" ANSI_RESET " [y/N] ", buf);

	int in = getc(stdin);
	if (in == 'y') {
		system(buf);
	}
	while (in != '\n' && in != EOF) {
		in = getc(stdin);
	}

	snprintf(buf, sizeof buf, "rm %s", liu_config_binary_name);
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

	printf(TEXT_FATAL "unknown command " ANSI_RED ANSI_BOLD ANSI_UNDERLINE "%s" ANSI_RESET "\n", cmd);
	return -1;
}