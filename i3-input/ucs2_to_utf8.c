/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <iconv.h>

static iconv_t conversion_descriptor = 0;

/*
 * Returns the input string, but converted from UCS-2 to UTF-8. Memory will be
 * allocated, thus the caller has to free the output.
 *
 */
char *convert_ucs_to_utf8(char *input) {
	size_t input_size = 2;
	/* UTF-8 may consume up to 4 byte */
	int buffer_size = 8;

	char *buffer = calloc(buffer_size, 1);
        if (buffer == NULL)
                err(EXIT_FAILURE, "malloc() failed\n");
	size_t output_size = buffer_size;
	/* We need to use an additional pointer, because iconv() modifies it */
	char *output = buffer;

	/* We convert the input into UCS-2 big endian */
        if (conversion_descriptor == 0) {
                conversion_descriptor = iconv_open("UTF-8", "UCS-2BE");
                if (conversion_descriptor == 0) {
                        fprintf(stderr, "error opening the conversion context\n");
                        exit(1);
                }
        }

	/* Get the conversion descriptor back to original state */
	iconv(conversion_descriptor, NULL, NULL, NULL, NULL);

	/* Convert our text */
	int rc = iconv(conversion_descriptor, (void*)&input, &input_size, &output, &output_size);
        if (rc == (size_t)-1) {
                perror("Converting to UCS-2 failed");
                return NULL;
	}

	return buffer;
}
