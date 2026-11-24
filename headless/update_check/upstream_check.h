// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HORIZON_UPSTREAM_CHECK_H
#define HORIZON_UPSTREAM_CHECK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define UPSTREAM_CHECK_BASE_TAG "v1.000.004"
#define UPSTREAM_CHECK_RELEASES_URL                                                                \
    "https://api.github.com/repos/heydemoura/EglantinePS5/releases?per_page=1"

    typedef enum upstream_check_state
    {
        UPSTREAM_CHECK_UNKNOWN = 0,
        UPSTREAM_CHECK_UP_TO_DATE = 1,
        UPSTREAM_CHECK_NEWER_AVAILABLE = 2
    } upstream_check_state;

    upstream_check_state upstream_check_latest_blocking(const char *base_tag, char *latest_tag_out,
                                                        size_t latest_tag_out_size);

#ifdef __cplusplus
}
#endif

#endif
