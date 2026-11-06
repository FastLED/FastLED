"""Fixed classic Xtensa Output-only conditional ELF slice classifier.

This module cannot create or admit managed evidence. No image/BIN mapping,
execution, other-mode preservation, IRQ/cache/timing or general memory safety
is established. The theorem is conditional on arrival at the selected CALL8
sites and valid nonaliasing prefix-load/volatile-sink storage. Authenticating
the sink pointer word does NOT prove its target object or safe prefix arrival.
Its normal-call premise includes windowed register/stack
preservation and memory nonaliasing: PWM lookup/release and ROM shift do not
arbitrarily overwrite the caller's config frame; the bound memset contract
writes exactly its specified 16 bytes at SP16, not the whole 24-byte config.

Instruction references: Cadence Xtensa ISA Summary (BEQZ.N p352, S32I.N
p594, ADDI.N p333, MOVI.N p506-507); Espressif WindowedOption.tex and
InstructionsRI16.tex (CALL8 and forced-negative PC-relative L32R).
"""

import struct
from dataclasses import dataclass, field


PIN = "fl::pinMode(int, fl::PinMode)"
PRODUCER = "esp32_global_generic_producer"
FIND = "fl::pwm_state::findByPin(int)"
RELEASE = "fl::pwm_state::releaseChannel(fl::pwm_state::PwmPinState*)"
ASSUMPTION = "xtensa-windowed-CALL8-normal-callee-ABI-and-bound-memset-buffer-contract"

# Fixed actual profile, NOT an installed-SDK qualification. A future managed
# factory must bind these actual source identities before admitting a record.
SDK_SOURCE_CONTRACT = {
    "core": "esp32-core-3.3.11",
    "core_isa_sha256": "8171435d7244c032535b495bd4e8dfb592f492275519d5657e5fa5a05fd613db",
    "XCHAL_HAVE_ABSOLUTE_LITERALS": 0,
    "esp32_rom_libgcc_sha256": "fde777f43fddcc8594b56745b79af404c2fb10fc9256c10a093fb4d51a361eb2",
    "__ashldi3": 0x4000C818,
}

_PIN = bytes.fromhex(
    "368100ad02a5f4ff8c1aa5fcff2c782738261c0c0c0ba2c11025d1b1cd020c1a0c0b8130f4e00800a921b931262324e633099c230c382613101df0006633f90c1889418961c600000c1889418ba1a58c0006f9ff0c188941895186fbff"
)
_PRODUCER = bytes.fromhex(
    "36410091d2f781d0f7c020008808c02000380991cff73a88c02000480991cdf74a88c02000580991ccf75a88c02000680991caf76a88c02000780991c9f77a88c02000f80991c7f7fa88c02000e80991c6f7ea88c02000d80991c4f7da88c02000c80991c3f7ca88c02000b80991c1f7ba88c02000a80991c0f7aa88c0200098090c1b9a8891bdf70c2ac02000890925fb000c1b0c3aa5fa00"
)
_BODIES = {
    FIND: bytes.fromhex(
        "3641008163f4bd0228089c62ad020c0892a008768914c80ab79c0a8080f48088a02028a01df01b88a2ca140c02c6fcff"
    ),
    RELEASE: bytes.fromhex(
        "366100ac02a80296ca010c0ba5f2fffba125b31a7cf889020c088912825204fba1a5b21ae5faff1df0"
    ),
    "memset": bytes.fromhex(
        "3621003030748073117033200073117033205d0207e2c417e2d14074419c0739053df076970a390539153925393552c51037640832650032650152c508276404390552c5041764073255002b55c02000076405324500c020001df0"
    ),
    "gpio_config": bytes.fromhex(
        "3641005222006222016085209c2882aff180880192af008085106099109098209c59a5b4b8b1f6f1dd0aed0bc116f20c1aa5a5b822a1021df092220272a00290771016b7017cc96099109088200c078ce8a5b1b8b1ebf1dd0aed0bc10bf246f3ff70c72050a520bd0681cbf1e0080007ea0286200070a720651f008c3aad07a5490082220270a72007e802461e0025baffad0782220227e802061d00e5e2ffad07882217e802861b0065cdffad07883216d806a57bffad07884216a806e58cffad07b852659effad078852162806e5a9ff7cfa81cef1a2da908a8792080081ccf18a99c020008809a088100c1a30aa11a08820c0200089091b772c8887170286d7ff0c02c6cbff006570ff86e0ff00000065d5ffc6e1ff00a5b7ff46e3ff000000657eff86e3ff00e58fff46e4ff000000a5aaff46e6ff"
    ),
}
_PUBLIC = {
    "Input": "selection_public_pin_mode_input",
    "Output": "selection_public_pin_mode_output",
    "InputPullup": "selection_public_pin_mode_input_pullup",
    "InputPulldown": "selection_public_pin_mode_input_pulldown",
}
_OFFSETS = {
    "mask": "selection_gpio_mask_offset",
    "mode": "selection_gpio_mode_offset",
    "pull_up": "selection_gpio_pull_up_offset",
    "pull_down": "selection_gpio_pull_down_offset",
    "intr": "selection_gpio_intr_offset",
}
_GPIO_MODES = (
    "selection_gpio_mode_input",
    "selection_gpio_mode_output",
    "selection_gpio_mode_input_output",
)
_MANGLED = {
    PIN: "_ZN2fl7pinModeEiNS_7PinModeE",
    FIND: "_ZN2fl9pwm_state9findByPinEi",
    RELEASE: "_ZN2fl9pwm_state14releaseChannelEPNS0_11PwmPinStateE",
}


