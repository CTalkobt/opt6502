# Testing Guide for opt6502

## Quick Start

```bash
# Install test dependencies (one-time)
pip3 install py65
sudo apt-get install xa65  # or: brew install xa

# Run all tests
./run_all_tests.sh

# Run specific test suites
./run_tests.sh                                    # Regression tests only
tests/idempotence/test_idempotence.sh            # Idempotence tests
cd tests/semantic && python3 run_semantic_tests.py  # Semantic tests
```

---

## Test Categories

### 1. Regression Tests (Golden File Comparison)
**Location:** `tests/{peephole,dead_code,6502_opt,65c02_opt,45gs02_opt,validation}/`

**Purpose:** Verify optimizer produces expected output for known test cases.

**How it works:**
- Compare optimizer output against pre-approved "golden" files
- Ensures no regressions in existing optimizations
- Fast, no external dependencies

**Running:**
```bash
./run_tests.sh
```

**Limitations:**
- ❌ Doesn't verify semantic correctness (only text comparison)
- ❌ Can't detect if optimization has side-effects
- ❌ Brittle (any output change breaks test, even harmless ones)

---

### 2. Semantic Equivalence Tests (Emulator-Based)
**Location:** `tests/semantic/`

**Purpose:** Verify optimized code behaves identically to original code.

**How it works:**
- Executes both original and optimized code in py65 emulator
- Compares final CPU state (registers, flags, memory)
- Guarantees correctness regardless of how optimization was achieved

**Running:**
```bash
# Install dependency first
pip3 install py65

# Run tests
cd tests/semantic
python3 run_semantic_tests.py [test_name]
```

**Test Structure:**
```
semantic/
├── input/
│   └── my_test.asm           # Original assembly
├── state/
│   ├── my_test_init.txt      # Initial CPU state (optional)
│   └── my_test_expect.txt    # Expected final state
└── output/
    └── (generated files)
```

**Example Test Case:**
```asm
; input/simple_load.asm
test:
    LDA #$42
    TAX
    RTS
```

```ini
; state/simple_load_expect.txt
[registers]
A=0x42
X=0x42
```

**Benefits:**
- ✅ Proves semantic correctness
- ✅ Detects side-effects and behavior changes
- ✅ Works regardless of optimization strategy
- ✅ Provides cycle count comparison

---

### 3. Idempotence Tests (Stability Verification)
**Location:** `tests/idempotence/`

**Purpose:** Verify optimizer produces stable, converging results.

**How it works:**
- Runs optimizer 3 times on same input
- Verifies passes 1, 2, and 3 produce identical output
- Detects optimizer instability and infinite optimization loops

**Running:**
```bash
tests/idempotence/test_idempotence.sh
```

**What it catches:**
- Optimizer doesn't converge (keeps changing output)
- Optimization cycles (A→B→A→B pattern)
- Non-deterministic behavior

**Expected outcome:**
```
Testing peephole/redundant_loads: PASS (idempotent after 3 passes)
Testing dead_code/unreachable: PASS (idempotent after 3 passes)
```

---

### 4. CPU-Specific Correctness Tests
**Location:** `tests/correctness/{6502,65c02,45gs02,65816}/`

**Purpose:** Verify CPU-specific optimizations are applied correctly.

**Critical Tests:**
- **6502:** No 65C02/65816 instructions used
- **65C02:** STZ stores literal zero (not Z register)
- **45GS02:** STZ stores Z register (not literal zero!)
- **65816:** 16-bit mode handling

**Running:**
```bash
tests/correctness/run_correctness_tests.sh
```

**Why critical:**
- 45GS02 STZ behavior is fundamentally different from 65C02
- Using wrong instruction can cause catastrophic bugs
- Must verify CPU target is respected

---

### 5. Performance Validation Tests
**Location:** `tests/performance/`

**Purpose:** Verify optimization claims (speed/size) are accurate.

**How it works:**
- Count cycles for original vs optimized
- Measure code size in bytes
- Verify "speed" mode actually reduces cycles
- Verify "size" mode actually reduces size

**Running:**
```bash
cd tests/performance
python3 validate_performance.py
```

**Metrics tracked:**
- Total cycle count
- Code size in bytes
- Percentage improvement
- Performance regression detection

---

## Writing New Tests

### Semantic Test

1. Create input assembly:
```bash
cat > tests/semantic/input/my_test.asm <<'EOF'
test:
    LDA #$10
    ADC #$05
    TAX
    RTS
EOF
```

