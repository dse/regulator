#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#define BUFSIZE 1024

int main(int argc, char **argv) {
    pa_simple *s;
    pa_sample_spec ss;
    int error;
    char ss_string[BUFSIZE];
    size_t bytes_per_update;
    unsigned char *data;
    size_t i, j, k, l;
    unsigned char sample;
    unsigned char sample_max;
    unsigned char sample_min;
    long counter;
    pa_usec_t latency;
    pa_buffer_attr ba;

    ss.format   = PA_SAMPLE_U8;
    ss.channels = 1;
    ss.rate     = 44100;
    bytes_per_update = pa_bytes_per_second(&ss) / 10;

    ba.maxlength = bytes_per_update;
    ba.fragsize  = bytes_per_update;

    pa_sample_spec_snprint(ss_string, BUFSIZE, &ss);

    fprintf(stderr, "%s: %s\n", argv[0], ss_string);

    if (!pa_sample_spec_valid(&ss)) {
        fprintf(stderr, "%s: invalid sample specification\n", argv[0]);
        exit(1);
    }
    if (!pa_sample_rate_valid(ss.rate)) {
        fprintf(stderr, "%s: invalid sample rate\n", argv[0]);
        exit(1);
    }

    s = pa_simple_new(NULL,     /* server name or default (localhost) */
                      argv[0],  /* name */
                      PA_STREAM_RECORD, /* direction */
                      NULL,             /* device name or default */
                      "record",         /* stream name */
                      &ss,              /* sample type */
                      NULL,             /* channel map */
                      &ba,              /* buffering attributes */
                      &error);          /* error code */
    if (!s) {
        fprintf(stderr, "%s: pa_simple_new() failed: %s\n", argv[0], pa_strerror(error));
        exit(1);
    }

    if ((latency = pa_simple_get_latency(s, error)) == (pa_usec_t)-1) {
        fprintf(stderr, "%s: pa_simple_get_latency() failed: %s\n", pa_strerror(error));
        exit(1);
    }
    printf("latency is %f seconds\n", 0.000001 * latency);

    data = (unsigned char *)malloc(bytes_per_update);
    counter = 0;

    for (;;) {
        if (pa_simple_read(s, data, bytes_per_update, &error) < 0) {
            fprintf(stderr, "%s: pa_simple_read failed: %s\n", argv[0], pa_strerror(error));
            exit(1);
        }
        sample_min = 255;
        sample_max = 0;
        for (i = 0; i < bytes_per_update; i += 1) {
            sample = data[i];
            if (sample < sample_min) {
                sample_min = sample;
            }
            if (sample > sample_max) {
                sample_max = sample;
            }
        }
        j = sample_min / 4;
        k = sample_max / 4;
        printf("%7ld %3d %3d ", counter, sample_min, sample_max);
        for (l = 0; l < 255 / 4; l += 1) {
            putchar(l < j ? ' ' : l <= k ? '-' : ' ');
        }
        putchar('\n');
        fflush(stdout);
        counter += 1;
    }
}
