//SPDX - License - Identifier:	GPL-2.0+
/*
 * (C) Copyright 2019, Foundries.IO
 * Jorge Ramirez-Ortiz <jorge@foundries.io>
 *
 */

#include <common.h>
#include <command.h>
#include <env.h>
#include <fiovb.h>
#include <image.h>
#include <linux/types.h>
#include <malloc.h>
#include <mmc.h>
#include <asm/arch/sys_proto.h>

enum fiovb_op { fiovb_rd, fiovb_wr, fiovb_del };

static int update_environment(enum fiovb_op op, const char *name,
			      const char *val, size_t len)
{
	char fiovb_name[40] = { };
	char fiovb_val[32] = { };

	switch (op) {
	case fiovb_rd:
		printf("Read %zu bytes [%s]\n", len, val);
		snprintf(fiovb_name, sizeof(fiovb_name), "fiovb.%s", name);
		snprintf(fiovb_val, sizeof(fiovb_val), "%s", val);
		env_set(fiovb_name, fiovb_val);
		break;
	case fiovb_wr:
		printf("Wrote %zu bytes\n", strlen(val) + 1);
		snprintf(fiovb_name, sizeof(fiovb_name), "fiovb.%s", name);
		env_set(fiovb_name, val);
		break;
	case fiovb_del:
		printf("Deleted %s\n", name);
		snprintf(fiovb_name, sizeof(fiovb_name), "fiovb.%s", name);
		env_set(fiovb_name, NULL);
		break;
	default:
		printf("Invalid operation");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

int do_fiovb_read(struct cmd_tbl *c, int flag, int argc, char *const argv[])
{
	enum fiovb_ret err = FIOVB_OK;
	const char *name = NULL;
	char *val = NULL;
	char *p = NULL;
	size_t bytes = 0;
	size_t olen = 0;
	int ret = 0;

	if (argc != 3)
		return CMD_RET_USAGE;

	bytes = simple_strtoul(argv[2], &p, 10);
	if (*p && *p != '\n')
		return CMD_RET_USAGE;

	name = argv[1];
	val = malloc(bytes);
	if (!val)
		return CMD_RET_FAILURE;

	err = fiovb_read(name, bytes, val, &olen);
	if (err) {
		printf("fiovb read failed (err = %d)\n", err);
		free(val);
		return CMD_RET_FAILURE;
	}

	ret = update_environment(fiovb_rd, name, val, olen);
	free(val);

	return ret;
}

int do_fiovb_write(struct cmd_tbl *c, int flag, int argc, char *const argv[])
{
	enum fiovb_ret err = FIOVB_OK;
	const char *name = NULL;
	const char *val = NULL;

	if (argc != 3)
		return CMD_RET_USAGE;

	name = argv[1];
	val = argv[2];

	err = fiovb_write(name, strlen(val) + 1, val);
	if (err) {
		printf("fiovb write failed (err = %d)\n", err);
		return CMD_RET_FAILURE;
	}

	return update_environment(fiovb_wr, name, val, 0);
}

int do_fiovb_delete(struct cmd_tbl *c, int flag, int argc, char *const argv[])
{
	enum fiovb_ret err = FIOVB_OK;
	const char *name = NULL;

	if (argc != 2)
		return CMD_RET_USAGE;

	name = argv[1];

	err = fiovb_delete(name);
	if (err) {
		printf("fiovb delete failed (err = %d)\n", err);
		return CMD_RET_FAILURE;
	}

	return update_environment(fiovb_del, name, NULL, 0);
}

int do_fiovb_init(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	return CMD_RET_SUCCESS;
}

#define LEGACY_OPS \
	U_BOOT_CMD_MKENT(delete_pvalue, 2, 0, do_fiovb_delete, "", ""), \
	U_BOOT_CMD_MKENT(write_pvalue, 3, 0, do_fiovb_write, "", ""),\
	U_BOOT_CMD_MKENT(read_pvalue, 3, 0, do_fiovb_read, "", ""),\
	U_BOOT_CMD_MKENT(init, 2, 0, do_fiovb_init, "", "")

static struct cmd_tbl cmd_fiovb[] = {
	U_BOOT_CMD_MKENT(delete, 2, 0, do_fiovb_delete, "", ""),
	U_BOOT_CMD_MKENT(write, 3, 0, do_fiovb_write, "", ""),
	U_BOOT_CMD_MKENT(read, 3, 0, do_fiovb_read, "", ""),
	LEGACY_OPS,
};

static int do_fiovb(struct cmd_tbl *cmdtp, int flag, int argc,
		    char * const argv[])
{
	struct cmd_tbl *cp;

	cp = find_cmd_tbl(argv[1], cmd_fiovb, ARRAY_SIZE(cmd_fiovb));

	argc--;
	argv++;

	if (!cp || argc > cp->maxargs)
		return CMD_RET_USAGE;

	if (flag == CMD_FLAG_REPEAT)
		return CMD_RET_FAILURE;

	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(
	fiovb, 29, 0, do_fiovb,
	"Foundries.io Verified Boot\n"
	" - valid names: m4hash, bootcount, upgrade_available, rollback",
	"\n\tread   <name> <bytes>  - reads   persistent value <name>\n"
	"\twrite  <name> <value>  - writes  persistent value <name>\n"
	"\tdelete <name>          - delete  persistent value <name>\n"
);
