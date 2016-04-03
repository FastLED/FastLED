*For the CC3200, this was originally designed to use the CC3200 drivers & includes with the TI Code Composer Studio. However, due to some errors as of 4/3/16, decided to compile using Energia libraries instead. Think errors have to do with Energia's makefile process vs normal GCC compile*

#Compiling w/ Energia & CCS#

This project can be compiled in Code Composer Studio most easily using the Energia import

##Requirements##

* Download CCS (Code Composer Studio)
* Download Energia (version 13+)
* Download FastLED-CC3200 library. Move into folder _[User Path]/Documents/Energia/libraries/_

##Importing Sketch/Example##

* Within CCS, go to File->Import->Energia->Energia Sketch or Example
* If prompted, provide the Energia install directory.
* Select the chosen sketch. I recommend FirstLight, found in the _FastLED/examples_ folder
* In the "Project Explorer" pane on the left, right-click on "lpcc3200_FastLED" and click Properties. In the left pane, select Built->GNU Compiler->Symbols, and in the "Define symbols" pane add the symbol "CC3200"

#GCC Setup Requirements:#

##Compiler##

* Create a **new configuration for GNU GCC** compiler (instead of TI Compiler).
		
	1. Use the Project Explorer menu at the left: click "Show Build Settings." 
	2. In the left pane, select CCS General. 
	3. If CC3200 isn't already selected, select "Wireless Connectivity MCUs" from the variant dropdown, then CC3200 to its right. 
	4. Under compiler version, select GNU (the compiler with the highest version number).

* **Use the g++ compiler.** There's some quirks that don't seem to resolve well with the C (GCC) compiler

	1. To do this, use the Project Explorer menu at the left: click "Show Build Settings."
	2. Click the "Variables" or "Build Variables" tab. 
	3. Click the checkbox at the bottom to "Show system variables".
	4. Double-click the "CG_TOOL_GCC" to change the variable value. Modify the file that it is referring to, to end with g++.exe from gcc.exe. So it should end with "arm-none-eabi-g++.exe"

* **Add Include Directories**: You will need the core CC3200 libraries (included in the TI CC3200 SDK) to ensure proper compilation
		
	1. Use the Project Explorer menu at the left: click "Show Build Settings." 
	2. In the "GNU Compiler" dropdown in the left pane, select Directories. 
	3. In the main box, use the Add Directory (the plus button) to add the following directories (browse to them as needed):
		* [path to CC3200 SDK]\driverlib
		* [path to CC3200 SDK]\example\common
		* [path to CC3200 SDK]\inc
		* [path to FastLED library]

* **Change the compiler flags/defines.** This config has been validated, others may or may not work.
	
	1. Use the Project Explorer menu at the left: click "Show Build Settings." 
	2. In the left pane, open the GNU Compiler dropdown, and select the "Optimization" page.
		1. Using the "Optimization Level" dropdown, select "Optimize most -O3"
		2. Click the checkbox "Optimize for space rather than speed -Os"
	3. Click the symbols page on the left tab.
		1. In the "Define Symbols" window, click the "Add..." button to add symbols. Add a symbol called "CC3200"
	4. Click the miscellaneous page on the left tab
		1. In the bottom half of the page that opens, double-click the line beginning "-std=", and set it to "-std=c++11"

##Linker##

* **Add CC3200 linker script**

	1. Using the Project explorer menu at the left, click "Show Build Settings." 
	2. In the left pane, open the GNU Linker dropdown, and select "Libraries"
	3. In the "Linker Command Files" section, add a new script. 
	The path to the script is [Path to FastLED]\platforms\arm\cc3200\extras\cc3200link.ld
	
* **Add Linker search directories**

	1. In the same window, in the "Library search path" section, add the following library paths:
		* "${CG_TOOL_SEARCH_PATH}": Click add, then "Variables...", then choose "${CG_TOOL_SEARCH_PATH}"
		* "${CG_GCC_SEARCH_PATH}": Click add, then "Variables...", then choose "${CG_GCC_SEARCH_PATH}"
		* Driverlib directory: Click add, then "Variables...", then choose "${CC3200_SDK_ROOT}". In the text box, append "\driverlib\gcc\exe" after the SDK Root variable.

* **Add libdriver.a (Driver library)**

	1. In the same window, in the "Libraries" section, ensure that the only two files are "driver" and "c". 
	2. If one doesn't exist, add either "driver" or "c" to the field.
	
* **Add flag specs=nosys.specs**

	1. In the left pane, navigate to "Miscellaneous" under the "GNU Linker" dropdown.
	2. In the main window, under the "Other flags" field, add the flag "--specs=nosys.specs"
	
* **Import the gcc version of startup code:** *startup_gcc.c*

	1. In the main window, right click the file and select "Add files..."
	2. Navigate to *[Path to FastLED]\platforms\arm\cc3200\extras\*, and select "startup_gcc.c"
	
###Other potential problems###

If you have errors related to *\_\_bss_start\_\_*, *end*, or other variables, change the Linker script to the "cc3200link.ld" file mentioned above.
