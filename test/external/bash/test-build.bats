#!/usr/bin/env bats

setup() {
  cd "$BATS_TEST_TMPDIR"
  printf "hello\nmyself\n\n\n\n\nyes" | muuk init
}

@test "muuk debug build works" {
    ! [ -d build ]
    run bash -c 'muuk build -p debug'
    echo $ouput >&2
    
    [ -d build ]
    [ -d build/debug ]
    ! [ -d build/release ]
    [ -f build/debug/hello ]
    [ "$status" -eq 0 ]

    run bash -c 'muuk run'
    echo "$output" >&2
    [ "$status" -eq 0 ]
}

@test "muuk release build works" {
    ! [ -d build ]
    run bash -c 'muuk build -p release'
    echo $ouput >&2

    [ -d build ]
    [ -d build/release ]
    ! [ -d build/debug ]
    [ -f build/release/hello ]
    [ "$status" -eq 0 ]

    run bash -c 'muuk run'
    echo "$output" >&2
    [ "$status" -eq 1 ]

    run bash -c 'muuk run --release'
    echo "$output" >&2
    [ "$status" -eq 0 ]
}