2. Create expected state:
```bash
cat > tests/semantic/state/my_test_expect.txt <<'EOF'
[registers]
A=0x15
X=0x15

[flags]
N=0
Z=0
C=0
EOF
```

3. Run test:
```bash
cd tests/semantic
python3 run_semantic_tests.py my_test
```

### Regression Test

1. Create input and expected output:
```bash
# Input
cat > tests/peephole/input/my_opt.asm <<'EOF'
    LDA #$00
    LDA #$00
EOF

# Expected (run optimizer to generate)
./opt6502 -speed tests/peephole/input/my_opt.asm tests/peephole/expected/my_opt.asm
```

2. Run regression tests:
```bash
./run_tests.sh
```

### Idempotence Test

Idempotence tests automatically run on all existing tests. No additional files needed.

---

## Continuous Integration

### GitHub Actions Workflow

```yaml
name: Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y xa65
          pip3 install py65

      - name: Build
        run: make

      - name: Run all tests
        run: ./run_all_tests.sh
```

---

## Test Debugging

### Semantic Test Fails

```bash
# Run specific test
cd tests/semantic
python3 run_semantic_tests.py my_test

# Check generated files
ls -la output/my_test_*

# Compare original vs optimized assembly
diff -u input/my_test.asm output/my_test_optimized.asm

# Manually test in py65
python3
>>> from py65.devices.mpu6502 import MPU
>>> mpu = MPU()
>>> # ... debug interactively
```

### Idempotence Test Fails

```bash
# Run specific file through multiple passes
./opt6502 -speed input.asm pass1.asm
./opt6502 -speed pass1.asm pass2.asm
./opt6502 -speed pass2.asm pass3.asm

# Compare passes
diff -u pass1.asm pass2.asm
diff -u pass2.asm pass3.asm

# Look for optimization cycles
```

### Regression Test Fails

```bash
# See what changed
./run_tests.sh  # Shows diff

# If change is intentional, update expected output
./opt6502 -speed tests/category/input/test.asm tests/category/expected/test.asm

# Verify with semantic test that behavior is correct
```

---

## Test Coverage

Current coverage:

- ✅ Peephole optimizations
- ✅ Dead code elimination
- ✅ Register tracking validation
- ✅ 6502/65C02/45GS02 CPU variants
- ✅ Idempotence (stability)
- ⚠️  Semantic equivalence (partial - expand coverage)
- ⚠️  Performance validation (planned)
- ❌ Fuzzing (future)
- ❌ Large program integration tests (future)

---

## Best Practices

### DO:
- ✅ Add semantic test for every optimization type
- ✅ Run idempotence tests regularly
- ✅ Add regression test for every bug fix
- ✅ Document expected behavior in test comments
- ✅ Use meaningful test names

### DON'T:
- ❌ Trust golden file tests alone
- ❌ Skip semantic validation for "simple" optimizations
- ❌ Assume optimization is safe without testing
- ❌ Ignore idempotence failures
- ❌ Test implementation details (test observable behavior)

---

## Test Philosophy

We test **what code does**, not **how it's optimized**.

**Good test:**
```python
# Verify final register state matches
assert mpu_original.a == mpu_optimized.a
```

**Bad test:**
```python
# Don't test specific optimization was applied
assert "STZ" in optimized_code
```

The optimizer should be free to choose any optimization strategy as long as behavior is preserved.

---

## Performance

### Test Suite Speed

- Regression tests: ~1-2 seconds
- Idempotence tests: ~5-10 seconds
- Semantic tests: ~10-30 seconds (depends on py65)

### Optimization

If tests are slow:
1. Use PyPy: `pypy3 -m pytest`
2. Parallelize: `pytest -n auto`
3. Cache assemblies: Don't reassemble unchanged files
4. Profile: Find slow tests and optimize them

---

## Getting Help

- **Test failures:** Check test output, compare diffs
- **Semantic test issues:** See `doc/emulator_integration_guide.md`
- **py65 problems:** https://github.com/mnaberez/py65
- **Questions:** See `doc/test_recommendations.md`

---

## Summary

**Test Layers:**
1. **Regression** - Fast, catches obvious breaks
2. **Semantic** - Proves correctness, catches subtle bugs
3. **Idempotence** - Ensures stability
4. **Correctness** - Validates CPU-specific behavior
5. **Performance** - Verifies optimization effectiveness

Run all tests with: `./run_all_tests.sh`

**Remember:** A test suite without semantic validation is incomplete. Always verify behavior, not just output text.
