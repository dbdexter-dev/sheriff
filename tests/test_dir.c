#include <stdio.h>
#include <stdlib.h>
#include "minunit.h"

#include "../src/dir.c"

static Direntry* mockup_dir();
const int dirsize = 256;
const int pathsize = 128;

/* Auxiliary functions {{{*/
void
dump_tree_names(Direntry* dir)
{
	int i;
	for (i=0; i<dir->count; i++) {
		printf("%s\n", dir->tree[i]->name);
	}
	printf("\n");
}

Direntry*
mockup_dir()
{
	FILE *fd;
	Direntry* ret;
	int i;

	fd = fopen("/dev/urandom", "r+");          /* :^) */
	if (!fd) {
		fprintf(stderr, "Shit's on fire, yo!\n");
		exit(-1);
	}

	ret = safealloc(sizeof(*ret));
	ret->tree = safealloc(sizeof(*ret->tree) * dirsize);
	ret->count = dirsize;
	ret->max_nodes = dirsize;
	ret->sel_idx = 0;

	ret->path = safealloc(sizeof(*ret->path) * (pathsize + 1));
	fread(ret->path, sizeof(*ret->path), pathsize, fd);
	ret->path[pathsize-1] = '\0';

	for (i=0; i<dirsize; i++) {
		ret->tree[i] = safealloc(sizeof(*ret->tree[i]));
		fread(ret->tree[i], sizeof(ret->tree[i]), 1, fd);
		ret->tree[i]->selected = ret->tree[i]->selected & 1;
	}

	fclose(fd);
	return ret;
}
/*}}}*/

char*
test_clear_dir_selection()
{
	Direntry *dir;
	int i, sel;

	dir = mockup_dir();
	clear_dir_selection(dir);
	for (i=0; i<dirsize; i++) {
		sel = dir->tree[i]->selected;
		mu_assert("test_clear_dir_selection didn't clear", !sel);
	}

	free_listing(&dir);
	return NULL;
}

char*
test_init_listing()
{
	Direntry *dir = NULL;
	const char path[] = "/sys";
	const char path2[] = "/proc";

	init_listing(&dir, path);

	mu_assert("test_init_listing not alloc'd", dir);
	mu_assert("test_init_listing not properly init'd", dir->path && dir->tree);
	mu_assert("Alloc'd more/less than needed", dir->count == dir->max_nodes - 2);

	init_listing(&dir, path2);

	mu_assert("test_init_listing 2 not alloc'd", dir);
	mu_assert("test_init_listing 2 not properly init'd", dir->path && dir->tree);
	mu_assert("Alloc'd less 2 than needed", dir->count <= dir->max_nodes - 2);

	free_listing(&dir);
	return NULL;
}

char*
test_sort_tree()
{
	Direntry *dir;
	int i;
	int order;

	dir = mockup_dir();
	mu_assert("test_sort_tree ended prematurely", !sort_tree(dir));
	for (i=1; i<dirsize && S_ISDIR(dir->tree[i]->mode); i++) {
		order = strcasecmp(dir->tree[i-1]->name, dir->tree[i]->name);
		mu_assert("test_sort_tree dir out-of-order detected", order <= 0);
	}
	for (i++; i<dirsize; i++) {
		order = strcasecmp(dir->tree[i-1]->name, dir->tree[i]->name);
		mu_assert("test_sort_tree file out-of-order detected", order <= 0);
	}

	free_listing(&dir);
	return NULL;
}

char*
test_try_select()
{
	int i, ci;
	Direntry* dir;
	dir = mockup_dir();

	for (i=-10; i<dirsize+10; i++) {
		ci = (i < 0 ? 0 : (i >= dirsize ? dirsize-1 : i));

		try_select(dir, i, 1);
		mu_assert("test_try_select didn't update sel_idx", dir->sel_idx == ci);
	}
	free_listing(&dir);
	return NULL;
}

