#!/usr/bin/python

from openpyxl import load_workbook
import sys
import math

omit_fields = ["Reserved", "des_rsvd", "rsvd", "field", "note"]
header_names = ["field", "low", "length", "default", "descriptions"]
headerMap = {}
max_bits = 32


def isValidRow(row):
    value = row[headerMap["field"]].value
    if value is None:
        return False
    valid = True
    value = value.strip().lower()
    for omit in omit_fields:
        if value.startswith(omit.lower()):
            valid = False
            break
    return valid


def parseRowHeader(row):
    for i, cell in enumerate(row):
        value = cell.value
        if not value:
            continue
        value = value.strip().lower()
        for h in header_names:
            if value.startswith(h):
                headerMap[h] = i
                break
    return headerMap


def convert_digit(v):
    v = v.replace("_", "")
    v = v.strip()
    idx = v.find("'")
    if idx < 0:
        return int(v)
    v = v[idx+1:].strip()
    f = v[0]
    v = v[1:]
    b = 10
    if f == 'b':
        b = 2
    elif f == 'o':
        b = 8
    elif f == 'h':
        b = 16
    return int(v, base=b)


def parseRegXlsx(filename, sheet_index):
    content = []
    wb = load_workbook(filename, data_only=True)
    sheet = wb.worksheets[sheet_index]
    rows = tuple(sheet.rows)
    headerMap = parseRowHeader(rows[0])
    content = []
    for row_gen in rows[1:]:
        row = tuple(row_gen)
        item = []
        if not isValidRow(row):
            continue
        for f in header_names:
            cell = row[headerMap[f]]
            if cell.value is None:
                item = None
                break
            try:
                v = str(cell.value)
            except:
                #print("warning:", v, "is not a string")
                continue

            item.append(cell.value)
        if item:
            item[0] = item[0].strip().lower()
            if item == "":
                continue
            item[1] = int(item[1])
            item[2] = int(item[2])
            item[3] = int(item[3], base=16)  # convert_digit(item[3])
            content.append(item)
    return content


def generateDefFile(content, prefix, start_field):
    if len(content) == 0:
        print("No register is found!")
        return
    prefix = prefix.lower()
    reg_ids = []
    reg_packs = []
    reg_default = []
    reg_default_names = []
    des_start = 0
    des_is_start = False
    des_struct = []
    word_count = 0
    max_name_len = 0
    offset_name = "{}_DES_OFFSET".format(prefix.upper())
    for idx, item in enumerate(content):
        n = item[0].strip()
        if max_name_len < len(n):
            max_name_len = len(n)
    max_name_len += 10
    for idx, item in enumerate(content):
        n, w, l, v, comment = item  # name where len
        comment = comment.strip().split('\n')
        comment = "\n// ".join(comment)
        n = n.strip()
        if n == start_field:
            des_start = w
            des_is_start = True
        num = (int)(math.floor((l+max_bits-1)/max_bits))
        for i in range(num):
            if num == 1:
                id_name = '{}_ID_{}'.format(prefix.upper(), n.upper())
                pack_name = '{}_PACK_{}'.format(prefix.upper(), n.upper())
            else:
                id_name = '{}_ID_{}_{}'.format(prefix.upper(), n.upper(), i)
                pack_name = '{}_PACK_{}_{}'.format(
                    prefix.upper(), n.upper(), i)
            ll = max_bits
            vv = v & ((1 << max_bits)-1)
            v = v >> max_bits
            if l-i*max_bits < max_bits:
                ll = (l-i*max_bits)
            padding = " "*(max_name_len-len(id_name))
            if not comment.startswith("reserved"):
                reg_ids.append("// " + comment);
            if des_is_start and des_start > 0:
                line = '#define {} {}{{ {} - {}, {} }}'.format(
                    id_name, padding, w+i*max_bits, offset_name, ll)
            else:
                line = '#define {} {}{{ {}, {} }}'.format(
                    id_name, padding, w+i*max_bits, ll)
            reg_ids.append(line)
            padding = " "*(max_name_len-len(pack_name))
            line = '#define {}(val) {}{{ {}, (val) }}'.format(
                pack_name, padding, id_name)
            reg_packs.append(line)
            if des_is_start:
                des_struct.append(n)
            if vv != 0:
                default_name = "{}_DEFAULT".format(pack_name)
                reg_default_names.append(default_name)
                line = '#define {} {}{}({})'.format(
                    default_name, padding, pack_name, hex(vv))
                reg_default.append(line)
    last_item = content[-1]
    n, w, l, v, _ = last_item
    word_count = int(math.floor((w + l - des_start + max_bits - 1)/max_bits))
    filename = prefix+"_reg_def"
    with open("./include/" + filename +".h", "w") as f:
        f.write("/*\n")
        f.write(" * This file was generated by gen_bd_regs_from_TPU_Reg_xlsx.py.\n")
        f.write(" * Please DO NOT edit it.\n")
        f.write(" */\n\n")
        f.write("#ifndef __{}_H__\n".format(filename.upper()))
        f.write("#define __{}_H__\n".format(filename.upper()))
        f.write('\n')
        f.write('#define {}_REG_COUNT ({})\n'.format(
            prefix.upper(), word_count))
        if des_start > 0:
            f.write('#define {} ({})\n'.format(offset_name, des_start))

        f.write('\n//{} reg id defines\n'.format(prefix))
        f.write('\n'.join(reg_ids))

        f.write('\n\n//{} pack defines\n'.format(prefix))
        f.write('\n'.join(reg_packs))

        f.write('\n\n//{} default values\n'.format(prefix))
        f.write('\n'.join(reg_default))
        f.write('\n')

        f.write('\n//default list')
        f.write("\n#define {}_PACK_DEFAULTS {{\\\n".format(prefix.upper()))
        f.write("  ")
        f.write(',\\\n  '.join(reg_default_names))
        f.write("\\\n}\n\n")
        f.write("#endif //__{}_H__".format(filename.upper()))
    print(filename+".h is generated!")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 ", sys.argv[0], " BM1686_TPU_TIU_Regx.x.xlsx")
        exit(1)

    content = parseRegXlsx(sys.argv[1], sheet_index=4)
    print(content)
    generateDefFile(content, prefix="bd", start_field="des_cmd_en")
