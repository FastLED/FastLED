"""Closed RP2040 saved-PRIMASK source control; no IRQ proof is granted here.

The actual source-restored mutation is independent from placement acceptance.
Future semantic qualification must inspect actual byte-bound baseline/mutant CFGs.
"""

SOURCE = "src/platforms/my9221.h"


def irq_mutant(source: bytes) -> bytes:
    marker = b"restore_interrupts(mSaved);"
    if source.count(marker) != 1:
        raise ValueError("missing or ambiguous fixed RP2040 restore source boundary")
    return source.replace(marker, b"restore_interrupts(0);")


import re
from collections import deque
from dataclasses import dataclass, replace

from ci import software_spi_my9221_rp2040_proof as placement


@dataclass(frozen=True, slots=True)
class IRQOutcome:
    entry_primask: int
    exit_primask: int | None
    sp_delta: int
    return_token: str
    final_parked: bool
    restore_pc: int | None


@dataclass(frozen=True, slots=True)
class IRQResult:
    errors: list[str]
    normal_returns: dict[int, list[IRQOutcome]]
    counterexamples: list[IRQOutcome]
    authentication_errors: list[str]
    decode_errors: list[str]


class IRQDecodeError(ValueError):
    def __init__(self, code: str):
        self.code = code
        super().__init__(code)


@dataclass(frozen=True, slots=True)
class Instruction:
    pc: int
    size: int
    op: str
    rd: int = -1
    rn: int = -1
    imm: int = 0
    registers: tuple[int, ...] = ()
    target: int = -1


def _register(number: int) -> str:
    return {13: "sp", 14: "lr", 15: "pc"}.get(number, f"r{number}")


