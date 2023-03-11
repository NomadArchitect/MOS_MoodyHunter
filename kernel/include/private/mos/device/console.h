// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/list.h"
#include "lib/sync/spinlock.h"
#include "mos/device/dm_types.h"
#include "mos/types.h"

typedef enum
{
    CONSOLE_CAP_NONE = 0,
    CONSOLE_CAP_COLOR = 1 << 0,
    CONSOLE_CAP_CLEAR = 1 << 1,
    CONSOLE_CAP_READ = 1 << 2,
    CONSOLE_CAP_SETUP = 1 << 3,
    CONSOLE_CAP_GET_SIZE = 1 << 4,
    CONSOLE_CAP_CURSOR_HIDE = 1 << 5,
    CONSOLE_CAP_CURSOR_MOVE = 1 << 6,
} console_caps_t;

typedef struct console_t console_t;

struct console_t
{
    as_linked_list;

    const char *name;
    console_caps_t caps;

    bool (*setup)(console_t *con);
    bool (*get_size)(console_t *con, u32 *width, u32 *height);

    bool (*set_cursor)(console_t *con, bool show);
    bool (*move_cursor)(console_t *con, u32 x, u32 y);
    bool (*get_cursor)(console_t *con, u32 *x, u32 *y);

    // VGA standard color codes
    bool (*get_color)(console_t *con, standard_color_t *fg, standard_color_t *bg);
    bool (*set_color)(console_t *con, standard_color_t fg, standard_color_t bg);

    // not meant to be called directly, use console_read / console_write instead
    int (*read_impl)(console_t *con, char *dest, size_t size);
    int (*write_impl)(console_t *con, const char *data, size_t size);

    bool (*clear)(console_t *con);
    bool (*close)(console_t *con);

    void *data;
    spinlock_t lock __aligned(8);
};

extern list_head consoles;
void console_register(console_t *con);
console_t *console_get(const char *name);
console_t *console_get_by_prefix(const char *prefix);

int console_read(console_t *con, char *dest, size_t size);
int console_write(console_t *con, const char *data, size_t size);
int console_write_color(console_t *con, const char *data, size_t size, standard_color_t fg, standard_color_t bg);
