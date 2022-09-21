#include <linux/slab.h>
#include <linux/uaccess.h>
#include "job_data.h"
#include <linux/zlib.h>
#include <linux/fs.h>


/**
 * @brief Defalte -> compression and infalte is decompression
 * 
 * @param strm 
 */

// static int strm_init (z_stream *strm)
// {   
//     int result = 0;
//     result = zlib_deflateInit (*strm, Z_DEFAULT_COMPRESSION);
    
//     return result;
// }

#define CHUNK 4096

static int compression_init(z_streamp strm){

	size_t size = max(zlib_deflate_workspacesize(MAX_WBITS, MAX_MEM_LEVEL),
			zlib_inflate_workspacesize());	
	
	strm->workspace = kmalloc(size, GFP_KERNEL);
	
	if (!(strm->workspace))
		return -ENOMEM;

	return 0;
}

int de_compress_file(struct file* infile, struct file* outfile){

	int err = 0;

    char *in_buffer = NULL, *out_buffer = NULL;

	z_streamp strm = NULL;

	int bytes_read = 0, bytes_wrote = 0, flush = Z_NO_FLUSH, have = 0;

	in_buffer = kmalloc(CHUNK, GFP_KERNEL);

    if(!in_buffer){
        pr_err("\nNo enough memory to allocate for input buffer while compressing");
        err = -ENOMEM;
        goto out;
    }

    out_buffer = kmalloc(CHUNK, GFP_KERNEL);

    if(!out_buffer){
        pr_err("\nNo enough memory to allocate for output buffer while compressing");
        err = -ENOMEM;
        goto out;
    }

    memset(out_buffer, '\0', CHUNK);
    memset(in_buffer, '\0', CHUNK);

    strm = (z_streamp) kmalloc(sizeof(z_stream), GFP_KERNEL);

    if(!strm){
		pr_err("\nNot enough available memory for workspace during compression\n");
        err = -ENOMEM;
		goto out;
	}

    compression_init(strm);

	strm->avail_in = 0;
	strm->next_in = NULL;

    err = zlib_inflateInit2(strm, DEF_WBITS);

    if(err != Z_OK){
        pr_err("\nInflate Init failed\n");
		goto out;
    }

	do {

		bytes_read = kernel_read(infile, in_buffer, CHUNK, &infile->f_pos);

		if(bytes_read < 0){
			pr_err("\nError while reading file during Decompression\n");
			err = -EINVAL;
			goto out;
		}

		strm->next_in = in_buffer;
		strm->avail_in = bytes_read;

		if(strm->avail_in == 0){
			break;
		}

		do{

			strm->avail_out = CHUNK;
			strm->next_out = out_buffer;
			err = zlib_inflate(strm, flush);

			if(err == Z_STREAM_ERROR){
				pr_err("\nError while Decompression\n");
				goto out;
			}

			if(err == Z_DATA_ERROR || err == Z_MEM_ERROR){
				pr_err("\ndata is corrupted. Decompression cannot proceed. Aborting %d \n", err);
				goto out;
			}

			have = CHUNK - strm->avail_out;

			bytes_wrote = kernel_write(outfile, out_buffer, have, &outfile->f_pos);

			if(bytes_wrote != have){
				pr_err("\nError while writing the compressed bytes to file\n");
				goto out;
			}

		} while(strm->avail_out == 0);

	} while(err != Z_STREAM_END);

out:

	if(strm)
		zlib_inflateEnd(strm);

    if(out_buffer)
        kfree(out_buffer);

    if(in_buffer)
        kfree(in_buffer);
    
    if(strm->workspace)
        kfree(strm->workspace);
    
    if(strm)
        kfree(strm);

    return err;

}

int compress_file(struct file* infile, struct file* outfile){

    int err = 0;

    char *in_buffer = NULL, *out_buffer = NULL;

	z_streamp strm = NULL;

	int bytes_read = 0, bytes_wrote = 0, flush = Z_NO_FLUSH, have = 0;

	in_buffer = kmalloc(CHUNK, GFP_KERNEL);

    if(!in_buffer){
        pr_err("\nNo enough memory to allocate for input buffer while compressing");
        err = -ENOMEM;
        goto out;
    }

    out_buffer = kmalloc(CHUNK, GFP_KERNEL);

    if(!out_buffer){
        pr_err("\nNo enough memory to allocate for output buffer while compressing");
        err = -ENOMEM;
        goto out;
    }

    memset(out_buffer, '\0', CHUNK);
    memset(in_buffer, '\0', CHUNK);

    strm = (z_streamp) kmalloc(sizeof(z_stream), GFP_KERNEL);

    if(!strm){
		pr_err("\nNot enough available memory for workspace during compression\n");
        err = -ENOMEM;
		goto out;
	}

    compression_init(strm);

	strm->next_in = NULL;
	strm->avail_in = 0;

	err = zlib_deflateInit2(strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);

    // err = zlib_deflateInit(strm, Z_DEFAULT_COMPRESSION);

    if(err != Z_OK){
        pr_err("\nDefalt Init failed\n");
		goto out;
    }

	do {

		bytes_read = kernel_read(infile, in_buffer, CHUNK, &infile->f_pos);

		if(bytes_read < 0){
			pr_err("\nError while reading file during compression\n");
			err = -EINVAL;
			goto out;
		}

		if(bytes_read == 0){
			pr_info("\nEntered here");
			flush = Z_FINISH;
		}

		strm->next_in = in_buffer;
		strm->avail_in = bytes_read;

		do{

			strm->avail_out = CHUNK;
			strm->next_out = out_buffer;
			err = zlib_deflate(strm, flush);

			if(err == Z_STREAM_ERROR){
				pr_err("\nError while compression\n");
				goto out;
			}

			have = CHUNK - strm->avail_out;

			bytes_wrote = kernel_write(outfile, out_buffer, have, &outfile->f_pos);

			if(bytes_wrote != have){
				pr_err("\nError while writing the compressed bytes to file\n");
				goto out;
			}

			//pr_info("Have : %d, %d", have, strm->avail_out);

		} while(strm->avail_out == 0);

		//pr_info("\nNext cycle\n");

		if(strm->avail_in != 0){
			pr_err("\n Available input is corrupted during compression \n");
			goto out;
		}

	} while(flush != Z_FINISH);

out:

	if(strm)
		zlib_deflateEnd(strm);

    if(out_buffer)
        kfree(out_buffer);

    if(in_buffer)
        kfree(in_buffer);
    
    if(strm->workspace)
        kfree(strm->workspace);
    
    if(strm)
        kfree(strm);

    return err;

}