"""Closed C6 output-configuration property, not an execution or timing proof.

The reviewed instruction contract is deliberately restrictive. It models the
two public pinMode calls and their mode-field stores, not arbitrary RISC-V.
Both PWM lookup outcomes are considered under the explicit normal-callee ABI
assumption. External captures have no admission path to audit evidence.
"""

import configparser
import hashlib
import json
import re
import shlex
import struct
from dataclasses import dataclass, field
from pathlib import Path


SOURCE = "src/platforms/esp/32/pin_esp32_native_impl.hpp"
STAGES = ("baseline", "old-output-mode-mutant", "restored")
PIN_MODE = "fl::pinMode(int, fl::PinMode)"
PRODUCER = "esp32_global_generic_producer"
FIND = "fl::pwm_state::findByPin(int)"
RELEASE = "fl::pwm_state::releaseChannel(fl::pwm_state::PwmPinState*)"
ASSUMPTION = "bound-normal-callees-preserve-riscv-abi-saved-registers"

# Root-captured GCC14/Arduino3.3.5/IDF5.5.1 encoded contracts. Only call
# relocations are decoded separately; every branch, register and SP operand
# remains fixed. The sole semantic alternative is the Output-mode cell.
_PIN = bytes.fromhex(
    "797122d426d24ad006d600182a89ae84f53d11c15d37f94763ec27034a8605458145232204fe232404fe232604fe97e0fffde7808003232ca4fc232eb4fc8947638ff40263cc970085479dc06381f402b250225492540259456182808d47e399f4fe85472320f4fe2324f4fe21a08d472320f4fe130584fdef001045d1bf85472320f4fe2322f4fef5b7"
)
# Actual one-assignment source mutant: GCC reuses a5 from its mode-dispatch
# comparison instead of emitting the baseline's separate Output constant.
_PIN_REUSED = bytes.fromhex(
    "797122d426d24ad006d600182a89ae84f53d11c15d37f94763ec27034a8605458145232204fe232404fe232604fe97e0fffde7808003232ca4fc232eb4fc89476381f40463cc9700054795c06388e402b250225492540259456182808d47e399f4fe85472320f4fe2324f4fe19a02320e4fe130584fdef007045d9bf2320f4fecdbf85472320f4fe2322f4feddb7"
)
_PRODUCER_PREFIX = bytes.fromhex(
    "411106c622c44ac0000826c237070342b707034283a707130325c712370703428325471337070342032687123707034283264712aa973707034203270712ae97b297b697ba97854537f780400945232ef796ef10f05785450d45ef107057"
)
_FIND = bytes.fromhex(
    "411122c406c60008b7f78040aa8603a5879919cd0147d1452146b307b702aa9703a80700630ad8000507e318c7fe0145b2402244410182803e85ddbf"
)
_RELEASE = bytes.fromhex(
    "31c1011122cc26ca06ce0010aa840841634505028145d5351305c4fe453dfd579cc023a20400239404001305c4fe553d6244f240d244056149bff2406244d244056182808280"
)


@dataclass(frozen=True)
class SDKMode:
    public_output_mode: int
    input_output_mode: int
    output_mode: int
    input_mode: int
    mode_offset: int
    config_size: int


@dataclass(frozen=True)
class ModeObservation:
    pin: int
    public_mode: int
    configured_mode: int
    pwm_lookup_nonnull: bool
    call_address: int
    store_address: int
    gpio_config_address: int


@dataclass
class ModeResult:
    authentication_errors: list[str] = field(default_factory=list)
    decode_errors: list[str] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)
    producer_calls_bound: bool = False
    gpio_config_stores_bound: bool = False
    sdk: SDKMode | None = None
    observations: list[ModeObservation] = field(default_factory=list)
    counterexamples: list[ModeObservation] = field(default_factory=list)
    assumptions: list[str] = field(default_factory=list)


def old_output_mode_mutant(source: bytes) -> bytes:
    statement = b"io_conf.mode = GPIO_MODE_INPUT_OUTPUT;"
    if source.count(statement) != 1:
        raise ValueError("missing or ambiguous fixed Output assignment")
    return source.replace(statement, b"io_conf.mode = GPIO_MODE_OUTPUT;", 1)


def validate_stage_sequence(stages: tuple[str, ...]) -> None:
    if stages != STAGES:
        raise ValueError("invalid fixed C6 mode stages")


