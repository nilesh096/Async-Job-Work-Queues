#include <linux/mutex.h>

DEFINE_MUTEX(mutex);

void lock_job(void){
    mutex_lock(&mutex);
}

void unlock_job(void){
    mutex_unlock(&mutex);
}


/**
 * @brief 
 * 
 
 sys.c
 struct job_ctx* ctx = get_job_ctx(job_id);

 hash.c

 remove_job(1);   

 if(work_busy){

    1.remove_job(1)
    2.cancel(ctx->w);

 }

 * 
 */