def _decode_one(pc: int, raw: bytes, opcode: str, operands: str) -> Instruction:
    """Only the Thumb forms needed by the two observed IRQ kernels/controls."""
    h = int.from_bytes(raw[:2], "little")
    rd, rn, imm = -1, -1, 0
    registers = ()
    target = -1
    args = ""
    if len(raw) == 4:
        second = int.from_bytes(raw[2:], "little")
        if h == 0xF3EF and second & 0xF0FF == 0x8010:
            op, rd = "mrs", (second >> 8) & 15
            args = f"{_register(rd)}, PRIMASK"
        elif h & 0xFFF0 == 0xF380 and second == 0x8810:
            op, rn = "msr", h & 15
            args = f"PRIMASK, {_register(rn)}"
        else:
            raise IRQDecodeError("irq-unsupported-instruction")
    elif len(raw) != 2:
        raise IRQDecodeError("irq-unsupported-instruction")
    elif h in (0xB672, 0xB662):
        op, args = ("cpsid" if h == 0xB672 else "cpsie"), "i"
    elif h & 0xFE00 in (0xB400, 0xBC00):
        op = "push" if h & 0xFE00 == 0xB400 else "pop"
        registers = tuple(n for n in range(8) if h & (1 << n))
        if h & 0x100:
            registers += (14 if op == "push" else 15,)
        if not registers:
            raise IRQDecodeError("irq-unsupported-instruction")
        args = "{" + ", ".join(_register(n) for n in registers) + "}"
    elif h & 0xFF00 == 0xB000:
        op, imm = ("sub" if h & 0x80 else "add"), (h & 127) * 4
        args = f"sp, #{imm}"
    elif h & 0xF000 == 0x9000:
        op, rd, imm = ("ldr" if h & 0x800 else "str"), (h >> 8) & 7, (h & 255) * 4
        rn = 13
        args = f"{_register(rd)}, [sp, #{imm}]"
    elif h & 0xF800 in (0x6000, 0x6800, 0x8800):
        family = h & 0xF800
        op = {0x6000: "str", 0x6800: "ldr", 0x8800: "ldrh"}[family]
        rd, rn = h & 7, (h >> 3) & 7
        imm = ((h >> 6) & 31) * (2 if op == "ldrh" else 4)
        args = f"{_register(rd)}, [{_register(rn)}, #{imm}]"
    elif h & 0xF800 == 0x2000:
        op, rd, imm = "movs", (h >> 8) & 7, h & 255
        args = f"{_register(rd)}, #{imm}"
    elif h & 0xF800 in (0x0000, 0x0800, 0x1000):
        rd, rn, imm = h & 7, (h >> 3) & 7, (h >> 6) & 31
        op = {0x0000: "lsls", 0x0800: "lsrs", 0x1000: "asrs"}[h & 0xF800]
        if op == "lsls" and imm == 0:
            op = "movs"
            args = f"{_register(rd)}, {_register(rn)}"
        else:
            imm = imm or 32
            args = f"{_register(rd)}, {_register(rn)}, #{imm}"
    elif h & 0xF800 in (0x3000, 0x3800):
        op, rd, imm = (
            ("adds" if h & 0xF800 == 0x3000 else "subs"),
            (h >> 8) & 7,
            h & 255,
        )
        args = f"{_register(rd)}, #{imm}"
    elif h & 0xF800 == 0x2800:
        op, rd, imm = "cmp", (h >> 8) & 7, h & 255
        args = f"{_register(rd)}, #{imm}"
    elif h & 0xFFC0 in (0x4280, 0x4000, 0xB200, 0xB280, 0xB2C0):
        op = {
            0x4280: "cmp",
            0x4000: "ands",
            0xB200: "sxth",
            0xB280: "uxth",
            0xB2C0: "uxtb",
        }[h & 0xFFC0]
        rd, rn = h & 7, (h >> 3) & 7
        args = f"{_register(rd)}, {_register(rn)}"
    elif h & 0xFF87 in (0x4700, 0x4780):
        op, rn = ("blx" if h & 0x80 else "bx"), (h >> 3) & 15
        args = _register(rn)
    elif h & 0xF800 == 0xE000 or (h & 0xF000 == 0xD000 and (h >> 8) & 15 < 14):
        conditions = (
            "eq",
            "ne",
            "cs",
            "cc",
            "mi",
            "pl",
            "vs",
            "vc",
            "hi",
            "ls",
            "ge",
            "lt",
            "gt",
            "le",
        )
        op = "b.n" if h & 0xF800 == 0xE000 else "b" + conditions[(h >> 8) & 15] + ".n"
        target = placement._direct_target(pc, raw, op)
        args = f"{target:x}"
        operands = operands.split(" <", 1)[0]
    elif h == 0xBF00:
        op = "nop"
    else:
        raise IRQDecodeError("irq-unsupported-instruction")
    # This closed kernel uses low registers for special-register transfers.
    # SP/PC/LR/high-register forms are outside the modeled state operations;
    # never let an MRS write a register while bypassing separate SP/CFG state.
    if (op == "mrs" and rd >= 8) or (op == "msr" and rn >= 8):
        raise IRQDecodeError("irq-unsupported-register")
    actual = re.sub(r"\s+", "", operands.split("@", 1)[0]).lower()
    if opcode != op or actual != re.sub(r"\s+", "", args).lower():
        raise IRQDecodeError("irq-raw-text-disagreement")
    return Instruction(pc, len(raw), op, rd, rn, imm, registers, target)


# Value lattice: unknown payload scalars never grow a loop-counter domain.
# Frame is a valid non-stack input-buffer token, not a concrete runtime address.
Value = tuple[str, int]
UNKNOWN: Value = ("unknown", 0)
FRAME: Value = ("frame", 0)
ENTRY_LR: Value = ("entry-lr", 0)


@dataclass(frozen=True, slots=True)
class State:
    regs: tuple[Value, ...]
    stack: tuple[tuple[int, Value], ...]
    sp: int
    primask: int | None
    park: int = 0
    restored: bool = False
    parked_at_restore: bool = False
    restore_pc: int | None = None


