/*
 * No changes are allowed to this file
 */

#include <stdio.h>
#include <elf.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdint.h>

void load_and_run_elf(const char* exe);
void loader_cleanup();
