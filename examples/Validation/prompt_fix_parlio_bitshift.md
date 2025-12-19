This is an ai prompt to fix the 1-bit shift buffer alignment issue in the PARLIO driver.


## AGENT LOOP INSTRUCTIONS

**Two sections of instructions: read only and writable

  * All agents are NOT to change what's between <BEGIN-READ-ONLY> and </BEGIN-READ-ONLY>
  * Agents are allowed to change what's between <BEGIN-WRITEABLE> and </BEGIN-WRITEABLE>

First section of this document will be read only, the next section will be writable for agents to change.


<BEGIN-READ-ONLY>

## Objective: Fix 1-bit shift error caused by buffer boundary misalignment

## Critical Constraint

**AGENTS ARE NOT ALLOWED TO MODIFY `trans_queue_depth` IN ANY WAY**

The `trans_queue_depth = 3` setting is MANDATORY and MUST NOT be changed. This value:
- Matches the 3 ring buffer count
- Allows all 3 buffers to be pre-queued without queue full errors
- Was established in LOOP.md as the correct configuration

**ONLY BUFFER SIZE ALIGNMENT IS ALLOWED**

Agents may ONLY modify buffer size calculations to ensure proper byte alignment.


## Problem Statement

**Current Issue**: 1-bit shift corruption in transmitted LED data

**Root Cause Analysis** (from commit 4fd8a0c08):
- Buffer splits occur on non-byte boundaries
- PARLIO hardware interprets inter-buffer gaps as bit-level shifts
- Per WLED-MM developer: "I also break my buffers so it's after the LSB of the triplet (or quad)"
- Byte boundaries are critical - nibble splits trigger corruption on any order bit

**Evidence**:
- Multi-run validation shows consistent bit shift errors
- Pattern: RGB(240,15,170) transmitted as corrupted values
- Shift happens at buffer boundaries, not uniformly across transmission

**Correct Approach**:
- Buffer sizes MUST align to LED byte boundaries (multiples of 3 bytes for RGB)
- Split ONLY after complete LED triplets (R,G,B) - never mid-LED
- Minimize risk: If 20µs gap is misinterpreted, worst case is LSB corruption (0,0,0 → 0,0,1)


## Instructions for progress

  * we are on a branch, so it's okay to commit changes to this branch on every transition to the next phase
  * `git add src examples`
  * `git commit -m "phase X: buffer alignment fix"`


## Detecting bad progress

  * The test to transmit 1000 leds no longer completes at all.
    * If test completes but with LED bit shift errors, that's actually ok (we're fixing those). It's the test not completing at all that's the problem.
    * when this happens and the test won't even complete, such as watchdog error, or the test seems to halt, do a `git reset --hard HEAD~1`. And back up one to the last working commit. Then try again to ensure the last iteration works.
    * When to NOT `git reset --hard HEAD~1`: commit `4fd8a0c08b7eb2bccfeabf89ca9c90b50ab634c6` is the reset commit point, do not go beyond this. If you absolutely need to then write ./RESOLUTION.md and then halt.


## First agent instructions

**Phase 1 Start - Analyze Buffer Size Calculation**:

