"""Compile and run real soft-turn/control sources with ARMCC and board-I/O fixtures."""
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from project_paths import ROOT, ARM_BIN, CACHE, ARTIFACTS, DEPENDENCIES, PROJECTS
from soft_turn_log import decode_packets

sys.path.insert(0, str(DEPENDENCIES))
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_SP


def run(cache, results):
    cache.mkdir(parents=True, exist_ok=True)
    results.mkdir(parents=True, exist_ok=True)
    project = ROOT / PROJECTS['chassis'][1]
    controls = ET.parse(project).find('./Targets/Target/TargetOption/TargetArmAds/Cads/VariousControls')
    include_dirs = [(project.parent / p.replace('\\', '/')).resolve()
                    for p in controls.findtext('IncludePath').split(';') if p]
    sources = [Path(__file__).with_name('test_soft_turn.c'),
               ROOT / 'application/chassis_soft_turn.c', ROOT / 'application/chassis_soft_turn_log.c',
               ROOT / 'application/referee_usart_task.c',
               ROOT / 'application/referee.c', ROOT / 'components/support/fifo.c',
               ROOT / 'components/support/CRC8_CRC16.c',
               ROOT / 'components/algorithm/user_lib.c', ROOT / 'components/controller/pid.c']
    commands, objects = [], []
    for source in sources:
        obj = cache / (source.stem + '.o')
        command = [str(ARM_BIN / 'armcc.exe'), '--cpu', 'Cortex-M4', '--fpu', 'SoftVFP',
                   '--thumb', '--c99', '--split_sections', '-O1', '-c', '-DSTM32F407xx', '-DUSE_HAL_DRIVER',
                   '-DARM_MATH_CM4', '-DCHASSIS_SOFT_TURN_TELEMETRY=1']
        command += ['-I' + str(p) for p in include_dirs]
        command += [str(source), '-o', str(obj)]
        commands.append(command)
        objects.append(obj)
    image = cache / 'soft-turn-check.axf'
    commands.append([str(ARM_BIN / 'armlink.exe'), '--ro_base', '0x10000', '--rw_base', '0x20000000',
                     '--entry', 'test_entry', *map(str, objects), '-o', str(image)])
    log = []
    for command in commands:
        completed = subprocess.run(command, capture_output=True, timeout=45)
        output = (completed.stdout + completed.stderr).decode('gb18030', errors='replace')
        log.append(output)
        if completed.returncode:
            (results / 'compiler.log').write_text('\n'.join(log), encoding='utf-8')
            raise RuntimeError(output)
    (results / 'compiler.log').write_text('\n'.join(log), encoding='utf-8')
    uc = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    uc.mem_map(0, 0x100000)
    uc.mem_map(0x20000000, 0x100000)
    uc.mem_map(0x40011000, 0x1000)  # USART6 registers; no peripheral behavior is simulated.
    with image.open('rb') as stream:
        elf = ELFFile(stream)
        for segment in elf.iter_segments():
            if segment['p_type'] == 'PT_LOAD': uc.mem_write(segment['p_vaddr'], segment.data())
        for section in elf.iter_sections():
            if section['sh_flags'] & 2 and section['sh_type'] == 'SHT_PROGBITS':
                uc.mem_write(section['sh_addr'], section.data())
        symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}
        entry = elf['e_entry']
    uc.reg_write(UC_ARM_REG_SP, 0x200FF000)
    stop_address = symbols['stop_test'] & ~1
    uc.hook_add(UC_HOOK_CODE, lambda machine, address, size, data: machine.emu_stop(),
                begin=stop_address, end=stop_address)
    uc.emu_start(entry | 1, 0, timeout=120000000, count=200000000)
    status, checks, failure, scenario, step, frames, seconds, minimum = struct.unpack(
        '<6I2f', bytes(uc.mem_read(symbols['test_result'], 32)))
    evidence = {
        'passed': status == 0x600D and failure == 0, 'status': hex(status), 'checks': checks,
        'failure': failure, 'scenario': scenario, 'step': step, 'telemetry_frames': frames,
        'ideal_90_degree_transition_seconds': seconds, 'minimum_transition_speed_m_s': minimum,
        'compiler': 'ARMCC 5.06, Cortex-M4, SoftVFP', 'emulator': 'Unicorn M-class/Thumb',
        'fixtures': 'CAN/UART/RTOS/INS I/O and CMSIS sin/cos (libm substitutes)',
        'limits': 'No peripherals, physical motor model, actual task timing or vehicle tip-over validation',
        'source_sha256': {str(s.relative_to(ROOT)): hashlib.sha256(s.read_bytes()).hexdigest() for s in sources},
    }
    for relative in ['application/chassis_task.c', 'application/chassis_task.h',
                     'application/chassis_calculate.c', 'application/chassis_calculate.h',
                     'application/chassis_behaviour.c', 'application/chassis_behaviour.h',
                     'application/remote_control.c', 'application/remote_control.h',
                     'application/CAN_receive.c', 'application/CAN_receive.h',
                     'application/chassis_soft_turn.h', 'application/chassis_soft_turn_log.h',
                     'application/robot_param.h', 'MDK-ARM/standard_tpye_c.uvprojx']:
        evidence['source_sha256'][relative] = hashlib.sha256((ROOT / relative).read_bytes()).hexdigest()
    packet = bytes(uc.mem_read(symbols['serial_record'], 146))
    if evidence['passed']:
        corrupt = bytearray(packet)
        corrupt[20] ^= 0x80
        decoded, bad_crc = decode_packets(b'noise' + packet + corrupt + packet + packet[:70])
        if len(decoded) != 2 or bad_crc != 1:
            raise RuntimeError('Telemetry CRC/resynchronization/truncation check failed')
        evidence['telemetry_decoder_checks'] = 3
    (results / 'telemetry-sample.bin').write_bytes(packet)
    (results / 'soft-turn-results.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')
    shutil.copy2(image, results / image.name)
    print(json.dumps(evidence, indent=2))
    if not evidence['passed']: raise RuntimeError('Soft-turn runtime check failed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--cache', type=Path, default=CACHE / 'validation/soft-turn')
    parser.add_argument('--results', type=Path, default=ARTIFACTS / 'soft-turn/validation')
    args = parser.parse_args()
    run(args.cache.resolve(), args.results.resolve())
