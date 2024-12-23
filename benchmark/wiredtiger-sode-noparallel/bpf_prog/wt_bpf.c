#include <linux/bpf.h>
#include <asm-generic/types.h>
#include <bpf/bpf_helpers.h>

char _license[] SEC("license") = "GPL";

#define __inline inline __attribute__((always_inline))
#define __noinline __attribute__((noinline))
#define __nooptimize __attribute__((optnone))

#define memcpy(dest, src, n) __builtin_memcpy((dest), (src), (n))
#define memset(dest, value, n) __builtin_memset((dest), (value), (n))

/*
 * Type definitions for ebpf
 */
#define uint64_t __u64
#define uint32_t __u32
#define uint8_t __u8
#define int64_t __s64
#define int32_t __s32
#define int8_t __s8
#define bool short
#define NULL 0
#define true 1
#define false 0

/*
 * Config definitions
 */
#define EBPF_BLOCK_SIZE 512
#define EBPF_MAX_DEPTH 6
#define EBPF_DEPTH_MASK 0x7
#define EBPF_KEY_MAX_LEN 18
#define EBPF_CONTEXT_MASK 0xfff

/*
 * Error numbers
 */
#define EBPF_EINVAL 22
#define EBPF_EAGAIN 11

/*
 * Page layout
 */
 struct ebpf_page_header {
     uint64_t recno; /* 00-07: column-store starting recno */
     uint64_t write_gen; /* 08-15: write generation */
     uint32_t mem_size; /* 16-19: in-memory page size */
     union {
         uint32_t entries; /* 20-23: number of cells on page */
         uint32_t datalen; /* 20-23: overflow data length */
     } u;
     uint8_t type; /* 24: page type */
#define EBPF_PAGE_COMPRESSED 0x01u   /* Page is compressed on disk */
#define EBPF_PAGE_EMPTY_V_ALL 0x02u  /* Page has all zero-length values */
#define EBPF_PAGE_EMPTY_V_NONE 0x04u /* Page has no zero-length values */
#define EBPF_PAGE_ENCRYPTED 0x08u    /* Page is encrypted on disk */
#define EBPF_PAGE_UNUSED 0x10u       /* Historic lookaside store page updates, no longer used */
     uint8_t flags; /* 25: flags */
     uint8_t unused; /* 26: unused padding */
#define EBPF_PAGE_VERSION_ORIG 0 /* Original version */
#define EBPF_PAGE_VERSION_TS 1   /* Timestamps added */
     uint8_t version; /* 27: version */
};
#define EBPF_PAGE_HEADER_SIZE 28

#define EBPF_PAGE_INVALID 0       /* Invalid page */
#define EBPF_PAGE_BLOCK_MANAGER 1 /* Block-manager page */
#define EBPF_PAGE_COL_FIX 2       /* Col-store fixed-len leaf */
#define EBPF_PAGE_COL_INT 3       /* Col-store internal page */
#define EBPF_PAGE_COL_VAR 4       /* Col-store var-length leaf page */
#define EBPF_PAGE_OVFL 5          /* Overflow page */
#define EBPF_PAGE_ROW_INT 6       /* Row-store internal page */
#define EBPF_PAGE_ROW_LEAF 7      /* Row-store leaf page */

struct ebpf_block_header {
    /* copy from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/block.h#L329 */

    uint32_t disk_size; /* 00-03: on-disk page size */
    uint32_t checksum; /* 04-07: checksum */
    uint8_t flags; /* 08: flags */
    uint8_t unused[3]; /* 09-11: unused padding */
};
#define EBPF_BLOCK_HEADER_SIZE 12

/*
 * Cell types & macros
 * extract from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/cell.h#L10
 */
#define EBPF_CELL_KEY_SHORT 0x01     /* Short key */
#define EBPF_CELL_KEY_SHORT_PFX 0x02 /* Short key with prefix byte */
#define EBPF_CELL_VALUE_SHORT 0x03   /* Short data */
#define EBPF_CELL_SHORT_TYPE(v) ((v)&0x03U)

#define EBPF_CELL_SHORT_MAX 63  /* Maximum short key/value */
#define EBPF_CELL_SHORT_SHIFT 2 /* Shift for short key/value */

#define EBPF_CELL_64V 0x04         /* Associated value */
#define EBPF_CELL_SECOND_DESC 0x08 /* Second descriptor byte */

