# Unit Test Enhancement Recommendations for opt6502

## Current Testing Weaknesses

The current test suite uses "golden file" comparison (input vs expected output) which validates that optimizations are **applied**, but doesn't verify they don't have **side-effects**. Specifically:

1. ❌ No semantic equivalence testing (does optimized code behave identically?)
2. ❌ No execution validation (are register/memory states preserved?)
3. ❌ No property-based testing (do invariants hold across all optimizations?)
4. ❌ No performance validation (do "speed" optimizations actually improve performance?)
5. ❌ No side-effect detection (are memory writes/flag changes preserved?)

---

## Recommended Testing Framework Structure

```
tests/
├── semantic/           # Emulator-based equivalence tests
│   ├── input/
│   ├── expected/
│   ├── output/
│   └── state/         # Initial/expected states for emulator runs
├── performance/        # Cycle count validation
│   ├── input/
│   ├── expected/
│   ├── output/
│   └── metrics/       # Cycle count comparisons
├── correctness/        # CPU-specific correctness tests
│   ├── 6502/
│   ├── 65c02/
│   ├── 45gs02/
│   └── 65816/
├── idempotence/        # Multiple-pass stability tests
│   ├── input/
│   └── output/
├── memory/             # Memory effect tracking tests
│   ├── input/
│   ├── expected/
│   └── traces/        # Memory write traces
├── integration/        # Real-world programs
│   └── programs/
├── peephole/          # Existing: optimization-specific tests
├── dead_code/         # Existing: dead code elimination tests
├── 6502_opt/          # Existing: CPU-specific optimization tests
├── 65c02_opt/         # Existing
├── 45gs02_opt/        # Existing
└── validation/        # Existing: register/flag tracking validation
```

---

## Priority 1: Emulator-Based Semantic Equivalence Testing

### Approach: Use Existing 6502 Emulators (No Custom Implementation)

Instead of writing a 6502 simulator, integrate existing battle-tested emulators:

#### Option A: py65 (Python-based, easiest integration)
- **Repository:** https://github.com/mnaberez/py65
- **License:** BSD
- **Pros:**
  - Pure Python, easy to install (`pip install py65`)
  - Simple API for loading/executing code
  - Read/write memory and registers
  - Cycle-accurate
  - Supports 6502, 65C02
- **Cons:**
  - No 45GS02 support (would need extension)
  - Slower than native C emulators

**Integration Strategy:**
```bash
# Install py65
pip3 install py65

# Create test runner script: tests/semantic/run_semantic_tests.py
# For each test case:
#   1. Assemble original.asm with xa or ca65
#   2. Assemble optimized.asm with xa or ca65
#   3. Load both into py65
#   4. Execute with same initial state
#   5. Compare final registers, flags, memory
```

#### Option B: lib6502 (C library, fast)
- **Repository:** https://github.com/oriontransfer/lib6502
- **License:** MIT
- **Pros:**
  - Fast native C implementation
  - Simple C API
  - Easy to integrate with opt6502.c
  - Can be compiled into test suite
- **Cons:**
  - Requires compilation
  - No Python bindings
  - Basic 6502 only (needs extension for 65C02/45GS02)

**Integration Strategy:**
```bash
# Vendor lib6502 into project
git submodule add https://github.com/oriontransfer/lib6502 vendor/lib6502

# Compile test harness that:
#   1. Links with opt6502.c and lib6502
#   2. Loads assembled binaries
#   3. Executes both versions
#   4. Compares states
```

#### Option C: fake6502 (Single-file C, ultra-lightweight)
- **File:** https://github.com/gianlucag/mos6502/blob/master/mos6502.c
- **License:** Public domain
- **Pros:**
  - Single C file, trivial integration
  - Cycle-accurate
  - Fast
  - Can extend for 65C02/45GS02
- **Cons:**
  - Less features than py65
  - Requires assembler integration

**Integration Strategy:**
```bash
# Copy fake6502 into project
cp mos6502.c tests/semantic/emulator.c

# Create test_runner.c that:
#   1. Uses system() to assemble with xa/ca65
#   2. Loads binary into fake6502 memory
#   3. Runs original, captures state
#   4. Resets, runs optimized, captures state
#   5. Compares and reports differences
```

#### **Recommended: Option A (py65) for Quick Implementation**

**Rationale:**
- Fastest to implement (no compilation, pure Python)
- Rich ecosystem and well-maintained
- Easy to extend with custom CPU features
- Can be run in CI/CD easily
- Can later migrate to C-based for performance if needed

---

### Semantic Test Case Structure

Each test case consists of:

```
tests/semantic/input/test_case_name.asm      # Original assembly
tests/semantic/state/test_case_name_init.txt # Initial state (optional)
tests/semantic/state/test_case_name_expect.txt # Expected final state
```

