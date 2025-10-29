import { AVRRunner } from "./execute";
import { formatTime } from "./format-time";
import { readFileSync } from "fs";
import { exit } from "process";

interface RunOptions {
  hexFile: string;
  timeout?: number; // timeout in seconds
  verbose?: boolean;
}

function parseArgs(): RunOptions {
  const args = process.argv.slice(2);

  if (args.length === 0 || args.includes("--help") || args.includes("-h")) {
    console.log(`
AVR8JS Test Runner - Execute AVR hex files in simulation

Usage: npm start <hex-file> [options]

Arguments:
  hex-file              Path to Intel HEX file to execute

Options:
  --timeout <seconds>   Maximum simulated time in seconds (default: 30)
  --verbose            Enable verbose output
  -h, --help           Show this help message

Example:
  npm start build/output.hex --timeout 5
`);
    exit(args.includes("--help") || args.includes("-h") ? 0 : 1);
  }

  const options: RunOptions = {
    hexFile: args[0],
    timeout: 30,
    verbose: false
  };

  for (let i = 1; i < args.length; i++) {
    if (args[i] === "--timeout" && i + 1 < args.length) {
      options.timeout = parseFloat(args[++i]);
    } else if (args[i] === "--verbose") {
      options.verbose = true;
    }
  }

  return options;
}

