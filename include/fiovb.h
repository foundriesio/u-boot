/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2019, Foundries.IO
 * Jorge Ramirez-Ortiz <jorge@foundries.io>
 */
#ifndef	_FIOVB_H
#define _FIOVB_H

#include <common.h>
#include <linux/types.h>
/*
 * FIOVB_OK
 * FIOVB_ERROR_IO: hardware I/O error.
 * FIOVB_ERROR_OOM:  unable to allocate memory.
 * FIOVB_ERROR_NO_SUCH_VALUE: persistent value does not exist.
 * FIOVB_ERROR_INVALID_VALUE_SIZE: named persistent value size is
 *					     not supported or does not match the
 *      				     expected size.
 * FIOVB_IO_RESULT_ERROR_INSUFFICIENT_SPACE: buffer too small for the requested
 *					     operation.
 * FIOVB_IO_RESULT_ERROR_ACCESS_CONFLICT: persistent object already exists and
 *					  no permission to overwrite.
 */
enum fiovb_ret {
	FIOVB_OK = 0,
	FIOVB_ERROR_OOM,
	FIOVB_ERROR_IO,
	FIOVB_ERROR_NO_SUCH_VALUE,
	FIOVB_ERROR_INVALID_VALUE_SIZE,
	FIOVB_ERROR_INSUFFICIENT_SPACE,
	FIOVB_ERROR_ACCESS_CONFLICT,
};

enum fiovb_ret fiovb_read(const char *name, size_t len, u8 *out, size_t *olen);
enum fiovb_ret fiovb_write(const char *name, size_t len, const u8 *val);
enum fiovb_ret fiovb_delete(const char* name);


#endif /* _FIOVB_H */
