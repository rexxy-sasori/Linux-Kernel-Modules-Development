#define pr_fmt(fmt) "cs423_mp4: " fmt

#include <linux/lsm_hooks.h>
#include <linux/security.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/binfmts.h>
#include "mp4_given.h"

// rememeber to put the entire directory in the linux security module
// The security modules need to be compiled with the linux kernel together !!
#define XATTR_LEN 256

static int get_inode_sid(struct inode *inode);
static int mp4_bprm_set_creds(struct linux_binprm *bprm);
static int mp4_cred_alloc_blank(struct cred *cred, gfp_t gfp);
static void mp4_cred_free(struct cred *cred);
static int mp4_cred_prepare(struct cred *new, const struct cred *old, gfp_t gfp);
static int mp4_inode_init_security(struct inode *inode, struct inode *dir, const struct qstr *qstr,const char **name, void **value, size_t *len);
static int mp4_has_permission(int ssid, int osid, int mask);
static int mp4_inode_permission(struct inode *inode, int mask);


/**
 * get_inode_sid - Get the inode mp4 security label id
 *
 * @inode: the input inode
 *
 * @return the inode's security id if found.
 *
 */
static int get_inode_sid(struct inode *inode)
{
	/*
	 * Add your code here
	 * ...
	 */
	char * xattr_buf;
	struct dentry * de;
	int xattr_ret = 0;
	int xattr_cred = 0;

	// if(printk_ratelimit()){
	// 	pr_info("FUNC %s CALLED\n", __func__);
	// }
	
	if(inode == NULL){
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	de = d_find_alias(inode);
	
	if(de == NULL){
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	xattr_buf = (char *)kmalloc(XATTR_LEN * sizeof(char), GFP_KERNEL);

	if(xattr_buf == NULL){
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		dput(de);
		kfree(xattr_buf);
		return -ENOMEM;
	}

	memset(xattr_buf, 0, XATTR_LEN);

	if(inode->i_op->getxattr != NULL){
		xattr_ret = inode->i_op->getxattr(de, XATTR_NAME_MP4, xattr_buf, XATTR_LEN);
		dput(de);

		if(xattr_ret < 0 && xattr_ret != -ENODATA){
			pr_err("FAIL IN %s AT %d code: %d\n", __func__, __LINE__, xattr_ret);
			kfree(xattr_buf);
			return MP4_NO_ACCESS;
		} 

		xattr_cred = __cred_ctx_to_sid(xattr_buf);
		kfree(xattr_buf);
		return xattr_cred;

	} else{
		dput(de);
		kfree(xattr_buf);
		return MP4_NO_ACCESS;
	}
}

/**
 * mp4_bprm_set_creds - Set the credentials for a new task
 *
 * @bprm: The linux binary preparation structure
 *
 * returns 0 on success.
 */
static int mp4_bprm_set_creds(struct linux_binprm *bprm)
{
	/*
	 * Add your code here
	 * ...
	 */
	
	int flags;
	struct mp4_security * sec;

	// if(printk_ratelimit()){
	// 	pr_info("FUNC %s CALLED\n", __func__);
	// }

	if(bprm == NULL){
		if(printk_ratelimit()){
			pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		}
		
		return 0;
	}


	if(bprm->cred == NULL){
		if(printk_ratelimit()){
			pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		}
		
		return 0;
	}


	sec = bprm->cred->security;

	if(sec == NULL){
		if(printk_ratelimit()){
			pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		}
		return 0;
	}

	if(bprm->file == NULL){
		if(printk_ratelimit()){
			pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		}
		return 0;
	}


	if(bprm->file->f_inode == NULL){
		if(printk_ratelimit()){
			pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		}
		return 0;
	}


	flags = get_inode_sid(bprm->file->f_inode);
	if(flags == MP4_TARGET_SID){
		sec->mp4_flags = MP4_TARGET_SID;
	}

	return 0;
}

/**
 * mp4_cred_alloc_blank - Allocate a blank mp4 security label
 *
 * @cred: the new credentials
 * @gfp: the atomicity of the memory allocation
 *
 */
static int mp4_cred_alloc_blank(struct cred *cred, gfp_t gfp)
{
	/*
	 * Add your code here
	 * ...
	 */

	struct mp4_security * sec;

	if(cred == NULL){
		if(printk_ratelimit()){
			pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		}

		return -EINVAL;
	}

	sec = (struct mp4_security *)kmalloc(sizeof(struct mp4_security), gfp);

	if(sec == NULL){
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		
		return -ENOMEM;
	}

	sec->mp4_flags = MP4_NO_ACCESS;
	cred->security = sec;

	return 0;
}


/**
 * mp4_cred_free - Free a created security label
 *
 * @cred: the credentials struct
 *
 */
static void mp4_cred_free(struct cred *cred)
{
	/*
	 * Add your code here
	 * ...
	 */

	struct mp4_security * sec;

	if(cred == NULL){
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);

		return;
	}

	if(cred->security == NULL){
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);

		return;
	}

	sec = cred->security; 

	kfree(sec);
	cred->security = NULL;
}

