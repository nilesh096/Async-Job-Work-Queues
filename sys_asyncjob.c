#include <linux/linkage.h>
#include <linux/slab.h>
#include <linux/moduleloader.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <crypto/hash.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/skcipher.h>
#include <linux/string.h>
#include <asm/current.h>
#include <linux/fs_struct.h>
#include "job_data.h"
#include "file_ops.h"
#include "constants.h"
#include <linux/cred.h>
#include <linux/atomic.h>
#include <linux/timekeeping.h>
#include <linux/ktime.h>
#include "utils.h"
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/delay.h>

asmlinkage extern long (*sysptr)(void* buffer);

struct workqueue_struct* wq;
struct workqueue_struct* high_prio_wq;

DECLARE_HASHTABLE(job_index, 10);

struct job_ctx {
	struct job_data job_info;
	int user_id;
	enum TASK_STATUS job_status;
	struct work_struct work;
};

struct job_index_table {
    int job_id;
	struct job_ctx* job_ctx;
	struct hlist_node node;
};

volatile int size = 0;

void job_status(unsigned int job_id, int user_id, char* res_fname) {

    struct job_index_table* curr = NULL;
    bool found = false;
	int htable_user_id;
	unsigned int htable_job_id;
	struct file* fptr = NULL;
	int ret = 0;
	void* buf = NULL;

	ret = open_file(&fptr, res_fname, O_CREAT| O_WRONLY, 0700);
	if (ret < 0){
		pr_alert("\nFailed to open file %s for writing\n", res_fname);
		goto out;
	}
	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0){
		pr_alert("\nFailed to allocate buffer memory\n");
		goto out;
	}
	memset(buf, '\0', PAGE_SIZE);

	sprintf(buf, "\n Getting Job Status for user %d and job id %u\n", user_id, job_id);
	write_data(buf, fptr);
	memset(buf, '\0', PAGE_SIZE);

    lock_job();
    hash_for_each_possible(job_index, curr, node, job_id){
      
	  htable_user_id = curr->job_ctx->user_id;
	  htable_job_id =  curr->job_ctx->job_info.job_id;

      if((curr != NULL) && (htable_job_id == job_id) && ((htable_user_id == user_id) || (user_id == 0))){

			found = true;

		  	if(work_pending(&(curr->job_ctx->work))){
				sprintf(buf, "\n Job %u is QUEUED \n", job_id);
				write_data(buf, fptr);
				memset(buf, '\0', PAGE_SIZE);
			}

			else if(work_busy(&(curr->job_ctx->work))){
				sprintf(buf, "\n Job %u is RUNNING \n", job_id);
				write_data(buf, fptr);
				memset(buf, '\0', PAGE_SIZE);
			}

            else if(curr->job_ctx->job_status == FINISHED){
				sprintf(buf, "\n Job %u successfully FINISHED \n", job_id);
				write_data(buf, fptr);
				memset(buf, '\0', PAGE_SIZE);
			}

			else{
				sprintf(buf, "\n Job %u finished with ERROR. \n", job_id);
				write_data(buf, fptr);
				memset(buf, '\0', PAGE_SIZE);
			}
        }
    }
	unlock_job();
    if (!found){
        sprintf(buf, "\n Job %u is invalid. \n", job_id);
		write_data(buf, fptr);
		memset(buf, '\0', PAGE_SIZE);
	}
out:
	close_file(&fptr);
	if (buf)
	  kfree(buf);
	  
    
}

void list_jobs(int user_id, char* res_fname){

	struct file* fptr = NULL;
	int ret = 0;
	void* buf = NULL;
	struct job_index_table* curr = NULL;
	unsigned bkt;
	

	ret = open_file(&fptr, res_fname, O_CREAT| O_WRONLY, 0700);
	if (ret < 0){
		pr_alert("\nFailed to open file %s for writing\n", res_fname);
		goto out;
	}
	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0){
		pr_alert("\nFailed to allocate buffer memory\n");
		goto out;
	}
	memset(buf, '\0', PAGE_SIZE);

	sprintf(buf, "\n Listing Jobs for user %d\n", user_id);
	write_data(buf, fptr);
	memset(buf, '\0', PAGE_SIZE);

	lock_job();

	hash_for_each(job_index, bkt, curr, node){
		
		if ((curr->job_ctx->user_id == user_id) || (user_id == 0)) {
			sprintf(buf, "\n Job id %d\n", curr->job_ctx->job_info.job_id);
			write_data(buf, fptr);
			memset(buf, '\0', PAGE_SIZE);
		}
	}
	unlock_job();

out:
	close_file(&fptr);
	if (buf)
		kfree(buf);
}

int cancel_job(unsigned int job_id, int user_id, char* res_fname){
	struct job_index_table* curr = NULL;
	struct hlist_node* to_del = NULL;
	int htable_user_id;
	unsigned int htable_job_id;
	struct file* fptr = NULL;
	int ret = 0;
	void* buf = NULL;

	ret = open_file(&fptr, res_fname, O_CREAT| O_WRONLY, 0700);
	if (ret < 0){
		pr_alert("\nFailed to open file %s for writing\n", res_fname);
		goto out_err;
	}
	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0){
		pr_alert("\nFailed to allocate buffer memory\n");
		goto out_err;
	}
	memset(buf, '\0', PAGE_SIZE);

	sprintf(buf, "\n Cancelling Job %d for user %d\n", job_id, user_id);
	write_data(buf, fptr);
	memset(buf, '\0', PAGE_SIZE);

	
	lock_job();
    hash_for_each_possible(job_index, curr, node, job_id){
      
	  htable_user_id = curr->job_ctx->user_id;
	  htable_job_id =  curr->job_ctx->job_info.job_id;
	  if((curr != NULL) && (htable_job_id == job_id) && ((htable_user_id == user_id) || (user_id == 0))){
		  
		  if(work_pending(&(curr->job_ctx->work))){

				sprintf(buf, "\n Job %u is pending, cancelling work \n", job_id);
				write_data(buf, fptr);
				memset(buf, '\0', PAGE_SIZE);

				if (!cancel_work_sync(&(curr->job_ctx->work))){
					sprintf(buf, "\n Job %u could not be cancelled \n", job_id);
					write_data(buf, fptr);
					memset(buf, '\0', PAGE_SIZE);
					ret = -1;
					goto out;
				}
				else {
					sprintf(buf, "\n Job %u cancelled \n", job_id);
					write_data(buf, fptr);
					memset(buf, '\0', PAGE_SIZE);

					to_del = &(curr->node);
					free_kernel_args(&(curr->job_ctx->job_info));
					kfree(curr->job_ctx);
					kfree(curr);
					break;
				}
			
			}
			else if(work_busy(&(curr->job_ctx->work))){
				sprintf(buf, "\n Job %u is already running, cannot cancel \n", job_id);
				write_data(buf, fptr);
				memset(buf, '\0', PAGE_SIZE);
			}
			else{
				sprintf(buf, "\n Job %u has already finished running, please check status. \n", job_id);
				write_data(buf, fptr);
				memset(buf, '\0', PAGE_SIZE);
			}

			break;
	  }
   }