#define EBPF_CELL_ADDR_DEL (0)            /* Address: deleted */
#define EBPF_CELL_ADDR_INT (1 << 4)       /* Address: internal  */
#define EBPF_CELL_ADDR_LEAF (2 << 4)      /* Address: leaf */
#define EBPF_CELL_ADDR_LEAF_NO (3 << 4)   /* Address: leaf no overflow */
#define EBPF_CELL_DEL (4 << 4)            /* Deleted value */
#define EBPF_CELL_KEY (5 << 4)            /* Key */
#define EBPF_CELL_KEY_OVFL (6 << 4)       /* Overflow key */
#define EBPF_CELL_KEY_OVFL_RM (12 << 4)   /* Overflow key (removed) */
#define EBPF_CELL_KEY_PFX (7 << 4)        /* Key with prefix byte */
#define EBPF_CELL_VALUE (8 << 4)          /* Value */
#define EBPF_CELL_VALUE_COPY (9 << 4)     /* Value copy */
#define EBPF_CELL_VALUE_OVFL (10 << 4)    /* Overflow value */
#define EBPF_CELL_VALUE_OVFL_RM (11 << 4) /* Overflow value (removed) */

#define EBPF_CELL_TYPE_MASK (0x0fU << 4)
#define EBPF_CELL_TYPE(v) ((v)&EBPF_CELL_TYPE_MASK)

#define EBPF_CELL_SIZE_ADJUST (EBPF_CELL_SHORT_MAX + 1)

/*
 * Variable-sized unpacking for unsigned integers
 * extracted from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/intpack.i#L254
 */
#define EBPF_POS_1BYTE_MARKER (uint8_t)0x80
#define EBPF_POS_2BYTE_MARKER (uint8_t)0xc0
#define EBPF_POS_MULTI_MARKER (uint8_t)0xe0
#define EBPF_POS_1BYTE_MAX ((1 << 6) - 1)
#define EBPF_POS_2BYTE_MAX ((1 << 13) + EBPF_POS_1BYTE_MAX)

struct wt_ebpf_scratch {
    uint64_t key_size;
    char key[EBPF_KEY_MAX_LEN];

    int32_t level;
    int32_t iteration;
    uint64_t page_offset;
    uint64_t prev_cell_descent_offset, prev_cell_descent_size;

    int32_t nr_page;
    uint64_t descent_index_arr[EBPF_MAX_DEPTH];
};

/* Extract bits <start> to <end> from a value (counting from LSB == 0). */
#define GET_BITS(x, start, end) (((uint64_t)(x) & ((1U << (start)) - 1U)) >> (end))

__noinline int ebpf_lex_compare(struct bpf_xrp *context, uint64_t key_offset_1, uint64_t key_len_1,
                                uint64_t key_offset_2, uint64_t key_len_2) {
    /* extracted from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/btree_cmp.i#L90 */
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t *k_base = (uint8_t *) context->scratch;
    uint8_t b1, b2;
    uint64_t len = (key_len_1 > key_len_2) ? key_len_2 : key_len_1;
    uint64_t max_len = EBPF_KEY_MAX_LEN;
    for (; len > 0 && max_len > 0; --len, --max_len, ++key_offset_1, ++key_offset_2) {
        b1 = *(k_base + (key_offset_1 & EBPF_CONTEXT_MASK));
        b2 = *(p_base + (key_offset_2 & EBPF_CONTEXT_MASK));
        if (b1 != b2)
            return (b1 < b2 ? -1 : 1);
    }
    return ((key_len_1 == key_len_2) ? 0 : (key_len_1 < key_len_2) ? -1 : 1);
}

__noinline int ebpf_unpack_posint(struct bpf_xrp *context, uint64_t p_offset,
                                  uint64_t *retp, uint64_t *p_delta) {
    uint64_t x = 0;
    uint8_t max_len = 15;  /* max_len is set to pass the ebpf verifier */
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t b;
    uint8_t len;

    if (retp == NULL || p_delta == NULL)
        return -EBPF_EINVAL;

    /* There are four length bits in the first byte. */
    b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
    len = (b & 0xf);
    ++p_offset;
    ++(*p_delta);

    for (; len > 0 && max_len > 0; --len, --max_len) {
        b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
        x = (x << 8) | b;
        ++p_offset;
        ++(*p_delta);
    }

    *retp = x;
    return 0;
}

