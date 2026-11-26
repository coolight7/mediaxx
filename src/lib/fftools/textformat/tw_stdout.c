/*
 * Copyright (c) The FFmpeg developers
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "avtextwriters.h"
#include "libavutil/opt.h"

/* mediaxx: 定义在 cmdutils.c；进程内调用时标准输出会被换成临时文件 */
FILE *fftools_stdout_get(void);

/* STDOUT Writer */

# define WRITER_NAME "stdoutwriter"

typedef struct StdOutWriterContext {
    const AVClass *class;
} StdOutWriterContext;

static const char *stdoutwriter_get_name(void *ctx)
{
    return WRITER_NAME;
}

static const AVClass stdoutwriter_class = {
    .class_name = WRITER_NAME,
    .item_name = stdoutwriter_get_name,
};

static inline void stdout_w8(AVTextWriterContext *wctx, int b)
{
    fputc(b, fftools_stdout_get());
}

static inline void stdout_put_str(AVTextWriterContext *wctx, const char *str)
{
    fputs(str, fftools_stdout_get());
}

static inline void stdout_vprintf(AVTextWriterContext *wctx, const char *fmt, va_list vl)
{
    vfprintf(fftools_stdout_get(), fmt, vl);
}


static const AVTextWriter avtextwriter_stdout = {
    .name                 = WRITER_NAME,
    .priv_size            = sizeof(StdOutWriterContext),
    .priv_class           = &stdoutwriter_class,
    .writer_put_str       = stdout_put_str,
    .writer_vprintf       = stdout_vprintf,
    .writer_w8            = stdout_w8
};

int avtextwriter_create_stdout(AVTextWriterContext **pwctx)
{
    int ret;

    ret = avtextwriter_context_open(pwctx, &avtextwriter_stdout);

    return ret;
}
