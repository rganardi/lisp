size_t strlcat(char *dst, const char *src, size_t size) {
	char *d = dst;
	const char *s = src;
	int i = 0;

	while (*d != '\0') {
#if DEBUGCAT
		printf("traversing %c\n", *d);
#endif
		d++;
		i++;
		if (i > size) {
			fprintf(stderr, "size is to small\n");
			*d = '\0';
			return i;
		}
	}

	while (*s != '\0') {
#if DEBUGCAT
		printf("appending %c\n", *s);
#endif
		*(d++) = *(s++);
		i++;
		if (i > size) {
			fprintf(stderr, "src can't fit in dst\n");
			*d = '\0';
			return i;
		}
	}
	*d = '\0';

	return i;
}
