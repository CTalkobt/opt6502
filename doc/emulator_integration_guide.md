# Emulator Integration Guide for opt6502 Testing

## Overview

This guide explains how to integrate 6502 emulation for semantic equivalence testing **without writing a custom emulator**. We leverage existing, battle-tested emulators.

---

## Recommended Solution: py65

### Why py65?

- ✅ Pure Python - no compilation required
- ✅ Easy installation: `pip3 install py65`
- ✅ Simple API for programmatic control
- ✅ Cycle-accurate emulation
- ✅ Supports 6502 and 65C02
- ✅ Active maintenance
- ✅ MIT License (permissive)
- ✅ Can be extended for 45GS02 support

### Installation

```bash
# System-wide
pip3 install py65

# User-only
pip3 install --user py65

# Virtual environment (recommended)
python3 -m venv venv
source venv/bin/activate
pip3 install py65
```

### Basic Usage Example

```python
from py65.devices.mpu6502 import MPU

# Create CPU instance
mpu = MPU()

# Load program at 0x1000
program = [
    0xA9, 0x42,  # LDA #$42
    0xAA,        # TAX
    0x60         # RTS
]

for i, byte in enumerate(program):
    mpu.memory[0x1000 + i] = byte

# Set PC to start
mpu.pc = 0x1000

# Execute until RTS
while mpu.memory[mpu.pc] != 0x60:
    mpu.step()

# Execute RTS
mpu.step()

# Check final state
print(f"A = 0x{mpu.a:02X}")  # A = 0x42
print(f"X = 0x{mpu.x:02X}")  # X = 0x42
```

---

## Integration Architecture

### Test Flow

```
┌─────────────────┐
│  Original ASM   │
└────────┬────────┘
         │
         ├─────────────────┐
         │                 │
         v                 v
   ┌──────────┐      ┌──────────┐
   │ Optimize │      │ Assemble │
   └────┬─────┘      └────┬─────┘
        │                 │
        v                 │
   ┌──────────┐           │
   │ Assemble │           │
   └────┬─────┘           │
        │                 │
        v                 v
   ┌─────────────────────────┐
   │  Load into py65 (x2)    │
   └──────────┬──────────────┘
              │
              v
   ┌─────────────────────────┐
   │  Execute both versions  │
   └──────────┬──────────────┘
              │
              v
   ┌─────────────────────────┐
   │  Compare final states   │
   └──────────┬──────────────┘
              │
              v
         PASS / FAIL
```

### Components

1. **Assembler** (xa65 or ca65)
   - Converts .asm → binary
   - Required for both original and optimized code

2. **py65 Emulator**
   - Loads binary into memory
   - Executes instruction-by-instruction
   - Provides register/memory access

3. **Test Runner** (`run_semantic_tests.py`)
   - Coordinates workflow
   - Compares states
   - Reports results

---

## py65 API Reference

### Creating CPU Instance

```python
from py65.devices.mpu6502 import MPU

mpu = MPU()  # 6502
# or
from py65.devices.mpu65c02 import MPU as MPU65C02
mpu = MPU65C02()  # 65C02
```

### Register Access

```python
# Read registers
a_value = mpu.a
x_value = mpu.x
y_value = mpu.y
sp_value = mpu.sp
pc_value = mpu.pc

# Write registers
mpu.a = 0x42
mpu.x = 0x00
mpu.y = 0xFF
mpu.sp = 0xFF
mpu.pc = 0x1000
```

### Flag Access

```python
# Processor status register (P)
p_value = mpu.p

# Individual flags (via bitmask)
carry = (mpu.p & 0x01) != 0
zero = (mpu.p & 0x02) != 0
interrupt = (mpu.p & 0x04) != 0
decimal = (mpu.p & 0x08) != 0
break_flag = (mpu.p & 0x10) != 0
unused = (mpu.p & 0x20) != 0
overflow = (mpu.p & 0x40) != 0
negative = (mpu.p & 0x80) != 0

# Set flags
mpu.p |= 0x01   # Set carry
mpu.p &= ~0x01  # Clear carry
```

### Memory Access

```python
# Read memory
value = mpu.memory[0x1000]

# Write memory
mpu.memory[0x1000] = 0x42

# Load binary
with open('program.bin', 'rb') as f:
    code = f.read()
    for i, byte in enumerate(code):
        mpu.memory[0x1000 + i] = byte
```

