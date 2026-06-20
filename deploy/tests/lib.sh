#!/usr/bin/env bash
# Mini-framework de pruebas (sin dependencias).
set -u
_TESTS_RUN=0
_TESTS_FAIL=0

assert_eq() {  # assert_eq EXPECTED ACTUAL [msg]
  _TESTS_RUN=$((_TESTS_RUN+1))
  if [ "$1" != "$2" ]; then
    _TESTS_FAIL=$((_TESTS_FAIL+1))
    printf 'FAIL: %s\n  esperado: [%s]\n  obtenido: [%s]\n' "${3:-assert_eq}" "$1" "$2" >&2
  fi
}

assert_contains() {  # assert_contains HAYSTACK NEEDLE [msg]
  _TESTS_RUN=$((_TESTS_RUN+1))
  case "$1" in
    *"$2"*) : ;;
    *)
      _TESTS_FAIL=$((_TESTS_FAIL+1))
      printf 'FAIL: %s\n  no contiene: [%s]\n  en: [%s]\n' "${3:-assert_contains}" "$2" "$1" >&2
      ;;
  esac
}

assert_not_contains() {  # assert_not_contains HAYSTACK NEEDLE [msg]
  _TESTS_RUN=$((_TESTS_RUN+1))
  case "$1" in
    *"$2"*)
      _TESTS_FAIL=$((_TESTS_FAIL+1))
      printf 'FAIL: %s\n  contiene (no debería): [%s]\n' "${3:-assert_not_contains}" "$2" >&2
      ;;
  esac
}

finish() {
  printf '%s: %d aserciones, %d fallos\n' "${0##*/}" "$_TESTS_RUN" "$_TESTS_FAIL"
  [ "$_TESTS_FAIL" -eq 0 ]
}
