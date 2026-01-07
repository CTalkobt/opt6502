#!/usr/bin/env python3
"""
Semantic Equivalence Test Runner for opt6502

Uses py65 emulator to verify that optimized code produces identical
behavior to original code.

Requirements:
    pip3 install py65

Usage:
    python3 run_semantic_tests.py [test_name]

    If test_name is provided, runs only that test.
    Otherwise, runs all tests in input/ directory.
"""

import sys
import subprocess
from pathlib import Path
try:
    from py65.devices.mpu6502 import MPU
except ImportError:
    print("Error: py65 not found. Install with: pip3 install py65")
    sys.exit(1)

try:
    import configparser
except ImportError:
    print("Error: configparser not found (should be in Python stdlib)")
    sys.exit(1)


def assemble_file(asm_file, output_bin, cpu='6502'):
    """
    Assemble .asm to binary using xa or ca65

    Tries multiple assemblers in order of preference:
    1. xa65 (simple, widely available)
    2. ca65/ld65 (cc65 toolchain)
    """
    asm_file = Path(asm_file)
    output_bin = Path(output_bin)

    # Try xa first
    result = subprocess.run(
        ['xa', '-o', str(output_bin), str(asm_file)],
        capture_output=True,
        text=True
    )

    if result.returncode == 0:
        return True

    # If xa failed, try ca65
    obj_file = output_bin.with_suffix('.o')
    result = subprocess.run(
        ['ca65', '-t', 'none', '-o', str(obj_file), str(asm_file)],
        capture_output=True,
        text=True
    )

    if result.returncode == 0:
        result = subprocess.run(
            ['ld65', '-t', 'none', '-o', str(output_bin), str(obj_file)],
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            return True

    print(f"Assembly failed for {asm_file}")
    print(f"Error: {result.stderr}")
    return False


def load_state_file(state_file):
    """Parse initial or expected state from .txt file"""
    state_file = Path(state_file)
    if not state_file.exists():
        return None

    config = configparser.ConfigParser()
    config.read(state_file)

    state = {
        'registers': {},
        'flags': {},
        'memory': {}
    }

    if 'registers' in config:
        for reg, val in config['registers'].items():
            state['registers'][reg.upper()] = int(val, 0)  # 0 = auto-detect base

    if 'flags' in config:
        for flag, val in config['flags'].items():
            state['flags'][flag.upper()] = int(val, 0)

    if 'memory' in config:
        for addr, val in config['memory'].items():
            state['memory'][int(addr, 0)] = int(val, 0)

    return state


def init_mpu(mpu, state):
    """Initialize MPU with given state"""
    if not state:
        return

    # Set registers
    if 'A' in state['registers']:
        mpu.a = state['registers']['A'] & 0xFF
    if 'X' in state['registers']:
        mpu.x = state['registers']['X'] & 0xFF
    if 'Y' in state['registers']:
        mpu.y = state['registers']['Y'] & 0xFF

    # Set flags
    if 'C' in state['flags']:
        if state['flags']['C']:
            mpu.p |= 0x01
        else:
            mpu.p &= ~0x01
    if 'N' in state['flags']:
        if state['flags']['N']:
            mpu.p |= 0x80
        else:
            mpu.p &= ~0x80
    if 'Z' in state['flags']:
        if state['flags']['Z']:
            mpu.p |= 0x02
        else:
            mpu.p &= ~0x02
    if 'V' in state['flags']:
        if state['flags']['V']:
            mpu.p |= 0x40
        else:
            mpu.p &= ~0x40

    # Set memory
    for addr, val in state['memory'].items():
        mpu.memory[addr] = val & 0xFF


def get_mpu_state(mpu, memory_addrs=None):
    """Extract current MPU state"""
    state = {
        'A': mpu.a,
        'X': mpu.x,
        'Y': mpu.y,
        'C': 1 if (mpu.p & 0x01) else 0,
        'N': 1 if (mpu.p & 0x80) else 0,
        'Z': 1 if (mpu.p & 0x02) else 0,
        'V': 1 if (mpu.p & 0x40) else 0,
        'memory': {}
    }

    # Include memory if addresses specified
    if memory_addrs:
        for addr in memory_addrs:
            state['memory'][addr] = mpu.memory[addr]

    return state


def compare_states(state1, state2, test_name, memory_addrs=None):
    """Compare two MPU states and report differences"""
    differences = []

    # Compare registers
    for key in ['A', 'X', 'Y']:
        if state1[key] != state2[key]:
            differences.append(
                f"  {key}: original=0x{state1[key]:02X}, "
                f"optimized=0x{state2[key]:02X}"
            )

    # Compare flags
    for key in ['C', 'N', 'Z', 'V']:
        if state1[key] != state2[key]:
            differences.append(
                f"  Flag {key}: original={state1[key]}, "
                f"optimized={state2[key]}"
            )

    # Compare memory if addresses specified
    if memory_addrs:
        for addr in memory_addrs:
            val1 = state1['memory'].get(addr, 0)
            val2 = state2['memory'].get(addr, 0)
            if val1 != val2:
                differences.append(
                    f"  Memory[0x{addr:04X}]: original=0x{val1:02X}, "
                    f"optimized=0x{val2:02X}"
                )

    if differences:
        print(f"✗ {test_name} FAILED - State mismatch:")
        for diff in differences:
            print(diff)
        return False
    else:
        print(f"✓ {test_name} PASSED")
        return True


def run_code_on_mpu(mpu, binary_file, load_addr=0x1000, max_cycles=10000):
    """Load and execute binary on MPU"""
    # Load binary into memory
    with open(binary_file, 'rb') as f:
        code = f.read()
        for i, byte in enumerate(code):
            mpu.memory[load_addr + i] = byte

    # Set PC to start of code
    mpu.pc = load_addr

    # Execute until RTS or max cycles
    cycle_count = 0
    for _ in range(max_cycles):
        opcode = mpu.memory[mpu.pc]

        # Check for RTS (0x60)
        if opcode == 0x60:
            # Execute the RTS
            mpu.step()
            break

        mpu.step()
        cycle_count += 1
    else:
        print(f"Warning: Code exceeded {max_cycles} cycles without RTS")
        return None

    return cycle_count


def run_semantic_test(test_name, test_dir, cpu='6502'):
    """Run semantic equivalence test for a single test case"""
    input_asm = test_dir / 'input' / f'{test_name}.asm'
    init_state_file = test_dir / 'state' / f'{test_name}_init.txt'
    expect_state_file = test_dir / 'state' / f'{test_name}_expect.txt'

    if not input_asm.exists():
        print(f"✗ {test_name} - Input file not found: {input_asm}")
        return False

    # Create output directory
    output_dir = test_dir / 'output'
    output_dir.mkdir(exist_ok=True)

    # Assemble original
    original_bin = output_dir / f'{test_name}_original.bin'
    if not assemble_file(input_asm, original_bin, cpu):
        print(f"✗ {test_name} - Failed to assemble original")
        return False

    # Optimize
    optimized_asm = output_dir / f'{test_name}_optimized.asm'
    opt_cmd = ['../../opt6502', '-speed']
    if cpu != '6502':
        opt_cmd.extend(['-cpu', cpu])
    opt_cmd.extend([str(input_asm), str(optimized_asm)])

    opt_result = subprocess.run(
        opt_cmd,
        capture_output=True,
        text=True,
        cwd=test_dir
    )

    if opt_result.returncode != 0:
        print(f"✗ {test_name} - Optimization failed:")
        print(opt_result.stderr)
        return False

    # Assemble optimized
    optimized_bin = output_dir / f'{test_name}_optimized.bin'
    if not assemble_file(optimized_asm, optimized_bin, cpu):
        print(f"✗ {test_name} - Failed to assemble optimized")
        return False

    # Load initial state
    init_state = load_state_file(init_state_file)

    # Load expected state (if provided)
    expect_state = load_state_file(expect_state_file)

    # Determine which memory addresses to track
    memory_addrs = set()
    if init_state and 'memory' in init_state:
        memory_addrs.update(init_state['memory'].keys())
    if expect_state and 'memory' in expect_state:
        memory_addrs.update(expect_state['memory'].keys())

    # Run original code
    mpu_original = MPU()
    init_mpu(mpu_original, init_state)
    cycles_original = run_code_on_mpu(mpu_original, original_bin)

    if cycles_original is None:
        print(f"✗ {test_name} - Original code did not complete")
        return False

    state_original = get_mpu_state(mpu_original, memory_addrs)

    # Run optimized code
    mpu_optimized = MPU()
    init_mpu(mpu_optimized, init_state)
    cycles_optimized = run_code_on_mpu(mpu_optimized, optimized_bin)

    if cycles_optimized is None:
        print(f"✗ {test_name} - Optimized code did not complete")
        return False

    state_optimized = get_mpu_state(mpu_optimized, memory_addrs)

    # Compare states
    result = compare_states(state_original, state_optimized, test_name, memory_addrs)

    # Report cycle difference
    if result:
        cycle_diff = cycles_original - cycles_optimized
        if cycle_diff > 0:
            print(f"  Performance: {cycles_original} → {cycles_optimized} cycles "
                  f"({cycle_diff} cycles saved, {cycle_diff/cycles_original*100:.1f}% improvement)")
        elif cycle_diff < 0:
            print(f"  Performance: {cycles_original} → {cycles_optimized} cycles "
                  f"({-cycle_diff} cycles added)")
        else:
            print(f"  Performance: {cycles_original} cycles (no change)")

    return result


def main():
    test_dir = Path(__file__).parent.resolve()

    # Check if opt6502 exists
    opt6502_path = test_dir.parent.parent / 'opt6502'
    if not opt6502_path.exists():
        print(f"Error: opt6502 not found at {opt6502_path}")
        print("Please compile with: make")
        sys.exit(1)

    # Find all test cases
    input_dir = test_dir / 'input'
    if not input_dir.exists():
        print(f"Error: Input directory not found: {input_dir}")
        sys.exit(1)

    # Check if specific test requested
    if len(sys.argv) > 1:
        test_cases = [sys.argv[1]]
    else:
        test_cases = sorted([f.stem for f in input_dir.glob('*.asm')])

    if not test_cases:
        print("No test cases found in input/")
        sys.exit(0)

    print("="*70)
    print("Semantic Equivalence Tests")
    print("="*70)
    print()

    passed = 0
    failed = 0

    for test_case in test_cases:
        if run_semantic_test(test_case, test_dir):
            passed += 1
        else:
            failed += 1
        print()

    print("="*70)
    print(f"Results: {passed} passed, {failed} failed")
    print("="*70)

    sys.exit(0 if failed == 0 else 1)


if __name__ == '__main__':
    main()