@dataclass(frozen=True)
class SDKLayout:
    public_modes: dict[str, int]
    offsets: dict[str, int]
    config_size: int
    gpio_input: int
    gpio_output: int
    gpio_input_output: int


@dataclass(frozen=True)
class Observation:
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
    sdk: SDKLayout | None = None
    observations: list[Observation] = field(default_factory=list)
    counterexamples: list[Observation] = field(default_factory=list)
    producer_calls_bound: bool = False
    # Encoded CALL8 targets/arguments only, NOT prefix execution or arrival.
    gpio_config_stores_bound: bool = False
    image_mapping_qualified: bool = False
    reachable_pinmode_addresses: list[int] = field(default_factory=list)
    bound_literal_addresses: list[int] = field(default_factory=list)
    assumptions: list[str] = field(default_factory=list)
    scope: str = "conditional-arrival-at-bound-Xtensa-CALL8-sites-Output-field-only"
    preconditions: list[str] = field(default_factory=list)


def _integer(value: object) -> int:
    if type(value) is not int or not 0 <= value <= 0xFFFFFFFF:
        raise ValueError("invalid raw ELF integer")
    return value


class _Authenticated:
    """Validate this sparse raw capture, without claiming a full ELF factory."""

    def __init__(self, capture: dict):
        if type(capture.get("machine")) is not int or capture["machine"] != 94:
            raise ValueError("not the fixed Xtensa ELF machine")
        self.symbols = capture["symbols"]
        expected = {
            PIN,
            PRODUCER,
            FIND,
            RELEASE,
            "memset",
            "gpio_config",
            "__ashldi3",
            "selection_gpio_config_size",
            *_PUBLIC.values(),
            *_OFFSETS.values(),
            *_GPIO_MODES,
        }
        if set(self.symbols) != expected:
            raise ValueError("missing or ambiguous selected symbol identity")
        self.programs = capture["program_headers"]
        for program in self.programs:
            for key in (
                "type",
                "file_offset",
                "vaddr",
                "paddr",
                "file_size",
                "mem_size",
                "flags",
                "align",
            ):
                _integer(program[key])
            if program["type"] == 1 and (
                program["file_size"] > program["mem_size"]
                or program["vaddr"] + program["mem_size"] > 1 << 32
            ):
                raise ValueError("incoherent LOAD extent")
        self.raw = {}
        for name, entry in self.symbols.items():
            raw = bytes.fromhex(entry["raw_symbol_hex"])
            if len(raw) != 16:
                raise ValueError("invalid raw symbol storage")
            _, address, size, info, other, index = struct.unpack("<IIIBBH", raw)
            if (
                address != _integer(entry["address"])
                or size != _integer(entry["size"])
                or info != entry["raw_symbol_info"]
                or index != entry["raw_symbol_index"]
                or other != 0
                or entry["raw_symbol"] != _MANGLED.get(name, name)
            ):
                raise ValueError("raw symbol/identity disagreement")
            _integer(entry["raw_symbol_file_offset"])
            if name == "__ashldi3":
                if (
                    info != 18
                    or index != 65521
                    or entry["kind"] != "ABS"
                    or entry["section"] != "ABS"
                    or address != SDK_SOURCE_CONTRACT[name]
                    or size != 24
                ):
                    raise ValueError("unbound actual SDK ROM helper")
                continue
            function = name in {PIN, PRODUCER, FIND, RELEASE, "memset", "gpio_config"}
            if (
                info != (18 if function else 17)
                or index != entry["section_index"]
                or entry["kind"] != ("F" if function else "O")
            ):
                raise ValueError("invalid raw selected symbol type")
            section = (
                ".iram0.text"
                if name == "memset"
                else ".flash.text"
                if function
                else ".flash.rodata"
            )
            if entry["section"] != section:
                raise ValueError("unreviewed selected section")
            data = self._storage(entry, function)
            if not function and (
                size != 4 or int.from_bytes(data, "little") != entry["uint32_le"]
            ):
                raise ValueError("raw SDK scalar disagreement")
            self.raw[name] = data
        owned = [entry for name, entry in self.symbols.items() if name != "__ashldi3"]
        for index, entry in enumerate(owned):
            for other in owned[index + 1 :]:
                if (
                    entry["address"] < other["address"] + other["size"]
                    and other["address"] < entry["address"] + entry["size"]
                ):
                    raise ValueError("competing selected symbol intervals")
        self.literals = {}
        for key, entry in capture["literal_words"].items():
            if (
                str(entry["address"]) != key
                or entry["address"] in self.literals
                or entry["size"] != 4
            ):
                raise ValueError("ambiguous literal storage")
            data = self._storage(entry, False)
            value = int.from_bytes(data, "little")
            if value != entry["uint32_le"]:
                raise ValueError("literal display/raw disagreement")
            self.literals[entry["address"]] = value

    def _storage(self, entry: dict, executable: bool) -> bytes:
        address, size = _integer(entry["address"]), _integer(entry["size"])
        offset, flags = _integer(entry["file_offset"]), _integer(entry["section_flags"])
        owner = _integer(entry["load_owner"])
        _integer(entry["section_index"])
        if (
            not size
            or address + size > 1 << 32
            or not flags & 2
            or (executable and not flags & 4)
        ):
            raise ValueError("invalid selected section extent")
        owners = [
            (index, program)
            for index, program in enumerate(self.programs)
            if program["type"] == 1
            and program["mem_size"]
            and program["vaddr"] < address + size
            and address < program["vaddr"] + program["mem_size"]
        ]
        if len(owners) != 1 or owners[0][0] != owner:
            raise ValueError("partial/complete competing LOAD or wrong owner")
        program = owners[0][1]
        if (
            not program["vaddr"]
            <= address
            < address + size
            <= program["vaddr"] + program["file_size"]
            or offset != program["file_offset"] + address - program["vaddr"]
            or (executable and not program["flags"] & 1)
        ):
            raise ValueError("selected storage/load disagreement")
        data = bytes.fromhex(entry["bytes_hex"])
        if len(data) != size:
            raise ValueError("truncated selected storage")
        return data

    def scalar(self, name: str) -> int:
        return int.from_bytes(self.raw[name], "little")


