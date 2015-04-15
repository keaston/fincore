#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

void show_incore(int fd, off_t st_size, const char *fname, int show_bytes)
{
	size_t page_size = sysconf(_SC_PAGE_SIZE);
	size_t file_pages, pages_in_core;
	void *map;
	unsigned char *vec;
	size_t i;

	file_pages = (st_size + page_size - 1) / page_size;
	vec = malloc(file_pages);

	if (!vec) {
		perror("malloc");
		goto out_err;
	}
	
	map = mmap(0, st_size, PROT_READ, MAP_SHARED, fd, 0);

	if (map == MAP_FAILED) {
		perror("mmap");
		goto out_err_vec;
	}

	if (mincore(map, st_size, vec)) {
		perror("mincore");
		goto out_err_map;
	}

	pages_in_core = 0;
	for (i = 0; i < file_pages; i++)
		pages_in_core += vec[i] & 1;

	if (show_bytes)
		printf("%s: %zu\n", fname, pages_in_core * page_size);
	else
		printf("%s: %.0f%%\n", fname, pages_in_core * 100.0 / file_pages);

out_err_map:
	munmap(map, st_size);
out_err_vec:
	free(vec);
out_err:
	return;
}
	
int main(int argc, char *argv[])
{
	int i;
	int show_bytes = 0;
	char *execname = argv[0];

	if (argc > 1 && !strcmp(argv[1], "-b"))
	{
		argc--;
		argv++;
		show_bytes = 1;
	}

	if (argc < 2) {
		fprintf(stderr, "Usage: %s [-b] <filename>\n", execname);
		return 1;
	}
	
	for (i = 1; i < argc; i++) {
		int fd;
		struct stat st;

		/* O_NONBLOCK used here so we don't wedge if someone gives us a FIFO. */
		fd = open(argv[i], O_RDONLY | O_NONBLOCK);
		if (fd < 0) {
			perror(argv[i]);
			continue;
		}

		if (fstat(fd, &st)) {
			perror(argv[i]);
			continue;
		}

		if (S_ISREG(st.st_mode) && st.st_size > 0)
			show_incore(fd, st.st_size, argv[i], show_bytes);
		
		close(fd);
	}

	return 0;
}
