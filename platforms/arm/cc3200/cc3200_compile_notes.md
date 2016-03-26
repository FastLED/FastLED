For the CC3200, this was designed to use the CC3200 drivers & includes with the TI Code Composer Studio.

Please compile this with the GNU GCC compiler. A few requirements to set it up:
	Create a new configuration for GNU GCC compiler (instead of TI Compiler).
		Use the Project Explorer menu at the left: click "Show Build Settings." In the left pane, select CCS General. If CC3200 isn't already selected, select "Wireless Connectivity MCUs" from the variant dropdown, then CC3200 to its right. Under compiler version, select GNU (the compiler with the highest version number).
	Make sure to use the g++ compiler. There's some quirks that don't seem to resolve well with the C (GCC) compiler
		To do this, use the Project Explorer menu at the left: click "Show Build Settings." Click the "Variables" or "Build Variables" tab. Click the checkbox at the bottom to "Show system variables". Double-click the "CG_TOOL_GCC" to change the variable value. Modify the file that it is referring to, to end with g++.exe from gcc.exe. So it should end with "arm-none-eabi-g++.exe"
	Add Include Directories
		You will need the core CC3200 libraries (included in the TI CC3200 SDK) to ensure proper compilation
		Use the Project Explorer menu at the left: click "Show Build Settings." In the "GNU Compiler" dropdown in the left pane, select Directories. In the main box, use the Add Directory (the plus button) to add the following directories (browse to them as needed):
			[path to CC3200 SDK]\driverlib
			[path to CC3200 SDK]\example\common
			[path to CC3200 SDK]\inc
	Change the compiler flags/defines
		We will need to change some compiler flags to get the ones we want and need for proper compilation. This config has been validated, others may or may not work.
		Use the Project Explorer menu at the left: click "Show Build Settings." In the left pane, open the GNU Compiler dropdown, and select the "Optimization" page.
			Using the "Optimization Level" dropdown, select "Optimize most -O3"
			Click the checkbox "Optimize for space rather than speed -Os"
		Click the symbols page on the left tab.
			In the "Define Symbols" window, click the "Add..." button to add symbols. Add a symbol called "CC3200"
		Click the miscellaneous page on the left tab
			In the bottom half of the page that opens, double-click the line beginning "-std=", and set it to "-std=c++11"
Check if needed:			Add a new flag with the value "-fpermissive"