def _word(data: bytes, offset: int, size: int) -> int:
    raw = data[offset : offset + size]
    if len(raw) != size:
        raise ValueError("instruction outside authenticated slice")
    return int.from_bytes(raw, "little")


def _signed(value: int, bits: int) -> int:
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def _call8(data: bytes, offset: int, base: int) -> int:
    word = _word(data, offset, 3)
    if word & 63 != 0x25:
        raise ValueError("not the bound CALL8 form")
    return (((base + offset) & ~3) + 4 + (_signed(word >> 6, 18) << 2)) & 0xFFFFFFFF


def _jump(data: bytes, offset: int, base: int) -> int:
    word = _word(data, offset, 3)
    if word & 63 != 6:
        raise ValueError("unsupported J encoding")
    return (base + offset + 4 + _signed(word >> 6, 18)) & 0xFFFFFFFF


def _l32r(
    data: bytes,
    offset: int,
    base: int,
    register: int,
    captured: _Authenticated,
    bound: set[int],
) -> int:
    word = _word(data, offset, 3)
    if word & 15 != 1 or (word >> 4) & 15 != register:
        raise ValueError("unsupported reachable PC-relative L32R")
    # Espressif RI16: the displacement is FORCED negative, not signed16.
    target = ((((base + offset) + 3) & ~3) + (((word >> 8) - 65536) << 2)) & 0xFFFFFFFF
    if target not in captured.literals:
        raise ValueError("reachable literal has no authenticated storage")
    bound.add(target)
    return captured.literals[target]


