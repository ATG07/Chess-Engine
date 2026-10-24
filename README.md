# Chess Engine

Hi! I am **Arjun Pathak**, a student at BITS Goa, and I am building a chess engine in modern C++.

I have always been interested in chess, and an engine felt like a fun project that could grow with me. Because chess engines combine performance, algorithms, and careful state management, this has also become a great way to practise new C++ features and understand when they are genuinely useful.

Some ideas I am exploring include:

- Template parameters and `constexpr` to move decisions from runtime to compile time
- C++20 ranges and `std::string_view` for parsing FEN without unnecessary copies
- Compact representations that keep frequently accessed state cache-friendly

## Current implementation

- A 64-bit bitboard representation for each piece type and colour
- A mailbox board alongside the bitboards for direct piece lookup
- FEN parsing, including castling rights, en passant state, and move counters
- Compact 16-bit moves containing source, destination, promotion, and move type
- Packed castling and en passant state
- Pawn, knight, bishop, rook, queen, and king move generation
- Legal king moves, including castling through safe squares
- Pseudo-legal generation for other pieces; pin, check-evasion, and en passant legality filtering is the current focus
- Ray-based sliding attacks for now, with lookup-based attacks planned later
- GoogleTest unit tests and a separate perft test suite containing standard tactical positions

The code is intentionally still a work in progress. At this stage I am prioritising correct move generation and perft results before beginning the search.

## Building

The project requires a C++20 compiler and CMake 3.16 or newer. GoogleTest is found locally when available, or downloaded by CMake during configuration.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
```

## Running tests

Run the unit tests without the slower perft suite:

```sh
ctest --test-dir build -LE perft --output-on-failure
```

Run only perft tests:

```sh
ctest --test-dir build -L perft --output-on-failure
```

On Windows, individual GoogleTest suites can also be filtered directly:

```powershell
.\build\tests\chess_unit_tests.exe --gtest_filter=BoardAttackTest.*
```

## Roadmap

My current plan is to:

1. Finish legal move generation using checker, blocker, and pin bitboards.
2. Complete reversible move handling and Zobrist hashing.
3. Validate correctness against standard perft positions.
4. Implement negamax with alpha-beta pruning, iterative deepening, quiescence search, and selective extensions.
5. Add move ordering, transposition tables, and other search improvements.
6. Build a strong evaluation function and eventually experiment with NNUE.
7. Explore a CNN-based evaluator as an optional low-depth experiment.

I hope to eventually, get my bot a rating higher than mine on lichess!