**State File Format (test_case_name_init.txt):**
```ini
; Initial CPU state
[registers]
A=0x00
X=0xFF
Y=0x42
Z=0x00  ; 45GS02 only

[flags]
C=0
N=1
Z=0
V=0

[memory]
; Address = Value (hex)
0x1000=0x00
0x1001=0xFF
0x2000=0x20
```

**Expected State File Format (test_case_name_expect.txt):**
```ini
; Expected final CPU state (after execution)
[registers]
A=0x42
X=0x00
Y=0xFF
Z=0x00

[flags]
C=1
N=0
Z=0
V=0

[memory]
0x1000=0x42
0x1001=0x00
0x2000=0x20
```

---

### Implementation: Semantic Test Runner (Python + py65)

**tests/semantic/run_semantic_tests.py:**

```python
#!/usr/bin/env python3
import sys
from py65.devices.mpu6502 import MPU
from pathlib import Path
import subprocess
import configparser

def assemble_file(asm_file, output_bin):
    """Assemble .asm to binary using xa or ca65"""
    # Use xa assembler (simple, available on most systems)
    result = subprocess.run(
        ['xa', '-o', output_bin, asm_file],
        capture_output=True
    )
    if result.returncode != 0:
        print(f"Assembly failed: {result.stderr.decode()}")
        return False
    return True

def load_state_file(state_file):
    """Parse initial state from .txt file"""
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
            state['registers'][reg.upper()] = int(val, 16)

    if 'flags' in config:
        for flag, val in config['flags'].items():
            state['flags'][flag.upper()] = int(val)

    if 'memory' in config:
        for addr, val in config['memory'].items():
            state['memory'][int(addr, 16)] = int(val, 16)

    return state

def init_mpu(mpu, state):
    """Initialize MPU with given state"""
    if state:
        if 'A' in state['registers']:
            mpu.a = state['registers']['A']
        if 'X' in state['registers']:
            mpu.x = state['registers']['X']
        if 'Y' in state['registers']:
            mpu.y = state['registers']['Y']

        # Set flags
        if 'C' in state['flags']:
            mpu.p = (mpu.p & ~0x01) | (state['flags']['C'] & 0x01)
        if 'N' in state['flags']:
            mpu.p = (mpu.p & ~0x80) | ((state['flags']['N'] & 0x01) << 7)
        if 'Z' in state['flags']:
            mpu.p = (mpu.p & ~0x02) | ((state['flags']['Z'] & 0x01) << 1)
        if 'V' in state['flags']:
            mpu.p = (mpu.p & ~0x40) | ((state['flags']['V'] & 0x01) << 6)

        # Set memory
        for addr, val in state['memory'].items():
            mpu.memory[addr] = val

def get_mpu_state(mpu):
    """Extract current MPU state"""
    return {
        'A': mpu.a,
        'X': mpu.x,
        'Y': mpu.y,
        'C': (mpu.p & 0x01),
        'N': (mpu.p & 0x80) >> 7,
        'Z': (mpu.p & 0x02) >> 1,
        'V': (mpu.p & 0x40) >> 6,
    }

def compare_states(state1, state2, test_name):
    """Compare two MPU states and report differences"""
    differences = []

    for key in ['A', 'X', 'Y', 'C', 'N', 'Z', 'V']:
        if state1[key] != state2[key]:
            differences.append(
                f"  {key}: original=0x{state1[key]:02X}, "
                f"optimized=0x{state2[key]:02X}"
            )

    if differences:
        print(f"✗ {test_name} FAILED - State mismatch:")
        for diff in differences:
            print(diff)
        return False
    else:
        print(f"✓ {test_name} PASSED")
        return True

def run_semantic_test(test_name, test_dir):
    """Run semantic equivalence test for a single test case"""
    input_asm = test_dir / 'input' / f'{test_name}.asm'
    init_state = test_dir / 'state' / f'{test_name}_init.txt'

    # Assemble original
    original_bin = test_dir / 'output' / f'{test_name}_original.bin'
    if not assemble_file(input_asm, original_bin):
        return False

    # Optimize and assemble
    optimized_asm = test_dir / 'output' / f'{test_name}_optimized.asm'
    opt_result = subprocess.run(
        ['./opt6502', '-speed', str(input_asm), str(optimized_asm)],
        capture_output=True
    )
    if opt_result.returncode != 0:
        print(f"Optimization failed: {opt_result.stderr.decode()}")
        return False

    optimized_bin = test_dir / 'output' / f'{test_name}_optimized.bin'
    if not assemble_file(optimized_asm, optimized_bin):
        return False

    # Load and execute original
    mpu_original = MPU()
    with open(original_bin, 'rb') as f:
        code = f.read()
        for i, byte in enumerate(code):
            mpu_original.memory[0x1000 + i] = byte

    init_state_data = load_state_file(init_state)
    init_mpu(mpu_original, init_state_data)
    mpu_original.pc = 0x1000

    # Execute until RTS or max cycles
    for _ in range(10000):
        if mpu_original.memory[mpu_original.pc] == 0x60:  # RTS
            break
        mpu_original.step()

    state_original = get_mpu_state(mpu_original)

    # Load and execute optimized
    mpu_optimized = MPU()
    with open(optimized_bin, 'rb') as f:
        code = f.read()
        for i, byte in enumerate(code):
            mpu_optimized.memory[0x1000 + i] = byte

    init_mpu(mpu_optimized, init_state_data)
    mpu_optimized.pc = 0x1000

    for _ in range(10000):
        if mpu_optimized.memory[mpu_optimized.pc] == 0x60:  # RTS
            break
        mpu_optimized.step()

    state_optimized = get_mpu_state(mpu_optimized)

    # Compare states
    return compare_states(state_original, state_optimized, test_name)

if __name__ == '__main__':
    test_dir = Path(__file__).parent

    # Find all test cases
    input_dir = test_dir / 'input'
    test_cases = [f.stem for f in input_dir.glob('*.asm')]

    passed = 0
    failed = 0

    for test_case in test_cases:
        if run_semantic_test(test_case, test_dir):
            passed += 1
        else:
            failed += 1

    print(f"\n{'='*60}")
    print(f"Semantic Tests: {passed} passed, {failed} failed")
    print(f"{'='*60}")

    sys.exit(0 if failed == 0 else 1)
```

