"""Run actual ARMCC-compiled frame helpers and calibration loader in Unicorn.

Only board I/O and the owning control objects are mocked. Both VTM layouts and
the four frame helper bodies come from the current sources; no test copies are
maintained. This does not simulate STM32 peripherals, RTOS scheduling or motors.
Dependencies are isolated under AI_Workspace/cache/dependencies/packages.
"""
import json
import argparse
import shutil
import re
import subprocess
import sys
from pathlib import Path

from audit import read_source, without_comments
from snapshot import PROJECTS
from validate_invariants import OUT, RESULTS, functions, preprocess
from project_paths import DEPENDENCIES, ARM_BIN

sys.path.insert(0, str(DEPENDENCIES))
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP

ARM = ARM_BIN


def layout(path, name):
    text = without_comments(read_source(path))
    start = text.index("#pragma pack(1)")
    end = text.index("#pragma pack()", start) + len("#pragma pack()")
    return text[start:end].replace("VTM_Data_t", name)


def generate():
    chassis, c_project = PROJECTS["chassis"]
    gimbal, g_project = PROJECTS["gimbal"]
    bodies = {}
    for board, root, project, rel, name in [
        ("chassis", chassis, c_project, "bsp/boards/bsp_usart.c", "gimbal_to_chassis"),
        ("chassis", chassis, c_project, "application/message_task.c", "send_to_gimbal"),
        ("gimbal", gimbal, g_project, "User/APP/message_task.c", "send_data_to_chassis"),
        ("gimbal", gimbal, g_project, "User/Bsp/bsp_usart.c", "chassis_to_gimbal"),
    ]:
        bodies[name] = functions(preprocess(root, project, rel, board))[name]
    bodies["gimbal_to_chassis"] = re.sub(r"\bvtm_data\b", "f4_guard.value", bodies["gimbal_to_chassis"])
    bodies["send_data_to_chassis"] = re.sub(r"\bvtm_data\b", "h7_guard.value", bodies["send_data_to_chassis"])
    cali = re.sub(r"^\s*#include[^\n]*\n", "", read_source(chassis / "application/calibrate_task.c"), flags=re.M)
    source = r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "calibrate_task.h"
#include "bsp_flash.h"
typedef union { float fp32; uint8_t bytes[4]; } fp32_to_bytes;
typedef uint8_t gimbal_behaviour_e;
typedef uint8_t chassis_behaviour_e;
'''
    source += layout(chassis / "application/video_transmission_module.h", "F4_VTM_Data_t")
    source += "\n" + layout(gimbal / "User/APP/video_transmission_module.h", "H7_VTM_Data_t")
    source += r'''
typedef char f4_vtm_size[(sizeof(F4_VTM_Data_t) == 21) ? 1 : -1];
typedef char h7_vtm_size[(sizeof(H7_VTM_Data_t) == 21) ? 1 : -1];
typedef char f4_crc_offset[(offsetof(F4_VTM_Data_t, crc) == 19) ? 1 : -1];
typedef char h7_crc_offset[(offsetof(H7_VTM_Data_t, crc) == 19) ? 1 : -1];
struct { uint8_t before[8]; F4_VTM_Data_t value; uint8_t after[8]; } f4_guard;
struct { uint8_t before[8]; H7_VTM_Data_t value; uint8_t after[8]; } h7_guard;
typedef struct { uint8_t shoot_mode; } shoot_control_t;
shoot_control_t shoot_control;
gimbal_behaviour_e gimbal_behaviour;
chassis_behaviour_e chassis_behaviour_mode;
typedef struct { uint8_t robot_id; } robot_status_t;
robot_status_t robot_status;
struct { struct { float radian_of_ecd; } gimbal_yaw_motor; } gimbal_control;
struct {
  float gimbal_radian_of_ecd;
  uint8_t gimbal_behaviour, gimbal_shoot_mode;
  float chassis_yaw, chassis_pitch, chassis_roll, vx, vy, wz;
} chassis_move;
struct {
  float chassis_yaw, chassis_pitch, chassis_roll;
  float chassis_vx, chassis_vy, chassis_wz;
  uint8_t chassis_mode, robot_id;
} chassis_data;
float GIMBAL_INS_yaw, GIMBAL_INS_pitch, GIMBAL_INS_roll;
float ins_angles[3];
float *get_INS_angle_point(void) { return ins_angles; }
float loop_float_constrain(float input, float min_value, float max_value);
uint8_t frame46[46], frame28[28];
uint16_t frame46_size, frame28_size;
void USART1_Transmit_DMA(uint8_t *bytes, uint16_t size) {
  frame46_size = size; if (size <= 46) memcpy(frame46, bytes, size);
}
void usart1_tx_dma_enable(uint8_t *bytes, uint16_t size) {
  frame28_size = size; if (size <= 28) memcpy(frame28, bytes, size);
}
uint8_t flash_image[120];
float loaded_scale[3], loaded_offset[3];
uint32_t gyro_load_count;
void flash_read(uint32_t address, uint32_t *dest, uint32_t words) {
  memcpy(dest, flash_image + address - ADDR_FLASH_SECTOR_9, words * 4U);
}
void INS_set_cali_gyro(float scale[3], float offset[3]) {
  memcpy(loaded_scale, scale, sizeof(loaded_scale));
  memcpy(loaded_offset, offset, sizeof(loaded_offset));
  ++gyro_load_count;
}
'''
    math_body = functions(read_source(gimbal / "User/Lib/user_lib.c"))["loop_float_constrain"]
    source += "float loop_float_constrain(float Input, float minValue, float maxValue)" + math_body + "\n"
    source += cali + "\n"
    for name, signature in [
        ("gimbal_to_chassis", "void gimbal_to_chassis(uint8_t *gimbal_data)"),
        ("send_data_to_chassis", "void send_data_to_chassis(void)"),
        ("send_to_gimbal", "void send_to_gimbal(void)"),
        ("chassis_to_gimbal", "void chassis_to_gimbal(uint8_t *data_chassis)"),
    ]:
        source += signature + bodies[name] + "\n"
    source += r'''
