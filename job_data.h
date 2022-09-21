// #include <linux/fs.h>
// #include <linux/types.h>

#ifndef JOB_DATA
#define JOB_DATA

struct job_data {

    int dummy1, dummy2; 

    unsigned int job_id;

    unsigned int job_priority;

    /* 
      1 - Delete multiple files
      2 - Stat multiple files
      3 - concat 2 or more files
      4 - hash file 
      5 - encrypt or decrypt files
      0 - None of the above, means we do job_ops
    */
    int task;


    /*
       Cancel/Delete Job
       Job status
       Change Priority
       List jobs
    */
    int job_ops;  

    /* update */

    char *infile_arr[10];

    unsigned int infile_arr_len;

    char *outfile_arr[10];

    unsigned int outfile_arr_len;

    char* password_hash;

    bool is_poll;

    char* result_fname;

};

enum TASK_STATUS {

  QUEUED,
  RUNNING,
  FINISHED,
  ERROR

};

#endif
