int vmprof_gettimeofday(void *dst)
{
    return gettimeofday((struct timeval *)dst, NULL);
}

int vmprof_gettimezone(void *dst) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strncpy(dst, tm->tm_zone, 5);
    return 0;
}