### Execution

```python
# Execute one instruction
mpu.step()

# Execute until condition
while mpu.pc < 0x2000:
    mpu.step()

# Execute with cycle limit
for _ in range(10000):
    if mpu.memory[mpu.pc] == 0x60:  # RTS
        break
    mpu.step()
```

---

## Handling Assembly

### Using xa65 Assembler

```bash
# Install xa65
sudo apt-get install xa65  # Debian/Ubuntu
brew install xa            # macOS

# Assemble
xa -o output.bin input.asm
```

**Python integration:**
```python
import subprocess

def assemble_with_xa(asm_file, output_bin):
    result = subprocess.run(
        ['xa', '-o', output_bin, asm_file],
        capture_output=True,
        text=True
    )
    return result.returncode == 0
```

### Using ca65 Assembler (cc65 toolchain)

```bash
# Install cc65
sudo apt-get install cc65  # Debian/Ubuntu
brew install cc65          # macOS

# Assemble
ca65 -t none -o output.o input.asm
ld65 -t none -o output.bin output.o
```

**Python integration:**
```python
import subprocess

def assemble_with_ca65(asm_file, output_bin):
    obj_file = output_bin.replace('.bin', '.o')

    # Assemble to object file
    result = subprocess.run(
        ['ca65', '-t', 'none', '-o', obj_file, asm_file],
        capture_output=True
    )
    if result.returncode != 0:
        return False

    # Link to binary
    result = subprocess.run(
        ['ld65', '-t', 'none', '-o', output_bin, obj_file],
        capture_output=True
    )
    return result.returncode == 0
```

---

## Extending py65 for 45GS02

py65 doesn't support 45GS02 out-of-box, but can be extended:

### Approach 1: Subclass MPU

```python
from py65.devices.mpu65c02 import MPU as MPU65C02

class MPU45GS02(MPU65C02):
    def __init__(self):
        super().__init__()
        self.z = 0x00  # Add Z register

    def opLDZ(self, addr):
        """LDZ - Load Z register"""
        self.z = self.memory[addr]
        self.FlagsNZ(self.z)

    def opSTZ(self, addr):
        """STZ - Store Z register (NOT zero!)"""
        # On 45GS02, STZ stores Z register
        self.memory[addr] = self.z
        # No flags affected

    # Add to instruction table
    # (requires understanding py65 internals)
```

### Approach 2: Mock Z Register Behavior

For testing purposes, simulate Z register without full emulation:

```python
class MPU45GS02Wrapper:
    def __init__(self):
        self.mpu = MPU65C02()
        self.z = 0x00  # Track Z register separately

    def execute_ldz(self, value):
        self.z = value

    def execute_stz(self, addr):
        self.mpu.memory[addr] = self.z
```

### Approach 3: Pre-process Assembly

Convert 45GS02 instructions to 65C02 equivalents for testing:

```python
def convert_45gs02_to_65c02(asm_file):
    """Convert 45GS02 specific instructions for emulation"""
    with open(asm_file) as f:
        lines = f.readlines()

    converted = []
    for line in lines:
        # LDZ #$42 → LDA #$42 (approximately)
        line = line.replace('LDZ', 'LDA')
        # STZ when Z is known → STA
        # (More complex, requires state tracking)
        converted.append(line)

    return converted
```

---

## State Comparison Strategies

### Basic State Comparison

```python
def compare_states(state1, state2):
    """Compare two CPU states"""
    differences = []

    # Compare registers
    for reg in ['A', 'X', 'Y', 'SP']:
        if state1[reg] != state2[reg]:
            differences.append(f"{reg}: {state1[reg]:02X} != {state2[reg]:02X}")

    # Compare flags
    for flag, mask in [('C', 0x01), ('Z', 0x02), ('N', 0x80), ('V', 0x40)]:
        val1 = (state1['P'] & mask) != 0
        val2 = (state2['P'] & mask) != 0
        if val1 != val2:
            differences.append(f"Flag {flag}: {val1} != {val2}")

    return differences
```

### Memory Region Comparison

