#include <stdio.h>
#include <time.h>

int main() {
    struct timespec requestStart, requestEnd;
    clock_gettime(CLOCK_REALTIME, &requestStart);

    for(int i = 0; i < 99999999; i++) {}

    clock_gettime(CLOCK_REALTIME, &requestEnd);

    double accum = ( requestEnd.tv_sec - requestStart.tv_sec )
        + ( requestEnd.tv_nsec - requestStart.tv_nsec )
        / 1E9;
    printf( "%lf\n", accum );
}
