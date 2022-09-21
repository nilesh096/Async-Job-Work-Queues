#include "job_data.h"

#define SHA256_LEN 32
#define MD5_LEN 16

extern int generate_file_hash (struct file *input, struct file *output);
extern int concat_file (struct file *input, struct file *output);
extern int open_file (struct file **fptr, char *name, int flags, int mode);
extern void close_file (struct file **fptr);
extern void delete_file (struct file *fp);
extern int mallock (void **buf, int len);
extern int file_stat (const char *name, struct kstat **fstat);
extern int check_file_type (struct kstat *stat, const char *name);
extern int generate_hash (const u8 *input, unsigned int hash_length, u8 *output);
extern int read_write (struct file *input, struct file *output, void *key, unsigned int flag);
extern int write_preamble (void *buf, struct file *output);
extern int read_preamble (void *buf, struct file *output);
extern int rename_file (struct file *input, struct file *output);
extern struct job_data* read_user_args(void *args);
extern void free_kernel_args(struct job_data* job_data);
extern int write_data (char *str, struct file *fp);
extern void copy(struct job_data* from, struct job_data* to);
extern int write_status(int job_id, int user_id, enum TASK_STATUS task_status);
extern int compress_file(struct file* infile, struct file* outfile);
extern int de_compress_file(struct file* infile, struct file* outfile);