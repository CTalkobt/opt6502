# Performance Validation Tests

## Purpose
Verify that "speed" optimizations actually reduce cycle counts, and "size" optimizations reduce code size.

## Metrics Tracked

1. **Cycle Count**: Total CPU cycles for execution
2. **Code Size**: Total bytes of assembled code
3. **Optimization Effectiveness**: Percentage improvement

## Test Structure

- `input/` - Original test cases with baseline performance
- `output/` - Optimized code
- `metrics/` - Performance comparison results

## Cycle Counting

For accurate cycle counting, each instruction's cycle count depends on:
- Base instruction cycles
- Addressing mode
- Page boundary crossing (indexed modes)
- Branch taken/not taken

## Running Tests

```bash
cd tests/performance
python3 validate_performance.py
```

## Expected Outcomes

- Speed mode: Cycle count should decrease (or remain same)
- Size mode: Code size should decrease (or remain same)
- No regression: Optimization should never increase both cycles AND size

## Baseline Metrics

Each test case should document baseline performance:

```
# metrics/test_name_baseline.txt
cycles_original=150
bytes_original=45
cycles_optimized=120
bytes_optimized=38
improvement_cycles=20.0%
improvement_size=15.6%
```