class SemanticError(ValueError):
    def __init__(self, code: str):
        self.code = code
        super().__init__(code)


def _integer(number: int) -> Value:
    return ("integer", number) if number in (0, 1) else UNKNOWN


def _memory_address(state: State, instruction: Instruction) -> int:
    if instruction.rn != 13:
        raise SemanticError("irq-stack-alias-unresolved")
    address = state.sp + instruction.imm
    if address % 4 or address < state.sp or address > 4:
        raise SemanticError("irq-stack-alias-unresolved")
    return address


def _callback(state: State, effect: str, pc: int, size: int, errors: set[str]) -> State:
    if state.restored:
        errors.add("irq-gpio-after-restore")
    if state.primask != 1:
        errors.add("irq-unmasked-emission")
    park = (
        1
        if effect == "clockLo"
        else (2 if effect == "dataLo" and state.park == 1 else 0)
    )
    regs = list(state.regs)
    # Authenticated five-instruction leaf callbacks touch only r2/r3 and flags;
    # their BX LR returns to this precise BLX continuation without stack writes.
    regs[2] = regs[3] = UNKNOWN
    regs[14] = ("continuation", (pc + size) | 1)
    return replace(
        state,
        regs=tuple(regs),
        park=park,
        parked_at_restore=state.parked_at_restore and not state.restored,
    )


def _restore(state: State, value: Value, pc: int, errors: set[str]) -> State:
    mask = value[1] if value[0] == "integer" and value[1] in (0, 1) else None
    if mask is None:
        errors.add("irq-restoration-unresolved")
    if state.park != 2:
        errors.add("irq-final-park-suffix-missing")
    return replace(
        state,
        primask=mask,
        restored=True,
        parked_at_restore=state.park == 2,
        restore_pc=pc,
    )


def _step(
    state: State, instruction: Instruction, effects: dict[int, str], errors: set[str]
) -> State:
    regs, stack = list(state.regs), dict(state.stack)
    op = instruction.op
    if op == "mrs":
        regs[instruction.rd] = (
            _integer(state.primask) if state.primask is not None else UNKNOWN
        )
    elif op == "cpsid":
        return replace(state, primask=1)
    elif op == "cpsie":
        return _restore(state, _integer(0), instruction.pc, errors)
    elif op == "msr":
        return _restore(state, regs[instruction.rn], instruction.pc, errors)
    elif op == "blx":
        value = regs[instruction.rn]
        if value[0] != "callback" or value[1] & 1 == 0 or value[1] & ~1 not in effects:
            raise SemanticError("irq-callback-target-unresolved")
        return _callback(
            state, effects[value[1] & ~1], instruction.pc, instruction.size, errors
        )
    elif op in ("sub", "add"):
        sp = state.sp + (instruction.imm if op == "add" else -instruction.imm)
        if sp > 0 or sp % 4:
            raise SemanticError("irq-stack-alias-unresolved")
        regs[13] = ("stack", sp)
        return replace(state, regs=tuple(regs), sp=sp)
    elif op == "push":
        sp = state.sp - 4 * len(instruction.registers)
        for index, reg in enumerate(instruction.registers):
            stack[sp + index * 4] = regs[reg]
        regs[13] = ("stack", sp)
        return replace(
            state, regs=tuple(regs), stack=tuple(sorted(stack.items())), sp=sp
        )
    elif op == "pop":
        for index, reg in enumerate(instruction.registers):
            regs[reg] = stack.get(state.sp + index * 4, UNKNOWN)
        sp = state.sp + len(instruction.registers) * 4
        regs[13] = ("stack", sp)
        return replace(state, regs=tuple(regs), sp=sp)
    elif op == "str":
        stack[_memory_address(state, instruction)] = regs[instruction.rd]
    elif op in ("ldr", "ldrh"):
        if instruction.rn == 13 and op == "ldr":
            regs[instruction.rd] = stack.get(
                _memory_address(state, instruction), UNKNOWN
            )
        elif regs[instruction.rn][0] == "frame":
            regs[instruction.rd] = UNKNOWN
        else:
            raise SemanticError("irq-stack-alias-unresolved")
    elif op == "movs":
        regs[instruction.rd] = (
            regs[instruction.rn] if instruction.rn >= 0 else _integer(instruction.imm)
        )
    elif op in ("adds", "subs"):
        regs[instruction.rd] = (
            FRAME if op == "adds" and regs[instruction.rd][0] == "frame" else UNKNOWN
        )
    elif op in ("lsls", "lsrs", "asrs", "sxth", "uxth", "uxtb", "ands"):
        regs[instruction.rd] = UNKNOWN
    elif op not in ("cmp", "nop", "bx") and not op.endswith(".n"):
        raise SemanticError("irq-unsupported-instruction")
    return replace(state, regs=tuple(regs), stack=tuple(sorted(stack.items())))