def _signed(value: int, bits: int) -> int:
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def _jal(data: bytes, offset: int, base: int) -> int:
    word = int.from_bytes(data[offset : offset + 4], "little")
    if word & 0xFFF != 0x0EF:  # JAL ra, exact linked call form
        raise ValueError("unsupported direct call")
    immediate = (
        (word >> 31) << 20
        | ((word >> 12) & 255) << 12
        | ((word >> 20) & 1) << 11
        | ((word >> 21) & 1023) << 1
    )
    return base + offset + _signed(immediate, 21)


def _cjal(data: bytes, offset: int, base: int) -> int:
    word = int.from_bytes(data[offset : offset + 2], "little")
    if word & 0xE003 != 0x2001:
        raise ValueError("unsupported compressed call")
    immediate = (
        ((word >> 12) & 1) << 11
        | ((word >> 11) & 1) << 4
        | ((word >> 9) & 3) << 8
        | ((word >> 8) & 1) << 10
        | ((word >> 7) & 1) << 6
        | ((word >> 6) & 1) << 7
        | ((word >> 3) & 7) << 1
        | ((word >> 2) & 1) << 5
    )
    return base + offset + _signed(immediate, 12)


def _word(entry: dict) -> int:
    raw = bytes.fromhex(entry["bytes_hex"])
    if entry["kind"] != "O" or entry["size"] != 4 or len(raw) != 4:
        raise ValueError("invalid SDK object extent")
    value = int.from_bytes(raw, "little")
    if entry.get("uint32_le", value) != value:
        raise ValueError("SDK display/raw disagreement")
    return value


def _function(symbols: dict, name: str) -> tuple[int, bytes]:
    item = symbols[name]
    raw = bytes.fromhex(item["bytes_hex"])
    if (
        item["kind"] != "F"
        or item["section"] != ".flash.text"
        or type(item["address"]) is not int
        or item["address"] & 1
        or item["size"] != len(raw)
        or not raw
    ):
        raise ValueError("invalid selected function extent")
    return item["address"], raw


def _equal_except(
    actual: bytes, expected: bytes, holes: tuple[tuple[int, int], ...]
) -> None:
    if len(actual) != len(expected):
        raise ValueError("unsupported instruction extent")
    normalized = bytearray(actual)
    for start, width in holes:
        normalized[start : start + width] = expected[start : start + width]
    if normalized != expected:
        raise ValueError("unsupported encoded instruction contract")


@dataclass(frozen=True)
class _ModePath:
    configured_mode: int
    store_offset: int
    call_offset: int


def _c_li(data: bytes, offset: int, register: int) -> int:
    word = int.from_bytes(data[offset : offset + 2], "little")
    if word & 0xE003 != 0x4001 or (word >> 7) & 31 != register:
        raise ValueError("unsupported decoded constant register")
    return _signed(((word >> 2) & 31) | ((word >> 12) & 1) << 5, 6)


def _mode_path(code: bytes, public_mode: int) -> _ModePath:
    if len(code) == len(_PIN):
        mode_offset, store_offset, call_offset = 0x6E, 0x70, 0x78
        template = _PIN
    elif len(code) == len(_PIN_REUSED):
        mode_offset, store_offset, call_offset = 0x3E, 0x7C, 0x76
        template = _PIN_REUSED
    else:
        raise ValueError("unsupported encoded pinMode extent")
    _equal_except(
        code,
        template,
        ((0x10, 2), (0x14, 2), (0x2E, 8), (mode_offset, 2), (call_offset, 4)),
    )
    configured = _c_li(code, mode_offset, 15)  # actual a5, not variant/stage name
    if configured not in (2, 3):
        raise ValueError("unsupported decoded Output value")
    if template is _PIN_REUSED:
        # Actual publicOutput=1 path: beq s1,a5 false; blt a5,s1 false;
        # beqz s1 false; beq s1,a4 true -> +7c SW a5,-32(s0).
        # The exact intervening bytes modify a4 only; a5 reaches that store.
        selector = _c_li(code, 0x48, 14)
        if (
            public_mode == configured
            or configured < public_mode
            or public_mode == 0
            or public_mode != selector
        ):
            raise ValueError("decoded public Output takes another mode branch")
    return _ModePath(configured, store_offset, call_offset)