__noinline int ebpf_vunpack_uint(struct bpf_xrp *context, uint64_t p_offset,
                                 uint64_t *xp, uint64_t *p_delta) {
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t b;
    int ret;

    if (xp == NULL || p_delta == NULL)
        return -EBPF_EINVAL;

    /* encoding scheme: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/intpack.i#L10 */
    b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
    switch (b & 0xf0) {
    case EBPF_POS_1BYTE_MARKER:
    case EBPF_POS_1BYTE_MARKER | 0x10:
    case EBPF_POS_1BYTE_MARKER | 0x20:
    case EBPF_POS_1BYTE_MARKER | 0x30:
        /* higher 2 bits of the first byte is 10 */
        *xp = GET_BITS(b, 6, 0);  /* extract integer from the remaining (8 - 2) = 6 bites */
        ++p_offset;
        ++(*p_delta);
        break;
    case EBPF_POS_2BYTE_MARKER:
    case EBPF_POS_2BYTE_MARKER | 0x10:
        /* higher 3 bits of the first byte is 110 */
        *xp = GET_BITS(b, 5, 0) << 8;
        ++p_offset;
        ++(*p_delta);
        b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
        *xp |= b;
        *xp += EBPF_POS_1BYTE_MAX + 1;
        ++(*p_delta);
        ++p_offset;
        break;
    case EBPF_POS_MULTI_MARKER:
        /* higher 4 bits of the first byte is 1110 */
        ret = ebpf_unpack_posint(context, p_offset, xp, p_delta);
        if (ret != 0) {
            return ret;
        }
        *xp += EBPF_POS_2BYTE_MAX + 1;
        return 0;
    default:
        return -EBPF_EINVAL;
    }

    return 0;
}

__noinline int ebpf_addr_to_offset(struct bpf_xrp *context, uint64_t p_offset,
                                   uint64_t *offset, uint64_t *size) {
    int ret = 0;
    uint64_t p_delta = 0;
    uint64_t raw_offset = 0, raw_size = 0, raw_checksum = 0;

    if (offset == NULL || size == NULL)
        return -EBPF_EINVAL;

    ret = ebpf_vunpack_uint(context, p_offset, &raw_offset, &p_delta);
    if (ret < 0) {
        return ret;
    }
    p_offset += p_delta;
    p_delta = 0;

    ret = ebpf_vunpack_uint(context, p_offset, &raw_size, &p_delta);
    if (ret < 0) {
        return ret;
    }
    p_offset += p_delta;
    p_delta = 0;

    ret = ebpf_vunpack_uint(context, p_offset, &raw_checksum, &p_delta);  /* checksum is not used */
    if (ret < 0) {
        return ret;
    }

    if (raw_size == 0) {
        *offset = 0;
        *size = 0;
    } else {
        /* assumption: allocation size is EBPF_BLOCK_SIZE */
        *offset = EBPF_BLOCK_SIZE * (raw_offset + 1);
        *size = EBPF_BLOCK_SIZE * raw_size;
    }
    return 0;
}

static __inline int ebpf_get_cell_type(uint8_t cell_desc) {
    return EBPF_CELL_SHORT_TYPE(cell_desc) ? EBPF_CELL_SHORT_TYPE(cell_desc) : EBPF_CELL_TYPE(cell_desc);
}

__noinline int ebpf_parse_cell_addr(struct bpf_xrp *context, uint64_t p_offset,
                                    uint64_t *offset, uint64_t *size, uint64_t *p_delta) {
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t b;
    uint64_t local_p_delta = 0;
    uint8_t cell_desc, flags;
    uint64_t addr_len = 0;
    int ret;

    if (offset == NULL || size == NULL || p_delta == NULL)
        return -EBPF_EINVAL;

    /* read the first cell descriptor byte (cell type, RLE count) */
    b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
    cell_desc = b;
    if ((ebpf_get_cell_type(cell_desc) != EBPF_CELL_ADDR_INT
         && ebpf_get_cell_type(cell_desc) != EBPF_CELL_ADDR_LEAF
         && ebpf_get_cell_type(cell_desc) != EBPF_CELL_ADDR_LEAF_NO)
        || ((cell_desc & EBPF_CELL_64V) != 0)) {
        return -EBPF_EINVAL;
    }
    ++p_offset;
    ++(*p_delta);

    /* read the second cell descriptor byte (if present) */
    if ((cell_desc & EBPF_CELL_SECOND_DESC) != 0) {
        b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
        flags = b;
        ++p_offset;
        ++(*p_delta);
        if (flags != 0) {
            return -EBPF_EINVAL;
        }
    }

    /* the cell is followed by data length and a chunk of data */
    ret = ebpf_vunpack_uint(context, p_offset, &addr_len, &local_p_delta);
    if (ret != 0) {
        return ret;
    }
    p_offset += local_p_delta;
    (*p_delta) += local_p_delta;
    local_p_delta = 0;

    /* convert addr to file offset */
    ret = ebpf_addr_to_offset(context, p_offset, offset, size);
    if (ret != 0) {
        return ret;
    }

    (*p_delta) += addr_len;
    return 0;
}