def _outcome(
    state: State, target: Value, entry: int, errors: set[str]
) -> IRQOutcome | None:
    if target != ENTRY_LR:
        errors.add("irq-return-token-unresolved")
        return None
    if state.sp != 0:
        errors.add("irq-stack-return-unproved")
        return None
    parked = state.park == 2 and state.parked_at_restore
    if not state.restored:
        errors.add("irq-restoration-missing")
    if not parked:
        errors.add("irq-final-park-suffix-missing")
    if state.primask is None:
        errors.add("irq-restoration-unresolved")
    elif state.primask != entry:
        errors.add("saved-primask-restoration-lost")
    return IRQOutcome(
        entry, state.primask, state.sp, "entry-lr", parked, state.restore_pc
    )


def _walk(
    instructions: dict[int, Instruction],
    entry: int,
    callbacks: dict[str, int],
    errors: set[str],
) -> list[IRQOutcome]:
    regs = [("entry-reg", n) for n in range(16)]
    regs[0], regs[1] = FRAME, UNKNOWN
    regs[2], regs[3] = (
        ("callback", callbacks["dataHi"] | 1),
        ("callback", callbacks["dataLo"] | 1),
    )
    regs[13], regs[14] = ("stack", 0), ENTRY_LR
    stack = (
        (0, ("callback", callbacks["clockToggle"] | 1)),
        (4, ("callback", callbacks["clockLo"] | 1)),
    )
    pending = deque([(min(instructions), State(tuple(regs), stack, 0, entry))])
    seen = set()
    depths = {}
    returns = set()
    effects = {address: name for name, address in callbacks.items()}
    while pending:
        pc, state = pending.popleft()
        if (pc, state) in seen:
            continue
        seen.add((pc, state))
        if pc not in instructions:
            errors.add("irq-cfg-target-unproved")
            continue
        if pc in depths and depths[pc] != state.sp:
            errors.add("irq-stack-alias-unresolved")
            continue
        depths[pc] = state.sp
        instruction = instructions[pc]
        try:
            after = _step(state, instruction, effects, errors)
        except SemanticError as error:
            errors.add(error.code)
            continue
        if instruction.op == "bx" or (
            instruction.op == "pop" and 15 in instruction.registers
        ):
            target = (
                after.regs[instruction.rn] if instruction.op == "bx" else after.regs[15]
            )
            outcome = _outcome(after, target, entry, errors)
            if outcome is not None:
                returns.add(outcome)
            continue
        if instruction.op.endswith(".n"):
            pending.append((instruction.target, after))
            if instruction.op == "b.n":
                continue
        pending.append((pc + instruction.size, after))
    if not returns:
        errors.add("irq-normal-return-unproved")
    return sorted(
        returns,
        key=lambda result: (
            result.entry_primask,
            result.exit_primask if result.exit_primask is not None else -1,
            result.final_parked,
        ),
    )