def _movi_n(data: bytes, offset: int, register: int) -> int:
    word = _word(data, offset, 2)
    # MOVI.N fixes bit7=0 as well as low opcode0xc; bit7=1 belongs
    # to the BEQZ.N/BNEZ.N density family, not an immediate payload.
    if word & 0x8F != 0x0C or (word >> 8) & 15 != register:
        raise ValueError("unsupported MOVI.N register/opcode")
    imm = (word >> 12) | (((word >> 4) & 7) << 4)
    # Cadence p506-507: range -32..95, sign is AND of top two bits.
    return imm - 128 if imm & 0x60 == 0x60 else imm


def _beqz_n(data: bytes, offset: int, base: int, register: int) -> int:
    word = _word(data, offset, 2)
    if word & 0xCF != 0x8C or (word >> 8) & 15 != register:
        raise ValueError("unsupported BEQZ.N")
    # Cadence p352: ZERO-extended, forward-only imm6.
    imm = ((word >> 12) & 15) | (((word >> 4) & 3) << 4)
    return base + offset + 4 + imm


def _immediate_branch(
    data: bytes, offset: int, base: int, opcode: int, register: int, comparison: int
) -> int:
    word = _word(data, offset, 3)
    # Only observed positive B4Const indices are supported here.
    if (
        word & 255 != opcode
        or (word >> 8) & 15 != register
        or (word >> 12) & 15 != comparison
        or comparison not in (1, 2, 3)
    ):
        raise ValueError("unsupported B4Const branch")
    return base + offset + 4 + _signed(word >> 16, 8)


def _s32i_n(data: bytes, offset: int, source: int, base: int) -> int:
    word = _word(data, offset, 2)
    if word & 15 != 9 or (word >> 4) & 15 != source or (word >> 8) & 15 != base:
        raise ValueError("unsupported S32I.N operands")
    return 4 * (word >> 12)  # Cadence p594: scaled four-byte displacement


def _addi_n(data: bytes, offset: int, destination: int, base: int) -> int:
    word = _word(data, offset, 2)
    if word & 15 != 11 or (word >> 12) & 15 != destination or (word >> 8) & 15 != base:
        raise ValueError("unsupported ADDI.N operands")
    imm = (word >> 4) & 15
    return imm if imm else -1  # Cadence p333, zero encoding means -1


def _template(data: bytes, expected: bytes, holes: tuple[tuple[int, int], ...]) -> None:
    if len(data) != len(expected):
        raise ValueError("unsupported closed slice size")
    normalized = bytearray(data)
    for offset, size in holes:
        normalized[offset : offset + size] = expected[offset : offset + size]
    if normalized != expected:
        raise ValueError("unknown encoded branch/register/SP contract")