def classify_capture(capture: dict) -> ModeResult:
    result = ModeResult()
    try:
        if capture["machine"] != 243:
            raise ValueError("not the selected RISC-V ELF")
        symbols = capture["symbols"]
        base, code = _function(symbols, PIN_MODE)
        caller, producer = _function(symbols, PRODUCER)
        find, find_code = _function(symbols, FIND)
        release, release_code = _function(symbols, RELEASE)
        config, _ = _function(symbols, "gpio_config")
        exports = (
            "selection_public_pin_mode_output",
            "selection_gpio_mode_input_output",
            "selection_gpio_mode_output",
            "selection_gpio_mode_input",
            "selection_gpio_mode_offset",
            "selection_gpio_config_size",
        )
        if len({symbols[name]["address"] for name in exports}) != 6:
            raise ValueError("ambiguous SDK exports")
        sdk = SDKMode(*(_word(symbols[name]) for name in exports))
        # SDK mode is a bit set: push-pull Output plus input-enable. Changing
        # the expected export to old Output cannot manufacture a positive.
        if (
            sdk.public_output_mode != 1
            or sdk.input_mode != 1
            or sdk.output_mode != 2
            or sdk.input_output_mode != sdk.input_mode | sdk.output_mode
            or sdk.mode_offset != 8
            or sdk.config_size != 24
        ):
            raise ValueError("unsupported SDK/public mode contract")
        absolute = capture["absolute_symbols"]
        if set(absolute) != {"__ashldi3"}:
            raise ValueError("missing or ambiguous SDK ABS function")
        symbol = absolute["__ashldi3"]
        _, address, size, info, other, index = struct.unpack(
            "<IIIBBH", bytes.fromhex(symbol["symbol_bytes_hex"])
        )
        if (
            index != 65521
            or info != 18
            or other != 0
            or size != 40
            or symbol["section_index"] != index
            or symbol["symbol_info"] != info
            or symbol["symbol_other"] != other
            or symbol["address"] != address
            or symbol["size"] != size
            or symbol["kind"] != "F"
            or symbol["section"] != "*ABS*"
        ):
            raise ValueError("SDK ABS raw symbol disagreement")
        binding = capture["rom_linker_binding"]
        if (
            binding["statement"] != f"__ashldi3 = 0x{address:x};"
            or binding["sha256"]
            != "6f601e0d26b1c9e46160edab23fad21d73fe3a1a6e0deb616906604609a0c680"
            or not binding["path"].endswith("/esp32c6/ld/esp32c6.rom.libgcc.ld")
        ):
            raise ValueError("unreviewed SDK ROM binding")
        result.sdk = sdk
    except (KeyError, TypeError, ValueError, OverflowError):
        result.authentication_errors = ["c6-mode-unbound-elf-or-sdk"]
        return result
    try:
        mode_path = _mode_path(code, sdk.public_output_mode)
        # Both optional PWM paths have their real targets bound; the explicit
        # normal-return ABI assumption preserves s0/s1/s2 across these calls.
        if _cjal(code, 0x10, base) != find or _cjal(code, 0x14, base) != release:
            raise ValueError("PWM call target differs")
        if find_code != _FIND or release_code != _RELEASE:
            raise ValueError("unreviewed normal-callee body")
        auipc, jalr = struct.unpack_from("<II", code, 0x2E)
        if auipc & 0xFFF != 0x097 or jalr & 0xFFFFF != 0x080E7:
            raise ValueError("unsupported ROM shift call")
        rom = (
            base + 0x2E + _signed(auipc & 0xFFFFF000, 32) + _signed(jalr >> 20, 12)
        ) & ~1
        if rom != address:
            raise ValueError("unreviewed SDK ROM shift target")
        if _jal(code, mode_path.call_offset, base) != config:
            raise ValueError("gpio_config call target differs")
        prefix = producer[: len(_PRODUCER_PREFIX)]
        _equal_except(prefix, _PRODUCER_PREFIX, ((0x52, 4), (0x5A, 4)))
        if _jal(producer, 0x52, caller) != base or _jal(producer, 0x5A, caller) != base:
            raise ValueError("public producer calls another entry")
        # Every exported load address in the fixed caller prefix is raw-bound.
        for upper, lower, name in (
            (0x10, 0x14, exports[5]),
            (0x0C, 0x18, exports[4]),
            (0x1C, 0x20, exports[0]),
            (0x24, 0x28, exports[1]),
            (0x2C, 0x30, exports[2]),
            (0x36, 0x3A, exports[3]),
        ):
            high = int.from_bytes(producer[upper : upper + 4], "little") & 0xFFFFF000
            load = int.from_bytes(producer[lower : lower + 4], "little")
            if high + _signed(load >> 20, 12) != symbols[name]["address"]:
                raise ValueError("producer SDK export load unbound")
        result.producer_calls_bound = result.gpio_config_stores_bound = True
        result.observations = [
            ModeObservation(
                pin,
                sdk.public_output_mode,
                mode_path.configured_mode,
                pwm,
                caller + offset,
                base + mode_path.store_offset,
                config,
            )
            for pin, offset in ((2, 0x52), (3, 0x5A))
            for pwm in (False, True)
        ]
        result.assumptions = [ASSUMPTION]
        result.counterexamples = [
            item
            for item in result.observations
            if item.configured_mode != sdk.input_output_mode
        ]
        result.errors = ["output-mode-mismatch"] if result.counterexamples else []
    except (KeyError, TypeError, ValueError, struct.error):
        result.decode_errors = ["c6-mode-unsupported-encoded-flow"]
    return result