def classify_irq_capture(capture: dict) -> IRQResult:
    empty = {0: [], 1: []}
    try:
        route = placement.authenticate_route(capture)
    except placement.RouteAuthenticationError as error:
        return IRQResult(
            ["irq-route-authentication-failed"], empty, [], [error.code], []
        )
    except (KeyError, ValueError, TypeError, OverflowError) as error:
        return IRQResult(
            ["irq-route-authentication-failed"], empty, [], [str(error)], []
        )
    try:
        instructions = {
            pc: _decode_one(pc, raw, opcode, operands)
            for pc, raw, opcode, operands in placement._instructions(route.kernel[1])
        }
        if not instructions:
            raise IRQDecodeError("irq-normal-return-unproved")
        for instruction in instructions.values():
            if instruction.target >= 0 and instruction.target not in instructions:
                raise IRQDecodeError("irq-cfg-target-unproved")
    except (IRQDecodeError, ValueError) as error:
        code = (
            error.code
            if isinstance(error, IRQDecodeError)
            else "irq-unsupported-instruction"
        )
        return IRQResult([code], empty, [], [], [code])
    suffixes = {
        "dataHi": "my9221RpPin2_hi()",
        "dataLo": "my9221RpPin2_lo()",
        "clockLo": "my9221RpPin3_lo()",
        "clockToggle": "my9221RpPin3_toggle()",
    }
    callbacks = {
        effect: placement._one(route.callbacks, suffix)[1][0]
        for effect, suffix in suffixes.items()
    }
    errors: set[str] = set()
    normal = {entry: _walk(instructions, entry, callbacks, errors) for entry in (0, 1)}
    counterexamples = [
        outcome
        for outcomes in normal.values()
        for outcome in outcomes
        if outcome.exit_primask is not None
        and outcome.exit_primask != outcome.entry_primask
    ]
    return IRQResult(sorted(errors), normal, counterexamples, [], [])


def validate_stage_analysis(stage: str, result: IRQResult) -> None:
    """Validate an internally obtained analysis; this alone authenticates no build."""
    if stage not in ("baseline", "unconditional-enable-mutant", "restored"):
        raise ValueError("unknown fixed IRQ stage")
    if (
        not isinstance(result, IRQResult)
        or result.authentication_errors
        or result.decode_errors
    ):
        raise ValueError("IRQ stage authentication/decode failed")
    if set(result.normal_returns) != {0, 1} or any(
        type(k) is not int for k in result.normal_returns
    ):
        raise ValueError("IRQ stage requires both entry masks")
    for entry, outcomes in result.normal_returns.items():
        if not outcomes:
            raise ValueError("IRQ stage has no normal returns")
        for outcome in outcomes:
            if (
                not isinstance(outcome, IRQOutcome)
                or type(outcome.entry_primask) is not int
                or outcome.entry_primask != entry
                or type(outcome.exit_primask) is not int
                or outcome.exit_primask not in (0, 1)
                or type(outcome.sp_delta) is not int
                or outcome.sp_delta != 0
                or outcome.return_token != "entry-lr"
                or outcome.final_parked is not True
                or type(outcome.restore_pc) is not int
                or outcome.restore_pc <= 0
                or outcome.restore_pc % 2
            ):
                raise ValueError("invalid IRQ normal-return outcome")
    outcomes = [
        outcome for values in result.normal_returns.values() for outcome in values
    ]
    mismatches = {
        outcome for outcome in outcomes if outcome.entry_primask != outcome.exit_primask
    }
    if stage == "unconditional-enable-mutant":
        if (
            result.errors != ["saved-primask-restoration-lost"]
            or any(outcome.exit_primask != 0 for outcome in outcomes)
            or not mismatches
            or set(result.counterexamples) != mismatches
        ):
            raise ValueError(
                "IRQ mutant lacks matching actual saved-mask counterexamples"
            )
    elif result.errors or result.counterexamples or mismatches:
        raise ValueError("IRQ baseline/restored does not preserve both entry masks")
