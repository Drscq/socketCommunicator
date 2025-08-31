# Development Guidelines

## Philosophy

### Core Beliefs

- Incremental progress over big bangs
- Learning from existing code
- Pragmatic over dogmatic
- Clear intent over clever code

### Simplicity Means

- Single responsibility per function/class
- Avoid premature abstractions
- No clever tricks
- If you need to explain it, it's too complex

## Process

### 1. Planning & Staging

Break complex work into 3-5 stages. Document in `IMPLEMENTATION_PLAN.md`:

## Stage N: [Name]
**Goal**: [Specific deliverable]
**Success Criteria**: [Testable outcomes]
**Tests**: [Specific test cases]
**Status**: [Not Started|In Progress|Complete]

- Update status as you progress
- Remove file when all stages are done

### 2. Implementation Flow

1. Understand
2. Test first (red)
3. Implement minimal to pass (green)
4. Refactor with tests passing
5. Commit with message linking to plan

### 3. When Stuck (After 3 Attempts)

Maximum 3 attempts per issue, then STOP. Document attempts, errors, and reasoning in the plan. Research alternatives and consider simpler approaches before proceeding.

## Technical Standards

### Architecture Principles

- Composition over inheritance; prefer DI
- Interfaces over singletons
- Explicit over implicit
- Test-driven when possible

### Code Quality

- Every commit compiles and passes tests
- Add tests for new functionality
- Run formatter/linter before committing
- Commit messages explain "why"

### Error Handling

- Fail fast with context
- Handle errors at appropriate level
- Never swallow exceptions silently

## Decision Framework

1. Testability
2. Readability
3. Consistency
4. Simplicity
5. Reversibility

## Project Integration

- Learn from similar components
- Reuse patterns/utilities
- Follow existing test patterns
- Use existing build/test/format tools; justify new tools

## Quality Gates

### Definition of Done

- [ ] Tests written and passing
- [ ] Code follows project conventions
- [ ] No linter/formatter warnings
- [ ] Commit messages are clear
- [ ] Implementation matches plan
- [ ] No TODOs without issue numbers

### Test Guidelines

- Test behavior, not implementation
- One assertion per test when feasible
- Clear, scenario-based test names
- Deterministic tests only

## Important Reminders

NEVER use `--no-verify`, disable tests, commit code that doesn't compile, or make assumptions without verification.

ALWAYS commit working code incrementally, update the plan, learn from implementations, and stop after 3 failed attempts to reassess.
