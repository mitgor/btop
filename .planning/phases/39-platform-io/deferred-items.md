# Deferred Items - Phase 39

## Pre-existing Build Issues

1. **src/osx/btop_collect.cpp:866,906** - `SMCConnection` constructor is private but called from non-friend code. This error predates phase 39 and blocks full `btop_test` linking on macOS. Not caused by any phase 39 changes.
