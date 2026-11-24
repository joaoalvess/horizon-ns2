// SPDX-License-Identifier: GPL-3.0-or-later
#include "upstream_check.h"

#include "self_update.h"
#include "update_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    upstream_check_capacity = 256 * 1024
};

static int content_version_of(const char *tag, char out[12])
{
    size_t length;
    if (tag == NULL || tag[0] != 'v')
        return 0;
    length = strlen(tag + 1);
    if (length != 9 && length != 10)
        return 0;
    (void)snprintf(out, 12, "%s%s", length == 9 ? "0" : "", tag + 1);
    return update_check_version_parse(out, NULL);
}

static const char *skip_space(const char *at, const char *end)
{
    while (at < end && (*at == ' ' || *at == '\t' || *at == '\r' || *at == '\n'))
        ++at;
    return at;
}

static int newest_tag(const char *body, size_t length, char *tag, size_t size)
{
    const char *end = body + length;
    const char *at = skip_space(body, end);
    if (at >= end || *at != '[')
        return 0;
    at = skip_space(at + 1, end);
    if (at >= end || *at != '{')
        return 0;
    return update_check_json_string(at, (size_t)(end - at), "tag_name", tag, size) == 1;
}

upstream_check_state upstream_check_latest_blocking(const char *base_tag, char *latest_tag_out,
                                                    size_t latest_tag_out_size)
{
    const self_update_platform *platform = self_update_console();
    char base[12];
    char latest[12];
    char tag[40];
    char *body;
    size_t length = 0;
    int status = 0;
    int found;
    int comparable = 0;
    int order;
    if (latest_tag_out != NULL && latest_tag_out_size > 0)
        latest_tag_out[0] = '\0';
    if (platform == NULL || !content_version_of(base_tag, base))
        return UPSTREAM_CHECK_UNKNOWN;
    body = (char *)malloc(upstream_check_capacity);
    if (body == NULL)
        return UPSTREAM_CHECK_UNKNOWN;
    found = platform->fetch(platform->user, UPSTREAM_CHECK_RELEASES_URL, body,
                            upstream_check_capacity, &length, &status) == 0 &&
            status == 200 && newest_tag(body, length, tag, sizeof(tag));
    free(body);
    if (!found || !content_version_of(tag, latest))
        return UPSTREAM_CHECK_UNKNOWN;
    order = update_check_version_compare(latest, base, &comparable);
    if (!comparable)
        return UPSTREAM_CHECK_UNKNOWN;
    if (latest_tag_out != NULL && latest_tag_out_size > 0)
        (void)snprintf(latest_tag_out, latest_tag_out_size, "%s", tag);
    return order > 0 ? UPSTREAM_CHECK_NEWER_AVAILABLE : UPSTREAM_CHECK_UP_TO_DATE;
}
