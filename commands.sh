#!/bin/sh

BAZEL=${BAZEL:-bazel}

${BAZEL} run @hedron_compile_commands//:refresh_all