def extract_capture(elf: bytes, symbol_text: str, rom_linker: Path) -> dict:
    """Read only the retained ELF, cross-binding selected SDK nm identities.

    The nm text supplies demangled names; raw ELF symbols, section storage and
    PT_LOAD bounds independently authorize every selected byte interval. ROM
    is an ABS symbol plus frozen vendor linker statement, not invented bytes.
    """
    if (
        elf[:7] != b"\x7fELF\x01\x01\x01"
        or len(elf) < 52
        or struct.unpack_from("<H", elf, 18)[0] != 243
    ):
        raise ValueError("unsupported selected ELF32 RISC-V")
    phoff, shoff = struct.unpack_from("<II", elf, 28)
    phsize, phcount, shsize, shcount, shnames = struct.unpack_from("<HHHHH", elf, 42)
    if (
        phsize != 32
        or shsize != 40
        or shnames >= shcount
        or phoff + phsize * phcount > len(elf)
        or shoff + shsize * shcount > len(elf)
    ):
        raise ValueError("malformed selected ELF tables")
    sections = [
        struct.unpack_from("<IIIIIIIIII", elf, shoff + i * shsize)
        for i in range(shcount)
    ]
    programs = [
        struct.unpack_from("<IIIIIIII", elf, phoff + i * phsize) for i in range(phcount)
    ]

    def storage(section: tuple) -> bytes:
        if section[4] + section[5] > len(elf):
            raise ValueError("ELF storage outside file")
        return elf[section[4] : section[4] + section[5]]

    names = storage(sections[shnames])
    reservation = _sdk_reservation(elf, sections, programs, names, rom_linker.parent)
    symbols = []
    for section in sections:
        if section[1] != 2:
            continue
        if section[9] != 16 or section[6] >= shcount or section[5] % 16:
            raise ValueError("malformed actual symbol table")
        strings = storage(sections[section[6]])
        for offset in range(0, section[5], 16):
            position = section[4] + offset
            raw = elf[position : position + 16]
            name, address, size, info, other, index = struct.unpack("<IIIBBH", raw)
            label = strings[name:].split(b"\0", 1)[0].decode("ascii")
            symbols.append((label, address, size, info, other, index, position, raw))
    needed = {
        PIN_MODE,
        PRODUCER,
        FIND,
        RELEASE,
        "gpio_config",
        "selection_public_pin_mode_output",
        "selection_gpio_mode_input_output",
        "selection_gpio_mode_output",
        "selection_gpio_mode_input",
        "selection_gpio_mode_offset",
        "selection_gpio_config_size",
        "__ashldi3",
    }
    selected = {}
    for line in symbol_text.splitlines():
        match = re.fullmatch(
            r"([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([A-Za-z])\s+(.+)", line.strip()
        )
        if match and match[4] in needed:
            label = match[4]
            if label in selected:
                raise ValueError("ambiguous actual selected nm symbol")
            address, size = int(match[1], 16), int(match[2], 16)
            candidates = [item for item in symbols if item[1:3] == (address, size)]
            if len(candidates) != 1:
                raise ValueError("nm/raw ELF symbol ambiguity")
            item = candidates[0]
            if label == "__ashldi3":
                if item[0] != label or item[5] != 65521:
                    raise ValueError("missing actual ABS ROM function")
                selected[label] = {
                    "address": address,
                    "size": size,
                    "section_index": item[5],
                    "symbol_info": item[3],
                    "symbol_other": item[4],
                    "symbol_table_file_offset": item[6],
                    "symbol_bytes_hex": item[7].hex(),
                    "kind": "F",
                    "section": "*ABS*",
                }
                continue
            if item[5] >= shcount:
                raise ValueError("selected symbol has no stored section")
            section = sections[item[5]]
            function = label in {PIN_MODE, PRODUCER, FIND, RELEASE, "gpio_config"}
            if (
                section[1] != 1
                or not section[2] & 2
                or (function and not section[2] & 4)
                or item[3] & 15 != (2 if function else 1)
                or not size
                or not section[3] <= address < address + size <= section[3] + section[5]
            ):
                raise ValueError("selected symbol outside allocated code/data extent")
            overlaps = [
                candidate
                for candidate_index, candidate in enumerate(sections)
                if candidate[2] & 2
                and candidate[5]
                and candidate[3] < address + size
                and address < candidate[3] + candidate[5]
                and not (
                    reservation
                    and function
                    and item[5] == reservation["text_index"]
                    and candidate_index == reservation["dummy_index"]
                )
            ]
            loads = [
                program
                for program_index, program in enumerate(programs)
                if program[0] == 1
                and (program[4] or program[5])
                and program[2] < address + size
                and address < program[2] + max(program[4], program[5])
                and not (
                    reservation
                    and function
                    and item[5] == reservation["text_index"]
                    and program_index == reservation["reservation_load_index"]
                )
            ]
            if len(overlaps) != 1 or len(loads) != 1:
                raise ValueError("ambiguous selected section/PT_LOAD")
            position = section[4] + address - section[3]
            load = loads[0]
            if (
                load[4] > load[5]
                or not load[2] <= address < address + size <= load[2] + load[4]
                or address + size > load[2] + load[5]
                or load[1] + load[4] > len(elf)
                or (function and not load[6] & 1)
                or position != load[1] + address - load[2]
                or position + size > len(elf)
            ):
                raise ValueError("section/load file storage disagrees")
            raw = elf[position : position + size]
            section_name = names[section[0] :].split(b"\0", 1)[0].decode("ascii")
            entry = {
                "address": address,
                "size": size,
                "section": section_name,
                "file_offset": position,
                "bytes_hex": raw.hex(),
                "kind": "F" if function else "O",
            }
            if not function:
                entry["uint32_le"] = int.from_bytes(raw, "little")
            selected[label] = entry
    if set(selected) != needed:
        raise ValueError("missing actual selected symbols")
    linker_bytes = rom_linker.read_bytes()
    statements = re.findall(
        r"^__ashldi3\s*=\s*0x([0-9a-fA-F]+);\s*$", linker_bytes.decode(), re.MULTILINE
    )
    if len(statements) != 1:
        raise ValueError("ambiguous SDK ROM linker definition")
    result = {
        "machine": 243,
        "elf_sha256": hashlib.sha256(elf).hexdigest(),
        "symbols": {
            name: item for name, item in selected.items() if name != "__ashldi3"
        },
        "absolute_symbols": {"__ashldi3": selected["__ashldi3"]},
        "rom_linker_binding": {
            "path": str(rom_linker),
            "sha256": hashlib.sha256(linker_bytes).hexdigest(),
            "statement": f"__ashldi3 = 0x{int(statements[0], 16):x};",
        },
    }
    if reservation:
        result["sdk_reservation"] = reservation
    return result


