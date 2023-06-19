#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_FILENAME_LEN 256
#define MAX_OUTFILE_NUM 5
#define MAX_BANK_NUM 8
#define MAX_BANK_BIT 3
#define INTERLEAVE_NUM 2
#define MAX_BANK_MASK ((1 << MAX_BANK_BIT) - 1)
#define MAX_COL_SIZE 0x2000
#define COL_BANK_MASK ((MAX_COL_SIZE << MAX_BANK_BIT) - 1)
#define COL_MASK (MAX_COL_SIZE - 1)

long create_palladium_file(unsigned char *src,
                           char *filename,
                           unsigned long long address,
                           unsigned long max_size);

long create_mem_image(char *filename,
                      unsigned char *out,
                      unsigned long long address,
                      unsigned long max_size);

int merge_interleave_to_linear(char *filename,
                               unsigned long long address,
                               unsigned long max_size,
                               int interleave,
                               unsigned char *tb_buf[],
                               int tb_num);

int divide_linear_to_interleave(char *filename,
                                unsigned long long address,
                                unsigned long max_size,
                                int interleave,
                                unsigned char *tb_buf[],
                                unsigned long tb_address[],
                                unsigned long tb_max_size[],
                                int tb_num);

void usage(int argc, char *argv[])
{
    printf("usage: %s <load/dump> infile size outfile address dir_prefix [interleave]\n", argv[0]);
    printf("        <load/dump> 0 - load 1 - dump 2 - create load.tcl 3 - dump.tcl only\n");

    return;
}

// return: maximum size
unsigned long init_param(unsigned long long address, unsigned long max_size, int *start_bank, int *start_col, int *start_byte, int *max_bank)
{
    unsigned long ret_max_size;

    *start_bank = (address / MAX_COL_SIZE) & MAX_BANK_MASK;
    *start_byte = address & 0xf;
    *start_col = (address & COL_MASK) - *start_byte;

    ret_max_size = max_size + *start_byte; // process start_byte more to align 16byte
    *max_bank = (max_size + *start_col + MAX_COL_SIZE - 1) / MAX_COL_SIZE;
    if (*max_bank > MAX_BANK_NUM)
        *max_bank = MAX_BANK_NUM;

    return ret_max_size;
}

