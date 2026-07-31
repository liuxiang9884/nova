# Plan Review Log: cpp-yyjson local vcpkg port for nova

Act 1 (grill) complete — plan locked with the user. MAX_ROUNDS=5.

## Round 1 — Codex

## Material findings

1. **The dependency is scoped incorrectly.** `cpp_yyjson` is used only by `yyjson_demo`, while examples are already top-level-only; making package resolution unconditional forces embedded consumers such as Orion to resolve an unused dependency ([CMakeLists.txt](/home/liuxiang/dev/nova-cpp-yyjson-vcpkg/CMakeLists.txt:40), [examples/CMakeLists.txt](/home/liuxiang/dev/nova-cpp-yyjson-vcpkg/examples/CMakeLists.txt:88)).
Fix: Move cpp-yyjson discovery/fallback into the top-level examples path instead of global `cmake/packages.cmake`.

2. **The plan says "use the target" but identifies no consumer-target change.** Current code exports a raw include directory through `nova` and manually links `yyjson`; this discards `cpp_yyjson::cpp_yyjson`'s transitive dependency, C++20 feature, and MSVC option ([CMakeLists.txt](/home/liuxiang/dev/nova-cpp-yyjson-vcpkg/CMakeLists.txt:32), [upstream CMakeLists.txt](/home/liuxiang/tmp/orion-native-x86-20260731.2kmSlB/cpp-yyjson/CMakeLists.txt:21)).
Fix: Link `yyjson_demo PRIVATE cpp_yyjson::cpp_yyjson` and remove `CPP_YYJSON_INCLUDE` from both `nova` and the demo.

3. **The retained fallback preserves the original failure and a supply-chain risk.** It still follows mutable `main`, uses credential-dependent SSH, and recursively initializes the irrelevant benchmark submodule ([cmake/packages.cmake](/home/liuxiang/dev/nova-cpp-yyjson-vcpkg/cmake/packages.cmake:3)).
Fix: Use HTTPS, pin the fallback to the same commit/tag, and set `GIT_SUBMODULES ""`.

4. **The port name and CMake package name conflict is unaddressed.** `vcpkg_cmake_config_fixup` defaults `PACKAGE_NAME` to port name `cpp-yyjson`, but CMake searches for `cpp_yyjson`; the resulting config can be installed where `find_package(cpp_yyjson)` will not find it ([fixup helper](/home/liuxiang/vcpkg/ports/vcpkg-cmake-config/vcpkg_cmake_config_fixup.cmake:9)).
Fix: Require `vcpkg_cmake_config_fixup(PACKAGE_NAME cpp_yyjson CONFIG_PATH lib/cmake/cpp_yyjson)` explicitly.

5. **The overlay-port definition is underspecified.** The plan names only `yyjson`, omitting required host dependencies for `vcpkg_cmake_configure`/`vcpkg_cmake_config_fixup`, source-integrity hashing, copyright installation, and the exact singular option names.
Fix: Specify `vcpkg-cmake` and `vcpkg-cmake-config` host dependencies, `SHA512`, copyright installation, and `CPPYYJSON_BUILD_TEST=OFF`/`CPPYYJSON_BUILD_BENCH=OFF`.

6. **"Reproducible" is not true in classic mode as planned.** Pinning cpp-yyjson alone leaves the vcpkg checkout, `yyjson` version/features, and triplet floating ([PLAN.md](/home/liuxiang/dev/nova-cpp-yyjson-vcpkg/PLAN.md:21)).
Fix: Pin and document the vcpkg repository commit plus triplet/features, or adopt a manifest baseline and narrow the stated reproducibility claim.

7. **Install-plus-configure validation cannot validate this header-only port.** With upstream tests disabled, installation compiles no cpp-yyjson consumer, so missing headers, broken exports, or lost transitive linkage can all pass ([PLAN.md](/home/liuxiang/dev/nova-cpp-yyjson-vcpkg/PLAN.md:25)).
Fix: Build and run at least `yyjson_demo` against the installed target, plus build the embedded `nova` target from Orion.

8. **The validation cannot prove FetchContent was bypassed.** Reused CMake caches or existing `_deps` sources can make configure succeed while masking wrong package selection, and the fallback branch is not tested at all.
Fix: Validate in fresh build directories with `--debug-find-pkg=cpp_yyjson`, assert `cpp_yyjson_DIR` is under vcpkg and `_deps/cpp-yyjson-src` is absent, then separately exercise the fallback branch.

9. **The proposed documentation is non-portable and conflates classic and manifest workflows.** It uses personal absolute paths, while passing an overlay during configure does not install an undeclared port in classic mode ([PLAN.md](/home/liuxiang/dev/nova-cpp-yyjson-vcpkg/PLAN.md:14)).
Fix: Document repository-relative overlay paths and separate explicit classic-mode installation from any manifest-mode configure instructions.

