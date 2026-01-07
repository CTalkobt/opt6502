# Documentation Directory

This directory contains all project documentation for opt6502.

## Documentation Index

### User Guides

- **[6502_optimizations_guide.md](./6502_optimizations_guide.md)** - Guide to 6502 optimization techniques
- **[45gs02_optimization_guide.md](./45gs02_optimization_guide.md)** - Guide to 45GS02 (MEGA65) specific optimizations

### Testing Documentation

- **[TESTING.md](./TESTING.md)** - Complete testing guide (quick start, usage, debugging)
- **[test_recommendations.md](./test_recommendations.md)** - Comprehensive testing strategy and 10 testing approaches
- **[emulator_integration_guide.md](./emulator_integration_guide.md)** - Detailed py65 emulator integration guide
- **[test_implementation_summary.md](./test_implementation_summary.md)** - Implementation status and next steps
- **[TESTING_FRAMEWORK_SUMMARY.md](./TESTING_FRAMEWORK_SUMMARY.md)** - Quick overview of testing framework

### Implementation Documentation

- **[VALIDATION_SUMMARY.md](./VALIDATION_SUMMARY.md)** - Register and flag tracking validation implementation

## Quick Links

### I want to...

**...understand what optimizations are available**
→ Start with [6502_optimizations_guide.md](./6502_optimizations_guide.md)
→ For MEGA65, see [45gs02_optimization_guide.md](./45gs02_optimization_guide.md)

**...run tests**
→ Quick start: [TESTING.md](./TESTING.md)
→ Overview: [TESTING_FRAMEWORK_SUMMARY.md](./TESTING_FRAMEWORK_SUMMARY.md)

**...add new tests**
→ Testing guide: [TESTING.md](./TESTING.md)
→ See examples in `../tests/semantic/`

**...understand the test strategy**
→ Full recommendations: [test_recommendations.md](./test_recommendations.md)
→ Implementation details: [test_implementation_summary.md](./test_implementation_summary.md)

**...integrate emulation testing**
→ Complete guide: [emulator_integration_guide.md](./emulator_integration_guide.md)

**...understand register tracking**
→ Implementation: [VALIDATION_SUMMARY.md](./VALIDATION_SUMMARY.md)

## Documentation Structure

```
doc/
├── README.md (this file)
│
├── Optimization Guides
│   ├── 6502_optimizations_guide.md
│   └── 45gs02_optimization_guide.md
│
├── Testing Framework
│   ├── TESTING.md                           # User-facing guide
│   ├── TESTING_FRAMEWORK_SUMMARY.md         # Quick overview
│   ├── test_recommendations.md              # Strategy & approaches
│   ├── test_implementation_summary.md       # Status & next steps
│   └── emulator_integration_guide.md        # py65 integration
│
└── Implementation Details
    └── VALIDATION_SUMMARY.md                # Register tracking
```

## External Documentation

- Main project README: [../README.md](../README.md)
- Test suite README: [../tests/README.md](../tests/README.md)
- Semantic tests README: [../tests/semantic/README.md](../tests/semantic/README.md)
- Correctness tests README: [../tests/correctness/README.md](../tests/correctness/README.md)
- Performance tests README: [../tests/performance/README.md](../tests/performance/README.md)