_LAYOUT_HASHES = {
    "memory.ld": "56f760762733ac2a37ff4b738f5a066d576f3f67c9add19966466c5f3a515155",
    "sections.ld": "31726567564a8bea7f39930cce429eb3134f870be25c974fead75e93e43fc94f",
}


def _sdk_reservation(
    elf: bytes,
    sections: list[tuple],
    programs: list[tuple],
    names: bytes,
    directory: Path,
) -> dict | None:
    """Qualify one SDK NOLOAD reservation, not a runtime image mapping.

    The ordinary ambiguity checks remain in force for all other descriptors.
    This exception is derived from frozen SDK linker scripts, exact geometry,
    zero file padding and the complete real-data mapping of its one RW LOAD.
    """

    def section_name(section: tuple) -> str:
        return names[section[0] :].split(b"\0", 1)[0].decode("ascii")

    named = [
        (index, section)
        for index, section in enumerate(sections)
        if section_name(section) == ".flash_rodata_dummy"
    ]
    if not named:
        return None
    if len(named) != 1:
        raise ValueError("ambiguous SDK NOLOAD reservation")
    scripts = {}
    text_scripts = {}
    for filename, expected in _LAYOUT_HASHES.items():
        path = directory / filename
        raw = path.read_bytes()
        if path.is_symlink() or hashlib.sha256(raw).hexdigest() != expected:
            raise ValueError("unbound C6 SDK reservation script")
        scripts[filename] = {"path": str(path), "sha256": expected}
        text_scripts[filename] = raw.decode("ascii")
    origins = [
        re.search(
            rf"{region}\s+\({flags}\)\s*:\s*org\s*=\s*(0x[0-9a-fA-F]+)",
            text_scripts["memory.ld"],
        )
        for region, flags in (("irom_seg", "RX"), ("drom_seg", "R"))
    ]
    page = re.search(
        r"\.\s*=\s*ALIGN\((0x[0-9a-fA-F]+)\)\s*\+\s*(0x[0-9a-fA-F]+);",
        text_scripts["sections.ld"],
    )
    if any(match is None for match in origins) or page is None:
        raise ValueError("unsupported bound SDK region expressions")
    origin = int(origins[0][1], 16)
    if int(origins[1][1], 16) != origin:
        raise ValueError("SDK code/rodata regions do not alias")
    page_size, header = int(page[1], 16), int(page[2], 16)

    def unique(label: str) -> tuple[int, tuple]:
        matches = [
            (index, section)
            for index, section in enumerate(sections)
            if section_name(section) == label
        ]
        if len(matches) != 1:
            raise ValueError("ambiguous SDK reservation companion")
        return matches[0]

    text_index, text = unique(".flash.text")
    dummy_index, dummy = named[0]
    app_index, app = unique(".flash.appdesc")
    alignment = text[8]
    if not alignment or alignment & (alignment - 1) or page_size & (page_size - 1):
        raise ValueError("invalid SDK reservation alignment")
    aligned = (origin + alignment - 1) & ~(alignment - 1)
    dummy_end = ((aligned + text[5] + page_size - 1) & ~(page_size - 1)) + header
    if (
        text[1:4] != (1, 6, origin)
        or not text[5]
        or dummy[1:4] != (8, 3, origin)
        or dummy[5] != dummy_end - origin
        or dummy[8] != 1
        or app[1] != 1
        or app[2] != 2
        or app[3] != dummy_end
        or not app[5]
    ):
        raise ValueError("unqualified SDK reservation geometry")
    rx = [
        (index, program)
        for index, program in enumerate(programs)
        if program[0] == 1
        and program[1:7] == (text[4], origin, origin, text[5], text[5], 5)
    ]
    rw = [
        (index, program)
        for index, program in enumerate(programs)
        if program[0] == 1 and program[2] == origin and program[6] == 6
    ]
    if len(rx) != 1 or len(rw) != 1:
        raise ValueError("unqualified SDK reservation LOAD owners")
    load_index, load = rw[0]
    if (
        load[3] != origin
        or load[1] != dummy[4]
        or load[4] != load[5]
        or load[4] <= dummy[5]
        or load[1] + load[4] > len(elf)
        or app[4] != load[1] + dummy[5]
        or any(elf[load[1] : load[1] + dummy[5]])
    ):
        raise ValueError("SDK reservation padding/LOAD disagrees")
    real = []
    for index, section in enumerate(sections):
        if index in (text_index, dummy_index) or not section[2] & 2 or not section[5]:
            continue
        if section[3] < origin + load[5] and origin < section[3] + section[5]:
            if (
                section[1] != 1
                or section[2] & 4
                or section[3] < dummy_end
                or section[3] + section[5] > origin + load[4]
                or section[4] != load[1] + section[3] - origin
            ):
                raise ValueError("incoherent real data inside SDK reservation LOAD")
            real.append((section[3], section[3] + section[5], index))
    real.sort()
    if not real or real[0][2] != app_index:
        raise ValueError("missing first real SDK appdesc mapping")
    previous = dummy_end
    for start, end, _ in real:
        if start < previous or any(
            elf[load[1] + previous - origin : load[1] + start - origin]
        ):
            raise ValueError("overlapping or nonzero real-data mapping gap")
        previous = end
    if any(elf[load[1] + previous - origin : load[1] + load[4]]):
        raise ValueError("nonzero unowned SDK LOAD tail")
    return {
        "kind": "pinned-c6-noload-rodata-reservation",
        "image_mapping_qualified": False,
        "text_index": text_index,
        "dummy_index": dummy_index,
        "reservation_load_index": load_index,
        "origin": origin,
        "reserved_size": dummy[5],
        "scripts": scripts,
        "conditional_elf_only": True,
        "text_section_header": list(text),
        "reservation_section_header": list(dummy),
        "reservation_load_header": list(load),
        "real_data_sections": [
            {"index": index, "header": list(sections[index])} for _, _, index in real
        ],
    }


