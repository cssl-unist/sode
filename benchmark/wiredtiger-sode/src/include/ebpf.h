/*
 * Config definitions
 */

#define EBPF_DATA_BUFFER_SIZE 4096
#define EBPF_SCRATCH_BUFFER_SIZE 4096
#define EBPF_BLOCK_SIZE 512
/* page is always block size */
#define EBPF_MAX_DEPTH 6
#define EBPF_KEY_MAX_LEN 18

/*
 * Error numbers
 */
#define EBPF_EINVAL 22
#define EBPF_NOT_FOUND 1

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

/* Extract bits <start> to <end> from a value (counting from LSB == 0). */
#define GET_BITS(x, start, end) (((uint64_t)(x) & ((1U << (start)) - 1U)) >> (end))

int ebpf_unpack_posint(uint8_t **pp, uint64_t *retp);
int ebpf_vunpack_uint(uint8_t **pp, uint64_t *xp);
int ebpf_addr_to_offset(uint8_t *addr, uint64_t *offset, uint64_t *size);
int ebpf_get_cell_type(uint8_t *cell);
int ebpf_parse_cell_addr(uint8_t **cellp, uint64_t *offset, uint64_t *size, 
                         bool update_pointer);

#define __NR_read_xrp 445
#define __NR_read_sode_parallel 447

int ebpf_lookup(int fd, uint64_t offset, uint8_t *key_buf, uint64_t key_size, 
                uint8_t *data_buf, uint8_t *scratch_buf, uint8_t **page_data_arr_p,
                uint64_t *child_index_arr, int *nr_page);

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
