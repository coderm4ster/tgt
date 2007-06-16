#ifndef __TARGET_DAEMON_H
#define __TARGET_DAEMON_H

#include "log.h"

#define SCSI_ID_LEN	24
#define SCSI_SN_LEN	8
#define VERSION_DESCRIPTOR_LEN 8

#define VENDOR_ID	"IET"

#define _TAB1 "    "
#define _TAB2 _TAB1 _TAB1
#define _TAB3 _TAB1 _TAB1 _TAB1
#define _TAB4 _TAB2 _TAB2

enum scsi_target_state {
	SCSI_TARGET_OFFLINE = 1,
	SCSI_TARGET_RUNNING,
};

enum scsi_lu_state {
	SCSI_LU_OFFLINE = 1,
	SCSI_LU_RUNNING,
};

struct tgt_cmd_queue {
	int active_cmd;
	unsigned long state;
	struct list_head queue;
};

struct lu_phy_attr {
	char scsi_id[SCSI_ID_LEN];
	char scsi_sn[SCSI_SN_LEN];

	/* SCSI Inquiry Params */
	char vendor_id[9];
	char product_id[17];
	char product_rev[5];
	uint16_t version_desc[VERSION_DESCRIPTOR_LEN];

 	char device_type;	/* Peripheral device type */
 	char qualifier;		/* Peripheral Qualifier */
	char removable;		/* Removable media */
	char online;		/* Logical Unit online */
	char reset;		/* Power-on or reset has occured */
	char sense_format;	/* Descrptor format sense data supported */
};

struct scsi_lu;
struct scsi_cmd;

struct device_type_operations {
	int (*cmd_perform)(int host_no, struct scsi_cmd *cmd);
};

struct device_type_template {
	unsigned char type;
	char *name;
	char *pid;

	int (*lu_init)(struct scsi_lu *lu);
	void (*lu_exit)(struct scsi_lu *lu);
	int (*lu_config)(struct scsi_lu *lu, char *arg);

	struct device_type_operations ops[256];
};

struct backingstore_template {
	int bs_datasize;
	int (*bs_open)(struct scsi_lu *dev, char *path, int *fd, uint64_t *size);
	void (*bs_close)(struct scsi_lu *dev);
	int (*bs_cmd_submit)(struct scsi_cmd *cmd);
	int (*bs_cmd_done) (struct scsi_cmd *cmd);
};

struct scsi_lu {
	int fd;
	uint64_t addr; /* persistent mapped address */
	uint64_t size;
	uint64_t lun;
	char *path;

	/* the list of devices belonging to a target */
	struct list_head device_siblings;

	struct tgt_cmd_queue cmd_queue;

	enum scsi_lu_state lu_state;

	uint64_t reserve_id;

	/* we don't use a pointer because a lld could change this. */
	struct device_type_template dev_type_template;

	struct backingstore_template *bst;

	/* TODO: needs a structure for lots of device parameters */
	struct lu_phy_attr attrs;
};

struct scsi_cmd {
	struct target *c_target;
	/* linked it_nexus->cmd_hash_list */
	struct list_head c_hlist;
	struct list_head qlist;

	uint64_t dev_id;

	uint64_t uaddr;
	uint32_t len;
	int mmapped;
	struct scsi_lu *dev;
	unsigned long state;

	uint64_t cmd_itn_id;
	uint32_t data_len;
	uint64_t offset;
	uint8_t *scb;
	int scb_len;
	uint8_t lun[8];
	int attribute;
	uint64_t tag;
	uint8_t rw;
	int async;
	int result;
	struct mgmt_req *mreq;

#define SCSI_SENSE_BUFFERSIZE	252
	unsigned char sense_buffer[SCSI_SENSE_BUFFERSIZE];
	int sense_len;

	/* workaround */
	struct list_head bs_list;
};

struct mgmt_req {
	uint64_t mid;
	int busy;
	int function;
	int result;

	/* for kernel llds */
	int host_no;
	uint64_t itn_id;
};

#ifdef USE_KERNEL
extern int kreq_init(void);
#else
static inline int kreq_init(void)	\
{					\
	return 0;			\
}
#endif

extern int kspace_send_tsk_mgmt_res(struct mgmt_req *mreq);
extern int kspace_send_cmd_res(uint64_t nid, int result, struct scsi_cmd *);

extern int ipc_init(void);
extern int tgt_device_create(int tid, uint64_t lun, char *args, int l_type, int backing);
extern int tgt_device_destroy(int tid, uint64_t lun);
extern int tgt_device_update(int tid, uint64_t dev_id, char *name);
extern int device_reserve(struct scsi_cmd *cmd);
extern int device_release(int tid, uint64_t itn_id, uint64_t lun, int force);
extern int device_reserved(struct scsi_cmd *cmd);

extern int tgt_target_create(int lld, int tid, char *args);
extern int tgt_target_destroy(int lld, int tid);
extern char *tgt_targetname(int tid);
extern int tgt_target_show_all(char *buf, int rest);

extern int tgt_bind_host_to_target(int tid, int host_no);
extern int tgt_unbind_host_to_target(int tid, int host_no);
extern int tgt_bound_target_lookup(int host_no);

typedef void (event_handler_t)(int fd, int events, void *data);
extern int tgt_event_add(int fd, int events, event_handler_t handler, void *data);
extern void tgt_event_del(int fd);
extern int tgt_event_modify(int fd, int events);
extern int target_cmd_queue(int tid, struct scsi_cmd *cmd);
extern void target_cmd_done(struct scsi_cmd *cmd);
struct scsi_cmd *target_cmd_lookup(int tid, uint64_t itn_id, uint64_t tag);
extern void target_mgmt_request(int tid, uint64_t itn_id, uint64_t req_id,
				int function, uint8_t *lun, uint64_t tag,
				int host_no);

extern void target_cmd_io_done(struct scsi_cmd *cmd, int result);

extern uint64_t scsi_get_devid(int lid, uint8_t *pdu);
extern int scsi_cmd_perform(int host_no, struct scsi_cmd *cmd);
extern void sense_data_build(struct scsi_cmd *cmd, uint8_t key, uint8_t asc,
			     uint8_t asq);
extern uint64_t scsi_rw_offset(uint8_t *scb);

extern enum scsi_target_state tgt_get_target_state(int tid);
extern int tgt_set_target_state(int tid, char *str);

extern int acl_add(int tid, char *address);
extern void acl_del(int tid, char *address);
extern char *acl_get(int tid, int idx);

extern int account_lookup(int tid, int type, char *user, int ulen, char *password, int plen);
extern int account_add(char *user, char *password);
extern void account_del(char *user);
extern int account_ctl(int tid, int type, char *user, int bind);
extern int account_show(char *buf, int rest);
extern int account_available(int tid, int dir);

extern int it_nexus_create(int tid, uint64_t itn_id, int host_no, char *info);
extern int it_nexus_destroy(int tid, uint64_t itn_id);

#endif