1. Read `src/platforms/esp/32/drivers/parlio/channel_engine_parlio.cpp` lines 1100-1130:
   - Understand current buffer size calculation
   - Identify where byte alignment happens (or doesn't happen)
   - Document in `.agent_task/phase/1.md`

2. Read `src/platforms/esp/32/drivers/parlio/channel_engine_parlio.cpp` lines 603-639:
   - Study `populateNextDMABuffer()` byte splitting logic
   - Line 605: `bytes_per_buffer = (mState.mIsrContext->mTotalBytes + PARLIO_RING_BUFFER_COUNT - 1) / PARLIO_RING_BUFFER_COUNT`
   - Document how buffers are split (is it LED-aligned?)

3. Calculate alignment for 1000 LEDs test case:
   - Total bytes: 1000 LEDs × 3 bytes/LED = 3000 bytes
   - Current split: 3000 ÷ 3 = 1000 bytes per buffer
   - LED alignment check: 1000 bytes ÷ 3 bytes/LED = 333.33 LEDs ⚠️ **NOT ALIGNED**
   - Buffer 0 ends mid-LED: After byte 999 (LED 332, byte 2 of 3) ❌

4. Document findings in `.agent_task/phase/1.md`


## Phases

### Phase 1: Analyze Buffer Alignment Issue

**Objective**: Understand where non-byte-aligned splits occur

**Method**: Code analysis + calculation
- Study buffer size calculation (lines 1100-1130)
- Study buffer population logic (lines 603-639)
- Calculate byte alignment for 1000 LED test case
- Identify exact line where alignment breaks

**Deliverable**: `.agent_task/phase/1.md` with:
- Buffer size calculation formula
- Example: 1000 LEDs → buffer split analysis
- Root cause: Line X causes non-aligned split

**Success**: Clear understanding of alignment problem → Move to Phase 2

---

### Phase 2: Design LED-Aligned Buffer Split

**Objective**: Modify buffer split to align on LED boundaries

**Method**: Algorithm design
- Design formula to round buffer sizes to multiples of 3 bytes
- Example: 1000 bytes → round down to 999 bytes (333 LEDs)
- Ensure total bytes still covered (handle remainder in last buffer)

**Constraints**:
- **DO NOT modify `trans_queue_depth = 3`** (forbidden)
- **ONLY modify buffer size calculations**
- Must handle edge cases (strip sizes not divisible by 3×RING_BUFFER_COUNT)

**Deliverable**: `.agent_task/phase/2.md` with:
- Proposed buffer alignment algorithm
- Test cases: 1000 LEDs, 300 LEDs, 10 LEDs
- Verification: All splits occur on LED boundaries

**Success**: Algorithm verified on paper → Move to Phase 3

---

### Phase 3: Implement Buffer Alignment Fix

**Objective**: Apply LED-aligned buffer split in code

**Method**: Code modification
- Modify `populateNextDMABuffer()` line 605-606 (or nearby)
- Add rounding logic: `bytes_per_buffer = round_down_to_multiple_of_3(bytes_per_buffer)`
- Test compilation: `bash compile esp32c6 Validation`

**Implementation Example** (pseudocode):
```cpp
// Ensure buffer splits on LED boundaries (3 bytes per RGB LED)
size_t bytes_per_buffer = (total_bytes + RING_BUFFER_COUNT - 1) / RING_BUFFER_COUNT;
bytes_per_buffer = (bytes_per_buffer / 3) * 3;  // Round down to multiple of 3
```

**Deliverable**: `.agent_task/phase/3.md` with:
- Exact code changes (file, line numbers)
- Compilation result
- Before/after buffer split example

**Success**: Code compiles cleanly → Move to Phase 4

---

### Phase 4: Validate with 1000 LED Test

**Objective**: Verify fix eliminates bit shift errors

**Method**: Run multi-run validation test
- Keep `LONG_STRIP_SIZE 1000`
- Run: `bash debug Validation --timeout 180 --no-fail-on`
- Analyze multi-run results (10 runs)

**Expected Behavior**:
- All 10 runs PASS (no bit shift errors)
- LED data transmitted correctly: RGB(240,15,170) → RGB(240,15,170)
- No corruption at buffer boundaries

**Deliverable**: `.agent_task/phase/4.md` with:
- Test output (pass/fail per run)
- Error analysis (if any remain)
- Confirmation: Buffer splits align to LED boundaries

**Success**: All 10 runs pass with 1000 LEDs → Move to Phase 5

---

### Phase 5: Stress Test with 3000 LEDs

**Objective**: Validate fix works with larger strip requiring true streaming

**Method**: Increase strip size beyond ring buffer capacity
- Modify `examples/Validation/ValidationConfig.h`: Set `LONG_STRIP_SIZE 3000`
- Run: `bash debug Validation --timeout 180 --no-fail-on`

**Expected Behavior**:
- 3000 LEDs = 9000 bytes (exceeds 3-buffer capacity)
- ISR streaming kicks in (mPreQueuedBuffers=false path)
- All buffers align to LED boundaries
- All 10 runs PASS

**Deliverable**: `.agent_task/phase/5.md` with:
- Test results for 3000 LEDs
- ISR callback count (verify streaming occurred)
- Confirmation: No bit shift errors with streaming

**Success**: All runs pass with 3000 LEDs → Move to Phase 6

---

### Phase 6: Multi-Lane Validation (Optional)

**Objective**: Ensure fix works across multiple lanes

**Method**: Progressive multi-lane testing
- Phase 6a: 2 lanes, 1000 LEDs
- Phase 6b: 4 lanes, 1000 LEDs
- Phase 6c: 8 lanes, 300 LEDs (hardware limit)

**Deliverable**: `.agent_task/phase/6.md` with:
- Results for each lane configuration
- Validation: Lane 0 data always correct

**Success**: All configurations pass → COMPLETE


For each of these phases, i want a sub document in .agent_task/phase/1.md, etc..

All the notes about phase 1 should be in the .agent_task/phase/1.md file.
All the notes about phase 2 should be in the .agent_task/phase/2.md file.
etc...

Keep the summary at .agent_task/ITER*.md terse by offloading the summary to the .agent_task/phase/*.md files.

When phase has completed, please update the .agent_task/LOOP2.md with your findings.
The .agent_task/LOOP2.md will declare what phase we are currently in at all times. Try to keep .agent_task/LOOP2.md as terse as possible. This is a long running task!!!!

Once a phase is ready for completion, the agent will mark it in .agent_task/LOOP2.md as REVIEW REQUESTED FOR MOVING FROM PHASE X TO PHASE Y.

At the start of the agent loop, the current agent will check the .agent_task/LOOP2.md for any REVIEW REQUESTED FOR MOVING FROM PHASE X TO PHASE Y. If the agent sees this then they become special, it's a TRANSITION REVIEW AGENT, see next section for instructions on this agent.


If the subsequent agent is successful fixes the issue then it will mark the current phase as the one under development still and allow the transiton back to the next phase to happen naturally (see above).

##### TRANSITION REVIEW AGENT INSTRUCTIONS

**Triggered on**

"TRANSITION REQUESTED: YES"

**Summary**:

The only job of this agent will be to evaluate the requested transition. It will make a ruling on whether the transition is allowed or not.


  * If allowed: Update the current phase to the requested phase and then halt.
  * If not allowed: Update the current phase to phase X, add in the note on the current phase a transition was requested but rejected because of the following reasons:
    * Cheating: Did the agent declare something was incompatable with the driver and then just disable it?
    * Subversion: Something was changed without proper justification and it needs validation.
    * Buffer Size Violation: Agent modified buffer capacity instead of alignment logic
    * trans_queue_depth Violation: Agent modified trans_queue_depth (FORBIDDEN)
    * Other: Any other reason that the agent thinks is valid.

**On ERROR/FAILURE**:

CURRENT PHASE ERROR: CHEATING (and or subversion and/or other)
CURRENT PHASE ERROR REVIEW SUMMARY:
<INSERT SUMMARY HERE>


**On Success**:

PHASE: section will be updated to the new phase.
CURRENT PHASE ERROR: NONE
CURRENT PHASE ERROR REVIEW SUMMARY: NONE

Please put your success review message in the .agent_task/phase/X_review.md file.


##### TRANSITION FIXIT AGENT INSTRUCTIONS

**Not Triggered on**

CURRENT PHASE ERROR: NONE

**Triggered on**

CURRENT PHASE ERROR: (ANYTHING BUT NONE)

**Sub agent prompt summary**:

You are an agent that is responsible to clear the errror state on a transition attempt.

Look in the CURRENT PHASE ERROR REVIEW SUMMARY: section and identify the error.

You will do the following:
  * identify the current phase, call it X
  * you will update .agent_task/PHASE/X.md where X is the current phase
    * find misinformation identified in the error summary your job is to fix it
      * using parallel sub agents to aggressively purge that misinformation/error status from all md files:
        * .agent_task/ITER*.md files
        * .agent_task/PHASE/X.md file
        * .agent_task/LOOP2.md file


Then after this purging has been done, check to see if the current transition refview summary requests that there is a fix that needs to be made in code:

  * Launch sub agents for each file that needs to be fixed.
  * Then compile the code with this command:
    * `bash compile esp32c6 Validation` and fix any errors that occur


After the compile state is clean, clear the error by setting:

CURRENT PHASE ERROR: NONE
CURRENT PHASE ERROR REVIEW SUMMARY: NONE

Then halt.



Run

bash debug Validation --expect "TX Pin: 0" --expect "RX Pin: 1" --expect "DRIVER_ENABLED: PARLIO" --timeout 180 --no-fail-on
</BEGIN-READ-ONLY>

---------------------------------------------------------

<BEGIN-WRITEABLE>


## CURRENT PHASE STATUS

PHASE: 0
TRANSITION REQUESTED: NO
TRANSITION TO PHASE: N/A
CURRENT PHASE ERROR: NONE
CURRENT PHASE ERROR REVIEW SUMMARY: NONE


## Background: The 1-Bit Shift Buffer Alignment Problem

**From LOOP.md Analysis**:
- LOOP.md successfully validated 1000 LEDs with pre-queue strategy
- `trans_queue_depth = 3` established as correct configuration
- `mPreQueuedBuffers` fix prevents gaps when all data fits in ring buffer

**New Problem Discovered** (commit 4fd8a0c08):
- Multi-run validation shows consistent 1-bit shift errors
- Pattern data corrupted at buffer boundaries
- Example: RGB(240,15,170) → corrupted values

**Root Cause** (from WLED-MM developer insight):
> "I also break my buffers so it's after the LSB of the triplet (or quad). In case that pause is
> misinterpreted as a 1 vs 0, it's not going to cause a visual disruption. At worst 0,0,0 becomes 0,0,1."

**Translation**:
- PARLIO hardware interprets inter-buffer gaps as potential bit-level shifts
- Buffer splits MUST occur on LED boundaries (after complete RGB triplet)
- Current code: Splits on arbitrary byte boundaries (e.g., mid-LED)

**Calculation for 1000 LEDs**:
- Total bytes: 1000 LEDs × 3 bytes = 3000 bytes
- Current split: 3000 ÷ 3 buffers = 1000 bytes/buffer
- LED alignment: 1000 ÷ 3 = 333.33 LEDs ⚠️ **MISALIGNED**
- Buffer 0 ends: Byte 999 (middle of LED 333) ❌
- Buffer 1 starts: Byte 1000 (middle of LED 333) ❌

**Required Fix**:
- Round buffer sizes to multiples of 3 bytes (LED boundaries)
- Example: 1000 bytes → 999 bytes (333 LEDs, aligned)
- Last buffer absorbs remainder


## Important Research Links

### WLED-MM PARLIO Driver Discussion

From: https://github.com/FastLED/FastLED/issues/2095

Key insights from WLED-MM developer:
- Supports 1/2/4/8/16-bit depths with ~5460 RGB LEDs per transmit
- Buffer breaks placed after LSB of RGB triplet to minimize visual corruption
- 20µs gap tolerance (below 50µs spec) prevents glitches
- If gap misinterpreted, worst case: LSB corruption (0,0,0 → 0,0,1)

### Reddit Demo

https://www.reddit.com/r/WLED/s/q1pZg1mnwZ

16×512 pixel demo showing PARLIO in action with proper buffer alignment.


## Critical Constraints for Agents

**FORBIDDEN MODIFICATIONS**:
1. ❌ **DO NOT modify `trans_queue_depth`** (must stay at 3)
2. ❌ **DO NOT change ring buffer count** (must stay at 3)
3. ❌ **DO NOT change buffer capacity** (18KB per buffer is correct)

**ALLOWED MODIFICATIONS**:
1. ✅ **Modify buffer split logic** (line ~605 in populateNextDMABuffer)
2. ✅ **Add LED boundary alignment** (round to multiples of 3 bytes)
3. ✅ **Adjust byte_count calculation** (ensure LED-aligned chunks)

**Key Code Locations**:
- `src/platforms/esp/32/drivers/parlio/channel_engine_parlio.cpp:605` - Buffer split calculation
- `src/platforms/esp/32/drivers/parlio/channel_engine_parlio.cpp:1218` - Ring buffer capacity
- `examples/Validation/ValidationConfig.h:25` - Test strip size


## First Task

First iteration agent shall:

1. **Analyze current buffer split logic** (lines 603-639 in channel_engine_parlio.cpp)
2. **Calculate alignment** for 1000 LED test case (show the math)
3. **Document findings** in `.agent_task/phase/1.md`
4. **Propose fix** in `.agent_task/phase/2.md` (design phase, no code yet)

**DO NOT MODIFY CODE YET** - Phase 1 is analysis only.