```python
def compare_memory(mpu1, mpu2, start, end):
    """Compare memory range between two MPUs"""
    differences = []

    for addr in range(start, end + 1):
        if mpu1.memory[addr] != mpu2.memory[addr]:
            differences.append(
                f"Memory[0x{addr:04X}]: "
                f"0x{mpu1.memory[addr]:02X} != 0x{mpu2.memory[addr]:02X}"
            )

    return differences
```

### Observable Points Comparison

Only compare state at "observable" points:
- Before branches
- Before subroutine returns
- Before memory writes to known addresses

```python
def find_observable_points(asm_file):
    """Parse assembly to find observable points"""
    observables = []

    with open(asm_file) as f:
        for line_num, line in enumerate(f, 1):
            if any(instr in line for instr in ['RTS', 'RTI', 'JMP', 'JSR']):
                observables.append(line_num)
            if any(instr in line for instr in ['STA', 'STX', 'STY']):
                observables.append(line_num)

    return observables
```

---

## Alternative Emulator Options

### lib6502 (C Library)

**Pros:**
- Fast (native C)
- Simple API
- Easy to integrate with opt6502.c

**Cons:**
- Requires compilation
- Less feature-rich than py65

**Basic Usage:**
```c
#include "lib6502.h"

M6502 *mpu = M6502_new(0, 0, 0);

// Load program
mpu->memory[0x1000] = 0xA9;  // LDA #$42
mpu->memory[0x1001] = 0x42;
mpu->memory[0x1002] = 0x60;  // RTS

// Execute
M6502_setPC(mpu, 0x1000);
while (mpu->memory[mpu->registers->pc] != 0x60) {
    M6502_run(mpu);
}

printf("A = 0x%02X\n", mpu->registers->a);

M6502_delete(mpu);
```

### fake6502 (Single File)

**Pros:**
- Ultra-lightweight (one .c file)
- Public domain
- Easy to embed

**Cons:**
- Minimal API
- Requires callback setup

**Basic Usage:**
```c
#include "fake6502.h"

uint8_t read6502(uint16_t address) {
    return memory[address];
}

void write6502(uint16_t address, uint8_t value) {
    memory[address] = value;
}

// Initialize
reset6502();
pc = 0x1000;

// Execute
while (memory[pc] != 0x60) {
    step6502();
}
```

---

## Practical Integration Checklist

- [ ] Install py65: `pip3 install py65`
- [ ] Install assembler: `sudo apt-get install xa65`
- [ ] Create test directory structure
- [ ] Implement `run_semantic_tests.py` script
- [ ] Create initial test cases with expected states
- [ ] Run tests and verify functionality
- [ ] Add to CI/CD pipeline
- [ ] Document any 45GS02 workarounds needed
- [ ] Expand test coverage gradually

---

## Performance Considerations

### py65 Speed
- ~100-500K instructions/second (Python)
- Adequate for unit tests (<10K instructions each)
- Not suitable for large program emulation

### Optimization
If tests are too slow:
1. Use PyPy instead of CPython: `pypy3 -m pip install py65`
2. Parallelize tests: Run multiple test cases concurrently
3. Cache assembled binaries: Don't reassemble unchanged files
4. Consider C-based emulator (lib6502) for hot path

---

## Troubleshooting

### py65 not found
```bash
pip3 install --user py65
export PATH="$HOME/.local/bin:$PATH"
```

### xa65 not found
```bash
# Debian/Ubuntu
sudo apt-get install xa65

# macOS
brew install xa

# From source
git clone https://github.com/fachat/xa65
cd xa65
make
sudo make install
```

### Assembly fails
- Check syntax (comments, labels, addressing modes)
- Try different assembler (xa vs ca65)
- Verify CPU target matches assembler expectations

### State comparison fails unexpectedly
- Verify initial state is truly identical
- Check for non-deterministic code (timing loops)
- Ensure code terminates properly (RTS)

---

## Summary

**Recommended Stack:**
- **Emulator**: py65 (Python, easy integration)
- **Assembler**: xa65 (simple, standard)
- **Language**: Python 3 (test runner)
- **Integration**: Subprocess for opt6502

**Workflow:**
1. Assemble original → binary1
2. Optimize + assemble → binary2
3. Load both into py65
4. Execute with same initial state
5. Compare final states
6. Report pass/fail

This approach provides **high confidence semantic testing** without the complexity of writing and maintaining a custom emulator.
