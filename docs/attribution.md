# Attribution and licensing

5tratumFW is a modified version of [Bitaxe ESP-Miner](https://github.com/bitaxeorg/ESP-Miner), using release **v2.14.2**, upstream commit **64680f8a4da0b9a3b532051f0aa18429fcf04e82**, as its source baseline.

The firmware and web interface retain the upstream [GNU General Public License version 3](../LICENSE) and existing copyright notices. 5tratumFW changes include the Gamma-only startup guard, settings-preserving boot, interface design, operating-settings export, scheduler/power controls, OLED layout and optional MUX peer status receiver. The repository history retains the upstream baseline; Gamma additions are recorded in separate commits.

The Business Source License and restrictions used in other WillItMod projects are not the license of this GPL-derived firmware. Original dependency licenses remain applicable to their own components; do not remove their notices when redistributing builds or source.

## Included dependencies and assets

- [libsecp256k1](https://github.com/bitcoin-core/secp256k1) is retained as a pinned Git submodule with its own MIT license and authorship notices. Corresponding source packages include that pinned source.
- ESP-IDF components, LVGL, Angular, PrimeNG/PrimeIcons and other dependencies retain their respective package licenses/notices. The pinned lockfiles and component manifests identify the build inputs.
- The upstream Portfolio 6x8 OLED font is a generated LVGL adaptation of VileR's Oldschool PC Font Pack v2.2. Its converted font data retains CC BY-SA 4.0; the source font, conversion and license are recorded in [third-party notices](third-party-notices.md).
- The 5tratum emblem identifies this firmware project. The source license does not imply endorsement by upstream Bitaxe authors.

The upstream shared source contains definitions for other boards because the firmware derives from that baseline. The 5tratumFW startup guard and published compatibility contract still allow only Gamma PCB 601/602. Upstream factory/release tooling is not used to publish this fork's builds.

Full additional license notices for the converted Portfolio font, bundled PrimeIcons font and libbase58 are retained in [LICENSES](../LICENSES/). See [third-party notices](third-party-notices.md) for primary source URLs, copyright statements and asset provenance.

## Source and releases

The build package contains a corresponding source archive, license, build configuration and pinned submodule source together with `esp-miner.bin`, `www.bin`, a provenance manifest and SHA-256 hashes. See [build instructions](build.md). This repository publishes development source; a target compile or host test is not an installed-device test.
