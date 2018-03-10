#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "minunit.h"

#include "../src/utils.c"

#define TPSIZE 5
#define NEEDLECOUNT 6

char *
test_octal()
{
	int i;
	int len;
	char buf[32];

	octal_to_str(0755, buf);
	mu_assert("octal_to_str conv 1 failed", !strcmp(buf, "-rwxr-xr-x"));
	octal_to_str(0001, buf);
	mu_assert("octal_to_str conv 2 failed", !strcmp(buf, "---------x"));

	for (i=0; i<077777; i++) {
		octal_to_str(i, buf);
		len = strlen(buf);
		mu_assert("octal_to_str strlen != 10", len == 10);
	}

	return NULL;
}

char *
test_join()
{
	const char a[] = "first/path/";
	const char *tp[TPSIZE] = { "normal/path", "/", "\xac", "\x00", "" };
	int i, size_delta;
	char *test;

	for (i=0; i<TPSIZE; i++) {
		test = NULL;
		test = join_path(a, tp[i]);
		mu_assert("test_join ret'd NULL", test);
		size_delta = strlen(a) + strlen(tp[i]) + strlen("/") - strlen(test);
		mu_assert("test_join size differs", !size_delta);
		free(test);
	}

	return NULL;
}

char *
test_strcasestr()
{
	const char haystack[] = "findthedeadmouseinthismessyenvironment";
	const char *needles[NEEDLECOUNT] = { "mouse", "MoUsE", "nothing", "ss", "dm", "messs" };
	const char *ptrs[NEEDLECOUNT] = { haystack+11, haystack+11, NULL, haystack+24, haystack+10, NULL};
	char *ptr;
	int i;

	for (i=0; i<NEEDLECOUNT; i++) {
		ptr = strcasestr(haystack, needles[i]);
		mu_assert("substring matching failed", ptr == ptrs[i]);
	}

	return NULL;
}

char *
test_strchomp()
{
	const int destsize = 256;
	const char *chomp[TPSIZE] = { "ke",
	                              "smallstring",
	                              "amediumstring",
	                              "amoderatelylongstringthatwillbetruncatedhopefully",
	                              ""
	                            };
	char dest[destsize];
	int i, j;
	int size_delta;

	for (i=32; i>=0; i--) {
		for (j=0; j<TPSIZE; j++) {
			memset(dest, '\0', destsize);
			strchomp(chomp[j], dest, i);
			size_delta = MIN(strlen(chomp[j]), i) - strlen(dest);
			mu_assert("test_strchomp size differs", !size_delta);
		}
	}
	return NULL;
}

char *
test_tohuman()
{
	unsigned long i;
	char buf[10];

	for (i=0; i<LONG_MAX; i *= 18) {
		tohuman(i, buf);
		mu_assert("test_tohuman size f'd up", strlen(buf) <= 6);
		i++;
	}
	return NULL;
}
