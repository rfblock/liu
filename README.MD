# LIU Build System
Liu is an experimental C-only build system, similar to make. Instead of using fast, efficient, and reliable build systems such as CMake, liu intends to make building as janky as possible by only using C. Liu was inspired by tsoding's [nob.h](https://github.com/tsoding/nob.h).

## Why "Liu"
LIU (also stylized "liu" or "Liu") is a standalone C-only build system. Liu's name can mean many different things
 * A multitude of chinese characters (i.e. six)
 * An acronym
 * An easy to type out string of three characters

## How to bootstrap LIU
liu can be bootstrapped by compiling it with a minimal gcc command. After cloning the repository,
```
gcc -o liu src/liu.c
```
This will create a perfectly usable liu binary, however it is recommended it to rebuild liu with itself.
```
./liu b
```
If you are brave enough, you can then add/copy your LIU binary into your PATH. Instructions will not be provided because I am not brave enough.

## How to use liu
### Configuration
To create a configuration file, simply run `liu g` to interactively generate one.

The `.liu` file should be placed in the root directory of a project. The file format is very basic, which consists of a key, followed by a space, then the value. All possible configuration variable are shown here:
```
# Required variables
BINARY_NAME a.out
COMPILER_FLAGS -Wall -Werror -Wextra
CC gcc

# Optional variables w/ default values
SOURCE_DIR src/
OBJECT_DIR obj/
TRACELOG off
```
### Building
To build, simply run `liu b`. **TODO**, add `liu d` and `liu r`

## Style guide
If, for whatever reason, you decide to contribute to this, there are a few basic styling guidelines:
 * No more than 3 levels of indentation (tabs, 8 spaces per tab preferred)
 * Forward function declarations should be avoided
 * In documentation, the styling of liu (liu, Liu, or LIU) should be as inconsistent as possible