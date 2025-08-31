# Copilot Instructions for socketCommunicator

Project snapshot (C++/CMake + GoogleTest): a single library target `socket_communicator` with a minimal class `Communicator` and a test binary `socket_communicator_test`. Keep changes small, compile often, and drive with tests per `CONTRIBUTING.md`.

## Structure and intent
- Library code
	- Headers: `src/include/` (e.g., `Communicator.h` exposes simple accessors)
	- Implementations: `src/lib/` (e.g., `Communicator.cpp`)
	- C++17 standard; optimize with `-O3 -w` globally
- Tests: `tests/` (GoogleTest). One test executable `socket_communicator_test` builds from listed test sources.
- Build system: CMake with FetchContent for GoogleTest (v1.15.2). Test binary output goes to `build/test/`.

## Daily workflow (macOS-friendly)
1) Configure and build
	 - cmake -S . -B build
	 - cmake --build build -j
2) Run tests
	 - ctest --test-dir build
	 - or run build/test/socket_communicator_test directly
3) Edit cycle
	 - Add headers under `src/include/` and .cpp files under `src/lib/`
	 - Update CMake: either add new .cpps to `add_library(socket_communicator ...)` or use `target_sources(socket_communicator PRIVATE src/lib/NewThing.cpp)`
	 - Add new test files under `tests/` and append them to `add_executable(socket_communicator_test ... )`

## Conventions and patterns specific to this repo
- Follow `CONTRIBUTING.md`:
	- Maintain a short `IMPLEMENTATION_PLAN.md` for multi-step tasks and remove it when done.
	- Test first → minimal implementation → refactor. Keep the build green.
- Include headers with quotes relative to `src/include/` (e.g., `"Communicator.h"`). Public API lives in headers; keep classes simple with explicit accessors.
- Platform flags: x86 SIMD flags are gated in CMake. Do not add `-maes/-mavx*` unconditionally; use `CMAKE_SYSTEM_PROCESSOR` checks. On macOS, the tests link with `-Wl,-ld_classic`.
- Test style: use plain `TEST(Suite, Name)` cases with clear, behavior-focused expectations (see `tests/CommunicatorTest.cpp`). Keep tests deterministic.

## Extending the codebase (examples)
- New class
	- Create `src/include/MyThing.h` and `src/lib/MyThing.cpp`
	- Register source: `target_sources(socket_communicator PRIVATE src/lib/MyThing.cpp)`
- New tests
	- Create `tests/MyThingTest.cpp` with GoogleTest cases
	- Add the file to `add_executable(socket_communicator_test ...)` so it compiles into the single test binary

## Troubleshooting
- Apple Silicon build errors like "unrecognized command-line option '-maes'": ensure SIMD flags remain x86-only (already handled in CMake).
- If GoogleTest fails to fetch, re-run configure; network issues can affect `FetchContent`.

## Source docs to consult
- `CONTRIBUTING.md` for philosophy, process, and quality gates.
- `CMakeLists.txt` for targets, flags, and how tests are wired.