/**
 * mp4_cred_prepare - Prepare new credentials for modification
 *
 * @new: the new credentials
 * @old: the old credentials
 * @gfp: the atomicity of the memory allocation
 *
 */
static int mp4_cred_prepare(struct cred *new, const struct cred *old, gfp_t gfp)
{
	struct mp4_security * sec;

	// if(printk_ratelimit()){
	// 	pr_info("FUNC %s CALLED\n", __func__);
	// }

	// check if input is valid
	if((old == NULL) || (old->security == NULL)){
		mp4_cred_alloc_blank(new, gfp);
		return 0;
	}

	sec = kmemdup(old->security, sizeof(struct mp4_security), gfp);
	
	if(sec == NULL){
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		
		return -ENOMEM;
	}

	new->security = sec;

	return 0;
}

/**
 * mp4_inode_init_security - Set the security attribute of a newly created inode
 *
 * @inode: the newly created inode
 * @dir: the containing directory
 * @qstr: unused
 * @name: where to put the attribute name
 * @value: where to put the attribute value
 * @len: where to put the length of the attribute
 *
 * returns 0 if all goes well, -ENOMEM if no memory, -EOPNOTSUPP to skip
 *
 */
static int mp4_inode_init_security(struct inode *inode, struct inode *dir,
				   const struct qstr *qstr,
				   const char **name, void **value, size_t *len)
{
	/*
	 * Add your code here
	 * ...
	 */
	struct mp4_security * curr_sec;
	
	// if(printk_ratelimit()){
	// 	pr_info("FUNC %s CALLED\n", __func__);
	// } 

	if (current_cred() == NULL) {
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
	    return -EOPNOTSUPP;
	}

	curr_sec = current_cred()->security;

	if (curr_sec == NULL) {
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
	    return -EOPNOTSUPP;
	}

	if (name == NULL){
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		return -EOPNOTSUPP;
	}

	if (value == NULL){
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		return -EOPNOTSUPP;
	}

	if (len == NULL){
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		return -EOPNOTSUPP;
	}


	if(curr_sec->mp4_flags == MP4_TARGET_SID){
		*name = kstrdup(XATTR_MP4_SUFFIX, GFP_KERNEL);
		*value = kstrdup("read-write", GFP_KERNEL);
		*len = strlen(*value);
		return 0;
	}
	
	return -EOPNOTSUPP;
}

/**
 * mp4_has_permission - Check if subject has permission to an object
 *
 * @ssid: the subject's security id
 * @osid: the object's security id
 * @mask: the operation mask
 *
 * returns 0 is access granter, -EACCES otherwise
 *
 */
static int mp4_has_permission(int ssid, int osid, int mask)
{
	/*
	 * Add your code here
	 * ...
	 */
	// if(printk_ratelimit()){
	// 	pr_info("FUNC %s CALLED\n", __func__);
	// }

	int access;

	if (mask & (MAY_NOT_BLOCK)){
		return 0;
	}

	switch(osid){
		case MP4_NO_ACCESS:
			if(ssid == MP4_TARGET_SID){
				access = -EACCES;
			} else{
				access = 0;
			}

			break;
		case MP4_READ_OBJ:
			if (mask & MAY_READ){
				access = 0;
			} else{
				access = -EACCES;
			}

			break;
		case MP4_READ_WRITE:
			if(ssid==MP4_TARGET_SID){
				if (mask & (MAY_READ | MAY_WRITE | MAY_APPEND)){
					access = 0;
				} else{
					access = -EACCES;
				}
			} else{
				if (mask & MAY_READ){
					access = 0;
				} else{
					access = -EACCES;
				}
			}

			break;
		case MP4_WRITE_OBJ:
			if(ssid==MP4_TARGET_SID){
				if (mask & (MAY_WRITE | MAY_APPEND)){
					access = 0;
				} else{
					access = -EACCES;
				}
			} else{
				if (mask == MAY_READ){
					access = 0;
				} else{
					access = -EACCES;
				}
			}

			break;
		case MP4_EXEC_OBJ:
			if (mask & (MAY_READ | MAY_EXEC)){
				access = 0;
			} else{
				access = -EACCES;
			}

			break;
		case MP4_READ_DIR:
			if(ssid==MP4_TARGET_SID){
				if (mask & (MAY_READ | MAY_EXEC | MAY_ACCESS)){
					access = 0;
				} else{
					access = -EACCES;
				}
			} else{
				access = 0;
			}

			break;
		case MP4_RW_DIR:
			access = 0;

			break;
		default:
			access = 0;

			break;
	}

	return access;
}