def classify_capture(capture: dict) -> ModeResult:
    result = ModeResult()
    try:
        captured = _Authenticated(capture)
        sdk = SDKLayout(
            {label: captured.scalar(name) for label, name in _PUBLIC.items()},
            {label: captured.scalar(name) for label, name in _OFFSETS.items()},
            captured.scalar("selection_gpio_config_size"),
            *(captured.scalar(name) for name in _GPIO_MODES),
        )
        if (
            sdk.public_modes
            != {"Input": 0, "Output": 1, "InputPullup": 2, "InputPulldown": 3}
            or sdk.offsets
            != {"mask": 0, "mode": 8, "pull_up": 12, "pull_down": 16, "intr": 20}
            or sdk.config_size != 24
            or (sdk.gpio_input, sdk.gpio_output, sdk.gpio_input_output) != (1, 2, 3)
            or sdk.gpio_input_output != sdk.gpio_input | sdk.gpio_output
        ):
            raise ValueError("unbound compiled SDK/public layout")
        result.sdk = sdk
    except (KeyError, TypeError, ValueError, OverflowError, struct.error):
        result.authentication_errors = ["xtensa-unbound-raw-symbol-literal-or-storage"]
        return result
    try:
        symbols, raw = captured.symbols, captured.raw
        code, producer = raw[PIN], raw[PRODUCER]
        base, caller = symbols[PIN]["address"], symbols[PRODUCER]["address"]
        _template(
            code, _PIN, ((5, 3), (0x0A, 3), (0x19, 3), (0x23, 2), (0x34, 2), (0x4E, 3))
        )
        export_loads = (
            (3, 9, "selection_gpio_mode_offset"),
            (6, 8, "selection_gpio_config_size"),
            (0x13, 9, _PUBLIC["Input"]),
            (0x1D, 9, _PUBLIC["InputPullup"]),
            (0x27, 9, _PUBLIC["InputPulldown"]),
            (0x31, 9, _OFFSETS["mask"]),
            (0x3B, 9, _OFFSETS["pull_up"]),
            (0x45, 9, _OFFSETS["pull_down"]),
            (0x4F, 9, _OFFSETS["intr"]),
            (0x59, 9, _PUBLIC["Output"]),
            (0x63, 9, _GPIO_MODES[2]),
            (0x6D, 9, _GPIO_MODES[1]),
            (0x77, 9, _GPIO_MODES[0]),
        )
        holes = tuple((offset + 1, 2) for offset, _, _ in export_loads) + (
            (0x86, 2),
            (0x8F, 3),
            (0x96, 3),
        )
        _template(producer[: len(_PRODUCER)], _PRODUCER, holes)
        for name, expected in _BODIES.items():
            if raw[name] != expected:
                raise ValueError("unreviewed actual normal callee body")
        bound: set[int] = set()
        for offset, register, name in export_loads:
            if (
                _l32r(producer, offset, caller, register, captured, bound)
                != symbols[name]["address"]
            ):
                raise ValueError("producer export pointer not bound to actual symbol")
        _l32r(
            producer, 0x85, caller, 9, captured, bound
        )  # authenticated volatile sink pointer; not runtime mapping
        if (
            _call8(producer, 0x8F, caller) != base
            or _call8(producer, 0x96, caller) != base
        ):
            raise ValueError("producer CALL8 target not exact pinMode entry")
        arguments = (
            (_movi_n(producer, 0x88, 10), _movi_n(producer, 0x81, 11), 0x8F),
            (_movi_n(producer, 0x94, 10), _movi_n(producer, 0x92, 11), 0x96),
        )
        if [(pin, mode) for pin, mode, _ in arguments] != [
            (2, sdk.public_modes["Output"]),
            (3, sdk.public_modes["Output"]),
        ]:
            raise ValueError("actual outgoing CALL8 arguments differ")
        for offset, target in (
            (5, FIND),
            (0x0A, RELEASE),
            (0x19, "memset"),
            (0x4E, "gpio_config"),
        ):
            if _call8(code, offset, base) != symbols[target]["address"]:
                raise ValueError("normal-callee CALL8 target is not exact bound entry")
        if _beqz_n(code, 8, base, 10) != base + 0x0D:
            raise ValueError("PWM conditional edge differs")
        if _movi_n(code, 0x0D, 8) != 39:
            raise ValueError("pin range guard differs")
        # The reviewed BLTU a8,a2 has only the exact register/condition bytes;
        # its signed8 target is independently checked. Fixed pins2/3 are <39.
        if (
            _word(code, 0x0F, 3) & 0xFFFF != 0x3827
            or base + 0x0F + 4 + _signed(_word(code, 0x0F, 3) >> 16, 8) != base + 0x39
        ):
            raise ValueError("range branch differs")
        memset_pointer = _word(code, 0x16, 3)
        if (
            memset_pointer & 0xFFFF != 0xC1A2
            or _signed(memset_pointer >> 16, 8) != 16
            or _movi_n(code, 0x12, 12) != 16
            or _movi_n(code, 0x14, 11) != 0
        ):
            raise ValueError("bound memset must zero exactly SP16..31")
        if _movi_n(code, 0x1E, 10) != 1 or _movi_n(code, 0x20, 11) != 0:
            raise ValueError("actual ROM shift input words differ")
        if _s32i_n(code, 0x28, 10, 1) != 8 or _s32i_n(code, 0x2A, 11, 1) != 12:
            raise ValueError("separate mask stores differ")
        if (
            _l32r(code, 0x22, base, 8, captured, bound)
            != symbols["__ashldi3"]["address"]
            or _word(code, 0x25, 3) != 0x0008E0
        ):
            raise ValueError("CALLX8 target/register not bound to actual ROM helper")
        # Evaluate only actual public Output=1; all other modes are unqualified.
        if (
            _immediate_branch(code, 0x2C, base, 0x26, 3, 2) != base + 0x54
            or _immediate_branch(code, 0x2F, base, 0xE6, 3, 3) != base + 0x3C
            or _beqz_n(code, 0x32, base, 3) != base + 0x48
            or _immediate_branch(code, 0x36, base, 0x26, 3, 1) != base + 0x4A
        ):
            raise ValueError("unreviewed mode-dispatch edge")
        configured = _movi_n(code, 0x34, 8)
        if configured not in (2, 3):
            raise ValueError("unreviewed decoded Output mode")
        if any(
            mode == 2 or mode >= 3 or mode == 0 or mode != 1 or pin > 39
            for pin, mode, _ in arguments
        ):
            raise ValueError("actual call arguments take a different reachable branch")
        store, pointer = _s32i_n(code, 0x4A, 8, 1), _addi_n(code, 0x4C, 10, 1)
        if (
            pointer != 8
            or store - pointer != sdk.offsets["mode"]
            or pointer + sdk.config_size != 32
        ):
            raise ValueError("actual config/store not within compiled SDK layout")
        if _jump(code, 0x51, base) != base + 0x39 or _word(code, 0x39, 2) != 0xF01D:
            raise ValueError("selected call/normal-return edge differs")
        reachable = (
            0,
            3,
            5,
            8,
            0x0A,
            0x0D,
            0x0F,
            0x12,
            0x14,
            0x16,
            0x19,
            0x1C,
            0x1E,
            0x20,
            0x22,
            0x25,
            0x28,
            0x2A,
            0x2C,
            0x2F,
            0x32,
            0x34,
            0x36,
            0x4A,
            0x4C,
            0x4E,
            0x51,
            0x39,
        )
        result.reachable_pinmode_addresses = sorted(
            base + offset for offset in reachable
        )
        result.bound_literal_addresses = sorted(bound)
        result.producer_calls_bound = result.gpio_config_stores_bound = True
        result.observations = [
            Observation(
                pin,
                mode,
                configured,
                pwm,
                caller + offset,
                base + 0x4A,
                symbols["gpio_config"]["address"],
            )
            for pin, mode, offset in arguments
            for pwm in (False, True)
        ]
        result.counterexamples = [
            item
            for item in result.observations
            if item.configured_mode != sdk.gpio_input_output
        ]
        result.errors = ["output-mode-mismatch"] if result.counterexamples else []
        result.assumptions = [ASSUMPTION]
        result.preconditions = [
            "arrival-at-the-two-encoded-CALL8-sites",
            "valid-nonaliasing-producer-prefix-loads-and-volatile-sink-storage",
            "actual-core-PC-relative-literals-and-pinned-SDK-ROM-contract",
            "normal-windowed-ABI-returns-with-no-uncontracted-caller-frame-writes",
        ]
    except (KeyError, TypeError, ValueError, OverflowError):
        result.decode_errors = ["xtensa-unsupported-reachable-encoding"]
    return result