__noinline int ebpf_parse_cell_key(struct bpf_xrp *context, uint64_t p_offset,
                                   uint64_t *key_offset, uint64_t *key_size, uint64_t *p_delta) {
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t b;
    uint64_t local_p_delta = 0;
    uint64_t data_len = 0;
    int ret;

    if (key_offset == NULL || key_size == NULL || p_delta == NULL)
        return -EBPF_EINVAL;
    (*key_offset) = 0;

    /* read the first cell descriptor byte (cell type, RLE count) */
    b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
    if ((ebpf_get_cell_type(b) != EBPF_CELL_KEY)
        || ((b & EBPF_CELL_64V) != 0)) {
        return -EBPF_EINVAL;
    }
    ++p_offset;
    ++(*p_delta);
    ++(*key_offset);

    /* key cell does not have the second descriptor byte */

    /* the cell is followed by data length and a chunk of data */
    ret = ebpf_vunpack_uint(context, p_offset, &data_len, &local_p_delta);
    if (ret != 0) {
        return ret;
    }
    data_len += EBPF_CELL_SIZE_ADJUST;
    p_offset += local_p_delta;
    (*p_delta) += local_p_delta;
    (*key_offset) += local_p_delta;
    local_p_delta = 0;

    *key_size = data_len;
    (*p_delta) += data_len;
    return 0;
}

__noinline int ebpf_parse_cell_short_key(struct bpf_xrp *context, uint64_t p_offset,
                                         uint64_t *key_offset, uint64_t *key_size, uint64_t *p_delta) {
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t b;
    uint64_t data_len;

    if (key_offset == NULL || key_size == NULL || p_delta == NULL)
        return -EBPF_EINVAL;
    (*key_offset) = 0;

    /* read the first cell descriptor byte */
    b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
    if (ebpf_get_cell_type(b) != EBPF_CELL_KEY_SHORT) {
        return -EBPF_EINVAL;
    }
    data_len = (b) >> EBPF_CELL_SHORT_SHIFT;
    *key_size = data_len;

    ++p_offset;
    ++(*p_delta);
    ++(*key_offset);

    (*p_delta) += data_len;
    return 0;
}

