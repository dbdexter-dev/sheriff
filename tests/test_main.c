#include "minunit.h"
#include "test_dir.h"
#include "test_utils.h"

int tests_run = 0;

char *
test_all_dir()
{
	mu_run_test(test_clear_dir_selection);
	mu_run_test(test_init_listing);
	mu_run_test(test_try_select);
	mu_run_test(test_sort_tree);
	return NULL;
}

char *
test_all_utils()
{
	mu_run_test(test_octal);
	mu_run_test(test_join);
	mu_run_test(test_strcasestr);
	mu_run_test(test_strchomp);
	mu_run_test(test_tohuman);
	return NULL;
}

int
main(int argc, char **argv)
{
	char *res;

	fprintf(stderr, "Testing utils.c\n");
	res = test_all_utils();
	if (res) {
		fprintf(stderr, "%s\n", res);
		goto end;
	}

	fprintf(stderr, "Testing dir.c\n");
	res = test_all_dir();
	if (res) {
		fprintf(stderr, "%s\n", res);
		goto end;
	}

	if (res) {
		fprintf(stderr, "%s\n", res);
	} else {
		fprintf(stderr, "ALL TESTS PASSED\n");
		goto end;
	}

end:
	fprintf(stderr, "Tests run: %d\n", tests_run);
	return 0;
}
