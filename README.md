# Project Description

The project involves submitting file system related jobs and executing them asynchronously in the linux kernel. An in-kernel queueing system is designed to support execution of the jobs asynchronously. 

The different jobs include concatenation of files, encryption-decryption of files, stat files, delete files, rename files, hashing files and compression-decompression of files. Users can poll for the results of the job or provide a file to which the results will be written. Users can also list all the jobs, change job priority, cancel pending jobs and check job status. Any new job submitted must be throttled if the queuing system is congested.

# Design Consideration

We implement the functionality as a loadable system call module called “jobasync”. We use this system call for submitting the jobs and collecting information about the jobs. The userland arguments are taken through command line arguments and passed to the system call. The arguments are copied to kernel memory before further processing. There are 2 main blankets of operations -
Submit file system related tasks/jobs to execute asynchronously.
Job operations to collect job status /modify jobs.

Executing Jobs
 
# 1. Job submission - Workqueue and the work item

To run the jobs asynchronously we referred to the concurrency managed workqueue (cmwq) implemented in the linux kernel. The workqueues manage a pool of workers that get scheduled on each CPU and a list of work items to be assigned to these workers. The workqueues takes care of managing the concurrency level of the work items/jobs, queing work items to be executed in FIFO order and providing support for canceling and maintaining status for each work item. Hence, we decided to use the in-built linux kernel workqueues instead of building a producer-consumer queuing system from scratch. 

We initialize 2 workqueues one for the high priority work that assign work items/jobs to a high priority worker pool and the other for the normal work items / jobs. This initialization is done when we install the syscall in the sycall init function. For higher priority jobs, the WQ_HIGHPRI
flag is set while creating the workqueue.

The work item is initialized and enqued for workqueue to manage and run it asynchronously.
We use containers to wrap the job parameters and other contextual information along with the work item.The pointer to the initialized work item is passed to the task handler function. Using containers_of method we can access any job related arguments needed for executing the task.

Job context container for a work item contains job related arguments, the user who created the job, the job status and the work item that was created. Below is the data structure - 

struct job_ctx {
	struct job_data job_info; // job related arguments
	int user_id; // user who created the job
	enum TASK_STATUS job_status; // status updated by worker (FINISHED/FAILED)
	struct work_struct work; // pointer to this is passed to the task handler by workqueue
};

Job arguments structure (struct job_data) -

    struct job_data {

    unsigned int job_id; // Job id 

    unsigned int job_priority; // Priority

    int task; // task number to run as work

    int job_ops;  // Job operations number

    char *infile_arr[10]; // array of pointers to input filenames

    unsigned int infile_arr_len; // length of input filenames array

    char *outfile_arr[10]; // array of pointers to output filenames

    unsigned int outfile_arr_len; // length of output filenames array

    char* password_hash; // Password MD5 hash for encryption/decryption

    bool is_poll; // bool to check if we are polling or writing to file

    char* result_fname; // result filename where we write to for polling/writing to file
    };


# 2. Job Types Supported 

     2.1 Encryption Decryption	
     We are using md5 for hashing the user password and passing it to the kernel. The preamble is the sha256 hash of the md5 hash. The md5 hash is used for encryption and decryption of the file. The mda5 hash is hashed to verify with the content of the preamble. 

     2.2 Delete multiple files
     We are using vfs_unlink to delete the files.
     
     2.3 Stat multiple files
     We are using vfs_stat to stat the file. We are returning mode, file size, inode number, uid, gid and number of links to the file. 

     2.4 Rename multiple files
     We are using vfs_rename to rename the files. 

     2.5 Hash multiple files
     We are computing sha256 sum for getting the hash of the file.

     2.6 Compress and Decompress files
     Compression and Decompression is done using the Zlib library in the linux kernel.

     2.7 Concatenate 2 or more files
     For concatenation we loop through each input file in the array and append the content to the output file.

# Help command  -

USAGE for submitting task: ./xhw3 [-h] [-t TASK_NUMBER] [-i INPUT_FILES] [-o OUTPUT_FILES] [-e PASSWD] -p|[-w RESULT_FILE]
   -h        Help: displays help menu.
   -t        Task numbers : 
 1 - DELETE FILES        2 - STAT FILES          3 - CONCAT FILES        4 - HASH FILES
 5 - ENCRYPT FILES       6 - DECRYPT FILES       7 - RENAME FILES        8 - COMPRESSION 
 9 - DECOMPRESSION       10 - NO_OP TASK 
   -i        Input files (Not required for NO_OP TASKS) 
   -o        Output files (Not required for DELETE FILES, NO_OP TASKS) 
   -e        Password for ENCRYPTION/DECRYPTION TASK
   -p        Polling for results
   -w        Filename for writing job results

# 3. Job Results