def validate_stage_analysis(stage: str, result: ModeResult) -> None:
    if (
        stage not in STAGES
        or result.authentication_errors
        or result.decode_errors
        or result.sdk is None
    ):
        raise ValueError("unbound C6 mode stage")
    if (
        not result.producer_calls_bound
        or not result.gpio_config_stores_bound
        or result.assumptions != [ASSUMPTION]
    ):
        raise ValueError("missing actual C6 mode route")
    if len(result.observations) != 4 or {
        (item.pin, item.pwm_lookup_nonnull) for item in result.observations
    } != {(2, False), (2, True), (3, False), (3, True)}:
        raise ValueError("incomplete or duplicate PWM branches")
    expected = (
        result.sdk.output_mode if stage == STAGES[1] else result.sdk.input_output_mode
    )
    if any(
        item.public_mode != result.sdk.public_output_mode
        or item.configured_mode != expected
        for item in result.observations
    ):
        raise ValueError("contradictory C6 mode observation")
    wrong = [
        item
        for item in result.observations
        if item.configured_mode != result.sdk.input_output_mode
    ]
    if (
        result.errors != (["output-mode-mismatch"] if wrong else [])
        or result.counterexamples != wrong
    ):
        raise ValueError("C6 mode witness differs from actual observations")


_FIXTURES = "ci/fixtures/software_spi_audit"
_EXAMPLE = "examples/AutoResearch/AutoResearch.ino"
_HOOK = "examples/AutoResearch/AutoResearchStartupHook.cpp"
_DEFINES = ("EXPECT_FORCED=1", "EXPECT_NO_PINMAP=0", "FASTLED_FORCE_SOFTWARE_PINS=1")


