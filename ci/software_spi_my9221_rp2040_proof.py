"""Closed RP2040 GPIO2/3 ELF-property checks; raw captures never authenticate proof.

The exact reviewed instruction shapes prove a potential direct public route and
whole-kernel/callback SRAM placement, not execution, IRQ correctness or timing.
"""

import hashlib
import re
from dataclasses import dataclass
from pathlib import Path


SOURCE = "src/platforms/arm/rp/rpcommon/my9221_emitter.h"
STAGES = ("baseline", "kernel-xip-mutant", "restored")


def kernel_mutant(source: bytes) -> bytes:
    marker = b"__no_inline_not_in_flash_func(my9221EmitRp)"
    if source.count(marker) != 2:
        raise ValueError("changed fixed RP2040 kernel boundary")
    return source.replace(marker, b"__attribute__((noinline)) my9221EmitRp")


def validate_stage_sequence(names: tuple[str, ...]) -> None:
    if names != STAGES:
        raise ValueError("missing, reordered or extra fixed RP2040 ELF stages")


# Reviewed actual SDK5.7 ARMv6-M local-producer capture. These are restrictive
# instruction-pattern contracts, not a general Thumb decoder. Unknown compiler
# shapes require review; no instruction is silently skipped.
PATTERNS = {
    "fl::CLEDController::showLeds(unsigned char)": "fa5e96801f06fe77767a4ddaf943204d1c8d724df16e2dea3d57e42ffa55d203",
    "my9221_rp2040_public_producer": "4e8f0c0b25d9e99a60c2e680c038a046aee7261d97d18a91131b311bcbdf5012",
    "main": "99fdccb96b3af4cca71c8e2efe5b8aa76f606123a889323b5e1b6d42ce02274b",
    "MY9221Controller<2, (unsigned char)3, (fl::EOrder)10, 133ul>::showPixels(PixelController<(fl::EOrder)10, 1, 4294967295ul>&)": "3137d48a0a9605d7ee674d4d9d1b478fb9d157623f2fba86ce4ac6955e92fd8a",
    "CPixelLEDController<(fl::EOrder)10, 1, 4294967295ul>::show(fl::CRGB const*, int, unsigned char)": "281c35fcf8d209202aaba319e5d5537962a0f099d8c54819cfd2a9bd2489e58f",
    "___ZN2fl6detailL12my9221EmitRpEPKtjNS0_12My9221RpPinsE_veneer": "2348a5c7a42c1a793b35a86f4ded3b394b1602f195e09e5060349151c6b4c52d",
    "fl::detail::my9221RpPin2_hi()": "7c4a23ffa3876255a1d0e0e2653631d4f7ce7cf9eb038074faebbe33f719b98e",
    "fl::detail::my9221RpPin2_lo()": "84ec6feb04bd8c7a44087c3d2dd2afb84d60f024bca9d31d5f6cbb1b7205ac30",
    "fl::detail::my9221RpPin3_lo()": "590a24b2fa5b064aad4b923d68cf351584803c318f087e0adb31eff3c2c1e917",
    "fl::detail::my9221RpPin3_toggle()": "9aef354ead48c7cbc519d4bc10b0a436e21a7d49b54f3744e36f0d9635a41cc8",
    "fl::detail::my9221EmitRp(unsigned short const*, unsigned int, fl::detail::My9221RpPins)": "324ba114dae56775269ad22bc1fd03007a770db8056d6b784a6f4ca019889682",
}
RAW_PATTERNS = {
    "fl::CLEDController::showLeds(unsigned char)": "1b1d840091fdc5212d7ccd84f5643375ce2008dad0a900017563257cd4d05c89",
    "my9221_rp2040_public_producer": "4008f6a03db284dd61517900f1084723e35b4a16dae9b6ca90861614bb11bc24",
    "main": "6881f1c019f31c3dcd14c736f997b5354320ccffcd15b8c3eea1f71a52e09d52",
    "MY9221Controller<2, (unsigned char)3, (fl::EOrder)10, 133ul>::showPixels(PixelController<(fl::EOrder)10, 1, 4294967295ul>&)": "8701b12f673f66489b1e0d3cbaa2800faf9464283ccba0f8bfcaa353e2551629",
    "CPixelLEDController<(fl::EOrder)10, 1, 4294967295ul>::show(fl::CRGB const*, int, unsigned char)": "a6af21839fd662db3025be0760f62ee848bb9aad761710ce751d01992ad25555",
    "___ZN2fl6detailL12my9221EmitRpEPKtjNS0_12My9221RpPinsE_veneer": "4df21fbe70855b04b8d930229218afc5a5e00f3a7fe1afb402c144c03b210873",
    "fl::detail::my9221RpPin2_hi()": "5f141cd158f621509c0100b8547a9115982569bacc3c7ccc57ff55835755df53",
    "fl::detail::my9221RpPin2_lo()": "7ac1284a3e25a929ad90ee5a264306490e679ac87b8fedd9af219d1e4ac9109e",
    "fl::detail::my9221RpPin3_lo()": "e3be16901488a69a3cbbeccd0431b83447db8a6c9cfd8e4b94d33dc698f5bc2b",
    "fl::detail::my9221RpPin3_toggle()": "fb94f8bb9459cfa939894639219e48e703e3570c2def8a2eb311e309010186f0",
    "fl::detail::my9221EmitRp(unsigned short const*, unsigned int, fl::detail::My9221RpPins)": "f51236b755949edd2b2f8a5c36b9cc658cc910e40d037f184abeb0c68d5e79f8",
}