The results of the job and the status can be viewed by the user using either polling or through the results being written to a file. This is used for returning results of job related operations (getting job status, canceling job etc) as well as getting the results of the task/job itself. 

   # 3.1 Polling 
    We use the poll(2) system call as an interprocess communication between the kernel and the userland process by writing to / reading from a FIFO file. The FIFO file is created in userland and the name of the file is passed to the kernel. The job running asynchronously will write to the FIFO file. The same file is opened in read mode in a pthread created in the userland. The poll system call will block for events (POLLIN event in our case) on the FIFO file. On receiving the event the content of the file will be read and printed to the terminal. The poller (pthread for polling) is created for each job submitted by the user through the system call. In the kernel we write meaningful messages which convey intermediate results of the job, any failures and final success message of the job. Once the file is closed in the kernel (one end of the FIFO pipe is closed), the poll system call will get aß POLLHUP event, which will end the polling process and successfully return. We also keep a timeout of 30 seconds in the poll system call. If no event happens, the thread will return. 

   # 3.2 Writing results to a file
      Users can also provide a file to which the results are written. The jobs will open the file and write intermediate results, failures and successful completion messages to a file. 


# Job Operations 

# 1. Hashtable for managing jobs 

To collect job related information like job status, listing of  jobs, canceling and reordering of jobs we make use of a hashtable in the kernel which uses job id as the key. The hashtable is implemented using chaining, with a link list for each bucket. We initialize a hashtable with 1024 buckets. The hashtable nodes in the link list are contained in a container having job context pointer and job id. The hashtable resources are freed when we unload the module using rmmod.

Below is the container structure for the node in the hashtable -

struct job_index_table {
  	 int job_id; // job id
	struct job_ctx* job_ctx; // job context pointer
	struct hlist_node node; // node stored in the hashtable
};

Our choice for using a hashtable is that it is in-memory and efficient. We also considered writing the status and job ids to files. But this approach would slow down the process due to the expensive IO operations. 

# 2. Job Operations supported 

# 2.1 Job status

To collect job status for a particular job id, we iterate through the hashtable and check if the job id in the container of the node matches the job id provided. The user id is also checked, so only  valid users are allowed to check their job statuses. The root users can check any job status, whereas other users can check only their job statuses. We use the work_busy and work_pending methods on the work structure stored in the job context pointer. These methods will return a boolean value indicating whether the job is EXECUTING or PENDING(waiting in the queue). If the job has completed, we can check whether it succeeded or failed using the job status enum variable stored in the job context. 

# 2.2 Listing Jobs

Similarly for listing the jobs,  we loop through every bucket in the hashtable and list all the job ids. The jobs belonging to the user are only returned. 

# 2.3 Canceling Jobs

For canceling of the job we iterate through the hashtable and check if the job id in the container of the node matches the job id provided.The user id is also checked, so only valid users are allowed to cancel the job. A job is canceled only if it is in PENDING state. The node in the hashtable is also deleted and resources for the job are freed. 

# 2.4 Reordering Jobs

Jobs can either run as a high priority job or as a normal job. Users can provide a job priority of 0 or 1 to set priority of existing jobs. A job priority of 0 means it should be submitted to the normal work pool for execution and a priority of 1 means it should be submitted to the high priority work pool. For this reordering we cancel the job if it is in PENDING state and queue it to the work queue designated for that priority (for instance, 0-normal workqueue, 1-high priority workqueue).

# 2.5 Throttling Jobs

We only allow 20 work items to be pending in the queue. Queuing and Dequeuing of jobs is synchronized using a mutex lock and a volatile counter. Once a counter reaches a MAX_QUEUE_SIZE we do not permit the work to be queued. 


# Help command -

USAGE for job operations: ./xhw3 [-h] [-j JOB_OPERATION] [-n JOB_ID] [-r JOB_PRIORITY] -p|[-w RESULT_FILE]
   -h        Help: displays help menu.
   -j        Job Operation Numbers : 
 1 - CANCEL JOB          2 - JOB STATUS          3 - REORDER_JOB         4 - LIST JOBS
   -n        Job id (Not required for LIST JOBS) 
   -r        Job priority 0 or 1 (ONLY USE for REORDERING JOB) 
   -p        Polling for results
   -w        Filename for writing job operation results


# Steps to reproduce the code

Clone git repo.
Make Kernel.
cd CSE-506; make clean all; sh install_module.sh.
Execute test0*.sh scripts
    
# References

Workqueues - https://www.kernel.org/doc/html/latest/core-api/workqueue.html
Workqueue examples - https://developer.nordicsemi.com/nRF_Connect_SDK/doc/1.3.2/zephyr/reference/kernel/threads/workqueue.html#work-item-lifecycle
Deferred work - https://linux-kernel-labs.github.io/refs/heads/master/labs/deferred_work.html
Linux Hashtables - https://kernelnewbies.org/FAQ/Hashtables
Encryption Decryption - https://kernel.readthedocs.io/en/sphinx-samples/crypto-API.html
Zlib - https://www.zlib.net/zlib_how.html
Zlib linux kernel - https://docs.huihoo.com/doxygen/linux/kernel/3.7/crypto_2zlib_8c_source.html
Hashing - https://stackoverflow.com/questions/11126027/using-md5-in-kernel-space-of-linux
Poll System call - https://man7.org/linux/man-pages/man2/poll.2.html
Pthreads - https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html
