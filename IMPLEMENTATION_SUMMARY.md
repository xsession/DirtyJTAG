# DirtyJTAG Production Readiness Update

Baseline reviewed: `feature/refactor_to_zephyr_based` at `17f1036accd8e32129545882e5996c796aee7127`.

This update implements the production-readiness work identified in the repository review:

- adds `host/requirements.txt` with the host runtime dependency;
- adds production, operator, release, validation and ADC-calibration documentation;
- adds `scripts/validate-release.sh`;
- updates `scripts/bootstrap.sh` to install host requirements;
- changes `dirty_jtag/VERSION` from conversational `rev-r` naming to `2.0.0.19-rc1` style;
- makes VPP/3.3 V/5 V safety windows configurable in Kconfig;
- adds build-time VTARGET/VPP/current gain and offset calibration settings;
- updates `rpi_pico_hal.c` to apply calibration and use configured safety thresholds;
- adds compile-time checks for invalid threshold ordering;
- updates CI to install host dependencies, run release validation, build the NodeMCU ESP-32S target, install the Xtensa toolchain, stage firmware artifacts, freeze the west manifest, record Python packages, and generate SHA-256 checksums;
- adds a production-documentation entry point to the root README.

## Apply

From the repository root on the reviewed branch:

```sh
git checkout feature/refactor_to_zephyr_based
git pull --ff-only
git apply --check DirtyJTAG-production-readiness.patch
git apply DirtyJTAG-production-readiness.patch
python -m pip install -r host/requirements.txt
./scripts/validate-release.sh
git add .
git commit -m "Add production readiness controls and documentation"
git push origin feature/refactor_to_zephyr_based
```

Because the GitHub app available to the assistant returned HTTP 403 for repository-content writes, the patch was not pushed automatically.
