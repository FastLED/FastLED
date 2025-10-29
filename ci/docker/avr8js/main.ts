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
  --timeout <seconds>   Maximum execution time (default: 10)
  --verbose            Enable verbose output
  -h, --help           Show this help message

Example:
  npm start build/output.hex --timeout 5
`);
    exit(args.includes("--help") || args.includes("-h") ? 0 : 1);
  }

  const options: RunOptions = {
    hexFile: args[0],
    timeout: 10,
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
  try {
    await runner.execute(cpu => {
      callbackCount++;
      if (callbackCount % 1000 === 0) {
        const time = formatTime(cpu.cycles / runner.MHZ);
        console.log(`[DEBUG][${time}] Execute callback #${callbackCount}, cycles=${cpu.cycles}`);
      }

      const elapsed = Date.now() - startTime;
      if (elapsed > timeoutMs) {
        const time = formatTime(cpu.cycles / runner.MHZ);
        console.error(`\n[DEBUG][${time}] Timeout reached after ${options.timeout}s`);
        if (options.verbose) {
          console.error(`[${time}] Timeout reached after ${options.timeout}s`);
        }
        runner.stop();
      }

      // Also stop if no output for 2 seconds (assuming program finished)
      const silentTime = Date.now() - lastOutputTime;
      if (output.length > 0 && silentTime > 2000) {
        const time = formatTime(cpu.cycles / runner.MHZ);
        console.error(`\n[DEBUG][${time}] No output for 2s, assuming completion`);
        if (options.verbose) {
          console.error(`[${time}] No output for 2s, assuming completion`);
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

  console.log(`\n[DEBUG] --- Execution Complete ---`);
  console.log(`[DEBUG] Wall time: ${executionTime.toFixed(3)}s`);
  console.log(`[DEBUG] Simulated time: ${formatTime(simTime)}`);
  console.log(`[DEBUG] CPU cycles: ${runner.cpu.cycles}`);
  console.log(`[DEBUG] Output length: ${output.length} bytes`);
  console.log(`[DEBUG] Output content: "${output}"`);
  console.log(`[DEBUG] Output (hex): ${Array.from(output).map(c => c.charCodeAt(0).toString(16).padStart(2, '0')).join(' ')}`);

  if (options.verbose) {
    console.log(`\n--- Execution Complete ---`);
    console.log(`Wall time: ${executionTime.toFixed(3)}s`);
    console.log(`Simulated time: ${formatTime(simTime)}`);
    console.log(`CPU cycles: ${runner.cpu.cycles}`);
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
