// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2018 Linaro Limited
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <tee.h>
#include <mmc.h>
#include <dm/device_compat.h>

#include "optee_msg.h"
#include "optee_private.h"

/*
 * Request and response definitions must be in sync with the secure side of
 * OP-TEE.
 */

/* Request */
struct rpmb_req {
	u16 cmd;
#define RPMB_CMD_DATA_REQ      0x00
#define RPMB_CMD_GET_DEV_INFO  0x01
	u16 dev_id;
	u16 block_count;
	/* Optional data frames (rpmb_data_frame) follow */
};

#define RPMB_REQ_DATA(req) ((void *)((struct rpmb_req *)(req) + 1))

/* Response to device info request */
struct rpmb_dev_info {
	u8 cid[16];
	u8 rpmb_size_mult;	/* EXT CSD-slice 168: RPMB Size */
	u8 rel_wr_sec_c;	/* EXT CSD-slice 222: Reliable Write Sector */
				/*                    Count */
	u8 ret_code;
#define RPMB_CMD_GET_DEV_INFO_RET_OK     0x00
#define RPMB_CMD_GET_DEV_INFO_RET_ERROR  0x01
};

static void release_mmc(struct optee_private *priv)
{
	int rc;

	if (!priv->rpmb_mmc || !priv->rpmb_inited)
		return;

	rc = mmc_switch_part(priv->rpmb_mmc, priv->rpmb_original_part);
	if (rc)
		debug("%s: blk_select_hwpart_devnum() failed: %d\n",
		      __func__, rc);

	priv->rpmb_inited = false;
}

static int check_mmc(struct mmc *mmc)
{
	if (!mmc) {
		debug("Cannot find RPMB device\n");
		return -ENODEV;
	}
	if (!(mmc->version & MMC_VERSION_MMC)) {
		debug("Device id is not an eMMC device\n");
		return -ENODEV;
	}
	if (mmc->version < MMC_VERSION_4_41) {
		debug("RPMB is not supported before version 4.41\n");
		return -ENODEV;
	}

	return 0;
}

static struct mmc *get_mmc(struct optee_private *priv, int dev_id)
{
	int rc;

	if (priv->rpmb_mmc && priv->rpmb_inited)
		return priv->rpmb_mmc;

	release_mmc(priv);

	/*
	 * Check if priv->rpmb_mmc was already set from DT node,
	 * otherwise use dev_id provided by OP-TEE OS
	 * and find mmc device by its dev_id
	 */
	if (!priv->rpmb_mmc)
		priv->rpmb_mmc = find_mmc_device(dev_id);

	rc = check_mmc(priv->rpmb_mmc);
	if (rc)
		return NULL;

	if (mmc_init(priv->rpmb_mmc)) {
		log(LOGC_BOARD, LOGL_ERR, "%s:MMC device %d init failed\n", __func__, dev_id);
		return NULL;
	}

	priv->rpmb_original_part = mmc_get_blk_desc(priv->rpmb_mmc)->hwpart;

	rc = mmc_switch_part(priv->rpmb_mmc, MMC_PART_RPMB);
	if (rc) {
		debug("Device id %d: cannot select RPMB partition: %d\n",
		      dev_id, rc);
		return NULL;
	}

	priv->rpmb_inited = true;
	return priv->rpmb_mmc;
}

static u32 rpmb_get_dev_info(struct optee_private *priv, u16 dev_id,
			     struct rpmb_dev_info *info)
{
	if (!priv->rpmb_mmc)
		priv->rpmb_mmc = find_mmc_device(dev_id);

	int i;

	if (!priv->rpmb_mmc)
		return TEE_ERROR_ITEM_NOT_FOUND;

	if (mmc_init(priv->rpmb_mmc)) {
		log(LOGC_BOARD, LOGL_ERR, "%s:MMC device %d init failed\n", __func__, dev_id);
		return TEE_ERROR_NOT_SUPPORTED;
	}

	if (!(priv->rpmb_mmc->ext_csd))
		return TEE_ERROR_GENERIC;

	for (i = 0; i < ARRAY_SIZE(priv->rpmb_mmc->cid); i++)
		((u32 *) info->cid)[i] = cpu_to_be32(priv->rpmb_mmc->cid[i]);

	info->rel_wr_sec_c = priv->rpmb_mmc->ext_csd[222];
	info->rpmb_size_mult = priv->rpmb_mmc->ext_csd[168];
	info->ret_code = RPMB_CMD_GET_DEV_INFO_RET_OK;

	return TEE_SUCCESS;
}

static u32 rpmb_process_request(struct optee_private *priv, void *req,
				ulong req_size, void *rsp, ulong rsp_size)
{
	struct rpmb_req *sreq = req;
	struct mmc *mmc;

	if (req_size < sizeof(*sreq))
		return TEE_ERROR_BAD_PARAMETERS;

	switch (sreq->cmd) {
	case RPMB_CMD_DATA_REQ:
		mmc = get_mmc(priv, sreq->dev_id);
		if (!mmc)
			return TEE_ERROR_ITEM_NOT_FOUND;
		if (mmc_rpmb_route_frames(mmc, RPMB_REQ_DATA(req),
					  req_size - sizeof(struct rpmb_req),
					  rsp, rsp_size))
			return TEE_ERROR_BAD_PARAMETERS;
		return TEE_SUCCESS;

	case RPMB_CMD_GET_DEV_INFO:
		if (req_size != sizeof(struct rpmb_req) ||
		    rsp_size != sizeof(struct rpmb_dev_info)) {
			debug("Invalid req/rsp size\n");
			return TEE_ERROR_BAD_PARAMETERS;
		}
		return rpmb_get_dev_info(priv, sreq->dev_id, rsp);

	default:
		debug("Unsupported RPMB command: %d\n", sreq->cmd);
		return TEE_ERROR_BAD_PARAMETERS;
	}
}

void optee_suppl_cmd_rpmb(struct udevice *dev, struct optee_msg_arg *arg)
{
	struct tee_shm *req_shm;
	struct tee_shm *rsp_shm;
	void *req_buf;
	void *rsp_buf;
	ulong req_size;
	ulong rsp_size;

	if (arg->num_params != 2 ||
	    arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_RMEM_INPUT ||
	    arg->params[1].attr != OPTEE_MSG_ATTR_TYPE_RMEM_OUTPUT) {
		arg->ret = TEE_ERROR_BAD_PARAMETERS;
		return;
	}

	req_shm = (struct tee_shm *)(ulong)arg->params[0].u.rmem.shm_ref;
	req_buf = (u8 *)req_shm->addr + arg->params[0].u.rmem.offs;
	req_size = arg->params[0].u.rmem.size;

	rsp_shm = (struct tee_shm *)(ulong)arg->params[1].u.rmem.shm_ref;
	rsp_buf = (u8 *)rsp_shm->addr + arg->params[1].u.rmem.offs;
	rsp_size = arg->params[1].u.rmem.size;

	arg->ret = rpmb_process_request(dev_get_priv(dev), req_buf, req_size,
					rsp_buf, rsp_size);
}

void optee_suppl_rpmb_release(struct udevice *dev)
{
	release_mmc(dev_get_priv(dev));
}
