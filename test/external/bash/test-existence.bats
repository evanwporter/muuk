#!/usr/bin/env bats

setup() {
  cd "$BATS_TEST_TMPDIR"
}

@test "muuk binary exists" {
  run command -v muuk
  [ "$status" -eq 0 ]
}