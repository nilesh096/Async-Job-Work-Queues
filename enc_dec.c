#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <crypto/hash.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/skcipher.h>
#include <linux/namei.h>
#include <linux/string.h>
#include "job_data.h"
#include "file_ops.h"

int generate_hash(const u8 *input, unsigned int hash_length, u8 *output)
{
	int err = 0;
	struct shash_desc *desc = NULL;
	struct crypto_shash *alg = NULL;
	const char *hash_algo = "sha256";

	alg = crypto_alloc_shash(hash_algo, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(alg)) {
		pr_alert("crypto_alloc_shash failed\n");
		err = PTR_ERR(alg);
		goto out_hash;
	}

	desc = kmalloc(crypto_shash_descsize(alg) + sizeof(*desc), GFP_KERNEL);
	if (desc == NULL) {
		err = -ENOMEM;
		goto out_hash;
	}

	desc->tfm = alg;

	err = crypto_shash_digest(desc, input, hash_length, output);
	if (err < 0) {
		pr_alert("Failed to generate digest\n");
		goto out_hash;
	}
	
out_hash:
	if (desc != NULL) {
		pr_info("Cleanup: Freeing struct shash_desc\n");
		desc->tfm = NULL;
		kfree(desc);
	}
	if ((alg != NULL) && (!IS_ERR(alg))) {
		pr_info("Cleanup: Freeing hash algo struct\n");
		crypto_free_shash(alg);
	}
	return err;
}

bool verify_preamble(const void *buf1, const void *buf2)
{
	return memcmp(buf1, buf2, SHA256_LEN) == 0;
}

static int encdec(char *iv, struct skcipher_request *req, 
				void *buf, int buf_len, struct scatterlist *sg,
				struct crypto_wait *wait, unsigned int flag)
{
	int ret = 0;

	skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG, crypto_req_done, wait);
	sg_init_one(sg, buf, buf_len);
	skcipher_request_set_crypt(req, sg, sg, buf_len, iv);
	crypto_init_wait(wait);

	if (flag & 0x01)
		ret = crypto_wait_req(crypto_skcipher_encrypt(req), wait);
	else
	{
		pr_info("Entered in decrypt");
		ret = crypto_wait_req(crypto_skcipher_decrypt(req), wait);
	}

	return ret;
}

int write_preamble(void *buf, struct file *output)
{
	ssize_t wbytes = 0;
	
	wbytes = kernel_write(output, buf, SHA256_LEN, &output->f_pos);

	if (wbytes < 0) 
		return wbytes;

	return 0;
}

int read_preamble(void *buf, struct file *input)
{
	int err = 0;
	void *rbuf = NULL;

	rbuf = kmalloc(SHA256_LEN, GFP_KERNEL);

	if (rbuf == NULL) {
		err = -ENOMEM;
		goto out_read;
	}

	err = kernel_read(input, rbuf, SHA256_LEN, &input->f_pos);
	if (err < 0)
		goto out_read;


	if (!verify_preamble(buf, rbuf)) {
		pr_alert("Preamble mismatch\n");
		err = -EACCES;
		goto out_read;
	}

out_read:
	if (rbuf) {
		pr_debug("Freeing read buffer for reading preamble");
		kfree(rbuf);
	}
	return err;
}

int read_write(struct file *input, struct file *output, 
				void *key, unsigned int flag) 
{

	ssize_t read_bytes = 0, write_bytes = 0;
	int err = 0;
	struct crypto_skcipher *skcipher = NULL;
	struct skcipher_request *req = NULL;
	char *ivdata = NULL;
	void *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if (buf == NULL) {
		err = -ENOMEM;
		goto out_rw;
	}

	pr_info("Able to allocate memory for read buffer");

	if (!(!(flag & 0x01) && !(flag & 0x02))) {
		skcipher = crypto_alloc_skcipher("ctr-aes-aesni", 0, 0);
    	if (IS_ERR(skcipher)) {
        	pr_alert("ERROR: Failed to create skcipher handle\n");
        	err =  PTR_ERR(skcipher);
			goto out_rw;
    	}

		req = skcipher_request_alloc(skcipher, GFP_KERNEL);
    	if (req == NULL) {
        	pr_alert("Failed to allocate skcipher request\n");
        	err = -ENOMEM;
        	goto out_rw;
    	}

		ivdata = (char *)kmalloc(16, GFP_KERNEL);
    	if (!ivdata) {
        	pr_alert("Failed to allocate ivdata\n");
        	goto out_rw;
    	}
		memset(ivdata, 98765, 16);

		if (crypto_skcipher_setkey(skcipher, key, MD5_LEN)) {
			pr_alert("Error in setting key in skcipher\n");
			err = -EAGAIN;
			goto out_rw;
		}
	}

	while ((read_bytes = kernel_read(input, buf, PAGE_SIZE, &input->f_pos)) > 0) {
		if (flag & 0x01 || flag & 0x02) {
			struct scatterlist *sg = NULL;
			struct crypto_wait *wait = NULL;

			sg = (struct scatterlist *)kmalloc(sizeof(struct scatterlist), GFP_KERNEL);
			if (!sg) {
				err = -ENOMEM;
				pr_alert("ERROR: Error in allocating memory for scatterlist\n");
				goto out_rw;
			}
			wait = (struct crypto_wait *)kmalloc(sizeof(struct crypto_wait), GFP_KERNEL);
			if (!wait) {
				err = -ENOMEM;
				pr_alert("ERROR: Error in allocating memory for crypto_wait\n");
				kfree(sg);
				goto out_rw;
			}

			err = encdec(ivdata, req, buf, read_bytes, sg, wait, flag);
			kfree(wait);
			wait = NULL;
			kfree(sg);
			sg = NULL;
		
			if (err < 0) {
				if (flag & 0x01) {
					pr_alert("ERROR: Encryption operation failed\n");
					goto out_rw;
				} else {
					pr_alert("ERROR: Decryption operation failed\n");
					goto out_rw;
				}
			}
		}

		write_bytes = kernel_write(output, buf, read_bytes, &output->f_pos);
		pr_info("Bytes written = %ld\n", write_bytes);

		if (write_bytes < 0) {
			pr_alert("Error in writing data to output file\n");
			err = write_bytes;
			goto out_rw;
		}
	}

out_rw:
	if (buf) {
		pr_info("Cleanup: Cleaning buffer for read");
		kfree(buf);
	}

	if ((skcipher != NULL) && (!IS_ERR(skcipher)))
	{
		pr_info("Cleanup: Cleaning up skcipher");
		crypto_free_skcipher(skcipher);
	}

	if (req) {
		pr_info("Cleanup: Cleaning up request");
        skcipher_request_free(req);
	}

	if (ivdata) {
		pr_info("Cleanup: Cleaning up ivdata");
        kfree(ivdata);
	}
	return err;
}