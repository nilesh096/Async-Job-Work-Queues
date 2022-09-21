#define _OPEN_THREADS
#include <asm/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <openssl/md5.h>
#include <sys/stat.h>
#include <time.h>
#include "poll_jobs.h"
#include "job_data.h"
#include "constants.h"

#ifndef __NR_asyncjob
#error asyncjob system call not defined
#endif

#define HELP_MSG(program_name, ret_code) do { \
fprintf(stderr, "USAGE for submitting task: %s %s\n", program_name, \
"[-h] [-t TASK_NUMBER] [-i INPUT_FILES] [-o OUTPUT_FILES] [-e PASSWD] -p|[-w RESULT_FILE]\n" \
"   -h        Help: displays help menu.\n" \
"   -t        Task numbers : \n" \
" 1 - DELETE FILES \t 2 - STAT FILES \t 3 - CONCAT FILES \t 4 - HASH FILES\n" \
" 5 - ENCRYPT FILES \t 6 - DECRYPT FILES \t 7 - RENAME FILES \t 8 - COMPRESSION \n" \
" 9 - DECOMPRESSION \t 10 - NO_OP TASK \n" \
"   -i        Input files (Not required for NO_OP TASKS) \n" \
"   -o        Output files (Not required for DELETE FILES, NO_OP TASKS) \n" \
"   -e        Password for ENCRYPTION/DECRYPTION TASK\n" \
"   -p        Polling for results\n" \
"   -w        Filename for writing job results\n"); \
fprintf(stderr, "USAGE for job operations: %s %s\n", program_name, \
"[-h] [-j JOB_OPERATION] [-n JOB_ID] [-r JOB_PRIORITY] -p|[-w RESULT_FILE]\n" \
"   -h        Help: displays help menu.\n" \
"   -j        Job Operation Numbers : \n" \
" 1 - CANCEL JOB \t 2 - JOB STATUS \t 3 - REORDER_JOB \t 4 - LIST JOBS\n" \
"   -n        Job id (Not required for LIST JOBS) \n" \
"   -r        Job priority 0 or 1 (ONLY USE for REORDERING JOB) \n" \
"   -p        Polling for results\n" \
"   -w        Filename for writing job operation results\n"); \
} while(0)



#define INVALID_OPTION(op) do { \
fprintf(stderr, "Invalid Option: %s\n", op); \
} while(0)


/*

Tasks - 

Handling <3, 4, 5, 6, 7>
-e for password
./xhw3 --task <5,6> --infile <infile> --outfile <outfile> -p | -w
if task is <4> -> ./xhw3 --task <4> --infile file1  (no outfile for hashing)
if task is <3, 7>
./xhw3 --t <3,7> -p | -w --infile file1, file2, file3 --outfile  file1, file2, file3 

Handling <1, 2>
./xhw3 -t <1, 2> -p | -w -i file1, file2, file3 

Job Operations

./xhw3 -j <1,2,3,4> -n <2> -r <1>

*/

int process_filenames(char* filenames, char** filename_arr, char* cwd){

	int num_file = 0;
	char* token = NULL;

	token = strtok(filenames, ",");
	num_file += 1;

	while( token != NULL ) {
	  filename_arr[num_file - 1] = (char*)malloc(strlen(token) + strlen(cwd) + 2);
	  memset(filename_arr[num_file - 1], '\0', strlen(token) + strlen(cwd) + 2);
	  sprintf(filename_arr[num_file - 1], "%s/%s", cwd, token);
      token = strtok(NULL, ",");
	  if (token != NULL){
		  num_file += 1;
	  }
   }

	return num_file;
}

void encrypt_password(char* password, unsigned char* buf)
{
	MD5_CTX md5_context;
	MD5_Init(&md5_context);
	MD5_Update(&md5_context, password, strlen(password));
	MD5_Final(buf, &md5_context);
}

