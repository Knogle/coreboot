#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Exercise desktop C6 CMOS migration and the retained Cezanne watchdog layout.

Compiles the actual setup C with bounded I/O fixtures. No hardware is accessed.
"""

import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess


HEADERS = {
    "types.h": "#include <stdbool.h>\n#include <stdint.h>\n",
    "amdblocks/acpimmio.h": "void pm_write8(unsigned int,uint8_t);\nuint8_t pm_read8(unsigned int);\nuint32_t pm_read32(unsigned int);\nvoid pm_write32(unsigned int,uint32_t);\n",
    "soc/southbridge.h": "#define PM_ACPI_CONF 0x74\n#define PM_ACPI_BLOCK_PCIE_PME (1U<<24)\n#define PM_ACPI_RTC_WAKE_EN (1U<<29)\n",
    "commonlib/helpers.h": "#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))\n#define BIT(n) (1U<<(n))\n",
    "console/console.h": "#define BIOS_NOTICE 0\n#define BIOS_WARNING 1\n#define BIOS_ERR 2\nint printk(int,const char*,...);\n",
    "cpu/x86/msr.h": "typedef struct {uint32_t lo,hi;} msr_t;\nmsr_t rdmsr(unsigned int);\nvoid wrmsr(unsigned int,msr_t);\n",
    "option.h": "unsigned int get_uint_option(const char*,unsigned int);\nint set_uint_option(const char*,unsigned int);\n",
    "pc80/mc146818rtc.h": "uint8_t cmos_read(unsigned int);\nvoid cmos_write(uint8_t,unsigned int);\nint cmos_error(void);\nint cmos_checksum_valid(int,int,int);\nvoid cmos_set_checksum(int,int,int);\nvoid sanitize_cmos(void);\n",
}

FIXTURE = r"""
#include <assert.h>
#include <string.h>
#include "setup_options.c"
static uint8_t storage[128];
static unsigned int writes;
uint8_t cmos_read(unsigned int n) { assert(n<128); return storage[n]; }
void cmos_write(uint8_t v,unsigned int n) {
    assert(n>=96 && n<=124); storage[n]=v; writes++;
}
int cmos_error(void) { return 0; }
int cmos_checksum_valid(int a,int b,int loc) {
    unsigned int sum=0;
    for(int i=a;i<=b;i++) sum+=storage[i];
    return (sum&65535)==(((unsigned int)storage[loc]<<8)|storage[loc+1]);
}
void cmos_set_checksum(int a,int b,int loc) {
    unsigned int sum=0;
    for(int i=a;i<=b;i++) sum+=storage[i];
    cmos_write(sum>>8,loc); cmos_write(sum,loc+1);
}
void sanitize_cmos(void) { assert(0); }
unsigned int get_uint_option(const char *name,unsigned int fallback) {
    if(!cmos_checksum_valid(96,122,123)) return fallback;
    for(unsigned int i=0;i<ARRAY_SIZE(options);i++)
        if(!strcmp(name,options[i].name)) return storage[options[i].byte];
    return fallback;
}
int set_uint_option(const char *name,unsigned int value) {
    for(unsigned int i=0;i<ARRAY_SIZE(options);i++)
        if(!strcmp(name,options[i].name)) {
            cmos_write(value,options[i].byte); cmos_set_checksum(96,122,123); return 0;
        }
    assert(0); return -1;
}
int printk(int level,const char *fmt,...) { (void)level;(void)fmt;return 0; }
msr_t rdmsr(unsigned int n) { (void)n;assert(0);return(msr_t){0}; }
void wrmsr(unsigned int n,msr_t v) { (void)n;(void)v;assert(0); }
void pm_write8(unsigned int n,uint8_t v) { (void)n;(void)v;assert(0); }
uint8_t pm_read8(unsigned int n) { (void)n;assert(0);return 0; }
uint32_t pm_read32(unsigned int n) { (void)n;assert(0);return 0; }
void pm_write32(unsigned int n,uint32_t v) { (void)n;(void)v;assert(0); }
void fixture_load(const uint8_t *data) {
    memcpy(storage,data,128);memset(values,0,sizeof(values));ready=false;writes=0;
}
unsigned int fixture_read(unsigned int n) { return cmos_read(n); }
unsigned int fixture_writes(void) { return writes; }
"""


def image(schema, desktop):
    data = bytearray((i*37+19)&255 for i in range(128))
    data[96:123] = bytes([3,schema,2,0,0,0,2,0,0,0,0,0,0,0,0,0,
                          0,8,0,0,0,12,0,0,0 if desktop else 2,0,2])
    return checksummed(data)


def checksummed(data):
    data[123:125] = sum(data[96:123]).to_bytes(2,"big")
    return data


def exercise(board, output, desktop):
    output.mkdir(parents=True)
    for name, content in HEADERS.items():
        path = output/"include"/name
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(content)
    settings = dict(B565D4_DESKTOP_SETUP=int(desktop), B565D4_BOARD_SETUP=int(not desktop),
                    B565D4_DESKTOP_NATIVE=int(desktop), STATIC_OPTION_TABLE=0)
    config = "#define CONFIG(n) CONFIG_ ## n\n"
    config += "".join(f"#define CONFIG_{name} {value}\n" for name,value in settings.items())
    (output/"config.h").write_text(config)
    (output/"fixture.c").write_text(FIXTURE)
    library = output/"fixture.so"
    command = ["cc","-std=c11","-O2","-Wall","-Wextra","-Werror","-shared","-fPIC",
               "-include",str(output/"config.h"),"-include","stdint.h",
               "-I"+str(output/"include"),"-I"+str(board/"include"),"-I"+str(board),
               str(output/"fixture.c"),"-o",str(library)]
    process = subprocess.run(command,capture_output=True,timeout=30)
    (output/"compile.stdout").write_bytes(process.stdout)
    (output/"compile.stderr").write_bytes(process.stderr)
    assert process.returncode==0,process.stderr.decode()
    lib = ctypes.CDLL(str(library.resolve()))
    for name in ["fixture_read","fixture_writes","b565d4_setup_core_c6_enabled"]:
        getattr(lib,name).restype=ctypes.c_uint
    cases=0

    def run(before):
        data=(ctypes.c_uint8*128).from_buffer_copy(before)
        lib.fixture_load(data)
        assert bool(lib.b565d4_setup_core_c6_enabled()) == (not desktop)
        lib.b565d4_setup_init()
        after=bytes(lib.fixture_read(i) for i in range(128))
        assert after[:96]==before[:96] and after[125:]==before[125:]
        assert int.from_bytes(after[123:125],"big")==sum(after[96:123])
        return after

    current = 6 if desktop else 5
    # Schema histories describe the fields introduced in each revision.
    introduced = {2:{118:1,119:1},3:{112:1,114:1,115:0,116:0,117:5},
                  4:{111:1},5:{122:0}}
    if desktop:
        introduced[6]={120:1}
    for schema in range(1,current+1):
        before=image(schema,desktop)
        after=run(before)
        expected=bytearray(before)
        expected[97]=current
        for revision,fields in introduced.items():
            if revision>schema:
                for byte,value in fields.items():
                    expected[byte]=value
        checksummed(expected)
        assert after==expected,(desktop,schema)
        assert bool(lib.b565d4_setup_core_c6_enabled()) == (bool(after[120]) if desktop else True)
        cases+=1
        assert run(after)==after and lib.fixture_writes()==0
        cases+=1

    for value in range(2 if desktop else 3):
        before=image(current,desktop)
        before[120]=value
        checksummed(before)
        assert run(before)==before
        assert bool(lib.b565d4_setup_core_c6_enabled()) == (bool(value) if desktop else True)
        cases+=1
    if desktop:
        for value in [2,127,255]:
            before=image(current,True)
            before[120]=value
            checksummed(before)
            expected=bytearray(before);expected[120]=1;checksummed(expected)
            assert run(before)==expected
            cases+=1
        # A corrupt checksum must reset to bounded defaults, including C6 enabled.
        before=image(6,True);before[120]^=1
        after=run(before)
        assert after[97]==6 and after[120]==1 and after[96]==0
        cases+=1
        # Restore defaults clears the request and enables C6 only for the next boot.
        before=image(6,True);before[120]=0;before[121]=1;checksummed(before)
        after=run(before)
        assert after[120]==1 and after[121]==0
        cases+=1
    return dict(desktop=desktop,schema=current,cases=cases,command=command,
                source_sha256=hashlib.sha256((board/"setup_options.c").read_bytes()).hexdigest())


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--board",type=Path,required=True,help="Board ramstage source directory")
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    args.output.mkdir(parents=True)
    results=[exercise(args.board,args.output/name,desktop)
             for name,desktop in [("vermeer",True),("cezanne",False)]]
    report=dict(status="PASS_HOST_ACTUAL_C_C6_SCHEMA6_AND_CEZANNE_SCHEMA5_WATCHDOG",
                cases=sum(row["cases"] for row in results),results=results,
                RTC_and_AMD_bytes_preserved=True,hardware_access=False)
    (args.output/"result.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report))


if __name__=="__main__":
    main()
