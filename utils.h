
#ifndef JOB_UTILS
#define JOB_UTILS

#define MAX_QUEUE_SIZE 10

// extern void job_index_init(void);
//extern void add_job(struct job_ctx* job_ctx, int job_id);
// extern void remove_job(int job_id);
// extern struct job_ctx* get_job_ctx(int job_id);
extern void lock_job(void);
extern void unlock_job(void);

#endif