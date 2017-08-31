/**
 * Parse /proc/net/wireless
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct wless_info {
    char iface[64];
    long status;

    /* Quality */
    long q_link;
    long q_level;
    long q_noise;

    /* Discarded packets */
    long disc_nwid;
    long disc_crypt;
    long disc_frag;
    long disc_retry;
    long disc_misc;

    long missed_beacon;
};

/**
 * Parse one line from /proc/net/wireless into a wless_info struct
 */
struct wless_info parse_wireless_line(char *line)
{
    struct wless_info result;
    char *token = strtok(line, " ");
    strcpy(result.iface, token);

    token = strtok(NULL, " ");
    result.status = strtol(token, NULL, 10);

    token = strtok(NULL, " ");
    result.q_link = strtol(token, NULL, 10);

    token = strtok(NULL, " ");
    result.q_level = strtol(token, NULL, 10);

    token = strtok(NULL, " ");
    result.q_noise = strtol(token, NULL, 10);

    token = strtok(NULL, " ");
    result.disc_nwid = strtol(token, NULL, 10);

    token = strtok(NULL, " ");
    result.disc_crypt = strtol(token, NULL, 10);

    token = strtok(NULL, " ");
    result.disc_frag = strtol(token, NULL, 10);

    token = strtok(NULL, " ");
    result.disc_retry = strtol(token, NULL, 10);

    token = strtok(NULL, " ");
    result.disc_misc = strtol(token, NULL, 10);

    token = strtok(NULL, " ");
    result.missed_beacon = strtol(token, NULL, 10);

    return result;
}

/**
 * Get the last line of /proc/net/wireless and extract the rssi value
 */
long get_rssi()
{
    char *line = NULL;
    FILE *fp;
    size_t len = 0;
    ssize_t read;
    struct wless_info result;

    fp = fopen("/proc/net/wireless", "r");
    if (fp) {
        // Throw away first two lines
        getline(&line, &len, fp);
        getline(&line, &len, fp);

        /* To get more than one line, a linked list of wless_info structs
         * should be used here. */
        while ((read = getline(&line, &len, fp)) != -1)
        {
            result = parse_wireless_line(line);
        }
        free(line);
        fclose(fp);
    }
    return result.q_level;
}

int main()
{
    long rssi = get_rssi();
    printf("Wireless link RSSI is: %ld dB\n", rssi);
}