/**
 * mp4_inode_permission - Check permission for an inode being opened
 *
 * @inode: the inode in question
 * @mask: the access requested
 *
 * This is the important access check hook
 *
 * returns 0 if access is granted, -EACCES otherwise
 *
 */
static int mp4_inode_permission(struct inode *inode, int mask)
{
	/*
	 * Add your code here
	 * ...
	 */

	struct dentry * de;
	char * inode_path;
	char * buf;
	int osid;
	int ssid;
	int permission;
	struct mp4_security * curr_sec;
	struct super_block * sup_block;
	char * info_id = NULL;

	// if(printk_ratelimit()){
	// 	pr_info("FUNC %s CALLED\n", __func__);
	// }

	if (current_cred() == NULL){
		return 0;
	}

	if (inode == NULL){
		return 0;
	}

	de = d_find_alias(inode);
	
	if (de == NULL){
		 return 0;
	}

	buf = (char *)kmalloc(XATTR_LEN * sizeof(char), GFP_KERNEL);
	
	if(buf == NULL){
		pr_alert("FAIL IN %s AT %d\n", __func__, __LINE__);
		dput(de);
		return 0;
	}

	inode_path = dentry_path_raw(de, buf, XATTR_LEN);
	dput(de);

	sup_block = inode->i_sb;
	if(sup_block != NULL){
		info_id = sup_block->s_id;
		if(strcmp(info_id, "proc") == 0){
			kfree(buf);
			return 0;
		}
		
		else if(strcmp(info_id, "dev") == 0){
			kfree(buf);
			return 0;
		}

		else if(strncmp(info_id, "sys", 3) == 0){
			kfree(buf);
			return 0;
		}

		else if(strncmp(info_id, "tmp", 3) == 0){
			kfree(buf);
			return 0;
		}
	}

	if(mp4_should_skip_path(inode_path)){
		kfree(buf);
		return 0;
	}


	if(current_cred()->security == NULL){
		ssid = MP4_NO_ACCESS;
	} else{
		curr_sec = current_cred()->security;
		ssid = curr_sec->mp4_flags;
	}
	
	osid = get_inode_sid(inode);

	permission = mp4_has_permission(ssid, osid, mask);
	if (permission != 0) {
		if(info_id != NULL){
			pr_info("path: %s, super_block: %s ACCESS DENIED: ssid %d, osid %d, mask %d\n", inode_path, info_id, ssid, osid, mask);
		} else{
			pr_info("path %s, ACCESS DENIED: ssid %d, osid %d, mask %d\n", inode_path, ssid, osid, mask);
		}
	    kfree(buf);
	    return -EACCES;
	}

	kfree(buf);
	return 0;
}


/*
 * This is the list of hooks that we will using for our security module.
 */
static struct security_hook_list mp4_hooks[] = {
	/*
	 * inode function to assign a label and to check permission
	 */
	LSM_HOOK_INIT(inode_init_security, mp4_inode_init_security),
	LSM_HOOK_INIT(inode_permission, mp4_inode_permission),

	/*
	 * setting the credentials subjective security label when laucnhing a
	 * binary
	 */
	LSM_HOOK_INIT(bprm_set_creds, mp4_bprm_set_creds),

	/* credentials handling and preparation */
	LSM_HOOK_INIT(cred_alloc_blank, mp4_cred_alloc_blank),
	LSM_HOOK_INIT(cred_free, mp4_cred_free),
	LSM_HOOK_INIT(cred_prepare, mp4_cred_prepare)
};

static __init int mp4_init(void)
{
	/*
	 * check if mp4 lsm is enabled with boot parameters
	 */
	if (!security_module_enable("mp4"))
		return 0;

	pr_info("mp4 LSM initializing..");

	/*
	 * Register the mp4 hooks with lsm
	 */
	security_add_hooks(mp4_hooks, ARRAY_SIZE(mp4_hooks));

	return 0;
}

/*
 * early registration with the kernel
 */
security_initcall(mp4_init);