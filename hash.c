#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <crypto/hash.h>
// #include <linux/crypto.h>
// #include <linux/scatterlist.h>
// #include <crypto/skcipher.h>
#include <linux/namei.h>
#include <linux/string.h>
#include "job_data.h"
#include "file_ops.h"

int generate_file_hash (struct file *input, struct file *output)
{
    int err = 0, x = 0;
    void *buf = NULL, *hash_cipher_key = NULL;
    ssize_t read_bytes = 0;
    struct shash_desc *desc = NULL;
    struct crypto_shash *alg = NULL;
    const char *hash_algo = "sha256";
    unsigned char *val;

    alg = crypto_alloc_shash(hash_algo, 0, CRYPTO_ALG_ASYNC);
    if (IS_ERR(alg)) {
        pr_alert("crypto_alloc_shash failed\n");
        err = PTR_ERR(alg);
        goto out;
    }
    
    err = mallock (&hash_cipher_key, SHA256_LEN);
    if (err < 0)
        goto out;

    desc = kmalloc(crypto_shash_descsize(alg) + sizeof(*desc), GFP_KERNEL);
    if (desc == NULL) {
        err = -ENOMEM;
        goto out;
    }

    desc->tfm = alg;

    err = mallock (&buf, PAGE_SIZE);
    if (err < 0)
        goto out;

    err = crypto_shash_init(desc);
	if(err)  {
		pr_alert("Failed to initialize shash\n");
		goto out;
	}

    while ((read_bytes = kernel_read(input, buf, PAGE_SIZE, &input->f_pos)) > 0)
    {
        err = crypto_shash_update(desc, (const u8 *)buf, read_bytes);
	    if(err) {
		    pr_alert("Failed to execute hashing function\n");
		    goto out;
	    }
    }

    err = crypto_shash_final(desc, (u8 *)hash_cipher_key);
	if(err) {
		pr_alert("Failed to complete hashing function\n");
		goto out;
	}
    pr_info ("Hash cipher key");
    err = kernel_write(output, hash_cipher_key, SHA256_LEN, &output->f_pos);
    if (err < 0) {
        pr_alert("Failed to write hash to output file");
        goto out; 
    }

    for (x = 0; x < SHA256_LEN; x++)
    {
        val = ((unsigned char *)hash_cipher_key) + x;
        pr_info("%02x and sizeof this is %ld", *val, sizeof(*val));
    }

out:
    if (buf)
        kfree(buf);
    if (hash_cipher_key)
        kfree(hash_cipher_key);
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
