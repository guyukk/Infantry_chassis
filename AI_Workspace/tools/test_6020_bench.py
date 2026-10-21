"""Run the bench firmware's pure gates and math with ARMCC code in Unicorn.

This does not emulate CAN, DBUS interrupts, FreeRTOS, or a physical motor.
"""
import json
import subprocess
import sys

from project_paths import ARM_BIN, CACHE, DEPENDENCIES, ROOT

sys.path.insert(0, str(DEPENDENCIES))
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP


def baseline_contract():
    old_task = subprocess.check_output(
        ["git", "show", "HEAD:application/chassis_task.c"], cwd=ROOT
    ).decode("utf-8", errors="replace")
    old_can = subprocess.check_output(
        ["git", "show", "HEAD:application/CAN_receive.c"], cwd=ROOT
    ).decode("utf-8", errors="replace")
    assert "chassis_move_control_loop->chassis_6020[i].angle_set = wheel_angle[i]" in old_task
    assert old_task.index("chassis_control_loop(&chassis_move)") < old_task.index("if (RC_data_is_error())")
    assert "HAL_CAN_AddTxMessage(&CHASSIS_6020_CAN" in old_can
    assert "= HAL_CAN_AddTxMessage(&CHASSIS_6020_CAN" not in old_can
    current_rc = (ROOT / "application/remote_control.c").read_text(encoding="utf-8")
    irq = current_rc.split("void USART3_IRQHandler(void)", 1)[1].split("void sbus_to_rc(", 1)[0]
    assert "RC_frame_is_valid(&rc_ctrl)" in irq
    assert "RC_data_is_error()" not in irq


def run():
    baseline_contract()
    work = CACHE / "validation/6020-bench"
    work.mkdir(parents=True, exist_ok=True)
    source = work / "bench-check.c"
    obj = work / "bench-check.o"
    image = work / "bench-check.axf"
    source.write_text(r'''
#include <stdint.h>
#include "chassis_6020_bench_logic.h"
#include "chassis_6020_bench_time.h"
volatile struct { uint32_t status, checks, failure; } test_result;
__asm void stop_test(void) {
  BKPT 0
  BX lr
}
void check(int ok, uint32_t code) {
  test_result.checks++;
  if (!ok) { test_result.failure = code; stop_test(); }
}
void test_entry(void) {
  float set;
  volatile float nan_value = 0.0f;
  check(bench_6020_healthy(1,1,1,1), 1);
  check(!bench_6020_healthy(0,1,1,1), 2);
  check(!bench_6020_healthy(1,0,1,1), 3);
  check(!bench_6020_healthy(1,1,0,1), 4);
  check(!bench_6020_healthy(1,1,1,0), 5);
  check(!bench_6020_can_arm(1,0,0,25), 6);
  check(!bench_6020_can_arm(0,1,0,25), 7);
  check(bench_6020_can_arm(1,1,25,25), 8);
  check(!bench_6020_can_arm(1,1,26,25), 9);
  check(bench_6020_can_arm(1,1,-25,25), 10);
  check(!bench_6020_can_arm(1,1,-26,25), 11);
  check(bench_6020_time_fresh(100,1,1,100), 12);
  check(!bench_6020_time_fresh(102,1,1,100), 13);
  check(!bench_6020_time_fresh(100,1,0,100), 14);
  check(bench_6020_time_fresh(5,0xfffffff0U,1,25), 15);
  check(!bench_6020_time_fresh(5,0xfffffff0U,1,20), 16);
  set = bench_6020_shortest_set(3.13f,-3.13f,3.14159265f);
  check(set > 3.14f && set < 3.17f, 17);
  set = bench_6020_shortest_set(-3.13f,3.13f,3.14159265f);
  check(set < -3.14f && set > -3.17f, 18);
  check(bench_6020_limit_current(1800.0f,1500) == 1500, 19);
  check(bench_6020_limit_current(-1800.0f,1500) == -1500, 20);
  check(bench_6020_limit_current(249.9f,1500) == 249, 21);
  check(bench_6020_limit_current(nan_value/nan_value,1500) == 0, 22);
  test_result.status = 0x600d;
  stop_test();
}
''', encoding="utf-8")
    commands = [
        [str(ARM_BIN / "armcc.exe"), "--cpu", "Cortex-M4", "--thumb", "--c99", "-O0", "-c",
         "-I" + str(ROOT / "application"), str(source), "-o", str(obj)],
        [str(ARM_BIN / "armlink.exe"), "--ro_base", "0x10000", "--rw_base", "0x20000000",
         "--entry", "test_entry", str(obj), "-o", str(image)],
    ]
    for command in commands:
        completed = subprocess.run(command, capture_output=True, timeout=30)
        if completed.returncode:
            raise RuntimeError((completed.stdout + completed.stderr).decode("gb18030", errors="replace"))
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0, 0x100000)
    uc.mem_map(0x20000000, 0x100000)
    with image.open("rb") as stream:
        elf = ELFFile(stream)
        for segment in elf.iter_segments():
            if segment["p_type"] == "PT_LOAD":
                uc.mem_write(segment["p_vaddr"], segment.data())
        for section in elf.iter_sections():
            if section["sh_flags"] & 2 and section["sh_type"] == "SHT_PROGBITS":
                uc.mem_write(section["sh_addr"], section.data())
        symbols = {s.name: s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()}
        entry = elf["e_entry"]
    uc.reg_write(UC_ARM_REG_SP, 0x200ff000)
    def stop_on_breakpoint(machine, address, size, _):
        if machine.mem_read(address, 2) == b"\x00\xbe":
            machine.emu_stop()
    uc.hook_add(UC_HOOK_CODE, stop_on_breakpoint)
    uc.emu_start(entry | 1, 0, timeout=10000000, count=100000)
    raw = bytes(uc.mem_read(symbols["test_result"], 12))
    status, checks, failure = [int.from_bytes(raw[i:i+4], "little") for i in (0, 4, 8)]
    result = {"baseline_contract": "confirmed", "checks": checks, "failure": failure,
              "passed": status == 0x600d and checks == 22 and failure == 0}
    print(json.dumps(result, ensure_ascii=False))
    if not result["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    run()