/*
__wt_page_inmem: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/bt_page.c#L128
__inmem_row_int: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/bt_page.c#L375
WT_CELL_FOREACH_ADDR: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/cell.i#L1155
__wt_cell_unpack_safe: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/cell.i#L663
__wt_row_search: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/row_srch.c#L331
*/
__noinline int ebpf_search_int_page(struct bpf_xrp *context,
                                    uint64_t user_key_offset, uint64_t user_key_size,
                                    uint64_t *descent_offset, uint64_t *descent_index) {
    uint8_t *p_base = (uint8_t *) context->data;
    uint8_t b;
    uint64_t p_offset = 0;
    uint64_t local_p_delta = 0;

    struct ebpf_page_header *header = (struct ebpf_page_header *) context->data;
    uint32_t nr_kv = header->u.entries / 2, i = 0, ii = 0;
    uint64_t prev_cell_descent_offset = 0, prev_cell_descent_size = 0;
    int ret = 0;

    asm volatile("r0 = 0" ::: "r0");

    if (descent_offset == NULL || descent_index == NULL) {
        bpf_printk("ebpf_search_int_page: invalid arguments");
        return -EBPF_EINVAL;
    }

    /* skip page header + block header */
    p_offset += (EBPF_PAGE_HEADER_SIZE + EBPF_BLOCK_HEADER_SIZE);

    /* traverse all key value pairs */
    for (i = 0, ii = EBPF_BLOCK_SIZE; i < nr_kv && ii > 0; ++i, --ii) {
        uint64_t cell_key_offset = 0, cell_key_size = 0;
        uint64_t cell_descent_offset = 0, cell_descent_size = 0;
        int cmp = 0;

        /*
         * searching for the corresponding descent.
         * each cell (key, addr) corresponds to key range [key, next_key)
         * extracted from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/row_srch.c#L331
         */

        /* parse key cell */
        b = *(p_base + (p_offset & EBPF_CONTEXT_MASK));
        switch (ebpf_get_cell_type(b)) {
        case EBPF_CELL_KEY:
            ret = ebpf_parse_cell_key(context, p_offset, &cell_key_offset, &cell_key_size, &local_p_delta);
            if (ret < 0) {
                bpf_printk("ebpf_search_int_page: ebpf_parse_cell_key failed, kv %d, ret %d", i, ret);
                return ret;
            }
            break;
        case EBPF_CELL_KEY_SHORT:
            ret = ebpf_parse_cell_short_key(context, p_offset, &cell_key_offset, &cell_key_size, &local_p_delta);
            if (ret < 0) {
                bpf_printk("ebpf_search_int_page: ebpf_parse_cell_short_key failed, kv %d ret %d", i, ret);
                return ret;
            }
            break;
        default:
            bpf_printk("ebpf_search_int_page: invalid cell type %d, kv %d", ebpf_get_cell_type(b), i);
            return -EBPF_EINVAL;
        }
        cell_key_offset += p_offset;
        p_offset += local_p_delta;
        local_p_delta = 0;

        /* parse addr cell */
        ret = ebpf_parse_cell_addr(context, p_offset, &cell_descent_offset, &cell_descent_size, &local_p_delta);
        if (ret < 0) {
            bpf_printk("ebpf_search_int_page: ebpf_parse_cell_addr failed, kv %d, ret %d", i, ret);
            return ret;
        }
        if (cell_descent_size != EBPF_BLOCK_SIZE) {
            bpf_printk("ebpf_search_int_page: descent size mismatch, expected %lld, got %lld", EBPF_BLOCK_SIZE, cell_descent_size);
            return -EBPF_EINVAL;
        }
        p_offset += local_p_delta;
        local_p_delta = 0;

        /*
         * compare with user key
         * extracted from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/row_srch.c#L331
         */
        if (i == 0)
            cmp = 1;  /* 0-th key is MIN */
        else
            cmp = ebpf_lex_compare(context, user_key_offset, user_key_size, cell_key_offset, cell_key_size);
        if (cmp == 0) {
            /* user key = cell key */
            *descent_offset = cell_descent_offset;
            *descent_index = i;
            return 0;
        } else if (cmp < 0) {
            /* user key < cell key */
            *descent_offset = prev_cell_descent_offset;
            *descent_index = i - 1;
            return 0;
        }
        prev_cell_descent_offset = cell_descent_offset;
        prev_cell_descent_size = cell_descent_size;
    }
    *descent_offset = prev_cell_descent_offset;
    *descent_index = i - 1;
    return 0;
}

SEC("prog")
__u32 wiredtiger_lookup(struct bpf_xrp *context) {
    struct wt_ebpf_scratch *scratch = (struct wt_ebpf_scratch *) context->scratch;
    struct ebpf_page_header *header = (struct ebpf_page_header *) context->data;
    uint64_t descent_offset = 0, descent_index = 0;
    uint64_t *src_ptr = (uint64_t *) context->data;
    uint64_t *dst_ptr = (uint64_t *) context->scratch;
    uint64_t src_offset, dst_offset;
    int i;
    int ret;

    dst_offset = 1024;
    dst_offset += EBPF_BLOCK_SIZE * scratch->level;
    dst_offset /= sizeof(uint64_t);
    src_offset = 0;
    for (i = 0; i < EBPF_BLOCK_SIZE / sizeof(uint64_t); ++i, ++src_offset, ++dst_offset) {
        *(dst_ptr + (dst_offset & (EBPF_CONTEXT_MASK >> 3))) = *(src_ptr + (src_offset & (EBPF_CONTEXT_MASK >> 3)));
    }
    ++scratch->nr_page;

    switch (header->type) {
    case EBPF_PAGE_ROW_INT:
        ret = ebpf_search_int_page(context, offsetof(struct wt_ebpf_scratch, key), scratch->key_size, &descent_offset, &descent_index);
        if (ret == 0) {
            scratch->descent_index_arr[scratch->level & EBPF_DEPTH_MASK] = descent_index;
            /* fill control fields in the context */
            if (scratch->level == EBPF_MAX_DEPTH - 1) {
                /* buffer is full, return to the application immediately */
                context->done = true;
            } else {
                context->done = false;
                context->next_addr[0] = descent_offset;
                context->size[0] = EBPF_BLOCK_SIZE;
            }
            /* update scratch */
            ++scratch->level;
        } else {
            bpf_printk("wiredtiger_lookup: ebpf_search_int_page failed, ret %d", ret);
        }
        break;
    case EBPF_PAGE_ROW_LEAF:
        /* reach leaf page, return to the application immediately */
        context->done = true;
        ret = 0;
        break;
    default:
        bpf_printk("wiredtiger_lookup: unknown page type %d", header->type);
        ret = -EBPF_EINVAL;
    }
    return -1 * ret;
}