volatile struct { uint32_t status, checks, failure; } test_result;
__asm void stop_test(void) {
  BKPT 0
  BX lr
}
void check(int ok, uint32_t code) {
  ++test_result.checks;
  if (!ok) { test_result.failure = code; stop_test(); }
}
void test_entry(void) {
  uint8_t *payload = (uint8_t *)&h7_guard.value;
  uint8_t *received = (uint8_t *)&f4_guard.value;
  float latitude = -1.0f;
  head_cali_t head;
  imu_cali_t gyro;
  memset(&h7_guard, 0xCD, sizeof(h7_guard));
  memset(&f4_guard, 0xAB, sizeof(f4_guard));
  for (uint32_t i = 0; i < sizeof(h7_guard.value); ++i) payload[i] = 0x40U + i;
  gimbal_control.gimbal_yaw_motor.radian_of_ecd = 1.25f;
  ins_angles[0] = -2.0f; ins_angles[1] = 0.375f; ins_angles[2] = -0.5f;
  gimbal_behaviour = 5; shoot_control.shoot_mode = 4;
  send_data_to_chassis();
  check(frame46_size == 46 && frame46[0] == 0xFE && frame46[45] == 0xFD, 1);
  check(!memcmp(frame46 + 1, &gimbal_control.gimbal_yaw_motor.radian_of_ecd, 4), 2);
  check(!memcmp(frame46 + 5, ins_angles, 12), 3);
  check(frame46[17] == 5 && frame46[18] == 4, 4);
  check(!memcmp(frame46 + 19, payload + 2, 19), 5);
  for (uint32_t i = 38; i <= 44; ++i) check(frame46[i] == 0, 6);
  /* Nonzero reserved bytes must not write beyond the receiver structure. */
  memset(frame46 + 38, 0xEF, 7);
  gimbal_to_chassis(frame46);
  check(!memcmp(received + 2, payload + 2, 19), 7);
  check(received[0] == 0xAB && received[1] == 0xAB, 8);
  for (uint32_t i = 0; i < 8; ++i) {
    check(f4_guard.before[i] == 0xAB && f4_guard.after[i] == 0xAB, 9);
    check(h7_guard.before[i] == 0xCD && h7_guard.after[i] == 0xCD, 10);
  }
  check(chassis_move.gimbal_radian_of_ecd == -1.25f, 11);
  check(GIMBAL_INS_yaw == -2.0f && GIMBAL_INS_pitch == 0.375f && GIMBAL_INS_roll == -0.5f, 12);
  check(chassis_move.gimbal_behaviour == 5 && chassis_move.gimbal_shoot_mode == 4, 13);
  chassis_move.chassis_yaw = 0.125f; chassis_move.chassis_pitch = -1.0f;
  chassis_move.chassis_roll = 0.5f;
  chassis_move.vx = 1.0f; chassis_move.vy = -0.25f; chassis_move.wz = 0.75f;
  chassis_behaviour_mode = 2; robot_status.robot_id = 103;
  send_to_gimbal();
  check(frame28_size == 28 && frame28[0] == 's' && frame28[27] == 'e', 14);
  check(frame28[13] == 2 && frame28[14] == 103, 15);
  chassis_to_gimbal(frame28);
  check(chassis_data.chassis_yaw == 0.125f && chassis_data.chassis_pitch == -1.0f && chassis_data.chassis_roll == 0.5f, 16);
  check(chassis_data.chassis_vx == 1.0f && chassis_data.chassis_vy == -0.25f && chassis_data.chassis_wz == 0.75f, 17);
  check(chassis_data.chassis_mode == 2 && chassis_data.robot_id == 103, 18);
  /* Legacy sector: HEAD at 0, flags at 11/35/63/91/119, GYR at 36. */
  memset(flash_image, 0xFF, sizeof(flash_image));
  memset(&head, 0, sizeof(head));
  head.self_id = 3; head.firmware_version = 17; head.temperature = 41; head.latitude = 31.25f;
  gyro.offset[0] = 0.1f; gyro.offset[1] = -0.2f; gyro.offset[2] = 0.3f;
  gyro.scale[0] = 1.0f; gyro.scale[1] = 1.1f; gyro.scale[2] = 0.9f;
  memcpy(flash_image, &head, 8); flash_image[11] = 0x55;
  memcpy(flash_image + 36, &gyro, 24); flash_image[63] = 0x55;
  cali_param_init(); get_flash_latitude(&latitude);
  check(get_control_temperature() == 41 && latitude == 31.25f, 19);
  check(gyro_load_count == 1 && !memcmp(loaded_offset, gyro.offset, 12) && !memcmp(loaded_scale, gyro.scale, 12), 20);
  flash_image[11] = 0xFF; flash_image[63] = 0;
  cali_param_init(); get_flash_latitude(&latitude); get_flash_latitude(0);
  check(get_control_temperature() == 41 && latitude == 22.0f, 21);
  check(gyro_load_count == 1, 22);
  test_result.status = 0x600D;
  stop_test();
}
'''
    return source


def run():
    OUT.mkdir(parents=True, exist_ok=True)
    RESULTS.mkdir(parents=True, exist_ok=True)
    source = OUT / "runtime-check.c"
    source.write_text(generate(), encoding="utf-8")
    chassis = PROJECTS["chassis"][0]
    obj = OUT / "runtime-check.o"
    image = OUT / "runtime-check.axf"
    commands = [
        [str(ARM / "armcc.exe"), "--cpu", "Cortex-M4", "--thumb", "--c99", "-O0", "-c",
         "-I" + str(chassis / "application"), "-I" + str(chassis / "bsp/boards"),
         "-I" + str(chassis / "components/support"), str(source), "-o", str(obj)],
        [str(ARM / "armlink.exe"), "--ro_base", "0x10000", "--rw_base", "0x20000000",
         "--entry", "test_entry", str(obj), "-o", str(image)],
    ]
    for command in commands:
        result = subprocess.run(command, capture_output=True, timeout=30)
        if result.returncode:
            raise RuntimeError(result.stdout.decode("gb18030", errors="replace") + result.stderr.decode("gb18030", errors="replace"))
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0, 0x100000)
    uc.mem_map(0x20000000, 0x100000)
    with image.open("rb") as stream:
        elf = ELFFile(stream)
        for segment in elf.iter_segments():
            if segment["p_type"] == "PT_LOAD":
                uc.mem_write(segment["p_vaddr"], segment.data())
        # ARMCC places RW initializers in the RO load region. Apply the same
        # execution-address initialization normally performed by __main.
        for section in elf.iter_sections():
            if section["sh_flags"] & 2 and section["sh_type"] == "SHT_PROGBITS":
                uc.mem_write(section["sh_addr"], section.data())
        symbols = {s.name: s["st_value"] for s in elf.get_section_by_name(".symtab").iter_symbols()}
        entry = elf["e_entry"]
    uc.reg_write(UC_ARM_REG_SP, 0x200FF000)
    def stop_on_breakpoint(machine, address, size, _):
        if machine.mem_read(address, 2) == b"\x00\xbe":
            machine.emu_stop()
    uc.hook_add(UC_HOOK_CODE, stop_on_breakpoint)
    uc.emu_start(entry | 1, 0, timeout=10000000, count=2000000)
    raw = bytes(uc.mem_read(symbols["test_result"], 12))
    values = [int.from_bytes(raw[i:i+4], "little") for i in range(0, 12, 4)]
    result = {"status": hex(values[0]), "checks": values[1], "failure": values[2],
              "compiler": "ARMCC 5.06", "emulator": "Unicorn Cortex-M/Thumb",
              "passed": values[0] == 0x600D and values[2] == 0}
    (RESULTS / "runtime-results.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    for evidence in (source, image):
        shutil.copy2(evidence, RESULTS / evidence.name)
    print(json.dumps(result, indent=2))
    if not result["passed"]:
        print("frame46:", bytes(uc.mem_read(symbols["frame46"], 46)).hex())
        print("h7_guard:", bytes(uc.mem_read(symbols["h7_guard"], 37)).hex())
        for name in symbols:
            if "vtm_data_ptr" in name:
                print(name, hex(symbols[name]), bytes(uc.mem_read(symbols[name], 4)).hex())
        raise RuntimeError("Runtime frame/calibration check failed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--cache", type=Path, default=OUT)
    parser.add_argument("--results", type=Path, default=RESULTS)
    args = parser.parse_args()
    OUT, RESULTS = args.cache.resolve(), args.results.resolve()
    run()
