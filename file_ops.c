#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include "file_ops.h"

int open_file (struct file **fptr, char *name, int flags, int mode)
{
    int err = 0;
    *fptr = filp_open (name, flags, mode);
    if (IS_ERR(*fptr)) {
		err = PTR_ERR(*fptr);
		pr_alert("Error in opening file %s", name);
	} 
    return err;
}

void close_file (struct file **fptr)
{
    if (*fptr != NULL) {
		if (!IS_ERR(*fptr)) {
			pr_info("Cleanup: Closing file");
			filp_close(*fptr, NULL);
		}
	}
}

int mallock (void **buf, int len)
{
    *buf = kmalloc(len, GFP_KERNEL);
    if (*buf == NULL)
        return -ENOMEM;
    return 0;
}

void delete_file(struct file *fp)
{
	inode_lock(fp->f_path.dentry->d_parent->d_inode);
	vfs_unlink(fp->f_path.dentry->d_parent->d_inode, fp->f_path.dentry, NULL);
	inode_unlock(fp->f_path.dentry->d_parent->d_inode);	
	pr_info("File %s successfully deleted\n", fp->f_path.dentry->d_iname);
}

int write_data (char *str, struct file *fp)
{
	ssize_t write_bytes = 0;
	write_bytes = kernel_write(fp, str, strlen(str), &fp->f_pos);
	return write_bytes;
}

int file_stat(const char *name, struct kstat **fstat)
{
    int ret = 0;
    mm_segment_t old_fs;
    *fstat = (struct kstat *)kmalloc(sizeof(struct kstat), GFP_KERNEL);

	if (*fstat == NULL) {
		ret = -ENOMEM;
		pr_alert("Unable to allocate memory for file stat");
		goto out;
	}

    old_fs = get_fs();
	set_fs(KERNEL_DS);
	ret = vfs_stat((const char __user *)name, *fstat);
	set_fs(old_fs);

out:
	return ret;
}

int check_file_type(struct kstat *stat, const char *name)
{
	int ret = 0;

	if (!S_ISDIR(stat->mode)) {
		if (!S_ISREG(stat->mode)) {
			pr_alert("File %s is not a regular file\n", name);
			ret = -EINVAL;
		}
	} else {
		ret = -EINVAL;
		pr_alert("File %s is a directory\n", name);
	}
	return ret;
}

int rename_file (struct file *input, struct file *output)
{
	int ret = 0;
	lock_rename(input->f_path.dentry->d_parent, output->f_path.dentry->d_parent);
	ret = vfs_rename(input->f_path.dentry->d_parent->d_inode,
						input->f_path.dentry, output->f_path.dentry->d_parent->d_inode,
						output->f_path.dentry, NULL, 0);
	unlock_rename(input->f_path.dentry->d_parent, output->f_path.dentry->d_parent);
	return ret;
}

int concat_file (struct file *kinfptr, struct file *koutfptr)
{
    ssize_t read_bytes = 0, write_bytes = 0;
    int err = 0;
    void *buf = NULL;

    err = mallock(&buf, PAGE_SIZE);
    if (err < 0)
        goto out;

    while ((read_bytes = kernel_read(kinfptr, buf, PAGE_SIZE, &kinfptr->f_pos)) > 0) {
        write_bytes = kernel_write(koutfptr, buf, read_bytes, &koutfptr->f_pos);
        if (write_bytes < 0) {
            err = write_bytes;
            pr_alert ("Error in writing to a file");
            goto out;
        }
    }

out:
    if (buf)
        kfree(buf);
    return err;
}

int write_status(int job_id, int user_id, enum TASK_STATUS task_status){

	void *path = NULL;
	int err = 0;
	struct file* fptr = NULL;
	int bytes_wrote = 0;
	void *job_status = NULL;

	err = mallock(&path, PATH_MAX);

	if(err < 0){
		pr_err("\n Cannot allocate memory for storing file path for %d id \n", job_id);
		goto out;
	}

	err = mallock(&job_status, PATH_MAX);

	if(err < 0){
		pr_err("\n Cannot allocate memory for storing file path for %d id \n", job_id);
		goto out;
	}

	memset(path, '\0', PATH_MAX);
	memset(job_status, '\0', PATH_MAX);

	sprintf(path, "/tmp/%d-%d.txt", user_id, job_id);

	sprintf(job_status, "%d", task_status);


	err = open_file(&fptr, path, O_WRONLY | O_CREAT, 700);

	if(err < 0){
		pr_err("\n Cannot open file for %d id \n", job_id);
		goto out;
	}

	bytes_wrote = kernel_write(fptr, job_status, strlen(job_status), &fptr->f_pos);

	if(bytes_wrote < 0){
		err = -EINVAL;
		pr_err("\n Cannot write to file for %d id \n", job_id);
		goto out;
	}

out:

	if(path)
		kfree(path);

	if(job_status)
		kfree(job_status);
	
	if(fptr && !IS_ERR(fptr))
		filp_close(fptr, NULL);
	
	return err;

}