# DirtyJTAG Release Process

## 1. Release types

- **Engineering:** development snapshot; may include experimental features.
- **Release candidate:** feature-frozen candidate intended for final hardware validation.
- **Production:** approved build with validation evidence and archived checksums.

## 2. Preconditions

Before creating a release candidate:

1. Working tree is clean.
2. `west.yml` pins the intended Zephyr version.
3. `dirty_jtag/VERSION` matches the planned release identifier.
4. `host/requirements.txt` is reviewed.
5. Native tests and syntax checks pass.
6. CI builds every advertised firmware target.
7. `docs/VALIDATION_MATRIX.md` reflects current evidence.
8. Safety and known-limitations documentation are reviewed.

## 3. Local verification

```sh
python -m pip install -r host/requirements.txt
./scripts/test-native.sh
./scripts/check-zephyr-syntax.sh
./scripts/validate-release.sh
```

Optional local Zephyr builds:

```sh
make rpi-pico
make bluepill
make nodemcu-esp32s
```

## 4. Pull request requirements

A release PR should contain:

- version change;
- release notes;
- validation-matrix updates;
- calibration changes, if any;
- changed hardware profiles/descriptors;
- explicit list of experimental features.

The PR should not be merged if mandatory CI is failing.

## 5. CI artifacts

CI stages firmware files under per-board directories and generates `SHA256SUMS`.

Expected candidate outputs include:

```text
dist/
  rpi-pico/
  bluepill/
  olimex-stm32h103/
  stm32-min-dev/
  nodemcu-esp32s/
  RELEASE_METADATA.txt
  SHA256SUMS
```

The exact file extensions vary by board, but every staged board must include `zephyr.elf`.

## 6. Hardware smoke test

Before promotion from RC to production:

1. Flash the CI-produced binary, not a local rebuild.
2. Confirm USB/UART enumeration.
3. Confirm safe idle at boot.
4. Verify target voltage measurement.
5. Verify target power on/off behavior.
6. Verify fault detection.
7. If applicable, verify VPP interlock with an instrumented test fixture.
8. Execute at least one approved programming/debug smoke test for every production-supported protocol group.
9. Record results in the validation matrix or linked validation report.

## 7. Release metadata

Archive:

- git commit SHA;
- git tag;
- Zephyr revision;
- DirtyJTAG version;
- CI workflow run ID;
- artifact SHA-256 file;
- hardware revision;
- calibration revision;
- validation report/reviewer.

## 8. Versioning

Use semantic version components in `dirty_jtag/VERSION`. Avoid tying firmware identity to conversational revision letters.

Suggested pre-release progression:

```text
2.0.0-rc1
2.0.0-rc2
2.0.0
```

## 9. Rollback

If a production regression is discovered:

1. stop promotion of the affected artifact;
2. mark the release as withdrawn/deprecated;
3. identify the last qualified release;
4. reproduce the failure with archived artifacts;
5. fix on a branch;
6. repeat the full release process.

Do not silently replace binaries under an existing version identifier.
