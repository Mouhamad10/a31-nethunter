#!/usr/bin/env python3
# Add support for the vold "time_offset=" mount option to the Samsung sdfat driver.
# Modern Android vold passes time_offset=<minutes>; this old sdfat rejects unknown
# options with -EINVAL, so the external SD gets marked "unmountable". We accept and
# ignore it (sdfat keeps local-time stamps, which is fine).
import sys, io

path = sys.argv[1] if len(sys.argv) > 1 else "fs/sdfat/sdfat.c"
src = io.open(path, "r", encoding="utf-8", errors="surrogateescape").read()

edits = [
    # 1) enum: add Opt_time_offset at the end of the option enum (after Opt_adj_req)
    ("\tOpt_adj_req,\n};",
     "\tOpt_adj_req,\n\tOpt_time_offset,\n};"),
    # 2) token table: register the pattern before the terminating Opt_err entry
    ('\t{Opt_adj_req, "adj_req"},\n\t{Opt_err, NULL}\n};',
     '\t{Opt_adj_req, "adj_req"},\n\t{Opt_time_offset, "time_offset=%d"},\n\t{Opt_err, NULL}\n};'),
    # 3) switch: accept and ignore time_offset (parse the int, do nothing)
    ("\t\tcase Opt_tz_utc:\n\t\t\topts->tz_utc = 1;\n\t\t\tbreak;",
     "\t\tcase Opt_tz_utc:\n\t\t\topts->tz_utc = 1;\n\t\t\tbreak;\n"
     "\t\tcase Opt_time_offset:\n"
     "\t\t\t/* vold passes time_offset (minutes); accept and ignore */\n"
     "\t\t\tif (match_int(&args[0], &option))\n"
     "\t\t\t\treturn 0;\n"
     "\t\t\tbreak;"),
]

for i, (old, new) in enumerate(edits, 1):
    n = src.count(old)
    if n != 1:
        sys.stderr.write("PATCH FAIL: edit %d matched %d times (expected 1)\n" % (i, n))
        sys.exit(2)
    src = src.replace(old, new)

io.open(path, "w", encoding="utf-8", errors="surrogateescape").write(src)
# verify
chk = io.open(path, "r", encoding="utf-8", errors="surrogateescape").read()
ok = ('Opt_time_offset,' in chk) and ('time_offset=%d' in chk) and ('case Opt_time_offset:' in chk)
print("SDFAT time_offset patch applied OK" if ok else "VERIFY FAILED")
sys.exit(0 if ok else 3)