def producer_sources(root: Path) -> dict[str, bytes]:
    return {
        _EXAMPLE: (root / _FIXTURES / "esp32_c6_mode_producer.cpp.txt").read_bytes(),
        _HOOK: (root / _FIXTURES / "esp32_c6_mode_startup.cpp.txt").read_bytes(),
    }


def _planned_database(info: dict) -> list[dict]:
    path = Path(info["prog_path"]).parent / "compile_commands.raw.json"
    rows = json.loads(path.read_text())
    if not isinstance(rows, list) or not rows:
        raise ValueError("missing selected planned compile database")
    return rows


def _verify_global_flags(info: dict) -> None:
    project = Path(info["prog_path"]).parents[3]
    ini = project / "platformio.ini"
    parser = configparser.ConfigParser(
        interpolation=None, inline_comment_prefixes=("#", ";"), strict=True
    )
    try:
        parser.read_string(ini.read_text())
        # The generated closed profile declares flags on this exact section;
        # implicit DEFAULT inheritance is outside its reviewed format.
        if parser.defaults():
            raise ValueError("unreviewed inherited generated INI flags")
        tokens = shlex.split(
            parser.get("env:esp32c6", "build_flags", raw=True), comments=True
        )
    except configparser.Error as error:
        raise ValueError("invalid selected generated INI flags") from error
    _require_fixed_defines(tokens)
    for row in _planned_database(info):
        tokens = row.get("arguments")
        if tokens is None:
            tokens = shlex.split(row["command"])
        _require_fixed_defines(tokens)


def _require_fixed_defines(tokens: list[str]) -> None:
    required = {define.split("=", 1)[0] for define in _DEFINES}
    normalized: list[tuple[str, str]] = []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        index += 1
        # This fixed profile never forwards opaque preprocessor switches.
        # Ordinary unrelated -Wp options remain outside the required-key set.
        if token == "-Xpreprocessor":
            raise ValueError("unreviewed forwarded preprocessor options")
        if token.startswith("-Wp,"):
            for part in token[4:].split(","):
                operand = part[2:] if part.startswith(("-D", "-U")) else part
                name = re.match(r"[A-Za-z_]\w*", operand)
                if name and name[0] in required:
                    raise ValueError("forwarded required-key preprocessor override")
        operation = token[:2]
        if operation not in ("-D", "-U"):
            continue
        operand = token[2:]
        if not operand:
            if index == len(tokens):
                raise ValueError("missing preprocessor operand")
            operand = tokens[index]
            index += 1
        if operand.startswith("-"):
            raise ValueError("invalid preprocessor operand")
        name = re.match(r"[A-Za-z_]\w*", operand)
        if name and name[0] in required:
            if operation == "-U":
                raise ValueError("required macro cannot be undefined")
            normalized.append((name[0], "-D" + operand))
    for define in _DEFINES:
        key = define.split("=", 1)[0]
        matches = [token for name, token in normalized if name == key]
        if matches != ["-D" + define]:
            raise ValueError(
                "fixed global definition absent/conflicting in selected flags"
            )