int get_interleave_param(unsigned long long address, unsigned long max_size, int tb_num, int interleave, unsigned long tb_address[], unsigned long tb_max_size[])
{
    int start_offset, start_address;
    int start_tb;
    int interleave_size = interleave << 10;
    int i, size;

    if (interleave_size == 0)
    {
        tb_address[0] = address;
        tb_max_size[0] = max_size;
        return 0;
    }
    /* tb address */
    start_tb = (address / interleave_size) % tb_num;
    start_offset = address & (interleave_size - 1);
    start_address = (address / tb_num) & ~(interleave_size - 1);
    for (i = start_tb + 1; i < tb_num; i++)
        tb_address[i] = start_address;
    for (i = 0; i < start_tb; i++)
        tb_address[i] = start_address + interleave_size;
    tb_address[start_tb] = start_address + start_offset;

    /* tb max size */
    memset(tb_max_size, 0, sizeof(tb_max_size[0]) * tb_num);
    size = 0;
    while (1)
    {
        int cnt = interleave_size - start_offset;

        if (cnt > max_size - size)
            cnt = max_size - size;

        tb_max_size[start_tb] += cnt;
        size += cnt;
        if (size >= max_size)
            break;

        /* update status */
        start_tb++;
        if (start_tb == tb_num)
            start_tb = 0;
        start_offset = 0;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    char *s;
    unsigned long max_size;
    int opt;
    FILE *fin = NULL;
    unsigned char *pBuf[INTERLEAVE_NUM] = {NULL};
    unsigned long tb_out_address[INTERLEAVE_NUM] = {0};
    unsigned long tb_max_size[INTERLEAVE_NUM] = {0};
    int ret = 0;
    long size;
    int i, j;
    unsigned long long address;
    char prefix[MAX_FILENAME_LEN];
    char fn[MAX_FILENAME_LEN];
    int max_bank;
    unsigned long bank_size[MAX_BANK_NUM];
    int start_bank, start_col, start_byte;
    int offset, tb_num;
    int interleave = 0, interleave_size;

    if (argc < 6)
    {
        usage(argc, argv);
        return -1;
    }

    opt = atoi(argv[1]);
    max_size = strtoul(argv[3], NULL, 0);

    address = strtoull(argv[5], NULL, 0);
    address = (address & 0xFFFFFFFF);

    if (argc == 6)
        prefix[0] = '\0';
    else
        strcpy(prefix, argv[6]);

    if (argc == 8)
    {
        interleave = atoi(argv[7]);
        interleave_size = interleave << 10;
    } else
    {
        interleave = 0;
        interleave_size = 0;
    }

    /* prepare parameters */
    if (interleave)
    {
        tb_num = INTERLEAVE_NUM;
        size = max_size / tb_num;
        size &= ~(interleave_size - 1);
        size += interleave_size;
    }
    else
    {
        size = max_size;
        tb_num = 1;
    }

    for (i = 0; i < tb_num; i++)
    {
        if (strcmp(argv[2], "NULL") != 0)
        {
            pBuf[i] = malloc(size);
            if (pBuf[i] == NULL)
            {
                printf("malloc %d buffer failed\n", i);
                ret = -1;
                goto EXIT;
            }
        }
    }

    switch (opt)
    {
    case 0: // load
        s = strrchr(argv[4], '.');
        if (s != NULL)
            *s = '\0';

        /* apply interleave mode or keep it in linear mode */
        if (divide_linear_to_interleave(argv[2], address, max_size, interleave, pBuf, tb_out_address, tb_max_size, tb_num) < 0)
        {
            printf("process linear file to interleave mode failed\n");
            ret = -1;
            goto EXIT;
        }

        /* crate memory file for each ddr */
        for (i = 0; i < tb_num; i++)
        {
            sprintf(fn, "%s_tb%d", argv[4], i);
            if (create_palladium_file(pBuf[i], fn, tb_out_address[i], tb_max_size[i]) < 0)
            {
                printf("create palladium memory failed\n");
                ret = -1;
                goto EXIT;
            }
        }

        break;

    case 1: // dump
        s = strrchr(argv[2], '.');
        if (s != NULL)
            *s = '\0';

        if (get_interleave_param(address, max_size, tb_num, interleave, tb_out_address, tb_max_size) < 0)
        {
            printf("get interleave param failed\n");
            ret = -1;
            goto EXIT;
        }

        max_size = 0;
        for (i = 0; i < tb_num; i++)
        {
            sprintf(fn, "%s_tb%d", argv[2], i);
            if ((tb_max_size[i] = create_mem_image(fn, pBuf[i], tb_out_address[i], tb_max_size[i])) < 0)
            {
                printf("dump palladium memory failed\n");
                ret = -1;
                goto EXIT;
            }
            max_size += tb_max_size[i];
        }

        if (merge_interleave_to_linear(argv[4], address, max_size, interleave, pBuf, tb_num) < 0)
        {
            printf("merge interleave to linear file failed\n");
            ret = -1;
            goto EXIT;
        }

        break;

    default:
        break;
    }

    /* create tcl script */
    /* get interleave parameter */
    if ((opt == 0 || opt == 2) && strcmp(argv[2], "NULL"))
    {
        fin = fopen(argv[2], "rb");
        if (fin == NULL)
        {
            printf("open faile %s failed\n", argv[2]);
            return -1;
        }

        size = ftell(fin);
        fseek(fin, 0, SEEK_END);
        size = ftell(fin) - size;
        fseek(fin, 0, SEEK_SET);
        size = size > max_size ? max_size : size;
        max_size = size;

        fclose(fin);
        fin = NULL;
    }
    if (get_interleave_param(address, max_size, tb_num, interleave, tb_out_address, tb_max_size) < 0)
    {
        printf("get interleave param failed\n");
        ret = -1;
        goto EXIT;
    }
    if (opt == 0 || opt == 2) // load.tcl
    {
        /* open file */
        s = strrchr(argv[4], '/');
        if (s == NULL)
            s = strrchr(argv[4], '\\');
        if (s != NULL)
        {
            *(s + 1) = '\0';
            sprintf(fn, "%sload.tcl", argv[4]);
        }
        else
            sprintf(fn, "load.tcl");

        fin = fopen(fn, "wt");
        if (fin == NULL)
        {
            printf("open file %s failed!\n", fn);
            ret = -1;
            goto EXIT;
        }

        for (j = 0; j < tb_num; j++)
        {
            max_size = init_param(tb_out_address[j], tb_max_size[j], &start_bank, &start_col, &start_byte, &max_bank);
            address = ((tb_out_address[j] / MAX_COL_SIZE) >> MAX_BANK_BIT) * MAX_COL_SIZE;
            address = (address >> 4) << 1; // for mem0~4 (mem0~3 2byte/line mem4 1byte/line), 16-byte aligned
            memset(bank_size, 0, sizeof(bank_size));

            offset = start_col;
            while (max_size)
            {
                for (i = start_bank; i < start_bank + max_bank; i++)
                {
                    size = max_size > (MAX_COL_SIZE - offset) ? (MAX_COL_SIZE - offset) : max_size;
                    bank_size[i & MAX_BANK_MASK] += size;
                    max_size -= size;
                    offset = 0;
                }
            }
            for (i = 0; i < MAX_BANK_NUM; i++)
                bank_size[i] = (bank_size[i] + 7) >> 3;

            offset = start_col >> 3;
            for (i = start_bank; i < start_bank + max_bank; i++)
            {
                int id = i & MAX_BANK_MASK;
                fprintf(fin, "memory -load %%readmemh {test_top.ddrc_tb_%d.model_4Gx16_x0.memcore} -file %s%s_tb%d_ba%d_mem0.h  -start 0x%lx -end 0x%lx\n",
                        j, prefix, argv[4], j, id, 0x4000000 * id + (unsigned long)(address + offset), 0x4000000 * id + (unsigned long)address + offset + bank_size[id] - 1);
                fprintf(fin, "memory -load %%readmemh {test_top.ddrc_tb_%d.model_4Gx16_x1.memcore} -file %s%s_tb%d_ba%d_mem1.h  -start 0x%lx -end 0x%lx\n",
                        j, prefix, argv[4], j, id, 0x4000000 * id + (unsigned long)(address + offset), 0x4000000 * id + (unsigned long)address + offset + bank_size[id] - 1);
                fprintf(fin, "memory -load %%readmemh {test_top.ddrc_tb_%d.model_4Gx16_x2.memcore} -file %s%s_tb%d_ba%d_mem2.h  -start 0x%lx -end 0x%lx\n",
                        j, prefix, argv[4], j, id, 0x4000000 * id + (unsigned long)(address + offset), 0x4000000 * id + (unsigned long)address + offset + bank_size[id] - 1);
                fprintf(fin, "memory -load %%readmemh {test_top.ddrc_tb_%d.model_4Gx16_x3.memcore} -file %s%s_tb%d_ba%d_mem3.h  -start 0x%lx -end 0x%lx\n",
                        j, prefix, argv[4], j, id, 0x4000000 * id + (unsigned long)(address + offset), 0x4000000 * id + (unsigned long)address + offset + bank_size[id] - 1);
                fprintf(fin, "memory -load %%readmemh {test_top.ddrc_tb_%d.model_4Gx16_x4.memcore} -file %s%s_tb%d_ba%d_mem4.h  -start 0x%lx -end 0x%lx\n",
                        j, prefix, argv[4], j, id, 0x4000000 * id + (unsigned long)(address + offset), 0x4000000 * id + (unsigned long)address + offset + bank_size[id] - 1);

                if (i == MAX_BANK_NUM - 1)
                    address += (MAX_COL_SIZE >> 3);
                offset = 0;
            }
        }

        fclose(fin);
        fin = NULL;
    }
    else if (opt == 3) // dump.tcl
    {
        s = strrchr(argv[4], '/');
        if (s == NULL)
            s = strrchr(argv[4], '\\');
        if (s != NULL)
        {
            *(s + 1) = '\0';
            sprintf(fn, "%sdump.tcl", argv[4]);
        }
        else
            sprintf(fn, "dump.tcl");
        fin = fopen(fn, "wt");
        if (fin == NULL)
        {
            printf("open file %s failed!\n", fn);
            ret = -1;
            goto EXIT;
        }

        for (j = 0; j < tb_num; j++)
        {
            max_size = init_param(tb_out_address[j], tb_max_size[j], &start_bank, &start_col, &start_byte, &max_bank);
            address = ((tb_out_address[j] / MAX_COL_SIZE) >> MAX_BANK_BIT) * MAX_COL_SIZE;
            address = (address >> 4) << 1; // for mem0~4 (mem0~3 2byte/line mem4 1byte/line), 16-byte aligned
            memset(bank_size, 0, sizeof(bank_size));

            offset = start_col;
            while (max_size)
            {
                for (i = start_bank; i < start_bank + max_bank; i++)
                {
                    size = max_size > (MAX_COL_SIZE - offset) ? (MAX_COL_SIZE - offset) : max_size;
                    bank_size[i & MAX_BANK_MASK] += size;
                    max_size -= size;
                    offset = 0;
                }
            }
            for (i = 0; i < MAX_BANK_NUM; i++)
                bank_size[i] = (bank_size[i] + 7) >> 3;

            offset = start_col >> 3;
            for (i = start_bank; i < start_bank + max_bank; i++)
            {
                int id = i & MAX_BANK_MASK;
                fprintf(fin, "memory -dump %%readmemh {test_top.ddrc_tb_%d.model_4Gx16_x0.memcore} -file %s%s_tb%d_ba%d_mem0.h  -start 0x%lx -end 0x%lx\n",
                        j, prefix, argv[4], j, id, 0x4000000 * id + (unsigned long)(address + offset), 0x4000000 * id + (unsigned long)(address + offset) + bank_size[id] - 1);
                fprintf(fin, "memory -dump %%readmemh {test_top.ddrc_tb_%d.model_4Gx16_x1.memcore} -file %s%s_tb%d_ba%d_mem1.h  -start 0x%lx -end 0x%lx\n",
                        j, prefix, argv[4], j, id, 0x4000000 * id + (unsigned long)(address + offset), 0x4000000 * id + (unsigned long)(address + offset) + bank_size[id] - 1);
                fprintf(fin, "memory -dump %%readmemh {test_top.ddrc_tb_%d.model_4Gx16_x2.memcore} -file %s%s_tb%d_ba%d_mem2.h  -start 0x%lx -end 0x%lx\n",
                        j, prefix, argv[4], j, id, 0x4000000 * id + (unsigned long)(address + offset), 0x4000000 * id + (unsigned long)(address + offset) + bank_size[id] - 1);
                fprintf(fin, "memory -dump %%readmemh {test_top.ddrc_tb_%d.model_4Gx16_x3.memcore} -file %s%s_tb%d_ba%d_mem3.h  -start 0x%lx -end 0x%lx\n",
                        j, prefix, argv[4], j, id, 0x4000000 * id + (unsigned long)(address + offset), 0x4000000 * id + (unsigned long)(address + offset) + bank_size[id] - 1);
                fprintf(fin, "memory -dump %%readmemh {test_top.ddrc_tb_%d.model_4Gx16_x4.memcore} -file %s%s_tb%d_ba%d_mem4.h  -start 0x%lx -end 0x%lx\n",
                        j, prefix, argv[4], j, id, 0x4000000 * id + (unsigned long)(address + offset), 0x4000000 * id + (unsigned long)(address + offset) + bank_size[id] - 1);

                if (i == MAX_BANK_NUM - 1)
                    address += (MAX_COL_SIZE >> 3);
                offset = 0;
            }
        }

        fclose(fin);
        fin = NULL;
    }

EXIT:
    if (fin)
    {
        fclose(fin);
        fin = NULL;
    }
    for (i = 0; i < INTERLEAVE_NUM; i++)
        if (pBuf[i])
        {
            free(pBuf[i]);
            pBuf[i] = NULL;
        }

    return 0;
}

int divide_linear_to_interleave(char *filename,
                                unsigned long long address,
                                unsigned long max_size,
                                int interleave,
                                unsigned char *tb_buf[],
                                unsigned long tb_address[],
                                unsigned long tb_max_size[],
                                int tb_num)
{
    FILE *fin = NULL;
    int start_offset, start_shift, start_address;
    int interleave_size = interleave << 10;
    int i;
    int size, ret;

    /* process input file */
    if (strcmp(filename, "NULL"))
    {
        fin = fopen(filename, "rb");
        if (fin == NULL)
        {
            printf("open faile %s failed\n", filename);
            return -1;
        }

        size = ftell(fin);
        fseek(fin, 0, SEEK_END);
        size = ftell(fin) - size;
        fseek(fin, 0, SEEK_SET);
        size = size > max_size ? max_size : size;
        max_size = size;
    }

    if (interleave_size == 0)
    {
        if (fin) fread(tb_buf[0], 1, max_size, fin);
        tb_max_size[0] = max_size;
        tb_address[0] = address;
        if (fin) fclose(fin);

        return 0;
    }
    /* read data */
    start_shift = (address / interleave_size) % tb_num;
    start_offset = address & (interleave_size - 1);
    memset(tb_max_size, 0, sizeof(tb_max_size[0]) * tb_num);
    size = 0;
    while (1)
    {
        int read_size;
        if (tb_buf[start_shift])
            read_size = fread(tb_buf[start_shift] + tb_max_size[start_shift], 1, interleave_size - start_offset, fin);
        else
            read_size = interleave_size - start_offset;

        if (read_size > max_size - size)
            read_size = max_size - size;

        tb_max_size[start_shift] += read_size;
        size += read_size;
        if (size >= max_size)
            break;

        /* update status */
        start_shift++;
        if (start_shift == tb_num)
            start_shift = 0;
        start_offset = 0;
    }

    if(fin)
        fclose(fin);

    /* update tb parameter */
    ret = get_interleave_param(address, max_size, tb_num, interleave, tb_address, tb_max_size);

    return ret;
}

int merge_interleave_to_linear(char *filename,
                               unsigned long long address,
                               unsigned long max_size,
                               int interleave,
                               unsigned char *tb_buf[],
                               int tb_num)
{
    FILE *fout;
    int start_offset, start_shift;
    int tb_offset[INTERLEAVE_NUM];
    int interleave_size = interleave << 10;
    int i;
    int size, ret;

    fout = fopen(filename, "wb");
    if (fout == NULL)
    {
        printf("open faile %s failed\n", filename);
        return -1;
    }

    if (interleave_size == 0)   // non-interleave case
    {
        fwrite(tb_buf[0], 1, max_size, fout);
        fclose(fout);
        return 0;
    }

    /* write data */
    start_shift = (address / interleave_size) % tb_num;
    start_offset = address & (interleave_size - 1);
    memset(tb_offset, 0, sizeof(tb_offset[0]) * tb_num);
    size = 0;

    while (1)
    {
        int cnt;

        cnt = interleave_size - start_offset;
        if (cnt > max_size - size)
            cnt = max_size - size;
        fwrite(tb_buf[start_shift] + tb_offset[start_shift], 1, cnt, fout);

        size += cnt;
        tb_offset[start_shift] += cnt;
        if (size >= max_size)
            break;

        /* update status */
        start_shift++;
        if (start_shift >= tb_num)
            start_shift = 0;
        start_offset = 0;
    }

    fclose(fout);

    return 0;
}

long create_mem_image(char *filename, unsigned char *out, unsigned long long address, unsigned long max_size)
{
    char fn[MAX_FILENAME_LEN];
    FILE *fp[MAX_BANK_NUM][MAX_OUTFILE_NUM];
    unsigned char *p[MAX_BANK_NUM], *pDst[MAX_BANK_NUM];
    unsigned long dstSize[MAX_BANK_NUM];
    int i;
    int j;
    unsigned long size_per_block;
    unsigned long count, size;
    long ret = 0;
    int max_bank;
    int start_bank, start_col, start_byte;

    memset(p, 0, sizeof(p));
    memset(fp, 0, sizeof(fp));

    max_size = init_param(address, max_size, &start_bank, &start_col, &start_byte, &max_bank);

    for (i = 0; i < MAX_OUTFILE_NUM; i++)
    {
        for (j = start_bank; j < start_bank + max_bank; j++)
        {
            int id = j & MAX_BANK_MASK;
            sprintf(fn, "%s_ba%d_mem%d.h", filename, id, i);
            fp[id][i] = fopen(fn, "rt");
            if (fp[id][i] == NULL)
            {
                printf("open file %s failed\n", fn);
                ret = -1;
                goto END1;
            }
        }
    }

    /* malloc */
    size_per_block = (max_size + max_bank - 1) / max_bank;
    size_per_block = (size_per_block + MAX_COL_SIZE - 1) / MAX_COL_SIZE;
    size_per_block = size_per_block * MAX_COL_SIZE;
    for (i = 0; i < MAX_BANK_NUM; i++)
    {
        p[i] = (unsigned char *)malloc(size_per_block);
        if (p[i] == NULL)
        {
            printf("malloc failed\n");
            ret = -1;
            goto END1;
        }
        else
            printf("malloc[%d] done: size = 0x%08lx\n", i, size_per_block);
    }

    /* remove first line @ */
    for (i = start_bank; i < start_bank + max_bank; i++)
    {
        int id = i & MAX_BANK_MASK;
        for (j = 0; j < MAX_OUTFILE_NUM; j++)
        {
            if (fscanf(fp[id][j], "@%*[^\n]\n") == EOF)
            {
                printf("[bank %d file %d] only one commet line\n", id, j);
                continue;
            }
        }
    }
    /* read data to each bank */
    memcpy(pDst, p, sizeof(p));
    for (i = start_bank; i < start_bank + max_bank; i++)
    {
        int id = i & MAX_BANK_MASK;
        j = 0;
        count = 0;
        while (1)
        {
            unsigned int temp;
            if (j == MAX_OUTFILE_NUM - 1)
            {
                if (fscanf(fp[id][j], "%x\n", &temp) == EOF)
                    break;

                if ((count < 16) && (pDst[id] - p[id] < size_per_block))
                    *pDst[id]++ = (unsigned char)(temp & 0xff);
                count = (count + 1) % 18;
            }
            else if (fscanf(fp[id][j], "%x\n", &temp) == EOF)
                break;
            else
            {
                int k;
                for (k = 0; k < 2; k++)
                {
                    if ((count < 16) && (pDst[id] - p[id] < size_per_block))
                        *pDst[id]++ = (unsigned char)(temp & 0xff);
                    temp = temp >> 8;
                    count = (count + 1) % 18;
                }
            }

            if (pDst[id] - p[id] >= size_per_block)
                break;

            j = (j + 1) % MAX_OUTFILE_NUM;
        }
    }
    count = 0;
    for (i = start_bank; i < start_bank + max_bank; i++)
    {
        int id = i & MAX_BANK_MASK;
        dstSize[id] = pDst[id] - p[id];
        count += dstSize[id];
        printf("bank[%d] size = 0x%08lx\n", id, dstSize[id]);
    }
    if (max_size > count) // protect max_size too large to drop into dead loop at next loop
        max_size = count;

    /* combind to whole binary */
    count = 0;
    memcpy(pDst, p, sizeof(p));
    i = start_bank;
    while (1)
    {
        int id = i & MAX_BANK_MASK;

        if (dstSize[id] == 0)
            break;
        else
            size = dstSize[id] > (MAX_COL_SIZE - start_col) ? (MAX_COL_SIZE - start_col) : dstSize[id];
        start_col = 0;

        size = size > (max_size - count) ? max_size - count : size;

        memcpy(out + count, pDst[id] + start_byte, size - start_byte);

        count += (size - start_byte);
        max_size -= start_byte;
        start_byte = 0;

        pDst[id] += size;
        dstSize[id] -= size;

        if (count >= max_size)
            break;

        if (++i >= (start_bank + max_bank))
            i = start_bank;
    }
    ret = count;

END1:
    for (i = 0; i < MAX_OUTFILE_NUM; i++)
        for (j = 0; j < MAX_BANK_NUM; j++)
            if (fp[j][i])
            {
                fclose(fp[j][i]);
                fp[j][i] = NULL;
            }

    for (i = 0; i < MAX_BANK_NUM; i++)
        if (p[i])
        {
            free(p[i]);
            p[i] = NULL;
        }

    return ret;
}

long create_palladium_file(unsigned char *src, char *filename, unsigned long long address, unsigned long max_size)
{
    char fn[MAX_FILENAME_LEN];
    FILE *fp[MAX_BANK_NUM][MAX_OUTFILE_NUM];
    unsigned long l;
    int i, j;
    int value = 0;
    int fid = 0;
    unsigned char *p[MAX_BANK_NUM] = {NULL}, *pSrc, *pDst[MAX_BANK_NUM];
    unsigned long total, size_per_block, size;
    long ret = 0;
    int max_bank = 0;
    int dstSize[MAX_BANK_NUM];
    int start_bank, start_col, start_byte;

    memset(fp, 0, sizeof(fp));
    memset(p, 0, sizeof(p));

    max_size = init_param(address, max_size, &start_bank, &start_col, &start_byte, &max_bank);

    for (i = 0; i < MAX_OUTFILE_NUM; i++)
    {
        for (j = start_bank; j < max_bank + start_bank; j++)
        {
            int id = j & MAX_BANK_MASK;
            sprintf(fn, "%s_ba%d_mem%d.h", filename, id, i);
            fp[id][i] = fopen(fn, "wt");
            if (fp[id][i] == NULL)
            {
                printf("create file %s failed\n", fn);
                ret = -1;
                goto END;
            }
        }
    }

    size_per_block = (max_size + max_bank - 1) / max_bank;
    size_per_block = (size_per_block + MAX_COL_SIZE - 1) / MAX_COL_SIZE;
    size_per_block = size_per_block * MAX_COL_SIZE;

    for (j = 0; j < MAX_BANK_NUM; j++)
    {
        p[j] = (unsigned char *)malloc(size_per_block / 16 * 18);
        if (p[j] == NULL)
        {
            printf("malloc failed\n");
            ret = -1;
            goto END;
        }
        else
            printf("malloc[%d] done: size = 0x%08lx\n", j, size_per_block / 16 * 18);
    }

    memcpy(pDst, p, sizeof(p));
    total = 0;
    pSrc = src;
    while (max_size)
    {
        for (i = start_bank; i < start_bank + max_bank; i++)
        {
            int id = i & MAX_BANK_MASK;
            size = max_size > (MAX_COL_SIZE - start_col) ? (MAX_COL_SIZE - start_col) : max_size;

            for (l = 0; l < start_byte; l++)
                *pDst[id]++ = 0;

            if (pSrc == NULL)
            {
                for (l = start_byte; l < size; l++)
                {
                    *pDst[id]++ = ((unsigned char *)&value)[(l - start_byte) & 0x3];
                    if ((l & 0xf) == 0xF)
                    {
                        *pDst[id]++ = 0xFF;
                        *pDst[id]++ = 0xFF;
                        total += 2;
                    }
                }
            }
            else
            {
                for (l = start_byte; l < size; l++)
                {
                    *pDst[id]++ = *pSrc++;
                    if ((l & 0xf) == 0xF)
                    {
                        *pDst[id]++ = 0xFF;
                        *pDst[id]++ = 0xFF;
                        total += 2;
                    }
                }
            }
            max_size -= size;
            total += size;
            start_col = 0;
            start_byte = 0;
        }
    }

    for (i = start_bank; i < start_bank + max_bank; i++)
    {
        int id = i & MAX_BANK_MASK;

        dstSize[id] = pDst[id] - p[id];
        pDst[id] = p[id];
        printf("bank[%d] size = 0x%08x\n", id, dstSize[id]);
    }
    for (i = start_bank; i < start_bank + max_bank; i++)
    {
        int id = i & MAX_BANK_MASK;
        fid = 0;
        for (j = 0; j < dstSize[id];)
        {
            if (fid == (MAX_OUTFILE_NUM - 1))
            {
                fprintf(fp[id][fid], "%02x\n", *pDst[id]++);
                j++;
            }
            else
            {
                fprintf(fp[id][fid], "%02x%02x\n", pDst[id][1], pDst[id][0]);
                pDst[id] += 2;
                j += 2;
            }
            fid = (fid + 1) % MAX_OUTFILE_NUM;
        }
    }

    ret = total;

END:
    for (i = 0; i < MAX_OUTFILE_NUM; i++)
        for (j = 0; j < MAX_BANK_NUM; j++)
            if (fp[j][i])
            {
                fclose(fp[j][i]);
                fp[j][i] = NULL;
            }

    for (i = 0; i < MAX_BANK_NUM; i++)
        if (p[i])
        {
            free(p[i]);
            p[i] = NULL;
        }

    return ret;
}