int parse_args(int argc, char** argv, struct job_data* jobargs, char* cwd){

	int option;
	bool is_task = false;
	bool is_job_ops = false;
	bool is_result_set = false;

	while ((option = getopt(argc, argv, "pw:i:o:t:n:r:j:e:h")) != -1) {
		switch(option) {
			case 'h':
			   HELP_MSG(*argv, 0);
			   exit(0);

			case 't':
				if (is_job_ops){
					 INVALID_OPTION("t");
					 HELP_MSG(*argv, 0);
					 return -1;
				}
				if (optarg == NULL) {
					printf("ERROR: Provide a valid task\n");
					HELP_MSG(*argv, 0);
					return -1;
				}
				if (is_task) {
					printf("ERROR: Task already set\n");
					HELP_MSG(*argv, 0);
					return -1;
				}
				is_task = true;
				(jobargs)->task = atoi(optarg);
				break;
			
			case 'i':
				if ((is_job_ops) || (!is_task)) {
					INVALID_OPTION("i");
					HELP_MSG(*argv, 0);
					return -1;
				}
				(jobargs)->infile_arr_len = process_filenames(optarg, (jobargs)->infile_arr, cwd);
				if ((jobargs)->infile_arr_len == 0) {
					printf("ERROR: Provide atleast one input file for the task\n");
					HELP_MSG(*argv, 0);
					return -1;
				}
				break;

			case 'o':
				if ((is_job_ops) || (!is_task)) {
					INVALID_OPTION("o");
					return -1;
				}
				if ((jobargs)->infile_arr_len == 0) {
					INVALID_OPTION("o");
					return -1;
				}
				(jobargs)->outfile_arr_len = process_filenames(optarg, (jobargs)->outfile_arr, cwd);
				break;
			
			case 'e':
				if ((!is_task) || (is_job_ops)) {
					INVALID_OPTION("e");
					return -1;
				}
				if ((jobargs->task != ENCRYPT) && (jobargs->task != DECRYPT))
					return -1;

				if ((optarg == NULL) || (strlen(optarg) <= 6)) {
					printf("ERROR: Password length should be greater than 6 characters\n");
					return -1;
				}

				jobargs->password_hash = optarg;
				unsigned char buf[32];
				encrypt_password(jobargs->password_hash, buf);
				jobargs->password_hash = (char*)malloc(16);
				memcpy(jobargs->password_hash, (void*) buf, 16);
				break;

			case 'p':
				if (is_result_set) {
					INVALID_OPTION("p");
				  	return -1;
				}
			  	(jobargs)->is_poll = true;
				is_result_set = true;
			  	break;

			case 'w':
				if (is_result_set) {
					INVALID_OPTION("w");
					return -1;
			  	}
				if (optarg == NULL)
					return -1;

				jobargs->result_fname = (char*)malloc(strlen(optarg) + strlen(cwd) + 2);
				memset(jobargs->result_fname, '\0', strlen(optarg) + strlen(cwd) + 2);
				sprintf(jobargs->result_fname, "%s/%s", cwd, optarg);
				(jobargs)->is_poll = false;
				is_result_set = true;
			  	break;

			case 'j':
				if (is_task){
					INVALID_OPTION("j");
					return -1;
				}
				if (optarg == NULL) {
					printf("ERROR: provide a valid job_ops\n");
					return -1;
				}
				(jobargs)->job_ops = atoi(optarg);
				is_job_ops = true;
				break;
			
			case 'n':
				if (is_task || (!is_job_ops)) {
					INVALID_OPTION("n");
					return -1;
				}
				if (optarg == NULL) {
					printf("ERROR: provide a valid job_id\n");
					return -1;
				}
				(jobargs)->job_id = atoi(optarg);
				break;

			case 'r':
				if (is_task ||(!is_job_ops)) {
					INVALID_OPTION("r");
					return -1;
				}
				if (optarg == NULL) {
					printf("ERROR: provide a valid job_priority\n");
					return -1;
				}
				(jobargs)->job_priority = atoi(optarg);
				break;
			default:
				printf("\nInvalid option\n");
				return -1;
		}		
	}

	if ((!is_task) && (!is_job_ops))
		return -1;
	
	if ((jobargs->infile_arr_len == 0) && (is_task) && (jobargs->task != NO_OP))
		return -1;

	if ((is_job_ops) && (jobargs->job_id == 0) && ((jobargs)->job_ops != JOB_LIST))
		return -1;

	if (!is_result_set)
		return -1;
	
	return 0;
}

int validate_job_ops (struct job_data *job_args)
{
	int job_ops = 0, ret = 0;
	job_ops = job_args->job_ops;

	switch (job_ops)
	{
		case JOB_STATUS:
		case JOB_PRIO_CHANGE:
			if (job_args->job_id == 0) {
				printf ("ERROR: Please provide a valid job_id\n");
				ret = -1;
				goto out;
			}
			break;
		default:
			printf("ERROR: Invalid job operation provided");
			ret = -1;
			goto out;	
	}
out:
	return ret;
}

int validate_task (struct job_data *job_args)
{
	int task = 0, ret = 0;
	task = job_args->task;

	switch (task)
	{
		case DELETE_FILES:
			if (job_args->outfile_arr_len > 0) {
				printf("ERROR: No output files expected for delete files task\n");
				ret = -1;
				goto out;
			}
			break;

		case ENCRYPT:
		case DECRYPT:
			printf("Entered here\n");
			if (job_args->outfile_arr_len != job_args->infile_arr_len) {
				printf("ERROR: For each input file there needs to be an output file\n");
				ret = -1;
				goto out;
			}
			if (!job_args->password_hash) {
				printf("ERROR: Please provide a valid password\n");
				ret = -1;
				goto out;
			}
			break;

		case CONCAT_FILES:
			if (job_args->outfile_arr_len != 1) {
				printf("ERROR: Only one output file needs to be there for concat\n");
				ret = -1;
				goto out;
			}
			if (job_args->infile_arr_len < 2) {
				printf("ERROR: Provide atleast 2 input files for concat\n");
				ret = -1;
				goto out;
			}
			break;

		case STAT_FILES:
		case RENAME:
		case HASH_FILES:
		case COMPRESSION:
		case DECOMPRESSION:
			if (job_args->outfile_arr_len != job_args->infile_arr_len) {
				printf("ERROR: For each input file there needs to be an output file\n");
				ret = -1;
				goto out;
			}
			break;
		case NO_OP:
			break;
		default:
			printf ("Please provide a valid task\n");
			ret = -1;
			break;
	}

out:
	return ret;
} 

