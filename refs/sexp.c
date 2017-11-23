#include <stdio.h>
#include <stdlib.h>
#include <bsd/string.h>
#include <unistd.h>

#define BUFFER_SIZE 256

static int read_line(int fd, char *buf, size_t bufsiz) {
	/* read at most bufsize chars from fd into buf (including '\0') */
	size_t i = 0;
	char c = '\0';

	do {
		if (read(fd, &c, sizeof(char)) != sizeof(char))
			return -1;
		buf[i++] = c;
	} while (c != '\n' && i < (bufsiz - 1));

	if (buf[i - 1] == '\n') {
		buf[i - 1] = ' '; /* eliminates '\n' */
	}
	buf[i] = '\0';
	return 0;
}

static int walk(char *str, int *level) {
	char *c = NULL;

	for (c = str; *c != '\0'; c++) {
		if (*c == '(') {
			(*level)++;
		} else if (*c == ')') {
			(*level)--;
		}

		if (*level < 0) {
			fprintf(stderr, "unbalanced parens\n");
			*level = 0;
			while (*c != '\0') {
				*c = '\0';
				c++;
			}
			return 1;
		}
		printf("%d", *level);
	}
	return 0;
}

void assert(int test, char *msg) {
	if (!test) {
		fprintf(stderr, "%s\n", msg);
	};
	return;
}

int parse(char *str) {
	printf("parsing %s\n", str);
	char *c = NULL;
	char *p = NULL;
	char *t = NULL;

	t = str;
	assert(*t == '(', "1");
	while (*t != '\0') {
		t++;
	}
	while (*t != ')' && t != str) {
		t--;
	}
	assert(*(--t) == ')', "2");
	//printf("%s\n", *t);

	p = str;
	c = str;

	while (*c != '\0') {
		if (*c == ' ') {
			*c = '\0';
			printf("p: %s\n", p);
			p = ++c;
			continue;
		}
		c++;
	}

	return 0;
}

int main() {
	char *buf;
	char *str;
	size_t i = 0;
	int err = 0;
	int *level;


	if (!(level = calloc(1, sizeof(int)))) {
		fprintf(stderr, "can't initialize level\n");
	}

	if (!(buf = calloc(BUFFER_SIZE, sizeof(buf)))) {
		fprintf(stderr, "can't initialize buf\n");
		exit(1);
	}
	if (!(str = calloc(BUFFER_SIZE, sizeof(str)))) {
		fprintf(stderr, "can't initialize str\n");
		exit(1);
	}

	while (read_line(STDIN_FILENO, buf, BUFFER_SIZE) == 0) {

		fprintf(stdout, "saved %s, read %s\n", str, buf);

		err = walk(buf, level);
		if (err) {
			fprintf(stderr, "error walking\n");
		}

		if (!(realloc(str, strlen(str) + BUFFER_SIZE))) {
			fprintf(stderr, "can't resize str\n");
		}
		i = strlcat(str, buf, strlen(str) + BUFFER_SIZE);

		printf("\ncopied %d\n", i);

		if (i >= strlen(str) + BUFFER_SIZE) {
			fprintf(stderr, "can't append current line\n");
		}

		if (*level == 0 && i > 0) {
			parse(str);
			free(str);
			str = calloc(BUFFER_SIZE, sizeof(str));
		}
	}
	free(str);
	free(level);
	free(buf);
	return 0;
}
