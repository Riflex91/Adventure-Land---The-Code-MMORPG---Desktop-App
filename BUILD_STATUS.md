# Build status

This branch is the native feature-parity rewrite. A release is considered valid only after both GitHub Actions jobs — Windows x64 and Linux x86_64 — compile, run the core regression test and upload artifacts successfully.

Current validation: runtime helper declaration order was fixed after the first full-data parity build exposed a cross-platform compile error. The next CI run validates the corrected source against the full official Adventure Land dataset on both platforms.