async function executeProgram(hex: string, options: RunOptions): Promise<number> {
  const runner = new AVRRunner(hex);
  let output = "";
  let lastOutputTime = Date.now();
  const startTime = Date.now();
  const timeoutMs = (options.timeout || 10) * 1000;

  console.log("[DEBUG] Setting up PORTB listener...");
  // Hook to PORTB register for LED output
  runner.portB.addListener(value => {
    const time = formatTime(runner.cpu.cycles / runner.MHZ);
    console.log(`[DEBUG][${time}] PORTB callback invoked: 0x${value.toString(16).padStart(2, '0')}`);
    if (options.verbose) {
      console.log(`[${time}] PORTB: 0x${value.toString(16).padStart(2, '0')}`);
    }
  });

  console.log("[DEBUG] Setting up USART onByteTransmit callback...");
  let usartCallbackCount = 0;
  // Serial port output support
  runner.usart.onByteTransmit = (value: number) => {
    try {
      usartCallbackCount++;
      const char = String.fromCharCode(value);
      const time = formatTime(runner.cpu.cycles / runner.MHZ);
      const wallTime = ((Date.now() - startTime) / 1000).toFixed(3);
      console.log(`[DEBUG][#${usartCallbackCount}, sim=${time}, wall=${wallTime}s] USART byte: 0x${value.toString(16).padStart(2, '0')} '${char.replace(/\r/g, '\\r').replace(/\n/g, '\\n')}'`);
      output += char;
      // Temporarily comment out to test
      // process.stdout.write(char);
      lastOutputTime = Date.now();
    } catch (err) {
      console.error(`[DEBUG] USART callback error: ${err}`);
      throw err;
    }
  };

  // Execute with periodic checks
  console.log("[DEBUG] Starting CPU execution...");
  let callbackCount = 0;
  let firstCheckpointTime = 0;
  try {
    await runner.execute(cpu => {
      callbackCount++;
      const time = formatTime(cpu.cycles / runner.MHZ);

      // Performance checkpoint at 50k cycles
      if (cpu.cycles >= 50000 && firstCheckpointTime === 0) {
        firstCheckpointTime = Date.now();
        const elapsed = (firstCheckpointTime - startTime) / 1000;
        const cyclesPerSec = cpu.cycles / elapsed;
        const targetCycles = runner.MHZ; // 16MHz
        const perfPercent = (cyclesPerSec / targetCycles * 100).toFixed(1);
        console.log(`[DEBUG][${time}] === 50k CYCLE CHECKPOINT ===`);
        console.log(`[DEBUG] Wall time: ${elapsed.toFixed(3)}s`);
        console.log(`[DEBUG] Cycles/sec: ${(cyclesPerSec / 1e6).toFixed(2)}M (${perfPercent}% of 16MHz target)`);
        console.log(`[DEBUG] Target for 30s: 480M cycles (16MHz * 30s)`);
      }

      if (callbackCount <= 10 || callbackCount % 10 === 0) {
        const interruptsEnabled = (cpu.data[95] & 0x80) !== 0; // SREG I bit
        console.log(`[DEBUG][${time}] Callback #${callbackCount}, cycles=${cpu.cycles}, txEnable=${runner.usart.txEnable}, IE=${interruptsEnabled}`);
      }

      const simTime = cpu.cycles / runner.MHZ;
      if (simTime > (options.timeout || 30)) {
        console.error(`\n[DEBUG][${time}] Timeout reached after ${options.timeout}s simulated time`);
        if (options.verbose) {
          console.error(`[${time}] Timeout reached after ${options.timeout}s simulated time`);
        }
        runner.stop();
      }
    });
    console.log(`[DEBUG] Execution completed. Total callbacks: ${callbackCount}`);
  } catch (err) {
    console.error(`\n[DEBUG] Execution error: ${err}`);
    console.error(`\nExecution error: ${err}`);
    return 1;
  }

  const executionTime = (Date.now() - startTime) / 1000;
  const simTime = runner.cpu.cycles / runner.MHZ;
  const cyclesPerSec = runner.cpu.cycles / executionTime;
  const perfPercent = (cyclesPerSec / runner.MHZ * 100).toFixed(1);
  const simToWallRatio = (simTime / executionTime).toFixed(2);

  console.log(`\n[DEBUG] --- Execution Complete ---`);
  console.log(`[DEBUG] Wall time: ${executionTime.toFixed(3)}s`);
  console.log(`[DEBUG] Simulated time: ${formatTime(simTime)}`);
  console.log(`[DEBUG] CPU cycles: ${runner.cpu.cycles}`);
  console.log(`[DEBUG] Performance: ${(cyclesPerSec / 1e6).toFixed(2)}M cycles/sec (${perfPercent}% of 16MHz target)`);
  console.log(`[DEBUG] Sim/Wall ratio: ${simToWallRatio}x (${simToWallRatio > 1 ? 'faster than real-time' : 'slower than real-time'})`);
  console.log(`[DEBUG] Output length: ${output.length} bytes`);
  console.log(`[DEBUG] Output content: "${output}"`);
  console.log(`[DEBUG] Output (hex): ${Array.from(output).map(c => c.charCodeAt(0).toString(16).padStart(2, '0')).join(' ')}`);

  if (options.verbose) {
    console.log(`\n--- Execution Complete ---`);
    console.log(`Wall time: ${executionTime.toFixed(3)}s`);
    console.log(`Simulated time: ${formatTime(simTime)}`);
    console.log(`CPU cycles: ${runner.cpu.cycles}`);
    console.log(`Performance: ${(cyclesPerSec / 1e6).toFixed(2)}M cycles/sec (${perfPercent}% of 16MHz)`);
  }

  // Check for required test output
  if (!output.includes("Test loop")) {
    console.error('\n[DEBUG] ERROR: "Test loop" not found in output');
    console.error('[DEBUG] Output was:', JSON.stringify(output));
    console.error('\nERROR: "Test loop" not found in output');
    console.error('This likely indicates the test did not execute properly');
    return 1;
  }

  return 0;
}

async function main() {
  const options = parseArgs();

  // Read hex file
  let hexContent: string;
  try {
    hexContent = readFileSync(options.hexFile, "utf-8");
  } catch (err) {
    console.error(`Error reading hex file '${options.hexFile}': ${err}`);
    exit(1);
  }

  if (options.verbose) {
    console.log(`Executing: ${options.hexFile}`);
    console.log(`Timeout: ${options.timeout}s\n`);
  }

  // Execute program
  const exitCode = await executeProgram(hexContent, options);
  exit(exitCode);
}

// Run main function
main().catch(err => {
  console.error(`Fatal error: ${err}`);
  exit(1);
});