void free_job_args (struct job_data *job_args)
{
	int i = 0;
	if (job_args) {
		if (job_args->result_fname)
			free (job_args->result_fname);
		if (job_args->password_hash)
			free (job_args->password_hash);
		for (i = 0; i < job_args->infile_arr_len; i++) {
			if (job_args->infile_arr[i])
				free (job_args->infile_arr[i]);
		}
		for (i = 0; i < job_args->outfile_arr_len; i++) {
			if (job_args->outfile_arr[i])
				free (job_args->outfile_arr[i]);
		}
		free (job_args);
	}
}

int main(int argc, char *argv[])
{
	int rc = 0, i = 0;
	void* thread_ret;
	pthread_t poller_thread_id;
	struct poller_args* pargs = NULL;
    struct job_data* job_args; 

	char *cwd = NULL;

	cwd = getcwd(NULL,0);
    if (cwd == NULL) {
        rc = -1;
        printf("Error in getting the CWD\n");
        goto clean;
    }
    printf("Current Working directory is %s\n", cwd);
	
	job_args = (struct job_data*)malloc(sizeof(struct job_data));

	job_args->job_id = 0;
	job_args->task = 0;
	job_args->job_ops = 0;
	job_args->infile_arr_len = 0;
	job_args->outfile_arr_len = 0;
	job_args->job_priority = 0;
	job_args->password_hash = NULL;
	job_args->result_fname = NULL;

	rc = parse_args(argc, argv, job_args, cwd);
	if (rc < 0) {
		printf ("ERROR: failed to parse arguments\n");
		goto clean;
	}


	if (job_args->task != 0){
		job_args->job_id = (int)time(NULL);
		printf("JOB ID: %d\n", job_args->job_id);
		rc = validate_task (job_args);
		if (rc < 0) {
			printf ("ERROR: Invalid task arguments provided\n");
			goto clean;
		}
		printf("Randomly generated filename (polling/writing) for JOB ID %s\n", job_args->result_fname);
	}


	if (job_args->is_poll) {

		// Creating a FIFO file for polling
		job_args->result_fname =(char*) malloc(4096);
		memset(job_args->result_fname, '\0', 4096);
		if (job_args->task == 0)
			sprintf(job_args->result_fname, "/tmp/job-status-%d", job_args->job_id);
		else	
			sprintf(job_args->result_fname, "/tmp/job-%d", job_args->job_id);
		rc = mkfifo(job_args->result_fname, 0777);
		if (rc < 0) {
			printf("ERROR: failed to open FIFO file %s\n", job_args->result_fname);
			goto clean;
		}
		pargs = (struct poller_args*)malloc(sizeof(struct poller_args));

		pargs->job_id = job_args->job_id;

		pargs->fname = (char*)malloc(strlen(job_args->result_fname));
		strcpy(pargs->fname, job_args->result_fname);

		if (pthread_create(&poller_thread_id, NULL, poll_file, pargs) != 0) {
			perror("pthread_create() error, Failed to invoke system call");
			goto clean;
		}
	}

	printf("After parsing %s\n", *(job_args->infile_arr));
	
	printf("##### PARAMETERS ########\n");
	printf("Input file count %d, Output file count %d\n", job_args->infile_arr_len, job_args->outfile_arr_len);
	printf("Input filenames\n");
	for (i=0; i < job_args->infile_arr_len; i++){
		
			printf("Input file %d, %s \n", i, *(job_args->infile_arr + i));
	}

	printf("Output filenames\n");
	for (i=0; i < job_args->outfile_arr_len; i++){
		
			printf("Output file %d, %s \n", i, *(job_args->outfile_arr + i));
	}

	printf(job_args->is_poll ? "is poll: true\n" : "is poll: false\n");
	printf("Password hash %s\n", job_args->password_hash);

  	rc = syscall(__NR_asyncjob, (void *)job_args);
	
	if (job_args->is_poll){
		if (pthread_join(poller_thread_id, &thread_ret) != 0) {
			perror("pthread_join() error");
			goto clean;
		}
		rc = remove(job_args->result_fname);
		if (rc == 0)
			printf("File %s successfully deleted\n", job_args->result_fname);
		else	
			printf("Failed to delete file %s with error %d\n", job_args->result_fname, rc);
	}

	if (rc == 0)
		printf("syscall returned %d\n", rc);
	else
		printf("syscall returned %d (errno=%d)\n", rc, errno);

clean:
	if (pargs != NULL) {
		if (pargs->fname != NULL){
			free(pargs->fname);
		}
		free(pargs);
	}
	free_job_args (job_args);
	exit(rc);
}