VERDICT: REVISE

### Codex response

Accepted all material findings. The revised plan scopes `cpp_yyjson` to the top-level `yyjson_demo` example instead of global `nova`, links the imported target directly, pins both vcpkg port and fallback to the same commit, disables fallback submodules, specifies vcpkg fixup/host dependencies/hash/copyright, narrows reproducibility claims to the documented vcpkg checkout + triplet, and strengthens validation with fresh build directories, debug package-finding evidence, absence of `_deps/cpp-yyjson-src`, `yyjson_demo` build/run, and an Orion embedded-nova focused build/configure check.

## Round 2 — Codex

The prior nine findings are substantially addressed, but three material gaps remain:

1. **The header-only port still lacks debug-package cleanup.** A normal two-config `vcpkg_cmake_install()` installs duplicate headers under `debug/include`; comparable header-only ports either set `VCPKG_BUILD_TYPE release` or remove that directory.
Fix: Add `set(VCPKG_BUILD_TYPE release)` or explicitly remove `${CURRENT_PACKAGES_DIR}/debug/include` after installation.

2. **Fallback isolation remains ambiguous.** After step 10 installs the port globally, a fresh build directory alone will still find it, so step 15 may never exercise FetchContent ([PLAN.md](/home/liuxiang/dev/nova-cpp-yyjson-vcpkg/PLAN.md:24)).
Fix: Force the fallback test with `-DCMAKE_DISABLE_FIND_PACKAGE_cpp_yyjson=ON` or use a genuinely separate vcpkg installed root.

3. **Classic-mode documentation still conflates installation and configuration.** The overlay is needed by the explicit `vcpkg install` command, but classic-mode CMake configuration does not need an overlay path after installation ([PLAN.md](/home/liuxiang/dev/nova-cpp-yyjson-vcpkg/PLAN.md:18)).
Fix: Document the overlay only on the classic install command; document CMake configuration separately with the toolchain and selected triplet.

VERDICT: REVISE

### Codex response

Accepted all three findings. The revised plan now requires `set(VCPKG_BUILD_TYPE release)` for the header-only overlay port, forces fallback smoke with `-DCMAKE_DISABLE_FIND_PACKAGE_cpp_yyjson=ON`, and separates classic-mode overlay installation from ordinary CMake configuration in the README instructions.

## Round 3 — Codex

All prior findings are addressed:

- Header-only port uses release-only packaging.
- Fallback testing explicitly disables package discovery.
- Classic installation and CMake configuration are correctly separated.
- Remaining SHA512 and network limitations are explicitly tracked with honest validation bounds.

No new material blockers found. The plan is sound enough to implement.

VERDICT: APPROVED

## Act 3 — Build

Implemented by Codex in worktree `/home/liuxiang/dev/nova-cpp-yyjson-vcpkg` on branch `build/cpp-yyjson-vcpkg`.

Validation outcomes:

- Installed overlay port with `/home/liuxiang/vcpkg/vcpkg install cpp-yyjson --overlay-ports=/home/liuxiang/dev/nova-cpp-yyjson-vcpkg/vcpkg-overlay-ports --triplet x64-linux`.
- Confirmed installed package files under `/home/liuxiang/vcpkg/installed/x64-linux/share/cpp_yyjson`, header at `/home/liuxiang/vcpkg/installed/x64-linux/include/cpp_yyjson.hpp`, and no duplicate `debug/include`.
- Configured `nova` in a fresh build directory with `--debug-find-pkg=cpp_yyjson`; `cpp_yyjson_DIR` resolved to `/home/liuxiang/vcpkg/installed/x64-linux/share/cpp_yyjson`, and no `_deps/cpp-yyjson-src` was generated.
- Built and ran `nova` `yyjson_demo` against the installed vcpkg target.
- Configured `orion` in a fresh build directory with `FETCHCONTENT_SOURCE_DIR_NOVA=/home/liuxiang/dev/nova-cpp-yyjson-vcpkg`; it used this local `nova` source and generated no `_deps/cpp-yyjson-src`.
- Built `orion` library target plus `twse_data_engine` and `taifex_stock_future_data_engine` focused targets.
- Forced fallback branch with `-DCMAKE_DISABLE_FIND_PACKAGE_cpp_yyjson=ON`; FetchContent used the pinned source without creating a `rapidjson` submodule, and fallback `yyjson_demo` built and ran. The fallback build produced upstream third-party header warnings under `cpp_yyjson.hpp`, but completed successfully.