def _blocks(text: str) -> dict[str, tuple[int, str]]:
    matches = list(re.finditer(r"^([0-9a-f]+) <(.+)>:\n", text, re.MULTILINE))
    result = {}
    for index, match in enumerate(matches):
        if match[2] in result:
            raise ValueError("ambiguous ELF function alias")
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        result[match[2]] = (int(match[1], 16), text[match.start() : end])
    return result


def _instructions(body: str) -> list[tuple[int, bytes, str, str]]:
    result = []
    for line in body.splitlines()[1:]:
        if not line.strip():
            continue
        match = re.fullmatch(r"([0-9a-f]+):\s+([0-9a-f ]+?)\s+\t([^\t ]+)\s*(.*)", line)
        if not match:
            raise ValueError("unsupported actual instruction record")
        words = match[2].split()
        raw = b"".join(
            int(word, 16).to_bytes(len(word) // 2, "little") for word in words
        )
        result.append((int(match[1], 16), raw, match[3], match[4]))
    return result


def _pattern(name: str, block: tuple[int, str]) -> str:
    start, body = block
    normalized = []
    for _, _, opcode, operands in _instructions(body):
        if opcode.startswith("."):
            if opcode != ".word":
                raise ValueError("unsupported ELF data directive")
            continue
        operands = operands.split("@")[0].strip()
        target = re.match(r"([0-9a-f]+) <(.+)>", operands)
        if target:
            target_name = target[2]
            operands = (
                "self+" + hex(int(target[1], 16) - start)
                if target_name.startswith(name + "+")
                else target_name
            )
            # Both a direct BL and the exact decoded veneer can reach the same
            # kernel when moving it between RAM and XIP in the single mutant.
            if "my9221EmitRp" in target_name and not target_name.startswith(name + "+"):
                operands = "fixed-my9221-kernel"
        normalized.append(opcode + " " + operands)
    return hashlib.sha256("\n".join(normalized).encode()).hexdigest()


def _read(capture: dict, address: int, size: int) -> bytes:
    matches = []
    for section in capture["section_bytes"].values():
        raw = bytes.fromhex(section["bytes_hex"])
        offset = address - section["vma"]
        if 0 <= offset and offset + size <= len(raw):
            matches.append(raw[offset : offset + size])
    if len(matches) != 1:
        raise ValueError("unmapped or ambiguous literal/code storage")
    return matches[0]


def _word(capture: dict, address: int) -> int:
    if address % 4:
        raise ValueError("unaligned literal storage")
    return int.from_bytes(_read(capture, address, 4), "little")


def _verify_raw(capture: dict, block: tuple[int, str]) -> tuple[int, int]:
    instructions = _instructions(block[1])
    if not instructions or instructions[0][0] != block[0]:
        raise ValueError("missing function bytes")
    end = block[0]
    for address, raw, _, _ in instructions:
        if address != end or _read(capture, address, len(raw)) != raw:
            raise ValueError("actual disassembly/code bytes disagree")
        end += len(raw)
    return block[0], end


def _literal_loads(capture: dict, block: tuple[int, str]) -> list[tuple[str, int]]:
    result = []
    for address, _, opcode, operands in _instructions(block[1]):
        if opcode != "ldr":
            continue
        match = re.match(r"(r\d+), \[pc, #(\d+)\]\s*@ \(([0-9a-f]+) ", operands)
        if not match:
            continue
        storage = ((address + 4) & ~3) + int(match[2])
        if storage != int(match[3], 16):
            raise ValueError("PC literal address inconsistent")
        result.append((match[1], _word(capture, storage)))
    return result


def _one(blocks: dict, contains: str) -> tuple[str, tuple[int, str]]:
    matches = [(name, block) for name, block in blocks.items() if contains in name]
    if len(matches) != 1:
        raise ValueError("missing or ambiguous selected function")
    return matches[0]


def _sdk_ram(capture: dict) -> tuple[int, int]:
    matches = re.findall(
        r"^RAM\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+xrw\s*$",
        capture["memory_map"],
        re.MULTILINE,
    )
    if len(matches) != 1:
        raise ValueError("SDK RAM range missing or ambiguous")
    start, size = (int(value, 16) for value in matches[0])
    if size <= 0 or start + size > 0x100000000:
        raise ValueError("SDK RAM range invalid")
    return start, start + size


def _extent_error(
    capture: dict, start: int, end: int, ram: tuple[int, int]
) -> str | None:
    if not ram[0] <= start < end <= ram[1]:
        return "kernel-not-in-sdk-sram"
    sections = re.findall(
        r"^\s*\[\s*\d+\]\s+(\S+)\s+PROGBITS\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)\s+\S+\s+([A-Z]+)\s",
        capture["elf_layout"],
        re.MULTILINE,
    )
    overlapping_sections = [
        (int(vma, 16), int(size, 16), flags)
        for _, vma, size, flags in sections
        if "A" in flags
        and "X" in flags
        and int(vma, 16) < end
        and start < int(vma, 16) + int(size, 16)
    ]
    if len(overlapping_sections) > 1:
        return "sram-extent-ambiguous"
    if len(overlapping_sections) != 1 or not (
        overlapping_sections[0][0]
        <= start
        < end
        <= overlapping_sections[0][0] + overlapping_sections[0][1]
    ):
        return "sram-symbol-range-missing"
    overlapping_loads = []
    for line in capture["elf_layout"].splitlines():
        fields = line.split()
        if fields and fields[0] == "LOAD" and len(fields) >= 8:
            vma, filesz, memsz = (
                int(fields[2], 16),
                int(fields[4], 16),
                int(fields[5], 16),
            )
            if vma < end and start < vma + memsz:
                overlapping_loads.append((vma, filesz, memsz, fields[6:-1]))
    if len(overlapping_loads) > 1:
        return "sram-extent-ambiguous"
    if len(overlapping_loads) == 1:
        vma, filesz, memsz, flags = overlapping_loads[0]
        if "E" in flags and vma <= start < end <= vma + min(filesz, memsz):
            return None
    return "sram-load-range-missing"


@dataclass(frozen=True)
class AuthenticatedRoute:
    kernel_name: str
    kernel: tuple[int, str]
    callbacks: dict[str, tuple[int, str]]
    entries: dict[str, set[int]]
    ram: tuple[int, int]


class RouteAuthenticationError(ValueError):
    def __init__(self, code: str):
        self.code = code
        super().__init__(code)


def classify_capture(capture: dict) -> list[str]:
    """The placement contract remains exact; supplied captures grant no proof."""
    try:
        route = authenticate_route(capture)
        _verify_block(capture, route.kernel_name, route.kernel, route.entries)
        return []
    except RouteAuthenticationError as error:
        return [error.code]
    except (KeyError, ValueError, TypeError, OverflowError):
        return ["unsupported-kernel-shape"]


def _signed(value: int, bits: int) -> int:
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


def _direct_target(address: int, raw: bytes, opcode: str) -> int:
    first = int.from_bytes(raw[:2], "little")
    if opcode == "bl" and len(raw) == 4:
        second = int.from_bytes(raw[2:], "little")
        if first & 0xF800 != 0xF000 or second & 0xD000 != 0xD000:
            raise ValueError("unsupported actual BL encoding")
        sign = (first >> 10) & 1
        i1 = 1 ^ ((second >> 13) & 1) ^ sign
        i2 = 1 ^ ((second >> 11) & 1) ^ sign
        immediate = (
            (sign << 24)
            | (i1 << 23)
            | (i2 << 22)
            | ((first & 0x3FF) << 12)
            | ((second & 0x7FF) << 1)
        )
        return address + 4 + _signed(immediate, 25)
    if len(raw) == 2 and first & 0xF800 == 0xE000:
        return address + 4 + _signed((first & 0x7FF) << 1, 12)
    if len(raw) == 2 and first & 0xF000 == 0xD000 and (first >> 8) & 15 < 14:
        return address + 4 + _signed((first & 255) << 1, 9)
    raise ValueError("unsupported direct branch encoding")


def _raw_pattern(block: tuple[int, str]) -> str:
    normalized = bytearray()
    for address, raw, opcode, _ in _instructions(block[1]):
        if opcode == "bl":
            _direct_target(address, raw, opcode)
            normalized.extend(bytes.fromhex("00f000f8"))
        elif opcode == ".word":
            normalized.extend(b"\0" * len(raw))
        else:
            normalized.extend(raw)
    return hashlib.sha256(normalized).hexdigest()


def _verify_block(
    capture: dict, name: str, block: tuple[int, str], entries: dict[str, set[int]]
) -> None:
    start, end = _verify_raw(capture, block)
    if (
        _pattern(name, block) != PATTERNS[name]
        or _raw_pattern(block) != RAW_PATTERNS[name]
    ):
        raise ValueError("unsupported reviewed instruction or byte shape")
    for address, raw, opcode, operands in _instructions(block[1]):
        if opcode == "bl" or (opcode.startswith("b") and opcode.endswith(".n")):
            target = re.match(r"([0-9a-f]+) <(.+)>", operands)
            if target is None:
                raise ValueError("missing actual direct target")
            decoded = _direct_target(address, raw, opcode)
            if decoded != int(target[1], 16):
                raise ValueError("raw direct target disagrees with display")
            if target[2].startswith(name + "+"):
                if not start <= decoded < end or decoded != start + int(
                    target[2].rsplit("+", 1)[1], 16
                ):
                    raise ValueError("changed internal branch boundary")
            elif decoded not in entries.get(target[2], set()):
                raise ValueError("actual call does not target exact named entry")
    _literal_loads(capture, block)


def _route_prefix(
    capture: dict,
    text: dict,
    all_blocks: dict,
    producer: tuple[int, str],
    ram: tuple[int, int],
) -> AuthenticatedRoute:
    # GCC complete/base/deleting destructors can share a demangled name at
    # distinct real entries. Bind each call's decoded address to an actual
    # header boundary, rather than silently retaining only the last spelling.
    entries: dict[str, set[int]] = {}
    for address, name in re.findall(
        r"^([0-9a-f]+) <(.+)>:$", capture["text_function_headers"], re.MULTILINE
    ):
        entries.setdefault(name, set()).add(int(address, 16))
    for name, block in all_blocks.items():
        entries.setdefault(name, set()).add(block[0])
    try:
        _verify_block(capture, "main", text["main"], entries)
    except (KeyError, ValueError):
        raise RouteAuthenticationError("public-producer-unreachable")
    try:
        _verify_block(capture, "my9221_rp2040_public_producer", producer, entries)
        vtables = re.findall(
            r"^\s*\d+:\s+([0-9a-f]+)\s+68\s+OBJECT\s+WEAK\s+DEFAULT\s+\d+\s+_ZTV16MY9221ControllerILi2ELh3ELN2fl6EOrderE10ELm133EE$",
            capture["elf_layout"],
            re.MULTILINE,
        )
        if len(vtables) != 1:
            raise ValueError("ambiguous actual vtable")
        vptr = int(vtables[0], 16) + 8
        if _literal_loads(capture, producer)[0] != ("r3", vptr):
            raise ValueError("local object vptr is not selected MY table")
        show_name, show = _one(text, "CLEDController::showLeds(")
        pixel_name, pixel = _one(text, ">::show(fl::CRGB const*")
        writer_name, writer = _one(text, ">::showPixels(")
        if _word(capture, vptr + 12) != (pixel[0] | 1) or _word(capture, vptr + 56) != (
            writer[0] | 1
        ):
            raise ValueError("wrong actual virtual slot")
        _verify_block(capture, show_name, show, entries)
        _verify_block(capture, pixel_name, pixel, entries)
    except (KeyError, ValueError):
        raise RouteAuthenticationError("public-controller-vptr-unbound")
    try:
        callbacks = [
            _one(all_blocks, name)[1]
            for name in (
                "my9221RpPin3_toggle()",
                "my9221RpPin2_lo()",
                "my9221RpPin3_lo()",
                "my9221RpPin2_hi()",
            )
        ]
        loads = _literal_loads(capture, writer)
        if len(loads) != 4:
            raise ValueError("callback argument literal set changed")
        if any(not value & 1 for _, value in loads):
            raise RouteAuthenticationError("callback-thumb-target-invalid")
        if loads != list(
            zip(("r6", "r3", "r6", "r2"), (block[0] | 1 for block in callbacks))
        ):
            raise RouteAuthenticationError("callback-target-mismatch")
        if (
            _pattern(writer_name, writer) != PATTERNS[writer_name]
            or _raw_pattern(writer) != RAW_PATTERNS[writer_name]
        ):
            raise RouteAuthenticationError("callback-target-mismatch")
        _verify_block(capture, writer_name, writer, entries)
    except RouteAuthenticationError:
        raise
    except (KeyError, ValueError):
        raise RouteAuthenticationError("kernel-veneer-target-invalid")
    kernel_name, kernel = _one(all_blocks, "my9221EmitRp(unsigned")
    # Resolve the actual writer edge to THIS kernel or its actually called,
    # byte-verified veneer. A similarly named unrelated entry is insufficient.
    calls = [
        _direct_target(address, raw, opcode)
        for address, raw, opcode, operands in _instructions(writer[1])
        if opcode == "bl" and "my9221EmitRp" in operands
    ]
    if len(calls) != 1:
        raise RouteAuthenticationError("kernel-veneer-target-invalid")
    if calls[0] != kernel[0]:
        veneers = [
            (name, block)
            for name, block in text.items()
            if "my9221EmitRp" in name and "veneer" in name and block[0] == calls[0]
        ]
        if len(veneers) != 1:
            raise RouteAuthenticationError("kernel-veneer-target-invalid")
        name, veneer = veneers[0]
        try:
            _verify_block(capture, name, veneer, entries)
            if _literal_loads(capture, veneer) != [("r0", kernel[0] | 1)]:
                raise RouteAuthenticationError("kernel-veneer-target-invalid")
        except (KeyError, ValueError):
            raise RouteAuthenticationError("kernel-veneer-target-invalid")
    return AuthenticatedRoute(
        kernel_name,
        kernel,
        {
            name: block
            for name, block in [
                _one(all_blocks, needle)
                for needle in (
                    "my9221RpPin2_hi()",
                    "my9221RpPin2_lo()",
                    "my9221RpPin3_lo()",
                    "my9221RpPin3_toggle()",
                )
            ]
        },
        entries,
        ram,
    )


def _declared_extent(capture: dict, needle: str, block: tuple[int, str]) -> None:
    matches = []
    for line in capture["elf_layout"].splitlines():
        match = re.fullmatch(
            r"\s*\d+:\s+([0-9a-f]+)\s+(\d+)\s+FUNC\s+\S+\s+DEFAULT\s+\d+\s+(\S+)", line
        )
        if match and needle in match[3] and "veneer" not in match[3]:
            matches.append((int(match[1], 16) & ~1, int(match[2])))
    start, end = _verify_raw(capture, block)
    if matches != [(start, end - start)]:
        raise RouteAuthenticationError("sram-symbol-range-missing")


def authenticate_route(capture: dict) -> AuthenticatedRoute:
    """Common byte-bound public route/leaf/storage extraction, no kernel shape waiver."""
    try:
        ram = _sdk_ram(capture)
    except (KeyError, ValueError, TypeError) as error:
        raise RouteAuthenticationError("sdk-memory-range-invalid") from error
    text = _blocks(capture["text_disassembly"])
    data = _blocks(capture["ram_disassembly"])
    all_blocks = {**text, **data}
    producer = text.get("my9221_rp2040_public_producer")
    main = text.get("main")
    if (
        producer is None
        or main is None
        or "<my9221_rp2040_public_producer>" not in main[1]
    ):
        raise RouteAuthenticationError("public-producer-unreachable")
    if (
        _pattern("my9221_rp2040_public_producer", producer)
        != PATTERNS["my9221_rp2040_public_producer"]
    ):
        raise RouteAuthenticationError("public-controller-vptr-unbound")
    route = _route_prefix(capture, text, all_blocks, producer, ram)
    for name, block in route.callbacks.items():
        _verify_block(capture, name, block, route.entries)
        _declared_extent(capture, name.split("::")[-1].split("(")[0], block)
        error = _extent_error(capture, *_verify_raw(capture, block), ram)
        if error:
            raise RouteAuthenticationError(error)
    _declared_extent(capture, "my9221EmitRp", route.kernel)
    error = _extent_error(capture, *_verify_raw(capture, route.kernel), ram)
    if error:
        raise RouteAuthenticationError(error)
    return route


def _verify_profile_state(
    saved: Path, phase: str, expected: dict, observed: dict
) -> None:
    import json

    changed = sorted(
        key
        for key in set(expected) | set(observed)
        if expected.get(key) != observed.get(key)
    )
    saved.mkdir(parents=True, exist_ok=True)
    (saved / f"profile-{phase}.json").write_text(
        json.dumps(
            {"changed_fields": changed, "expected": expected, "observed": observed},
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    if changed:
        raise ValueError(
            f"RP2040 frozen profile identity changed: {changed}; retained {saved}"
        )
