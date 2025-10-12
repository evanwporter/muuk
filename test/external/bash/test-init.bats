#!/usr/bin/env bats

setup() {
  cd "$BATS_TEST_TMPDIR"
}

@test "muuk init works" {
    run bash -c 'printf "hello\nmyself\n\n\n\n\nyes" | muuk init'
    echo "$PWD" >&2
    [ -f muuk.toml ]
    [ -d src ]
    [ -f src/hello.cpp ]
    [ -d include ]
    [ -f include/hello.hpp ]
    [ "$status" -eq 0 ]
}