**Usage:**
```bash
cd tests/semantic
python3 run_semantic_tests.py
```

---

## Priority 2: Flag State Transition Testing

Leverage existing `validate_register_and_flag_tracking()` to create deterministic test cases.

**Implementation:**

1. Run optimizer with `-trace 2` to capture register/flag states
2. Parse trace output into structured format
3. Compare state transitions between original and optimized
4. Verify states match at "observable points":
   - Before/after branches
   - Before RTS
   - Before memory writes

**tests/flag_state/compare_traces.sh:**
```bash
#!/bin/bash
# Compare flag states between original and optimized code

INPUT=$1
./opt6502 -speed -trace 2 "$INPUT" /tmp/original_trace.asm > /tmp/original_trace.txt
./opt6502 -speed "$INPUT" /tmp/optimized.asm
./opt6502 -speed -trace 2 /tmp/optimized.asm /tmp/optimized_trace.asm > /tmp/optimized_trace.txt

# Extract register states at observable points
grep "Register state at line" /tmp/original_trace.txt > /tmp/orig_states.txt
grep "Register state at line" /tmp/optimized_trace.txt > /tmp/opt_states.txt

# Compare (need to align by instruction, not line number)
diff -u /tmp/orig_states.txt /tmp/opt_states.txt
```

---

## Priority 3: Optimization Idempotence Testing

**tests/idempotence/test_idempotence.sh:**
```bash
#!/bin/bash
set -e

echo "Testing optimization idempotence..."

for testfile in tests/*/input/*.asm; do
    testname=$(basename "$testfile" .asm)

    # First optimization pass
    ./opt6502 -speed "$testfile" /tmp/pass1.asm

    # Second optimization pass
    ./opt6502 -speed /tmp/pass1.asm /tmp/pass2.asm

    # Compare outputs
    if diff -u /tmp/pass1.asm /tmp/pass2.asm; then
        echo "✓ $testname: Idempotent"
    else
        echo "✗ $testname: NOT idempotent (optimizer unstable)"
        exit 1
    fi
done

echo "All idempotence tests passed!"
```

---

## Priority 4: Cycle-Accurate Performance Validation

**Approach: Build cycle counter from instruction table**

**tests/performance/cycle_counter.py:**
```python
#!/usr/bin/env python3
# Cycle counter for 6502 instructions

CYCLE_TABLE = {
    'LDA': {'immediate': 2, 'zeropage': 3, 'absolute': 4, 'indexed': 4},
    'STA': {'zeropage': 3, 'absolute': 4, 'indexed': 5},
    'ADC': {'immediate': 2, 'zeropage': 3, 'absolute': 4},
    'JMP': {'absolute': 3},
    'JSR': {'absolute': 6},
    'RTS': {'implied': 6},
    'BNE': {'relative': 2},  # +1 if branch taken, +2 if page crossed
    # ... complete table for all instructions
}

def count_cycles(asm_file):
    """Count total cycles for assembly file"""
    total_cycles = 0
    with open(asm_file) as f:
        for line in f:
            # Parse instruction and addressing mode
            # Look up in cycle table
            # Add to total
            pass
    return total_cycles

# Compare original vs optimized
original_cycles = count_cycles('input/test.asm')
optimized_cycles = count_cycles('output/test.asm')

improvement = (original_cycles - optimized_cycles) / original_cycles * 100
print(f"Cycles: {original_cycles} → {optimized_cycles} ({improvement:.1f}% improvement)")
```