out:
	if(to_del)
		hash_del(to_del);
	unlock_job();
	
out_err:
       close_file(&fptr);
	   if (buf)
	      kfree(buf);

	return ret;
       
}

int reorder_job(unsigned int job_id, int user_id, unsigned int job_prio, char* result_fname){

	struct job_index_table* curr = NULL;
	int htable_user_id;
	unsigned int htable_job_id;
	struct file* fptr = NULL;
	int ret = 0;
	void* buf = NULL;
	bool queue_status = true;

	ret = open_file(&fptr, result_fname, O_CREAT| O_WRONLY, 0700);
	if (ret < 0){
		pr_alert("\nFailed to open file %s for writing\n", result_fname);
		goto out_err;
	}
	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0){
		pr_alert("\nFailed to allocate buffer memory\n");
		goto out_err;
	}
	memset(buf, '\0', PAGE_SIZE);

	sprintf(buf, "\n Reordering Job %d for user %d to priortity %d\n", job_id, user_id, job_prio);
	write_data(buf, fptr);
	memset(buf, '\0', PAGE_SIZE);

	
	lock_job();
    hash_for_each_possible(job_index, curr, node, job_id){
      
	  htable_user_id = curr->job_ctx->user_id;
	  htable_job_id =  curr->job_ctx->job_info.job_id;
	  if((curr != NULL) && (htable_job_id == job_id) && ((htable_user_id == user_id) || (user_id == 0))){
		  
		  if(work_pending(&(curr->job_ctx->work))){

				sprintf(buf, "\n Job %u is pending, cancelling work \n", job_id);
				write_data(buf, fptr);
				memset(buf, '\0', PAGE_SIZE);

				if (!cancel_work_sync(&(curr->job_ctx->work))){
					sprintf(buf, "\n Job %u could not be cancelled \n", job_id);
					write_data(buf, fptr);
					memset(buf, '\0', PAGE_SIZE);
					ret = -1;
					goto out;
				}
				else {
					sprintf(buf, "\n Job %u cancelled \n", job_id);
					write_data(buf, fptr);
					memset(buf, '\0', PAGE_SIZE);

					if (job_prio == 0){
						sprintf(buf, "\n Job %u to be scheduled on normal queue \n", job_id);
						write_data(buf, fptr);
						memset(buf, '\0', PAGE_SIZE);

						queue_status = queue_work(wq, &(curr->job_ctx->work));
						if (!queue_status){
							sprintf(buf, "\n Job %u failed to be scheduled on normal queue \n", job_id);
							write_data(buf, fptr);
							memset(buf, '\0', PAGE_SIZE);
							ret = -1;
						    goto out;
						}
					} else {
						sprintf(buf, "\n Job %u to be scheduled on high priority queue \n", job_id);
						write_data(buf, fptr);
						memset(buf, '\0', PAGE_SIZE);

						queue_status = queue_work(high_prio_wq, &(curr->job_ctx->work));
						if (!queue_status){
						    sprintf(buf, "\n Job %u failed to be scheduled on high priority queue \n", job_id);
							write_data(buf, fptr);
							memset(buf, '\0', PAGE_SIZE);
							ret = -1;
							goto out;
						}
					}
					break;
				}
			
			}
			else if(work_busy(&(curr->job_ctx->work))){
				sprintf(buf, "\n Job %u is already running, cannot cancel \n", job_id);
				write_data(buf, fptr);
				memset(buf, '\0', PAGE_SIZE);
			}
			else{
				sprintf(buf, "\n Job %u has already finished running, please check status. \n", job_id);
				write_data(buf, fptr);
				memset(buf, '\0', PAGE_SIZE);
			}
	  }
   }

out:
	unlock_job();
	
out_err:
       close_file(&fptr);
	   if (buf)
	      kfree(buf);

	return ret;
       

}

int do_rename_file (char *file1, char *file2)
{
	struct file *fp1 = NULL, *fp2 = NULL;
	int ret = 0;
	struct kstat *fstat = NULL;
	bool is_exist = false;

	ret = open_file (&fp1, file1, O_RDONLY, 0);
	if (ret < 0)
		goto out;
	
	ret = file_stat (file2, &fstat);

	if (ret == 0)
		is_exist = true;

	ret = open_file (&fp2, file2, O_WRONLY|O_CREAT, fp1->f_inode->i_mode);
	if (ret < 0) 
		goto out;

	ret = rename_file (fp1, fp2);
	if (ret < 0) {
		pr_alert ("Rename failed");
		goto out_delete;
	}

out_delete:
	if (!is_exist)
		delete_file (fp2);

out:
	close_file (&fp2);
	close_file (&fp1);
	if (fstat)
		kfree(fstat);
	return ret;
}

