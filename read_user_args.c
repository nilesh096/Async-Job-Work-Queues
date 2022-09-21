#include <linux/uaccess.h>
#include <linux/namei.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/zlib.h>
#include "job_data.h"
#include "file_ops.h"

int copy_from_user_space(void** from, void** to){

    int err = 0;

    struct filename* kinfile = NULL;

    if(*from == NULL)
        return 0;

    kinfile = getname(*from);

    if(IS_ERR(kinfile)){
        pr_err("\nCannot do getname while reading infile\n");
        err = PTR_ERR(kinfile);
        goto out;
    }

    *to = kmalloc(strlen(kinfile->name), GFP_KERNEL);
    
    if(!(*to)){
        err = -ENOMEM;
        pr_err("\nCannot allocate memory for storing input file");
        goto out;
    }

    memset(*to, '\0', strlen(*to));
    
    pr_info("\nOutput created\n");

    strcpy(*to, kinfile->name);

    pr_info("From generic function , %s", (char *) (*to));

    err = mallock(from, strlen(*to));

    if(err < 0){
        pr_err("\n Cannot allocate memeory for kernel args\n ");
        goto out;
    }

    memset(*from, '\0', strlen(*to));

    strcpy(*from, *to);

out:

    if(*to){
        kfree(*to);
        *to = NULL;
    }

    if(kinfile && !IS_ERR(kinfile))
        putname(kinfile);
    
    return err;

}

struct job_data* read_user_args(void *args){

    int err = 0;
    void *buf = NULL;
    struct job_data* job_data = NULL;
    int i = 0;

    if(args == NULL){
        err = -EINVAL;
        goto out_error;
    }
    
    job_data = kmalloc(sizeof(struct job_data), GFP_KERNEL);

    if(!job_data){
        err = -ENOMEM;
        goto out_error;
    }

    if(copy_from_user(job_data, args, sizeof(struct job_data))){
        err = -EINVAL;
        goto out_error;
    }

    for(i=0;i<job_data->infile_arr_len;i++){

        err = copy_from_user_space( (void *) (&job_data->infile_arr[i]), &buf);

        if(err < 0){
            pr_err("\n Error in copy from user space for infile\n");
            goto out_error;
        }

    }

    for(i=0;i<job_data->outfile_arr_len;i++){

        err = copy_from_user_space( (void *) (&job_data->outfile_arr[i]), &buf);

        if(err < 0){
            pr_err("\n Error in copy from user space for outfile\n");
            goto out_error;
        }

    }

    pr_info("\nCopied Done All input and output arrays\n");

    err = copy_from_user_space( (void *) (&job_data->password_hash), &buf);

    if(err < 0){
        pr_err("\n Error in copy from user space for password hash \n");
        goto out_error;
    }

    err = copy_from_user_space( (void *) (&job_data->result_fname), &buf);

    if(err < 0){
        pr_err("\n Error in copy from user space for result file name\n");
        goto out_error;
    }

    pr_info("\nCopied Done\n");

    goto out;

out:

    return job_data;

out_error:

    free_kernel_args(job_data);
    
    return ERR_PTR(err);

}

void free_kernel_args(struct job_data* job_data){

    int i = 0;
    printk("\nFreeing here\n");

    if(!job_data)
        return;

    if(job_data->password_hash)
        kfree(job_data->password_hash);
    
    if(job_data->result_fname)
        kfree(job_data->result_fname);
    
    for(i=0;i<job_data->infile_arr_len;i++){
        kfree(job_data->infile_arr[i]);
    }

    for(i=0;i<job_data->outfile_arr_len;i++){
        kfree(job_data->outfile_arr[i]);
    }

    job_data->password_hash = NULL;
    job_data->result_fname = NULL;

    printk("\nFreed\n");

}

void copy(struct job_data* from, struct job_data* to){

    int i = 0;

    if(!from || !to){
        return;
    }

    to->password_hash = from->password_hash;
    to->result_fname = from->result_fname;
    to->infile_arr_len = from->infile_arr_len;
    to->outfile_arr_len = from->outfile_arr_len;
    to->job_id = from->job_id;
    to->task = from->task;
    to->job_ops = from->job_ops;

    for(i=0;i<from->infile_arr_len;i++){
        to->infile_arr[i] = from->infile_arr[i];
    }

    for(i=0;i<from->outfile_arr_len;i++){
        to->outfile_arr[i] = from->outfile_arr[i];
    }

}