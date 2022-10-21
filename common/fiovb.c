//SPDX - License - Identifier:	GPL-2.0+
/*
 * (C) Copyright 2019, Foundries.IO
 * Jorge Ramirez-Ortiz <jorge@foundries.io>
 *
 */

#include <blk.h>
#include <fastboot.h>
#include <fiovb.h>
#include <image.h>
#include <malloc.h>
#include <part.h>
#include <tee/optee_ta_fiovb.h>
#include <tee.h>

static struct udevice *tee;
static uint32_t session;

static int get_open_session(void)
{
	const struct tee_optee_ta_uuid uuid = TA_FIOVB_UUID;
	struct tee_open_session_arg arg = { };

	if (!tee) {
		tee = tee_find_device(tee, NULL, NULL, NULL);
		if (!tee)
			return -ENODEV;
	}

	if (!session) {
		tee_optee_ta_uuid_to_octets(arg.uuid, &uuid);
		if (tee_open_session(tee, &arg, 0, NULL))
			return -ENODEV;

		session = arg.session;
	}

	return 0;
}

static enum fiovb_ret invoke_func(u32 func, ulong num_param,
				  struct tee_param *param)
{
	struct tee_invoke_arg arg = { };

	if (get_open_session())
		return FIOVB_ERROR_IO;

	memset(&arg, 0, sizeof(arg));
	arg.session = session;
	arg.func = func;

	if (tee_invoke_func(tee, &arg, num_param, param))
		return FIOVB_ERROR_IO;

	switch (arg.ret) {
	case TEE_SUCCESS:
		return FIOVB_OK;

	case TEE_ERROR_OUT_OF_MEMORY:
		return FIOVB_ERROR_OOM;

	case TEE_ERROR_STORAGE_NO_SPACE:
		return FIOVB_ERROR_INSUFFICIENT_SPACE;

	case TEE_ERROR_ITEM_NOT_FOUND:
		return FIOVB_ERROR_NO_SUCH_VALUE;

	case TEE_ERROR_ACCESS_CONFLICT:
		return FIOVB_ERROR_ACCESS_CONFLICT;

	case TEE_ERROR_TARGET_DEAD:
		/*
		 * The TA has paniced, close the session to reload the TA
		 * for the next request.
		 */
		tee_close_session(tee, session);
		session = 0;
		tee = NULL;

		return FIOVB_ERROR_IO;
	default:
		return FIOVB_ERROR_IO;
	}
}

enum fiovb_ret fiovb_read(const char *name, size_t blen, u8 *out, size_t *olen)
{
	size_t name_size = strlen(name) + 1;
	struct tee_param param[2] = { };
	struct tee_shm *shm_name = NULL;
	struct tee_shm *shm_buf = NULL;
	enum fiovb_ret rc = 0;

	if (get_open_session())
		return FIOVB_ERROR_IO;

	if (tee_shm_alloc(tee, name_size, TEE_SHM_ALLOC, &shm_name))
		return FIOVB_ERROR_OOM;

	if (tee_shm_alloc(tee, blen, TEE_SHM_ALLOC, &shm_buf)) {
		rc = FIOVB_ERROR_OOM;
		goto free_name;
	}

	memcpy(shm_name->addr, name, name_size);
	memset(param, 0, sizeof(param));

	param[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = shm_name;
	param[0].u.memref.size = name_size;

	param[1].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INOUT;
	param[1].u.memref.shm = shm_buf;
	param[1].u.memref.size = blen;

	rc = invoke_func(TA_FIOVB_CMD_READ_PERSIST_VALUE, 2, param);
	if (rc)
		goto out;

	if (param[1].u.memref.size > blen) {
		rc = FIOVB_ERROR_NO_SUCH_VALUE;
		goto out;
	}

	*olen = param[1].u.memref.size;
	memcpy(out, shm_buf->addr, *olen);
out:
	tee_shm_free(shm_buf);
free_name:
	tee_shm_free(shm_name);

	return rc;
}

enum fiovb_ret fiovb_write(const char *name, size_t len, const u8 *value)
{
	size_t nlen = strlen(name) + 1;
	struct tee_param param[2] = { };
	struct tee_shm *shm_name = NULL;
	struct tee_shm *shm_buf = NULL;
	enum fiovb_ret rc = 0;

	if (!len)
		return FIOVB_ERROR_NO_SUCH_VALUE;

	if (get_open_session())
		return FIOVB_ERROR_IO;

	if (tee_shm_alloc(tee, nlen, TEE_SHM_ALLOC, &shm_name))
		return FIOVB_ERROR_OOM;

	if (tee_shm_alloc(tee, len, TEE_SHM_ALLOC, &shm_buf)) {
		rc = FIOVB_ERROR_OOM;
		goto free_name;
	}

	memcpy(shm_name->addr, name, nlen);
	memcpy(shm_buf->addr, value, len);

	memset(param, 0, sizeof(param));

	param[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = shm_name;
	param[0].u.memref.size = nlen;

	param[1].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[1].u.memref.shm = shm_buf;
	param[1].u.memref.size = len;

	rc = invoke_func(TA_FIOVB_CMD_WRITE_PERSIST_VALUE, 2, param);
	tee_shm_free(shm_buf);

free_name:
	tee_shm_free(shm_name);

	return rc;
}

enum fiovb_ret fiovb_delete(const char *name)
{
	size_t nlen = strlen(name) + 1;
	struct tee_param param[1] = { };
	struct tee_shm *shm_name = NULL;
	enum fiovb_ret rc = 0;

	if (get_open_session())
		return FIOVB_ERROR_IO;

	rc = tee_shm_alloc(tee, nlen, TEE_SHM_ALLOC, &shm_name);
	if (rc)
		return FIOVB_ERROR_OOM;

	memcpy(shm_name->addr, name, nlen);

	memset(param, 0, sizeof(param));

	param[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	param[0].u.memref.shm = shm_name;
	param[0].u.memref.size = nlen;

	rc = invoke_func(TA_FIOVB_CMD_DELETE_PERSIST_VALUE, 1, param);
	tee_shm_free(shm_name);

	return rc;
}
