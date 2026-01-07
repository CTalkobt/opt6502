# opt6502 Test Suite

## Test Categories

### Existing Tests (Regression)

- **peephole/** - Peephole optimization tests
- **dead_code/** - Dead code elimination tests
- **6502_opt/** - 6502-specific optimization tests
- **65c02_opt/** - 65C02-specific optimization tests
- **45gs02_opt/** - 45GS02-specific optimization tests
- **validation/** - Register and flag tracking validation

### New Test Framework

- **semantic/** - Emulator-based semantic equivalence tests
- **idempotence/** - Multi-pass stability tests
- **correctness/** - CPU-specific correctness tests
- **performance/** - Cycle count and size validation
- **memory/** - Memory effect tracking tests

## Running All Tests

```bash
# Run existing regression tests
./run_tests.sh

# Run new semantic equivalence tests (requires py65)
cd tests/semantic
pip3 install py65
python3 run_semantic_tests.py

# Run idempotence tests
tests/idempotence/test_idempotence.sh

# Run all tests (requires test suite setup)
make test
```

## Test Requirements

### For Semantic Tests
- Python 3.6+
- py65: `pip3 install py65`
- Assembler: xa65 or ca65

### For All Tests
- Compiled opt6502: `make`
- Bash shell
- Standard Unix utilities (diff, sed, etc.)

## Adding New Tests

### Semantic Test
1. Create `tests/semantic/input/my_test.asm`
2. (Optional) Create `tests/semantic/state/my_test_init.txt` for initial state
3. Create `tests/semantic/state/my_test_expect.txt` for expected state
4. Run: `cd tests/semantic && python3 run_semantic_tests.py my_test`

### Regression Test (Golden File)
1. Create `tests/category/input/my_test.asm`
2. Create `tests/category/expected/my_test.asm` (expected optimized output)
3. Run: `./run_tests.sh`

## Test Philosophy

### What We Test

1. **Semantic Correctness**: Optimized code behaves identically to original
2. **Performance Claims**: Speed optimizations actually improve speed
3. **Stability**: Optimizer produces consistent, converging results
4. **CPU Correctness**: CPU-specific features used appropriately
5. **No Side Effects**: Optimizations don't change observable behavior

### What We Don't Test

- Specific optimization strategies (implementation detail)
- Internal data structures
- Exact output format (as long as semantically equivalent)

## Test Coverage Goals

- ✅ All optimization types have semantic tests
- ✅ All CPU variants have correctness tests
- ✅ All test cases pass idempotence checks
- ✅ Performance improvements are validated
- ✅ Edge cases and corner cases covered

## Continuous Integration

Tests run automatically on:
- Every push to main branch
- Every pull request
- Nightly builds

See `.github/workflows/test.yml` for CI configuration.

## Debugging Test Failures

### Semantic Test Fails
1. Check `tests/semantic/output/` for generated files
2. Compare original vs optimized assembly
3. Run with manual py65 session to debug

### Idempotence Test Fails
1. Check diff output for what's changing between passes
2. Look for optimization cycles (A→B→A pattern)
3. May indicate optimizer bug or missing convergence check

### Correctness Test Fails
1. Verify correct CPU target specified
2. Check if CPU-specific instruction used incorrectly
3. Review optimization logic for that CPU

## Performance Benchmarks

Run full performance suite:
```bash
cd tests/performance
./run_benchmarks.sh
```

Results stored in `metrics/` directory with historical trends.

## Test Maintenance

- Add new test for every bug fix
- Update expected outputs when optimization improves
- Remove obsolete tests only with careful consideration
- Document any test that seems non-obvious

## Questions?

See `doc/test_recommendations.md` for detailed testing strategy and rationale.
