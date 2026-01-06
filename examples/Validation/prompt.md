This is an sample ai prompt to allow fully automated testing of the parlio driver. Adapt for your own needs.


## AGENT LOOP INSTRUCTIONS

**Two sections of instructions: read only and writable

  * All agents are NOT to change what's between <BEGIN-READ-ONLY> and </BEGIN-READ-ONLY>
  * Agents are allowed to change what's between <BEGIN-WRITEABLE> and </BEGIN-WRITEABLE>

First section of this document will be read only, the next section will be writable for agents to change.


<BEGIN-READ-ONLY>

## Objective: Test parlio driver with multiple lanes

  * Make Parlio only USE MSB
  * when something goes wrong, re-read the tests and then on the next test dump out the raw edge timings from the RX device to see what the signal looks like.

## First agent instructions

  * before works begins, please modify validation.ino to JUST_PARLIO (this will disable SPI and RMT)
  * please run validation test with all expected keywords and confirm this works. if it doesn't then halt immediatly and report and obverseer will manually fix the issue and restart the agent loop.


## Phases

  * Phase 1: Test parlio driver with 1 lanes and small leds (10 leds)
    * 1a: test with 3000 leds
  * Phase 2: Test parlio driver with 2 lanes and small leds (10 leds)
  * Phase 3: Test parlio driver with 2 lanes and large leds (3000 leds)
  * Phase 4: Test parlio driver with 4 lanes and small leds (10 leds)
  * Phase 5: Test parlio driver with 4 lanes and large leds (3000 leds)
  * Phase 6: Test parlio driver with 8 lanes and small leds (10 leds)
  * Phase 7: Test parlio driver with 8 lanes and large leds (3000 leds)


For each of these phases, i want a sub document in .agent_task/phase/1.md, etc..

All the notes about phase 1 should be in the .agent_task/phase/1.md file.
All the notes about phase 2 should be in the .agent_task/phase/2.md file.
etc...

Keep the summary at .agent_task/ITER*.md terse by offloading the summary to the .agent_task/phase/*.md files.

When phase has completed, please update the .agent_task/LOOP.md with your findings.
The .agent_task/LOOP.md will declare what phase we are currently in at all times. Try to keep .agent_task/LOOP.md as terse as possible. This is a long running task!!!!

Once a phase is ready for completion, the agent will mark it in .agent_task/LOOP.md as REVIEW REQUESTED FOR MOVING FROM PHASE X TO PHASE Y.

At the start of the agent loop, the current agent will check the .agent_task/LOOP.md for any REVIEW REQUESTED FOR MOVING FROM PHASE X TO PHASE Y. If the agent sees this then they become special, it's a TRANSITION REVIEW AGENT, see next section for instructions on this agent.


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
        * .agent_task/LOOP.md file


Then after this purging has been done, check to see if the current transition refview summary requests that there is a fix that needs to be made in code:

  * Launch sub agents for each file that needs to be fixed.
  * Then compile the code with this command:
    * `bash validate` and fix any errors that occur


After the compile state is clean, clear the error by setting:

CURRENT PHASE ERROR: NONE
CURRENT PHASE ERROR REVIEW SUMMARY: NONE

Then halt.



Run

bash debug Validation --expect "TX Pin: 0" --expect "RX Pin: 1" --expect "DRIVER_ENABLED: PARLIO" --fail-on ERROR
</BEGIN-READ-ONLY>

---------------------------------------------------------

<BEGIN-WRITEABLE>


## CURRENT PHASE STATUS

PHASE: 0
TRANSITION REQUESTED: NO
CURRENT PHASE ERROR: NONE
CURRENT PHASE ERROR REVIEW SUMMARY: NONE


## Background notes about previous attempts

the agent said that the PARLIO driver is mixing all bits to one pin???!!!

So I thought that data was transposed into one lane and that the driver would handle demuxing the data into the correct lane.

This may not be correct at this case. The agent is encourage to research this and put it at .agent_task/PARLIO_RESEARCH.md under the appropriate header.

Iterations 1 and 2 need to use multiple parallel sub agents and research the PARLIO driver from espressif, store this in .agent_task/PARLIO_RESEARCH.md.

## First task:

First iteration agent shall change validation.ino so that it is multilane == 2, SPI and RMT are disabled. No other changes are necessary. Once this is done then mark this as complete.