int do_concat_file (char *infile, char *outfile)
{
	int ret = 0;
	struct file *kinfptr = NULL, *koutfptr = NULL;

	ret = open_file (&kinfptr, infile, O_RDONLY, 0);
	if (ret < 0)
		goto out;
	
	ret = open_file (&koutfptr, outfile, O_WRONLY|O_CREAT|O_APPEND, 0777);
	if (ret < 0)
		goto out;

	ret = concat_file (kinfptr, koutfptr);
	if (ret < 0)
		goto out;

out:
	close_file (&kinfptr);
	close_file (&koutfptr);
	return ret;
}

int encrypt_decrypt (char *input, char *output, void *enckey, unsigned int flag,
						void *buf, struct file *kfinal, unsigned int job_id)
{
	struct file *kinfptr = NULL, *koutfptr = NULL, *ktmpfptr = NULL;
	int ret = 0;
	bool is_exists = false;
	void *tmp_outfile = NULL;
	void *hash_cipher_key = NULL;
	struct kstat *fstat = NULL;

	ret = mallock (&hash_cipher_key, SHA256_LEN);
	if (ret < 0)
		goto out;
	memset(hash_cipher_key, '\0', SHA256_LEN);

	ret = generate_hash ((const u8 *)enckey, 16, (u8 *)hash_cipher_key);
	if (ret < 0)
		goto out;
	pr_info("Hash of hash is %s", (char *)hash_cipher_key);

	ret = open_file (&kinfptr, input, O_RDONLY, 0);
	if (ret < 0)
		goto out;
	pr_info("File %s opened successfully", input);

	ret = file_stat (output, &fstat);

	if (ret == 0) {
		is_exists = true;
		ret = check_file_type(fstat, output);
		if (ret < 0)
			goto out;
	}
	
	ret = open_file (&koutfptr, output, O_WRONLY|O_CREAT, kinfptr->f_inode->i_mode);
	if (ret < 0)
		goto out;
	pr_info("File %s opened successfully", output);

	ret = mallock (&tmp_outfile, strlen(output) + 14);
	if (ret < 0)
		goto out_delete_file;
	memset(tmp_outfile, '\0', strlen(output) + 14);

	sprintf(tmp_outfile, "%s_%d.tmp", output, current->pid);

	ret = open_file (&ktmpfptr, (char *)tmp_outfile, O_WRONLY|O_CREAT, kinfptr->f_inode->i_mode);
	if (ret < 0)
		goto out_delete_file;
	pr_info("File %s opened successfully", ktmpfptr->f_path.dentry->d_iname);

	if (flag & 0x01) {
		//write the preamble to the temp file
		ret = write_preamble(hash_cipher_key, ktmpfptr);
		if (ret < 0) {
			sprintf(buf, "[Job: %u] ERROR: Failed to write the preamble\n", job_id);
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
			pr_alert("Failed to write preamble in the output file\n");
			goto out_delete_temp;
		}
		sprintf(buf, "[Job: %u] Preamble written successfully\n", job_id);
		write_data (buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
	}
	if (flag & 0x02) {
		//read the preamble from the input file
		ret = read_preamble(hash_cipher_key, kinfptr);
		if (ret < 0) {
			pr_alert("Failed to verfiy the preamble\n");
			sprintf(buf, "[Job: %u] ERROR: Failed to verify preamble\n", job_id);
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
			goto out_delete_temp;
		}
		sprintf(buf, "[Job: %u] Preamble verified successfully\n", job_id);
		write_data (buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
	}

	ret = read_write(kinfptr, ktmpfptr, enckey, flag);
	if (ret < 0) {
		pr_alert("Error in enc/dec\n");
		sprintf(buf, "[Job: %u] ERROR: Failed to encrypt/decrypt file %s, error: %d\n", job_id, input, ret);
		write_data (buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
		goto out_delete_temp;
	} 
	else {
		ret = rename_file (ktmpfptr, koutfptr);
		if (ret < 0) {
			pr_alert("Rename failed\n");
			sprintf(buf, "[Job: %u] ERROR: Failed to encrypt/decrypt file %s, error: %d\n", job_id, input, ret);
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
			goto out_delete_temp;
		} else {
			pr_info("Rename Successful\n");
			sprintf(buf, "[Job: %u] Successfully encrypted/decrypted file %s\n", job_id, input);
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
			goto out;
		}
	}

out_delete_temp:
	delete_file(ktmpfptr);

out_delete_file:
	if (!is_exists)
		delete_file(koutfptr);

out:
	close_file (&kinfptr);
	close_file (&koutfptr);
	close_file (&ktmpfptr);
	if (hash_cipher_key)
		kfree(hash_cipher_key);
	if (tmp_outfile)
		kfree(tmp_outfile);
	if (fstat)
		kfree(fstat);
	return ret;
}

int get_file_hash (char *infile, char *outfile)
{
	int ret = 0;
	struct file *kinfptr = NULL, *koutfptr = NULL;

	ret = open_file (&kinfptr, infile, O_RDONLY, 0);
	if (ret < 0)
		goto out;
	
	ret = open_file (&koutfptr, outfile, O_WRONLY|O_CREAT, 0700);
	if (ret < 0)
		goto out;

	ret = generate_file_hash (kinfptr, koutfptr);
	
	if (ret < 0)
		goto out;
	
out:
	close_file (&kinfptr);
	close_file (&koutfptr);
	return ret;
}

int do_delete_file (char *file_name)
{
	struct file *fp = NULL;
	int err = 0;

	err = open_file (&fp, file_name, O_RDONLY, 0);
	if (err < 0)
		return err;

	delete_file(fp);
	close_file(&fp);
	return err;
}

int prepare_stat (char *outfile, struct kstat *fstat)
{
	int ret = 0;
	void *buf = NULL;
	struct file *fptr = NULL;

	ret = open_file (&fptr, outfile, O_WRONLY|O_CREAT, fstat->mode);
	if (ret < 0)
		goto out;
	
	ret = mallock (&buf, PAGE_SIZE);
	if (ret < 0)
		goto out;

	memset (buf, '\0', PAGE_SIZE);
	sprintf(buf, "Inode Number: %llu\n", fstat->ino);
	pr_info("%s", (char *)buf);
	write_data ((char *)buf, fptr);

	memset (buf, '\0', PAGE_SIZE);
	sprintf(buf, "Mode: %o\n", fstat->mode & 0777);
	pr_info("%s", (char *)buf);
	write_data ((char *)buf, fptr);	

	memset (buf, '\0', PAGE_SIZE);
	sprintf(buf, "NLink: %x\n", fstat->nlink);
	pr_info("%s", (char *)buf);
	write_data ((char *)buf, fptr);

	memset (buf, '\0', PAGE_SIZE);
	sprintf(buf, "Size: %lld\n", fstat->size);
	pr_info("%s", (char *)buf);
	write_data ((char *)buf, fptr);

	memset (buf, '\0', PAGE_SIZE);
	sprintf(buf, "User ID: %d\n", fstat->uid.val);
	pr_info("%s", (char *)buf);
	write_data ((char *)buf, fptr);

	memset (buf, '\0', PAGE_SIZE);
	sprintf(buf, "Group ID: %d\n", fstat->gid.val);
	pr_info("%s", (char *)buf);
	write_data ((char *)buf, fptr);

out:
	if (buf)
		kfree (buf);
	close_file (&fptr);
	return ret;
}
/**
 * @brief This does not DO anything. It is for demo purpose on how to cancel jobs.
 * 
 * @param w 
 */

void no_op_task (struct work_struct* w){
	
	int ret = 0;
	struct job_ctx* job_data_ptr = NULL;
	struct file *kfinal = NULL;
	void *buf = NULL;
	unsigned int job_id = 0;
	enum TASK_STATUS status = QUEUED;
	job_data_ptr = container_of(w, struct job_ctx, work);

	job_id = job_data_ptr->job_info.job_id;

	lock_job();
	job_data_ptr->job_status = RUNNING;
	size -= 1;
	unlock_job();

	ret = open_file (&kfinal, job_data_ptr->job_info.result_fname, O_WRONLY|O_CREAT, 0700);
	
	if (ret < 0) {
		pr_alert ("[Job: %u] ERROR: Failed to open result file\n", job_id);
		status = ERROR;
		goto out;
	}

	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0) {
		pr_alert ("[Job: %u] ERROR: Failed allocate buffer for messages\n", job_id);
		status = ERROR;
		goto out;
	}
	memset (buf, '\0', PAGE_SIZE);
	sprintf (buf, "[Job: %u] Current state: RUNNING\n", job_id);
	write_data (buf, kfinal);
	memset (buf, '\0', PAGE_SIZE);

	msleep(20000);

	sprintf (buf, "[Job: %u] FINISHED successfully\n",
							 job_id);	
	write_data (buf, kfinal);
	memset (buf, '\0', PAGE_SIZE);

	status = FINISHED;

out:
	if (buf)
		kfree(buf);
	close_file (&kfinal);
	lock_job();
	job_data_ptr->job_status = status;
	free_kernel_args(&(job_data_ptr->job_info));
	unlock_job();
}

void stat_task (struct work_struct* w)
{
	int ret = 0, i = 0;
	void *buf = NULL;
	struct file *kfinal = NULL;
	struct job_ctx* job_data_ptr;
	struct kstat *fstat = NULL;
	char* infile_name = NULL, *outfile_name = NULL;
	unsigned int job_id = 0;
	enum TASK_STATUS status = QUEUED;
	job_data_ptr = container_of(w, struct job_ctx, work);

	job_id = job_data_ptr->job_info.job_id;

	lock_job();
	job_data_ptr->job_status = RUNNING;
	size -=1;
	unlock_job();

	ret = open_file (&kfinal, job_data_ptr->job_info.result_fname, O_WRONLY|O_CREAT, 0700);
	
	if (ret < 0) {
		pr_alert ("[Job: %u] ERROR: Failed to open result file\n", job_id);
		goto out;
	}

	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0) {
		pr_alert ("[Job: %u] ERROR: Failed allocate buffer for messages\n", job_id);
		goto out;
	}
	memset (buf, '\0', PAGE_SIZE);
	sprintf (buf, "[Job: %u] Current state: RUNNING\n", job_id);
	write_data (buf, kfinal);
	memset (buf, '\0', PAGE_SIZE);

	for(i=0;i<job_data_ptr->job_info.infile_arr_len;i++)
	{
		infile_name = job_data_ptr->job_info.infile_arr[i];
		outfile_name = job_data_ptr->job_info.outfile_arr[i];

		ret = file_stat(infile_name, &fstat);
		if (ret >= 0) {
			ret = prepare_stat (outfile_name, fstat);
			if (fstat) {
				kfree (fstat);
				fstat = NULL;
			}
			if (ret < 0) {
				status = ERROR;
				sprintf (buf, "[Job: %u] ERROR: failed to get the stat of file %s, error: %d\n",
							 job_id, infile_name, ret);	
				write_data (buf, kfinal);
				memset (buf, '\0', PAGE_SIZE);
				continue;
			}
			sprintf (buf, "[Job: %u] Stat of file %s written successfully to %s\n",
							 job_id, infile_name, outfile_name);	
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);

		} else {
			if (fstat) {
				kfree (fstat);
				fstat = NULL;
			}
			sprintf (buf, "[Job: %u] ERROR: failed to get the stat of file %s, error: %d\n",
							 job_id, infile_name, ret);	
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
			status = ERROR;
		}
	}
	if (status != ERROR) {
		sprintf(buf, "[job: %u] Stat of all files successful\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
		status = FINISHED;
		sprintf(buf, "[job: %u] Final Status: FINISHED\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);

	} else {
		sprintf(buf, "[job: %u] Stat of some/all files failed with error\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
		sprintf(buf, "[job: %u] Final Status: ERROR\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
	}

out:
	if (buf)
		kfree(buf);
	close_file (&kfinal);
	lock_job();
	job_data_ptr->job_status = status;
	free_kernel_args(&(job_data_ptr->job_info));
	unlock_job();
}

void delete_task (struct work_struct* w)
{
	int ret = 0, i = 0;
	void *buf = NULL;
	struct file *kfinal = NULL;
	struct job_ctx* job_data_ptr;
	char* infile_name = NULL;
	unsigned int job_id = 0;
	enum TASK_STATUS status = QUEUED;
	job_data_ptr = container_of(w, struct job_ctx, work);

	job_id = job_data_ptr->job_info.job_id;

	lock_job();
	job_data_ptr->job_status = RUNNING;
	size -= 1;
	unlock_job();

	ret = open_file (&kfinal, job_data_ptr->job_info.result_fname, O_WRONLY|O_CREAT, 0700);
	
	if (ret < 0) {
		pr_alert ("[Job: %u] ERROR: Failed to open result file\n", job_id);
		goto out;
	}

	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0) {
		pr_alert ("[Job: %u] ERROR: Failed allocate buffer for messages\n", job_id);
		goto out;
	}
	memset (buf, '\0', PAGE_SIZE);
	sprintf (buf, "[Job: %u] Current state: RUNNING\n", job_id);
	write_data (buf, kfinal);
	memset (buf, '\0', PAGE_SIZE);


	for(i=0;i<job_data_ptr->job_info.infile_arr_len;i++)
	{
		infile_name = job_data_ptr->job_info.infile_arr[i];
		ret = do_delete_file (infile_name);
		if(ret < 0) {
			sprintf (buf, "[Job: %u] ERROR: Failed to delete the file %s with error %d\n",
					 	job_id, infile_name, ret);
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
			status = ERROR;
		} else {
			sprintf (buf, "[Job: %u] file %s deleted successfully\n", job_id, infile_name);
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
		}
	}

	if (status != ERROR) {
		sprintf(buf, "[job: %u] Deletion of all files successful\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
		status = FINISHED;
		sprintf(buf, "[job: %u] Final Status: FINISHED\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);

	} else {
		sprintf(buf, "[job: %u] Deletion of some/all files failed with error\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
		sprintf(buf, "[job: %u] Final Status: ERROR\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
	}

out:
	if (buf)
		kfree (buf);
	close_file (&kfinal);
	lock_job();
	job_data_ptr->job_status = status;
	free_kernel_args(&(job_data_ptr->job_info));
	unlock_job();
}

void concat_task (struct work_struct* w)
{
	int ret = 0, i = 0;
	unsigned int job_id = 0;
	struct job_ctx* job_data_ptr;
	char* infile_name = NULL, *outfile_name = NULL;
	struct file *kinfptr = NULL, *koutfptr = NULL, *kfinal = NULL;
	void *buf = NULL;
	enum TASK_STATUS status = QUEUED;
	job_data_ptr = container_of(w, struct job_ctx, work);

	job_id = job_data_ptr->job_info.job_id;

	lock_job();
	job_data_ptr->job_status = RUNNING;
	size -= 1;
	unlock_job();

	outfile_name = job_data_ptr->job_info.outfile_arr[0];

	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0) {
		status = ERROR;
		pr_alert ("[job: %u] ERROR: failed to allocated memory for buffer\n", job_id);
		goto out;
	}
	memset (buf, '\0', PAGE_SIZE);

	ret = open_file (&kfinal, job_data_ptr->job_info.result_fname, O_WRONLY|O_CREAT, 0700);
	if (ret < 0) {
		status = ERROR;
		pr_alert ("[job: %u] ERROR: failed to open the result file\n", job_id);
		goto out;
	}

	ret = open_file (&koutfptr, outfile_name, O_WRONLY|O_CREAT, 0700);
	if (ret < 0) {
		status = ERROR;
		sprintf(buf, "[job: %u] ERROR: failed to open file %s with error %d\n",job_id, outfile_name, ret);
		write_data (buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
		goto out;
	}
	else {
		sprintf(buf, "[job: %u] File %s opened successfully\n", job_id, outfile_name);
		write_data (buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
	}

	for(i=0;i<job_data_ptr->job_info.infile_arr_len;i++)
	{
		infile_name = job_data_ptr->job_info.infile_arr[i];
		ret = open_file (&kinfptr, infile_name, O_RDONLY, 0);
		if (ret < 0) {
			status = ERROR;
			sprintf(buf, "[job: %u] ERROR: failed to open file %s with error %d\n", job_id, infile_name, ret);
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
			goto out;
		}
		else {
			sprintf(buf, "[job: %u] File %s opened successfully\n", job_id, infile_name);
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
		}
		ret = concat_file (kinfptr, koutfptr);
		if (ret < 0) {
			sprintf(buf, "[job: %u] ERROR: Failed to concatenate files %s and %s\n", job_id, infile_name, outfile_name);
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
			status = ERROR;
			goto out;
		}
		else {
			sprintf(buf, "[job: %u] Concatenated %s to %s\n", job_id, infile_name, outfile_name);
			write_data (buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
		}

		close_file (&kinfptr);
		kinfptr = NULL;
	}
	sprintf(buf, "[job: %u] Concatenation Successful\n", job_id);
	write_data (buf, kfinal);
	memset (buf, '\0', PAGE_SIZE);
	status = FINISHED;

out:
	close_file (&koutfptr);
	close_file (&kinfptr);
	close_file (&kfinal);
	if (buf)
		kfree (buf);
	lock_job();
	job_data_ptr->job_status = status;
	free_kernel_args(&(job_data_ptr->job_info));
	unlock_job();
}

void rename_task (struct work_struct* w)
{
	int ret = 0, i = 0;
	unsigned int job_id = 0;
	struct job_ctx* job_data_ptr;
	char* infile_name = NULL, *outfile_name = NULL;
	void *buf = NULL;
	struct file *kfinal = NULL;
	enum TASK_STATUS status = QUEUED;
	job_data_ptr = container_of(w, struct job_ctx, work);

	lock_job();
	job_data_ptr->job_status = RUNNING;
	size -= 1;
	unlock_job();

	job_id = job_data_ptr->job_info.job_id;

	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0) {
		status = ERROR;
		pr_alert ("[job: %u] ERROR: failed to allocated memory for buffer\n", job_id);
		goto out;
	}
	memset (buf, '\0', PAGE_SIZE);

	ret = open_file (&kfinal, job_data_ptr->job_info.result_fname, O_WRONLY|O_CREAT, 0700);
	if (ret < 0) {
		status = ERROR;
		pr_alert ("[job: %u] ERROR: failed to open the result file\n", job_id);
		goto out;
	}

	sprintf (buf, "[Job: %u] Current state: RUNNING\n", job_id);
	write_data (buf, kfinal);
	memset (buf, '\0', PAGE_SIZE);

	for(i = 0; i < job_data_ptr->job_info.infile_arr_len; i++)
	{
		infile_name = job_data_ptr->job_info.infile_arr[i];
		outfile_name = job_data_ptr->job_info.outfile_arr[i];
		ret = do_rename_file (infile_name, outfile_name);
		if (ret < 0) {
			sprintf(buf, "[job: %u] ERROR: Renaming file %s to %s failed with error %d\n",
							job_id, infile_name, outfile_name, ret);
			write_data(buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
			status = ERROR;
		}
		else {
			sprintf(buf, "[job: %u] Renaming file %s to %s successful\n", job_id, infile_name, outfile_name);
			write_data(buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
		}
	}

	if (status != ERROR) {
		sprintf(buf, "[job: %u] Renaming of all files successful\n", job_id);
		write_data(buf, kfinal);
		status = FINISHED;
		sprintf(buf, "[job: %u] Final Status: FINISHED\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
	} else {
		sprintf(buf, "[job: %u] Renaming of some/all files failed with error\n", job_id);
		write_data(buf, kfinal);
		sprintf(buf, "[job: %u] Final Status: ERROR\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
	}

out:
	if (buf)
		kfree (buf);
	close_file (&kfinal);
	lock_job();
	job_data_ptr->job_status = status;
	free_kernel_args(&(job_data_ptr->job_info));
	unlock_job();
}

void enc_dec_task (struct work_struct* w)
{
	int ret = 0, i = 0;
	struct job_ctx* job_data_ptr;
	struct file *kfinal = NULL;
	char* infile_name = NULL, *outfile_name = NULL;
	unsigned int job_id = 0;
	unsigned char flag;
	void *buf = NULL;
	enum TASK_STATUS status = QUEUED;
	job_data_ptr = container_of(w, struct job_ctx, work);

	lock_job();
	job_data_ptr->job_status = RUNNING;
	size -= 1;
	unlock_job();
	job_id = job_data_ptr->job_info.job_id;

	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0) {
		status = ERROR;
		pr_alert ("[job: %u] ERROR: failed to allocated memory for buffer\n", job_id);
		goto out;
	}
	memset (buf, '\0', PAGE_SIZE);

	ret = open_file (&kfinal, job_data_ptr->job_info.result_fname, O_WRONLY|O_CREAT, 0700);
	if (ret < 0) {
		status = ERROR;
		pr_alert ("[job: %u] ERROR: failed to open the result file\n", job_id);
		goto out;
	}

	sprintf (buf, "[Job: %u] Current state: RUNNING\n", job_id);
	write_data (buf, kfinal);
	memset (buf, '\0', PAGE_SIZE);

	if (job_data_ptr->job_info.task == ENCRYPT)
		flag = (unsigned char)0x01;
	else
		flag = (unsigned char)0x02;

	for(i=0;i<job_data_ptr->job_info.infile_arr_len;i++)
	{	
		infile_name = job_data_ptr->job_info.infile_arr[i];
		outfile_name = job_data_ptr->job_info.outfile_arr[i];
		ret = encrypt_decrypt (infile_name, outfile_name, job_data_ptr->job_info.password_hash, flag,
								buf, kfinal, job_id);
		
		if (ret < 0) {
			pr_err("ERROR: Failed to encrypt/decrypt the file %s\n", infile_name);
			status = ERROR;
		}
	}

	if (status != ERROR) {
		sprintf(buf, "[job: %u] Encryption/Decryption successfull\n", job_id);
		write_data(buf, kfinal);
		status = FINISHED;
		sprintf(buf, "[job: %u] Final Status: FINISHED\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
	} else {
		sprintf(buf, "[job: %u] Encryption/Decryption failed\n", job_id);
		write_data(buf, kfinal);
		sprintf(buf, "[job: %u] Final Status: ERROR\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
	}

out:
	if (buf)
		kfree (buf);
	close_file (&kfinal);
	lock_job();
	job_data_ptr->job_status = status;
	free_kernel_args(&(job_data_ptr->job_info));
	unlock_job();

}

void hash_task(struct work_struct* w){

	int ret = 0, i = 0;
	void *buf = NULL;
	unsigned int job_id = 0;
	struct file *kfinal = NULL;
	struct job_ctx* job_data_ptr;
	char* infile_name = NULL, *outfile_name = NULL;
	enum TASK_STATUS status = QUEUED;
	job_data_ptr = container_of(w, struct job_ctx, work);

	lock_job();
	job_data_ptr->job_status = RUNNING;
	size -= 1;
	unlock_job();

	job_id = job_data_ptr->job_info.job_id;

	ret = open_file (&kfinal, job_data_ptr->job_info.result_fname, O_WRONLY|O_CREAT, 0700);
	
	if (ret < 0) {
		pr_alert ("[Job: %u] ERROR: Failed to open result file\n", job_id);
		goto out;
	}

	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0) {
		pr_alert ("[Job: %u] ERROR: Failed allocate buffer fro messages\n", job_id);
		goto out;
	}
	memset (buf, '\0', PAGE_SIZE);

	sprintf(buf, "[Job: %u] Current state: RUNNING\n", job_id);
	write_data(buf, kfinal);
	memset (buf, '\0', PAGE_SIZE);

	for(i=0;i<job_data_ptr->job_info.infile_arr_len;i++){
		
		infile_name = job_data_ptr->job_info.infile_arr[i];
		outfile_name = job_data_ptr->job_info.outfile_arr[i];
		ret = get_file_hash(infile_name, outfile_name);
		
		if(ret < 0){
			pr_err("\nCannot generate hash of input file\n");
			sprintf(buf, "[Job: %u] ERROR: failed to generate hash of file %s with error %d\n",
						 job_id, infile_name, ret);
			write_data(buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
			status = ERROR;
		} else {
			sprintf(buf, "[Job: %u] Hash of file %s generated successfully\n", job_id, infile_name);
			write_data(buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
		}
	}

	if (status != ERROR) {
		sprintf(buf, "[job: %u] hash of all files generated successfully\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
		status = FINISHED;
		sprintf(buf, "[Job: %u] Final state: FINISHED\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);

	} else {
		sprintf(buf, "[job: %u] hash of all/some files failed\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
		sprintf(buf, "[Job: %u] Final state: ERROR\n", job_id);
		write_data(buf, kfinal);
		status = ERROR;
		memset (buf, '\0', PAGE_SIZE);
	}

out:
	if (buf)
		kfree(buf);
	close_file(&kfinal);
	lock_job();
	job_data_ptr->job_status = status;
	free_kernel_args(&(job_data_ptr->job_info));
	pr_info("\nFreed all resources\n");
	unlock_job();
}

int do_compression(char *input_file, char *out_file, unsigned char flag){

	struct file *in_file_ptr = NULL, *out_file_ptr = NULL;
	int err = 0;

	err = open_file(&in_file_ptr, input_file, O_RDONLY, 0);

	if(err < 0){
		pr_err("\nCannot open file for compression");
		goto out;
	}

	err = open_file(&out_file_ptr, out_file, O_WRONLY|O_CREAT, in_file_ptr->f_inode->i_mode);

	if(err < 0){
		pr_err("\nCannot open file for compression");
		goto out;
	}

	if(flag)
		err = de_compress_file(in_file_ptr,out_file_ptr);
	else
		err = compress_file(in_file_ptr,out_file_ptr);

	if(err < 0){
		pr_err("\nCompression/Decompression failed\n");
		goto out;
	}

out:

	close_file(&in_file_ptr);
	close_file(&out_file_ptr);
	return err;
}

void compression_task(struct work_struct* w){

	int ret = 0, i = 0;
	void *buf = NULL;
	unsigned int job_id = 0;
	struct file *kfinal = NULL;
	struct job_ctx* job_data_ptr;
	char* infile_name = NULL, *outfile_name = NULL;
	enum TASK_STATUS status = QUEUED;
	unsigned char flag = 0;
	job_data_ptr = container_of(w, struct job_ctx, work);

	lock_job();
	job_data_ptr->job_status = RUNNING;
	size -= 1;
	unlock_job();

	job_id = job_data_ptr->job_info.job_id;

	if(job_data_ptr->job_info.task == DECOMPRESSION){
		flag = 0x01;
	}

	ret = open_file (&kfinal, job_data_ptr->job_info.result_fname, O_WRONLY|O_CREAT, 0700);
	
	if (ret < 0) {
		pr_alert ("[Job: %u] ERROR: Failed to open result file\n", job_id);
		goto out;
	}

	ret = mallock(&buf, PAGE_SIZE);
	if (ret < 0) {
		pr_alert ("[Job: %u] ERROR: Failed allocate buffer fro messages\n", job_id);
		goto out;
	}
	memset (buf, '\0', PAGE_SIZE);

	sprintf(buf, "[Job: %u] Current state: RUNNING\n", job_id);
	write_data(buf, kfinal);
	memset (buf, '\0', PAGE_SIZE);

	for(i=0;i<job_data_ptr->job_info.infile_arr_len;i++){
		
		infile_name = job_data_ptr->job_info.infile_arr[i];
		outfile_name = job_data_ptr->job_info.outfile_arr[i];
		ret = do_compression(infile_name, outfile_name, flag);
		
		if(ret < 0){
			sprintf(buf, "[Job: %u] ERROR: failed to compress/decompress file %s with error %d\n",
						 job_id, infile_name, ret);
			write_data(buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
			status = ERROR;
		} else {
			sprintf(buf, "[Job: %u] File %s compressed/decompressed successfully\n", job_id, infile_name);
			write_data(buf, kfinal);
			memset (buf, '\0', PAGE_SIZE);
		}
	}

	if (status != ERROR) {
		sprintf(buf, "[job: %u] All files compressed/decompressed successfully\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
		status = FINISHED;
		sprintf(buf, "[Job: %u] Final state: FINISHED\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);

	} else {
		sprintf(buf, "[job: %u] Compression/Decompression of all/some files failed\n", job_id);
		write_data(buf, kfinal);
		memset (buf, '\0', PAGE_SIZE);
		sprintf(buf, "[Job: %u] Final state: ERROR\n", job_id);
		write_data(buf, kfinal);
		status = ERROR;
		memset (buf, '\0', PAGE_SIZE);
	}

out:
	if (buf)
		kfree(buf);
	close_file(&kfinal);
	lock_job();
	job_data_ptr->job_status = status;
	free_kernel_args(&(job_data_ptr->job_info));
	pr_info("\nFreed all resources\n");
	unlock_job();
}

asmlinkage long asyncjob(void *args)
{	
	int ret = 0;
	struct job_data* job_data = NULL;
	void *buffer = NULL;
	int task = 0;
	int job_ops = 0;
	int user_id = get_current_user()->uid.val;
	struct job_ctx* job_ptr = NULL;
	bool job_added = false;
	struct job_index_table* jit = NULL;

	job_ptr = (struct job_ctx*)kmalloc(sizeof(struct job_ctx), GFP_KERNEL);

	if (job_ptr == NULL){
		ret = -ENOMEM;
		goto out_error;
	}

	job_ptr->user_id = user_id;
	job_data = read_user_args(args);

	if(IS_ERR(job_data)){
		pr_err("\nJob Info cannot be read from user\n");
		goto out_error;
	}

	jit = kmalloc(sizeof(struct job_index_table), GFP_KERNEL);

	if(!jit){
		pr_err("\nNO memory for storing job into hash table\n");
		goto out_error;
	}

	jit->job_ctx = job_ptr;
	jit->job_id = job_data->job_id;

	task = job_data->task;

	if(task != 0){
		
		lock_job();

		if(size >= MAX_QUEUE_SIZE){
			pr_err("\n Too many requests per second\n");
			unlock_job();
			goto out_error;
		}

		size += 1;

		job_ptr->job_status = QUEUED;

		hash_add(job_index, &jit->node, job_data->job_id);

		switch(task){

			case DELETE_FILES:
				INIT_WORK((&(job_ptr->work)), delete_task);
				copy(job_data, &(job_ptr->job_info) );
				job_ptr->job_status = QUEUED;
				job_added = queue_work(wq, (&(job_ptr->work)));
				break;

			case STAT_FILES:
				INIT_WORK((&(job_ptr->work)), stat_task);
				
				// copy args from job_ptr to job_data
				copy(job_data, &(job_ptr->job_info) );
				job_ptr->job_status = QUEUED;
				job_added = queue_work(wq, (&(job_ptr->work)));
				break;
				
			case CONCAT_FILES:
				INIT_WORK((&(job_ptr->work)), concat_task);
				
				// copy args from job_ptr to job_data
				copy(job_data, &(job_ptr->job_info) );
				job_ptr->job_status = QUEUED;
				job_added = queue_work(wq, (&(job_ptr->work)));
				break;

			case HASH_FILES:
				INIT_WORK((&(job_ptr->work)), hash_task);
				
				// copy args from job_ptr to job_data
				copy(job_data, &(job_ptr->job_info) );
				job_ptr->job_status = QUEUED;
				job_added = queue_work(wq, (&(job_ptr->work)));
				break;

			case COMPRESSION:
			case DECOMPRESSION:
					INIT_WORK((&(job_ptr->work)), compression_task);
					
					// copy args from job_ptr to job_data
					copy(job_data, &(job_ptr->job_info) );
					job_ptr->job_status = QUEUED;
					job_added = queue_work(wq, (&(job_ptr->work)));
					break;
				
			case RENAME:
				INIT_WORK((&(job_ptr->work)), rename_task);
				
				// copy args from job_ptr to job_data
				copy(job_data, &(job_ptr->job_info) );
				job_ptr->job_status = QUEUED;
				job_added = queue_work(wq, (&(job_ptr->work)));
				break;

			case ENCRYPT:
			case DECRYPT:
				INIT_WORK((&(job_ptr->work)), enc_dec_task);
				
				// copy args from job_ptr to job_data
				copy(job_data, &(job_ptr->job_info) );
				job_ptr->job_status = QUEUED;
				job_added = queue_work(wq, (&(job_ptr->work)));
				break;

			case NO_OP:
				INIT_WORK((&(job_ptr->work)), no_op_task);
				
				// copy args from job_ptr to job_data
				copy(job_data, &(job_ptr->job_info) );
				job_ptr->job_status = QUEUED;
				job_added = queue_work(wq, (&(job_ptr->work)));
				break;
				
			default:
				break;

		}

		if(!job_added){
			size -= 1;
			hash_del(&jit->node);
			// free_kernel_args(job_data);
			pr_info("\nFreeing resources after job failed\n");
			unlock_job();
			goto out_error;
		}

		unlock_job();

	}

	else{

		job_ops = job_data->job_ops;
		copy(job_data, &(job_ptr->job_info));

        switch(job_ops){
			case JOB_LIST:
			 	list_jobs(user_id, job_data->result_fname);
                break;
            case JOB_STATUS:
                job_status(job_data->job_id, user_id, job_data->result_fname);
                break;
			case DEL_JOB:
			    ret = cancel_job(job_data->job_id, user_id, job_data->result_fname);
				if (ret < 0){
					pr_err("\nFailed to cancel job %d\n", job_data->job_id);
					goto out;
				}
			    break;
            case JOB_PRIO_CHANGE:
			   ret = reorder_job(job_data->job_id, user_id, job_data->job_priority,
			    				 job_data->result_fname);
			   if (ret < 0){
				   pr_err("\nFailed to reorder job %d \n", job_data->job_id);
				   goto out;
			   }
			   break;
            default:
                free_kernel_args(job_data);
                break;         
        } 

	}
	
	goto out;

out_error:

	if(job_data)
		free_kernel_args(job_data);

	if(job_ptr)
		kfree(job_ptr);
	
	if(jit)
		kfree(jit);
	

out:
	if(job_data)
		kfree(job_data);
	if (buffer)
		kfree(buffer);
	return ret;
}

static int __init init_sys_asyncjob(void)
{
	wq = create_workqueue("job_wq");
	high_prio_wq = alloc_workqueue("%s", WQ_UNBOUND, WQ_HIGHPRI|WQ_MEM_RECLAIM, 3, "high_prio_wq");
	printk("installed new sys_asyncjob module\n");
	if (sysptr == NULL)
		sysptr = asyncjob;

	hash_init(job_index);
	return 0;
}
static void  __exit exit_sys_asyncjob(void)
{	
	unsigned bkt;
	struct job_index_table* curr = NULL;

	if (sysptr != NULL)
		sysptr = NULL;
	printk("removed sys_asyncjob module\n");

	lock_job();

	hash_for_each(job_index, bkt, curr, node){
		if(curr){
			if(curr->job_ctx->job_status == QUEUED || curr->job_ctx->job_status == RUNNING){
				free_kernel_args(&(curr->job_ctx->job_info));
			}
			kfree(curr->job_ctx);
			kfree(curr);
			pr_info("\nCleanup: Freed hash table resource");
		}
	}

	unlock_job();
	// flush_workqueue(wq);
	destroy_workqueue(wq);
}
module_init(init_sys_asyncjob);
module_exit(exit_sys_asyncjob);
MODULE_LICENSE("GPL");