---

## Priority 5: Memory Effect Tracking

**tests/memory/track_memory_writes.py:**
```python
#!/usr/bin/env python3
# Extract all memory write operations and compare

import re

def extract_writes(asm_file):
    """Extract all STA/STX/STY/STZ operations"""
    writes = []
    with open(asm_file) as f:
        for line_num, line in enumerate(f, 1):
            # Match store instructions
            match = re.match(r'\s*(STA|STX|STY|STZ)\s+(.+)', line)
            if match:
                instr = match.group(1)
                operand = match.group(2)
                writes.append((line_num, instr, operand))
    return writes

original_writes = extract_writes('input/test.asm')
optimized_writes = extract_writes('output/test.asm')

# Compare write sequences
# Verify optimization didn't remove necessary writes
# Verify redundant write elimination is safe
```

---

## Priority 6: CPU-Specific Correctness Tests

**tests/correctness/65c02/test_stz_stores_zero.asm:**
```asm
; Verify 65C02 STZ stores literal zero (not Z register)
test_stz:
    LDA #$FF
    STA $1000
    ; This should be optimized to STZ on 65C02
    LDA #$00
    STA $1000
    ; Expected: $1000 = 0x00
    LDA $1000
    RTS
; Expected A = 0x00 after execution
```

**tests/correctness/45gs02/test_stz_stores_z_register.asm:**
```asm
; Verify 45GS02 STZ stores Z register (NOT zero!)
test_45gs02_stz:
    LDZ #$42
    STZ $1000
    ; Expected: $1000 = 0x42 (Z register value)
    LDA $1000
    RTS
; Expected A = 0x42 after execution
```

---

## Integration with CI/CD

**Suggested GitHub Actions Workflow:**

**.github/workflows/test.yml:**
```yaml
name: opt6502 Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y xa65 python3 python3-pip
          pip3 install py65

      - name: Build optimizer
        run: make

      - name: Run existing regression tests
        run: ./run_tests.sh

      - name: Run semantic equivalence tests
        run: |
          cd tests/semantic
          python3 run_semantic_tests.py

      - name: Run idempotence tests
        run: |
          cd tests/idempotence
          ./test_idempotence.sh

      - name: Run performance validation
        run: |
          cd tests/performance
          python3 validate_cycles.py

      - name: Run CPU-specific correctness tests
        run: |
          cd tests/correctness
          ./run_correctness_tests.sh
```

---

## Migration Strategy for Existing Tests

1. **Keep existing tests as-is** (they still provide value for regression)
2. **Add new test categories incrementally**:
   - Week 1: Implement semantic testing with py65
   - Week 2: Add idempotence tests
   - Week 3: Add cycle counting
   - Week 4: Add CPU-specific correctness tests

3. **Gradually migrate existing tests**:
   - Convert `tests/peephole/` tests to include semantic validation
   - Convert `tests/dead_code/` tests to include state files
   - Add cycle count baselines to all existing tests

---

## Summary of Recommended Tools

| Tool | Purpose | Installation | Pros |
|------|---------|--------------|------|
| **py65** | Emulation | `pip install py65` | Easy, Python, extensible |
| **xa65** | Assembler | `apt-get install xa65` | Simple, standard |
| **diff** | Comparison | Built-in | Universal |
| **Python** | Test scripting | Built-in | Flexible, readable |

---

## Expected Outcomes

After implementing these recommendations:

1. ✅ **Semantic correctness guaranteed** - All optimizations proven to preserve behavior
2. ✅ **Performance validated** - "Speed" optimizations demonstrably reduce cycles
3. ✅ **CPU-specific bugs prevented** - 65C02/45GS02 differences tested
4. ✅ **Regression prevention** - Idempotence catches optimizer instability
5. ✅ **Confidence in optimization** - Can add aggressive optimizations safely

---

## Next Steps

1. Install py65: `pip3 install py65`
2. Create `tests/semantic/` directory structure
3. Implement `run_semantic_tests.py` script
4. Convert 2-3 existing tests to semantic format
5. Run and validate
6. Gradually expand test coverage

---

## References

- py65: https://github.com/mnaberez/py65
- 6502 instruction cycle table: http://6502.org/tutorials/6502opcodes.html
- lib6502: https://github.com/oriontransfer/lib6502
- fake6502: https://github.com/gianlucag/mos6502