def compile_fixed(workspace: Path, saved: Path, environment: dict) -> dict:

    from ci import software_spi_init_proof as native

    saved.mkdir(parents=True, exist_ok=True)
    command = [
        "bash",
        "compile",
        "esp32c6",
        "--examples",
        "AutoResearch",
        "--defines",
        ",".join(_DEFINES),
        "-v",
        "--build-info",
        str(saved / "build-info.json"),
    ]
    (saved / "invocation.json").write_text(
        json.dumps(
            {"argv": command, "cwd": str(workspace), "complete_executed_argv": False},
            indent=2,
        )
        + "\n"
    )
    completed = native._run(
        command, workspace, saved / "build.log", build=True, environment=environment
    )
    # A failed compile cannot lend stale previous-stage database/ELF metadata.
    if completed.returncode != 0:
        raise ValueError(f"C6 vendor build failed; retained {saved}")
    route = [
        str(Path(environment["FBUILD_EXECUTABLE"]).resolve()),
        str(workspace / ".build/fbuild/esp32c6"),
        "build",
        "-e",
        "esp32c6",
        "-v",
    ]
    if "Running: " + " ".join(route) not in completed.stdout:
        raise ValueError("unqualified actual C6 submission route")
    info = json.loads((saved / "build-info.json").read_text())["esp32c6"]
    if (
        info.get("env") != "esp32c6"
        or info["board"] != "ESP32_C6_DEVKITC_1"
        or info["platform"] != "espressif32"
        or "-DF_CPU=160000000L" not in info["cxx_flags"]
        or "-march=rv32imac_zicsr_zifencei" not in info["cxx_flags"]
    ):
        raise ValueError("unsupported fixed C6 ISA/CPU/board")
    return info


def sdk_roots(info: dict) -> tuple[Path, Path]:
    # Current fbuild reports includes=[] on ESP32; bind the actual framework
    # paths in every planned compile row instead of guessing from that field.
    raw = json.dumps(_planned_database(info))
    roots = {
        Path(match).resolve()
        for match in re.findall(r"(/[^\s\"']*/esp32-3\.3\.5)(?=/)", raw)
    }
    if len(roots) != 1:
        raise ValueError("missing or ambiguous planned C6 framework tree")
    framework = roots.pop()
    if not (
        framework / "tools/esp32-arduino-libs/esp32c6/ld/esp32c6.rom.libgcc.ld"
    ).is_file():
        raise ValueError("unreviewed C6 SDK linker binding")
    tools = Path(info["cc_path"]).resolve().parents[2]
    if "riscv32-esp-elf-14.2.0_20260121" not in str(tools):
        raise ValueError("unreviewed selected C6 compiler package")
    return framework, tools


def planned_configuration(info: dict) -> dict[str, str]:
    _verify_global_flags(info)
    project = Path(info["prog_path"]).parents[3]
    files = {
        project / "platformio.ini",
        Path(info["prog_path"]).parent / "compile_commands.raw.json",
    }
    files.update(project.rglob("*.rsp"))
    files.update(project.rglob("compile_commands.raw.json"))
    result = {}
    for path in sorted(files):
        if not path.is_file() or path.is_symlink():
            raise ValueError("unsafe selected C6 configuration input")
        result[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def capture_retained(
    workspace: Path, saved: Path, info: dict, environment: dict
) -> dict:

    from ci import software_spi_init_proof as native

    elf = saved / "firmware.elf"
    before = hashlib.sha256(elf.read_bytes()).hexdigest()
    commands = {
        "symbols": [info["nm_path"], "-C", "-S", "--defined-only", str(elf)],
        "disassembly": [info["objdump_path"], "-d", "-C", "-z", str(elf)],
        "layout": [info["readelf_path"], "-W", "-S", "-l", "-s", str(elf)],
    }
    outputs = {}
    for name, command in commands.items():
        completed = native._run(
            command,
            workspace,
            saved / (name + ".txt"),
            build=False,
            environment=environment,
        )
        if completed.returncode != 0:
            raise ValueError("C6 SDK retained-ELF inspection failed")
        outputs[name] = completed.stdout
    framework, _ = sdk_roots(info)
    linker = framework / "tools/esp32-arduino-libs/esp32c6/ld/esp32c6.rom.libgcc.ld"
    capture = extract_capture(elf.read_bytes(), outputs["symbols"], linker)
    if "sdk_reservation" not in capture:
        raise ValueError("actual fixed C6 SDK reservation was not qualified")
    if hashlib.sha256(elf.read_bytes()).hexdigest() != before:
        raise ValueError("retained ELF changed during actual inspection")
    (saved / "inspection-commands.json").write_text(
        json.dumps(commands, indent=2) + "\n"
    )
    (saved / "classifier-capture.json").write_text(json.dumps(capture, indent=2) + "\n")
    (saved / "esp32c6.rom.libgcc.ld").write_bytes(linker.read_bytes())
    for filename in _LAYOUT_HASHES:
        (saved / filename).write_bytes((linker.parent / filename).read_bytes())
    (saved / "sdk-layout-diagnostics.json").write_text(
        json.dumps(capture["sdk_reservation"], indent=2) + "\n"
    )
    return capture
