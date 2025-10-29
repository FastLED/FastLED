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
  const startTime = Date.now();

  // Hook to PORTB register for LED output
  runner.portB.addListener(value => {
    const time = formatTime(runner.cpu.cycles / runner.MHZ);
    if (options.verbose) {
      console.log(`[${time}] PORTB: 0x${value.toString(16).padStart(2, '0')}`);
    }
  });

  // Serial port output support
  runner.usart.onByteTransmit = (value: number) => {
    const char = String.fromCharCode(value);
    output += char;
    process.stdout.write(char);
  };

  // Execute with periodic checks
  try {
    await runner.execute(cpu => {
      const simTime = cpu.cycles / runner.MHZ;
      if (simTime > (options.timeout || 30)) {
        if (options.verbose) {
          const time = formatTime(cpu.cycles / runner.MHZ);
          console.error(`\n[${time}] Timeout reached after ${options.timeout}s simulated time`);
        }
        runner.stop();
      }
    });
  } catch (err) {
    console.error(`\nExecution error: ${err}`);
    return 1;
  }

  const executionTime = (Date.now() - startTime) / 1000;
  const simTime = runner.cpu.cycles / runner.MHZ;
  const cyclesPerSec = runner.cpu.cycles / executionTime;
  const perfPercent = (cyclesPerSec / runner.MHZ * 100).toFixed(1);

  if (options.verbose) {
    console.log(`\n--- Execution Complete ---`);
    console.log(`Wall time: ${executionTime.toFixed(3)}s`);
    console.log(`Simulated time: ${formatTime(simTime)}`);
    console.log(`CPU cycles: ${runner.cpu.cycles}`);
    console.log(`Performance: ${(cyclesPerSec / 1e6).toFixed(2)}M cycles/sec (${perfPercent}% of 16MHz)`);
  }

  // Check for required test output
  if (!output.includes("Test loop")) {
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
