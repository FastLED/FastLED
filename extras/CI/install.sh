#!/usr/bin/env bash
[[ "$_" != "$0" ]] && declare INSTALL_SH_SOURCED="1" || declare INSTALL_SH_SOURCED="0"
# the above must be the first line, useful to know if need to 'return' or 'exit'
# alternative:
# [[ "$0" != "$BASH_SOURCE" ]] && INSTALL_SH_SOURCED=1 || INSTALL_SH_SOURCED=0
if [[ $INSTALL_SH_SOURCED -eq 1 ]]; then
  ret=return
else
  declare -rg TRAVIS_BUILD_DIR="/mnt/c/src/libraries/FastLED"
  ret=exit
fi
readonly ret

# Require bash 4.3 or greater...
# bash 4.3 allows globally declaring assocaitive arrays from within functions
# bash 4.3 enables checking key of associative array exists as '[[ -v array[key] ]]'
# bash 4.3 enables use of 'nameref' to allow passing function arguments by reference
#          (e.g., caller passes name w/o leading '$', function uses 'declare/local -n ref=$1')
#          (e.g., to fill a caller's associative array with values....)
function internal_require_minimum_bash_version() {
  # NOTE: This function should presume as close to
  # NOTHING about the bash version as reasonbly possible.
  if [[ -z "$1" ]]; then
    echo "Invalid internal function call: at least major version parameter required"
    return 0
  fi

  local MINIMUM_MAJOR="$1"
  local MINIMUM_MINOR="${2:-0}"  # second argument default is zero
  local MINIMUM_PATCH="${3:-0}"  # third  argument default is zero

  # Now start checking the version parts against the required minimums,
  # in order of major, minor, patch, until one is either larger or smaller
  # than the required minimum.

  if   [[ ${BASH_VERSINFO[0]} -lt ${MINIMUM_MAJOR} ]]; then
    return 0; # fail
  elif [[ ${BASH_VERSINFO[0]} -gt ${MINIMUM_MAJOR} ]]; then
    return 1; # success!
  elif [[ ${BASH_VERSINFO[1]} -lt ${MINIMUM_MINOR} ]]; then
    return 0; # fail
  elif [[ ${BASH_VERSINFO[1]} -gt ${MINIMUM_MINOR} ]]; then
    return 1; # success!
  elif [[ ${BASH_VERSINFO[2]} -lt ${MINIMUM_PATCH} ]]; then
    return 0; # fail
  elif [[ ${BASH_VERSINFO[2]} -gt ${MINIMUM_PATCH} ]]; then
    return 1; # success!
  else
    # It's known that prior test could have been '-ge' (or even just excluded)
    # However, such small optimizations are not worth deviation from patterns
    return 1; # success!
  fi
}
internal_require_minimum_bash_version 4 3
if [[ $? -eq 0 ]]; then
  printf "BASH VERSION < 4.3: %s\n" "${BASH_VERSION}" >&2
  $ret 1
fi

# Overview of behavior:
# . Only platform keys that exist in ALL_PLATFORMS can be built.
# . All platforms in ALL_PLATFORMS are installed when source the script
# . All boards are installed when source the script
# 
# . Environment variable 'platforms' is space-separated list of platform keys to build
#   _ may be set to 'all' to build all supported platforms
# . Caller calls function 'build_platform'
#
# . By default, all samples are built for all platforms.
#   _ Any sample can be skipped for all platforms by creating file '.test.skip'
#   _ Any sample can indicate only specific platforms will build by creating,
#     for each specific platform to build, a file of form '.test.only.{platform_key}'
#   _ Any sample can be skipped for single platform by creating file '.test.skip.{platform_key}'
#

# TODO List:
# [ ] verify per-function overrides correspond to existing function names
# [ ] find way to support Teensy boards -- see https://www.pjrc.com/teensy/techspecs.html for add-on
# [ ] add NRF51 board?  Currently has zero code coverage....
# [ ] determine and set a proper compilation warning level
# [ ] fix wget (arduino install) so arduino distribution file goes into temporary directory
# [ ] use 'curl --silent' instead of wget to download files?

# Steps to add support for an additional board:
#
# 1. In function internal_declare_BOARD_MANAGER_PREFERENCES(),
#    add the board support package JSON url,
#    so arduino can find the board information,
#
# 2. in function internal_declare_ALL_PLATFORMS(),
#    add a friendly name for a specific board of your choice,
#    The string is used as argument to 'arduino --board',
#    with the format 'package:arch:board[:parameters]'
#        package is json.packages.name
#        arch    is json.packages.platforms.architecture
#        board   is from installed package's board.txt,
#                and corresponds to a first period-delimited string
#        parameters are also from board.txt...
#
#    Note that the package and arch will also be extracted
#    from this string to install the board
#    (see: function internal_setup_install_dependencies())
# 
# Here's an example of additional parameters from 
# the boards.txt that came with adafruit's nrf52 BSP:
#
#    feather52840.menu.softdevice.s140v6=0.2.9 (s140 6.1.1)
#    ^^^^^^^^^^^^ ^^^^ ^^^^^^^^^^ ^^^^^^
#    |||||||||||| |||| |||||||||| ||||||
#    |||||||||||| |||| |||||||||| \____\___ parameter part after the equals sign
#    |||||||||||| |||| ||||||||||
#    |||||||||||| |||| \________\__________ parameter part before the equals sign
#    |||||||||||| |||| 
#    |||||||||||| \__\_____________________ must be menu to enable use as parameter?
#    ||||||||||||
#    \__________\__________________________ the board name
#
# This can result in an example string in ALL_PLATFORMS such as:
#    adafruit:nrf52:feather52840:softdevice=s140v6,debug=l0
#

# Emulate minimum needed environment variables from TRAVIS-CI build environment when running locally:
if [[ $INSTALL_SH_SOURCED -ne 1 ]]; then
  declare -rg TRAVIS_BUILD_DIR="/mnt/c/src/libraries/FastLED"
fi

function internal_declare_colors {
  # define readonly, global colors
  declare -rg    VT_GREY='\033[1;30m'    # bold/bright + black
  declare -rg    VT_GRAY='\033[1;30m'    # bold/bright + black
  declare -rg     VT_RED='\033[0;31m'    # normal      + red
  declare -rg    VT_LRED='\033[1;31m'    # bold/bright + red
  declare -rg   VT_GREEN='\033[0;32m'    # normal      + green
  declare -rg  VT_LGREEN='\033[1;32m'    # bold/bright + green
  declare -rg  VT_ORANGE='\033[0;33m'    # normal      + yellow
  declare -rg  VT_YELLOW='\033[1;33m'    # bold/bright + yellow
  declare -rg    VT_BLUE='\033[0;34m'    # normal      + blue
  declare -rg   VT_LBLUE='\033[1;34m'    # bold/bright + blue
  declare -rg  VT_PURPLE='\033[0;35m'    # normal      + magenta
  declare -rg VT_LPURPLE='\033[1;35m'    # bold/bright + magenta
  declare -rg    VT_CYAN='\033[0;36m'    # normal      + cyan
  declare -rg   VT_LCYAN='\033[1;36m'    # bold/bright + cyan
  declare -rg   VT_LGREY='\033[0;37m'    # normal      + light grey
  declare -rg   VT_LGRAY='\033[0;37m'    # normal      + light gray
  declare -rg   VT_WHITE='\033[1;37m'    # bold/bright + light grey/gray
  declare -rg  VT_NORMAL='\033[0m'       # normal
}
internal_declare_colors

function internal_declare_log_levels {
  # Log levels:
  # 0 == fatal
  # 1 == + error
  # 2 == + progress/action
  # 3 == + warning
  # 4 == + info
  # 5 == + verbose
  declare -g LOG_LEVEL_DEFAULT=2
  declare -gA LOG_LEVEL_BY_FUNCTION
  #LOG_LEVEL_BY_FUNCTION["internal_build_get_samples_for_platform"]=1
  #LOG_LEVEL_BY_FUNCTION["internal_declare_ALL_PLATFORMS"]=5
  #LOG_LEVEL_BY_FUNCTION["internal_declare_BOARD_MANAGER_PREFERENCES"]=5
  #LOG_LEVEL_BY_FUNCTION["internal_link_build_directory_as_arduino_library"]=5
  #LOG_LEVEL_BY_FUNCTION["internal_parse_build_platforms_parameters"]=5
  #LOG_LEVEL_BY_FUNCTION["internal_run_once_setup"]=5
  #LOG_LEVEL_BY_FUNCTION["internal_setup_arduino_ide"]=5
  #LOG_LEVEL_BY_FUNCTION["internal_setup_install_dependencies"]=5
  LOG_LEVEL_BY_FUNCTION["internal_setup_teensyduino_headless"]=5
  #LOG_LEVEL_BY_FUNCTION["internal_uninstall_arduino"]=5
  LOG_LEVEL_BY_FUNCTION["build_platforms"]=5
  LOG_LEVEL_BY_FUNCTION["log_TRAVIS_VARIABLES"]=2
  readonly LOG_LEVEL_BY_FUNCTION
  readonly LOG_LEVEL_DEFAULT
}
internal_declare_log_levels
# USAGE: an internal function, must be called exactly two levels away from caller
# first argument is the requested log level of the calling function
# e.g., caller --> log_error --> should_log
# 
# Example for logging from log_warning:
#   log_check 3 && printf ...
#   if [[ ! $(log_check 3) ]]; then ... fi
function log_check {
  local MY_LOG_LEVEL="$1"
  if [[ -v LOG_LEVEL_BY_FUNCTION[${FUNCNAME[2]}] ]]; then
    if [[ "${LOG_LEVEL_BY_FUNCTION[${FUNCNAME[2]}]}" -lt "${MY_LOG_LEVEL}" ]]; then
      return 1
    else
      return 0
    fi
  else
    if [[ "${LOG_LEVEL_DEFAULT}" -lt "${MY_LOG_LEVEL}" ]]; then
      return 1
    else
      return 0
    fi
  fi
}

# define logging functions in central place, so can easily modify standardized output
# YELLOW  == fold_start, section_header, progress, action_start, action_failed (additional lines)
# LRED    == fatal, error, build_error, action_failed
# LCYAN   == warning, may_be_cached
# WHITE   == info
# NORMAL  == verbose
# LPURPLE == 
# LGREEN  == action_succeeded


function log_section_header {
  if [[ ! $(log_check 2) ]]; then
    printf "${VT_YELLOW}\n"
    printf "%5d: ########################################################################\n" "${BASH_LINENO[0]}"
    printf "%5d: %s\n" "${BASH_LINENO[0]}" "${1}"
    printf "%5d: ########################################################################\n" "${BASH_LINENO[0]}"
    printf "${VT_NORMAL}\n" 
  fi
  return 0
}
function log_fatal {
  printf "${VT_LRED}%5d: %8s: %s${VT_NORMAL}\n" "${BASH_LINENO[0]}" "FATAL" "${1}"
  return 0
}
function log_error {
  log_check 1 && printf "${VT_LRED}%5d: %8s: %s${VT_NORMAL}\n" "${BASH_LINENO[0]}" "ERROR" "${1}"
  return 0
}
function log_progress {
  log_check 2 && printf "${VT_YELLOW}%5d: %8s: %s ${1}${VT_NORMAL}\n" "${BASH_LINENO[0]}" "PROGRESS" "${FUNCNAME[1]}()"
  return 0
}
function log_warning {
  log_check 3 && printf "${VT_LCYAN}%5d: %8s: %s${VT_NORMAL}\n" "${BASH_LINENO[0]}" "WARNING" "${1}"
  return 0
}
function log_info {
  log_check 4 && printf "${VT_WHITE}%5d: %8s: %s${VT_NORMAL}\n" "${BASH_LINENO[0]}" "INFO" "${1}"
  return 0
}
function log_verbose {
  log_check 5 && printf "${VT_NORMAL}%5d: %8s: %s${VT_NORMAL}\n" "${BASH_LINENO[0]}" "VERBOSE" "${1}"
  return 0
}
function log_build_error {
  if [[ ! $(log_check 1) ]]; then
    log_start_fold "FAIL: ${1}"
    printf "${VT_LPURPLE}%5d: BUILD FAILED: [%s]\n"     "${BASH_LINENO[0]}" "${1}"
    printf "%5d: ----------------------------------------------------------------------\n" "${BASH_LINENO[0]}"
    printf "${VT_LRED}%s${VT_LPURPLE}\n" "${2}"
    printf "%5d: ----------------------------------------------------------------------{$VT_NORMAL}\n" "${BASH_LINENO[0]}"
    log_end_fold "FAIL: ${1}"
  fi
}
function log_action_start {
  log_check 2 && printf "${VT_YELLOW}%5d: %8s: %s ... ${VT_NORMAL}" "${BASH_LINENO[0]}" "ACTION" "${1}"
  return 0
}
function log_action_failed {
  if [[ ! $(log_check 2) ]]; then
    printf "${VT_LRED}FAIL${VT_NORMAL}\n"
    while [[ ! -z "${1}" ]]; do
      printf "${VT_YELLOW}%5d: %8s: %s ... ${VT_NORMAL}\n" "${BASH_LINENO[0]}" "VERBOSE" "${1}"
      shift
    done
  fi
}
function log_action_succeeded {
  log_check 2 && printf "${VT_LGREEN}success${VT_NORMAL}\n"
}
function log_action_may_be_cached {
  if [[ ! $(log_check 2) ]]; then
    printf "${VT_LCYAN}cached (or failed) %s${VT_NORMAL}\n" "${1}"
  fi
}

declare -ga my_fold_sections
declare -g disable_folding=0
declare -g loop_count=0
# use namerefs to pass the argument by reference
function internal_encode_fold_section_name {
  local -n sectionNameRef="${1}"
  local modifiedName="${sectionNameRef}"
  modifiedName="${modifiedName//[^A-Za-z0-9]/_}"
  modifiedName="${modifiedName/%_/''}"
  if [[ -z "$modifiedName" ]]; then
    # fatal error ... exit
    printf "FATAL INTERNAL ERROR: invalid fold section name: '%s'\n" "${sectionName}"
    return 1
  fi
  sectionNameRef="$modifiedName"
  return 0
}
function log_start_fold {
  local originalName="${1}";
  local sectionName="${1}";

  # pass by reference so value can be changed by called function
  internal_encode_fold_section_name sectionName

  my_fold_sections+=("$sectionName")
  if [[ "$disable_folding" -ne 0 ]]; then
    printf "ignoring fold start b/c disabled: %s\n" "${sectionName}"
  else
    printf "travis_fold:start:%s\n"  "${sectionName}"
  fi
  printf "${VT_YELLOW}%5d: %s\n" "${BASH_LINENO[0]}" "${originalName}"

  return 0
}
function log_end_fold {

  local sectionName="${1}";

  # pass by reference so value can be changed by called function
  internal_encode_fold_section_name sectionName

  if [[ "${#my_fold_sections}" -eq 0 ]]; then
    printf "fold error: nothing left to close, requested: %s\n" "${sectionName}"
    return 1
  fi

  local toMatch="${my_fold_sections[-1]}"


  # what result if failed to close prior fold section(s)?
  # A. automatically close those prior fold sections
  #    PRO: avoids need for BASH to trap errors everywhere
  #    CON: can hide script errors and push stuff into wrong folds
  # B. error/exit with warning in log of failure
  # C. Auto-close, but disable internal folding from here on out...
  #
  # This script currently implements choice "C"
  if [[ "$sectionName" != "$toMatch" ]]; then
    # disable future folds, then close all existing folds, and return error
    printf "FAILED TO MATCH: %s != %s\n" "${sectionName}" "${toMatch}"
    log_close_all_folds
    disable_folding=1
    return 1
  fi

  if [[ "$disable_folding" -ne 0 ]]; then
    printf "ignoring fold close b/c disabled: %s\n" "${sectionName}"
  else
    printf "travis_fold:end:%s\n" "${sectionName}"
  fi
  unset 'my_fold_sections[-1]'
  return 0
}
function log_close_all_folds {
    while [[ "${#my_fold_sections[@]}" -ne 0 ]]; do
      log_end_fold "${my_fold_sections[-1]}"
    done
    return 0
}
function log_TRAVIS_VARIABLES {
  # p_key="${1:-}" # set to empty string if no argument provided
  if [[ $(log_check 5) ]]; then
    printf "travis_fold:start:travis_variables"
    printf "Travis Environment Variables"
    log_section_header "Travis Variables"
    printf "%5d: %28s=%s\n"  $LINENO  "CI"  "${CI:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS" "${TRAVIS:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "CONTINUOUS_INTEGRATION" "${CONTINUOUS_INTEGRATION:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "DEBIAN_FRONTEND" "${DEBIAN_FRONTEND:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "HAS_JOSH_K_SEAL_OF_APPROVAL" "${HAS_JOSH_K_SEAL_OF_APPROVAL:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "USER" "${USER:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "HOME" "${HOME:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "LANG" "${LANG:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "LC_ALL" "${LC_ALL:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "RAILS_ENV" "${RAILS_ENV:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "RACK_ENV" "${RACK_ENV:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "MERB_ENV" "${MERB_ENV:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "JRUBY_OPTS" "${JRUBY_OPTS:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "JAVA_HOME" "${JAVA_HOME:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_ALLOW_FAILURE" "${TRAVIS_ALLOW_FAILURE:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_APP_HOST" "${TRAVIS_APP_HOST:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_BRANCH" "${TRAVIS_BRANCH:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_BUILD_DIR" "${TRAVIS_BUILD_DIR:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_BUILD_ID" "${TRAVIS_BUILD_ID:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_BUILD_NUMBER" "${TRAVIS_BUILD_NUMBER:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_BUILD_WEB_URL" "${TRAVIS_BUILD_WEB_URL:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_COMMIT" "${TRAVIS_COMMIT:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_COMMIT_MESSAGE" "${TRAVIS_COMMIT_MESSAGE:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_COMMIT_RANGE" "${TRAVIS_COMMIT_RANGE:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_DEBUG_MODE" "${TRAVIS_DEBUG_MODE:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_EVENT_TYPE" "${TRAVIS_EVENT_TYPE:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_JOB_ID" "${TRAVIS_JOB_ID:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_JOB_NAME" "${TRAVIS_JOB_NAME:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_JOB_NUMBER" "${TRAVIS_JOB_NUMBER:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_JOB_WEB_URL" "${TRAVIS_JOB_WEB_URL:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_OS_NAME" "${TRAVIS_OS_NAME:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_OSX_IMAGE" "${TRAVIS_OSX_IMAGE:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_PULL_REQUEST" "${TRAVIS_PULL_REQUEST:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_PULL_REQUEST_BRANCH" "${TRAVIS_PULL_REQUEST_BRANCH:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_PULL_REQUEST_SHA" "${TRAVIS_PULL_REQUEST_SHA:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_PULL_REQUEST_SLUG" "${TRAVIS_PULL_REQUEST_SLUG:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_REPO_SLUG" "${TRAVIS_REPO_SLUG:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_SECURE_ENV_VARS" "${TRAVIS_SECURE_ENV_VARS:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_SUDO" "${TRAVIS_SUDO:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_TEST_RESULT" "${TRAVIS_TEST_RESULT:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_TAG" "${TRAVIS_TAG:-}"
    printf "%5d: %28s=%s\n"  $LINENO  "TRAVIS_BUILD_STAGE_NAME" "${TRAVIS_BUILD_STAGE_NAME:-}"
    printf "travis_fold:end:travis_variables"
  fi
  return 0
}
log_TRAVIS_VARIABLES
log_section_header "Initializing script"

function internal_validate_ALL_PLATFORMS {
  # verify keys are alphanumeric only
  # -- required because used as filenames to skip tests
  # -- required to allow easy split of single environment variable into separate platforms
  # -- keys 'all', 'none', and 'skip' are reserved names
  #    -- 'all' is used to indicate all the platforms should be built
  #    -- 'none' is used for test inclusion/exclusion
  #    -- 'skip' is used for test inclusion/exclusion
  for p_key in "${!ALL_PLATFORMS[@]}"; do
    if [[ ! "$p_key" =~ ^[A-Za-z0-9_]+$ ]]; then
      log_fatal "INVALID PLATFORM KEY: '${p_key}' -- SCRIPT MUST EXIT"
      return 101
    fi
    if [[ "$p_key^^" = "ALL" ]]; then
      log_fatal "INVALID PLATFORM KEY: '${p_key}' -- SCRIPT MUST EXIT"
      return 102
    fi
    if [[ "$p_key^^" = "NONE" ]]; then
      log_fatal "INVALID PLATFORM KEY: '${p_key}' -- SCRIPT MUST EXIT"
      return 103
    fi
    if [[ "$p_key^^" = "SKIP" ]]; then
      log_fatal "INVALID PLATFORM KEY: '${p_key}' -- SCRIPT MUST EXIT"
      return 104
    fi
  done
}
# This function creates read-only, global, associative array ALL_PLATFORMS
function internal_declare_ALL_PLATFORMS {
  # Checkbox to indicate CI status for each board:
  # [ ] := board info only
  # [.] := type of board with the given processor found
  # [_] := board support package exists, compilation possible
  # [x] := enabled for CI

  # TODO: For teensy boards, see https://www.pjrc.com/teensy/techspecs.html
  #       Are there any other arduino BSPs that are k20, k66, or kl26?
  # [.] teensy32 ==> platforms\arm\k20    []
  # [.] teensy36 ==> platforms\arm\k66    []
  # [.] teensylc ==> platforms\arm\kl26   []
  # [.] ???????? ==> platforms\arm\nrf51  []

  # SUMMARY: Only pins 2-4 are available on ALL the supported boards
  # uno      == pins: 0-19 ()
  # bluepill == pins: 2-4, 10-22, 29-34, 37-43, 45-46 (__STM32F1__)
  # trinket  == pins: 0-5                             (__AVR_ATtiny85__)
  # esp32    == pins: 0-5, 12-19, 21-23, 25-27, 32-33
  # esp8266  == pins: 0-5, 12-16                      (FASTLED_ESP8266_RAW_PIN_ORDER)
  # esp8266  == pins: 0-15                            (FASTLED_ESP8266_D1_PIN_ORDER)
  # esp8266  == pins: 0-10                            (FASTLED_ESP8266_NODEMCU_PIN_ORDER)
  # m4       == pins: 0-19                            (ADAFRUIT_ITSYBITSY_M4_EXPRESS)
  # nrf52840 == pins: 0, 2-19, 22-26                  (ARDUINO_NRF52840_FEATHER)

  declare -gA ALL_PLATFORMS
  ALL_PLATFORMS["uno"]="arduino:avr:uno"                                                     # (ATMega328   -- avr        )
  ALL_PLATFORMS["gemma"]="arduino:avr:gemma"                                                 # (???         -- avr        )
  ALL_PLATFORMS["zero"]="arduino:samd:arduino_zero_native"                                   # (SAMD21G18   -- arm\d21    )
  ALL_PLATFORMS["due"]="arduino:sam:arduino_due_x"                                           # (DUE X       -- arm\sam    )
  ALL_PLATFORMS["trinket"]="adafruit:avr:trinket5"                                           # (ATtiny85    -- avr        )
  ALL_PLATFORMS["m4"]="adafruit:samd:adafruit_itsybitsy_m4:speed=120"                        # (SAMD51      -- arm\d51    )
  ALL_PLATFORMS["nrf52840"]="adafruit:nrf52:feather52840:softdevice=s140v6,debug=l0"         # (nrf52840    -- arm\nrf52  )
  ALL_PLATFORMS["stm32"]="stm32duino:STM32F1:genericSTM32F103R:device_variant=STM32F103RE"   # (STM32F103RE -- arm\stm32  )
  ALL_PLATFORMS["esp32"]="esp32:esp32:featheresp32:FlashFreq=80"                             # (ESP32       -- esp32      )
  ALL_PLATFORMS["esp8266"]="esp8266:esp8266:huzzah:eesz=4M3M,xtal=80"                        # (ESP8266     -- esp8266    )
  #Uncomment to verify de-duplication of platforms works
  #ALL_PLATFORMS[dupe_due]="arduino:sam:arduino_due_x"
  readonly ALL_PLATFORMS
}
# Generate two read-only, global variables: ALL_BSP_URIS and BOARD_MANAGER_PREFERENCES
# 1. ALL_BSP_URIS is a read-only, global array
#    Each value of the array corresponds to
#    a single board support package URI.
# 2. BOARD_MANAGER_PREFERENCES is a read-only global string
#    simplifies use in setting up the Arduino environment to use the URIs from ALL_BSP_URIS
#    created by comma-joining the BSP array, and prefixing with 'boardsmanager.additional.urls='
#
# For example:
#     arduino --pref "${BOARD_MANAGER_PREFERENCES}" --save-prefs 2>&1
function internal_declare_BOARD_MANAGER_PREFERENCES {
  # NOTE: elements in this array cannot contain embedded spaces or commas
  declare -ga ALL_BSP_URIS
  ALL_BSP_URIS+=("https://adafruit.github.io/arduino-board-index/package_adafruit_index.json")
  ALL_BSP_URIS+=("http://arduino.esp8266.com/stable/package_esp8266com_index.json")
  ALL_BSP_URIS+=("https://dl.espressif.com/dl/package_esp32_index.json")
  ALL_BSP_URIS+=("http://dan.drown.org/stm32duino/package_STM32duino_index.json")
  readonly ALL_BSP_URIS
  
  declare -g BOARD_MANAGER_PREFERENCES
  # abuse IFS to use it to join values with comma
  IFS=, eval 'BOARD_MANAGER_PREFERENCES="boardsmanager.additional.urls=${ALL_BSP_URIS[*]}"'

  readonly BOARD_MANAGER_PREFERENCES
}
# install teensyduino
function internal_setup_teensyduino_headless {
  log_warning "Teensyduino installation support not tested/enabled yet."
  return 0

  local filename="TeensyduinoInstall.linux64"
  local uri="https://www.pjrc.com/teensy/td_146/${filename}"
  cd $OLDPWD
  log_action_start "downloading teensyduino"
  wget --quiet "${uri}"
  RETURN_VALUE=$?
  if [[ $RETURN_VALUE -ne 0 ]]; then
    log_action_failed "wget returned ${RETURN_VALUE} for ${uri}"
    return $RETURN_VALUE
  fi
  log_action_succeeded

  chmod +x ${filename}
  log_action_start "installing teensyduino"
  "./$filename" -dir="$HOME/arduino_ide"
  RETURN_VALUE=$?
  if [[ $RETURN_VALUE -ne 0 ]]; then
    log_action_failed "install failed ${RETURN_VALUE}"
    return $RETURN_VALUE
  fi
  log_action_succeeded
  return 0
}
# uninstalls arduino by removing $HOME/arduino_ide
function internal_uninstall_arduino {
  if [[ -d "$HOME/arduino_ide" || -f "$HOME/arduino_ide" ]]; then
    local RETURN_VALUE=0
    # Enable shell option 'extglob'?
    shopt -s extglob
    cd "$HOME/"
    log_action_start "uninstalling arduino"
    rm -rf "$HOME/arduino_ide"
    RETURN_VALUE=$?
    if [[ $RETURN_VALUE -ne 0 ]]; then
      log_action_failed "Unable to remove prior ($HOME/arduino_ide)"
      return $RETURN_VALUE
    fi
    log_action_succeeded

    mkdir "$HOME/arduino_ide"
    RETURN_VALUE=$?
    if [[ $RETURN_VALUE -ne 0 ]]; then
      log_error "Unable to recreate arduino directory ($HOME/arduino_ide)"
      return $RETURN_VALUE
    fi
    cd "$OLDPWD"
  fi
  return 0
}
# ensure arduino is installed to $HOME/arduino_ide
function internal_setup_arduino_ide {

  # add the arduino CLI to our PATH
  [[ ! -d "$HOME/arduino_ide/"  ]] && mkdir $HOME/arduino_ide
  if [[ ! -d "$HOME/arduino_ide/" ]]; then
    log_error  "could not create directory for Arduino IDE ('$HOME/arduino_ide')"
    return 100
  fi

  export PATH="$HOME/arduino_ide:$PATH"

  if [[ ! -z $ARDUINO_IDE_VERSION ]]; then
    log_verbose "NOTE: Your .travis.yml specifies a specific version of the Arduino IDE: ${ARDUINO_IDE_VERSION}"
  else
    export ARDUINO_IDE_VERSION="1.8.9"
    log_verbose "NOTE: Using default arduino IDE version ${ARDUINO_IDE_VERSION}"
  fi

  local RETURN_VALUE=0
  # if matching version file (which this script creates) does not exist in that directory,
  # then need to uninstall existing version
  if [[ -f "$HOME/arduino_ide/$ARDUINO_IDE_VERSION" && -f "$HOME/arduino_ide/arduino" ]]; then
    log_verbose "Arduino IDE already installed (cached?)"
    return 0
  fi
    
  internal_uninstall_arduino
  RETURN_VALUE=$?
  if [[ $RETURN_VALUE -ne 0 ]]; then
    log_error "uninstall of arduino failed"
    return ${RETURN_VALUE}
  fi

  # TODO: put into a temp directory, not current working directory....
  log_action_start "Downloading Arduino IDE"
  wget --quiet https://downloads.arduino.cc/arduino-$ARDUINO_IDE_VERSION-linux64.tar.xz
  RETURN_VALUE=$?
  if [[ $RETURN_VALUE -ne 0 ]]; then
    log_action_failed "wget of arduino failed (${RETURN_VALUE})"
    return ${RETURN_VALUE}
  fi
  log_action_succeeded

  log_action_start "UNPACKING ARDUINO IDE... "
  tar xf arduino-$ARDUINO_IDE_VERSION-linux64.tar.xz -C $HOME/arduino_ide/ --strip-components=1
  RETURN_VALUE=$?
  if [[ $RETURN_VALUE -ne 0 ]]; then
    log_action_failed "extraction of arduino ide failed (${RETURN_VALUE})"
    return ${RETURN_VALUE}
  fi

  log_action_succeeded
  touch $HOME/arduino_ide/$ARDUINO_IDE_VERSION

  return 0
}
# creates a symbolic link under $HOME/arduino_ide/libraries to $TRAVIS_BUILD_DIR
function internal_link_build_directory_as_arduino_library {

  local RETURN_VALUE=0
  
  # Caller can specify the name of the library directory to be created...
  if [[ -z "$ARDUINO_LIBRARY_NAME" ]]; then
    declare -g ARDUINO_LIBRARY_NAME="Test_Library"
  fi

  local symlink="$HOME/arduino_ide/libraries/$ARDUINO_LIBRARY_NAME"
  log_verbose "Ensuring travis build directory linked as Arduino library '$ARDUINO_LIBRARY_NAME'"
 
  # if already exists as a symlink, delete it
  if [[ -L "$symlink" ]]; then
    log_verbose "Removing existing symbolic link at '$symlink'"
    rm "$symlink"
  fi

  # if still exists, FAIL 
  if [[ -L "$symlink" ]]; then
    log_error "failed to remove old symbolic link, failing all builds."
    return 101
  fi
  if [[ -e "$symlink" ]]; then
    log_error "intended symbolic link name exists as normal file/dir, failing all builds."
    return 102
  fi

  log_verbose "Creating symlink '$symlink' -> '$TRAVIS_BUILD_DIR'... "
  ln -s -f "$TRAVIS_BUILD_DIR" "$symlink"
  RETURN_VALUE=$?
  if [[ $RETURN_VALUE -ne 0 ]]; then
    log_error "failed to create symbolic link for library (${RETURN_VALUE})"
    return ${RETURN_VALUE}
  fi
  
  if [[ ! -L "$symlink" ]]; then
    log_error "symbolic link for library failed to be created"
    return 103
  fi

  if [[ ! -e "$symlink" ]]; then
    log_error "??? symbolic link for library points to non-existing directory ??? "
    return 104
  fi
  return 0
}

# Install the board for a given platform
function internal_install_board {
  if [[ ! -n ${1} ]]; then
    log_error "Argument required to indicate platform to ensure the board is installed for"
    return 1
  fi
  if [[ ! -v ALL_PLATFORMS["${1}"] ]]; then
    log_error "Platform key '${1}' does not exist in ALL_PLATFORMS?"
    return 2
  fi

  # automatically extract the package:arch portion of the ALL_PLATFORMS value:
  #     package:arch:board[:parameters]
  #     arduino --install-boards package:arch[:version]
  local full_board="${ALL_PLATFORMS[${1}]}"
  local board=""
  
  if [[ "$full_board" =~ ^[^:]+:[^:]+ ]]; then
    board="${BASH_REMATCH[0]}"
  else
    log_error "Platform key '${1}' failed to extract --install-boards from '${full_board}'"
    return 3
  fi
  
  log_action_start "Installing board '${board}'"
  local DEPENDENCY_OUTPUT=$(arduino --install-boards ${board} 2>&1)
  local __=$?
  if [[ $__ -ne 0 ]]; then
    log_action_may_be_cached "error ($__)"
  else
    log_action_succeeded
  fi

  ### SADLY, the error for a true failure is the same as when the board already is installed....
  #if [[ 1 -eq 1 ]]; then
  #  local board="THIS_BOARD_DOES_NOT:EXIST"
  #  log_action_start "TEST FAILURE TO INSTALL '${board}'"
  #  DEPENDENCY_OUTPUT=$(arduino --install-boards ${board} 2>&1)
  #  local __=$?
  #  if [[ $__ -ne 0 ]]; then
  #    log_action_may_be_cached "error ($__)"
  #  else
  #    log_action_succeeded
  #  fi
  #fi
}

function internal_install_nrfutil {
  local __=0
  log_section_header "Installing adafruit-nrfutil (prerequisite for nrf boards)"

  # from user native-api: Note that you’ll have to 'sudo apt-get python3-setuptools python3-pip' to install packages with pip3.
  # from user methane: We can use 'pyenv global 3.7.1' to activate Python 3.7.1 and we can use 'pip3'.
  # currently, setting in global env: PATH=/opt/python/3.7.1/bin:$PATH
  python -VV
  python3 -VV
  pip -V
  pip3 -V
  installer_output=$(pip3 install --user adafruit-nrfutil 2>&1)
  __=$?
  if [[ $__ -ne 0 ]]; then
    log_action_failed "Failed to install adafruit-nrfutil with return code '${__}':" "${installer_output}"
    return $__;
  fi
  export PATH="$HOME/.local/bin:$PATH"
  return 0
}

# configures arduino preferences, ensures the various board support packages are installed
# TODO: modify this to be called once per-board, just before attempting to build for that board
#       rather than installing all the boards every time....
function internal_setup_install_dependencies {

  local RETURN_VALUE=0

  log_section_header "INSTALLING DEPENDENCIES"

  ## TODO: What compiler warning level should be used?
  ##       Will 'all' warnings work for all platforms? (I think no, due to nrf52 SDK errors)
  # log_action_start "Setting build preferences"
  #DEPENDENCY_OUTPUT=$(arduino --pref "compiler.warning_level=all" --save-prefs 2>&1)
  #RETURN_VALUE=$?
  #if [[ $RETURN_VALUE -ne 0 ]]; then
  #  log_action_failed "Failed to set compiler preferences: ${RETURN_VALUE}"
  #  return ${RETURN_VALUE}
  #fi
  #log_action_succeeded
  
  log_action_start "Adding board manager URIs"
  DEPENDENCY_OUTPUT=$(arduino --pref "${BOARD_MANAGER_PREFERENCES}" --save-prefs 2>&1)
  RETURN_VALUE=$?
  if [[ $RETURN_VALUE -ne 0 ]]; then
    log_action_failed "Failed to set uris: ${RETURN_VALUE}"
    return ${RETURN_VALUE}
  fi
  log_action_succeeded

  log_verbose "Current packages list:"
  if [[ ! -d ~/.arduino15/packages/ ]]; then
    log_info "Packages directory does not exist ... this may be normal for fresh arduino install."
  else
    log_verbose "$(ls ~/.arduino15/packages/)"
  fi
  internal_install_nrfutil
  return $?
}
# automatically called when source'ing this file, ensures arduino is setup with boards installed
function internal_script_config {
  log_section_header "Running internal script configuration"
  internal_declare_ALL_PLATFORMS
  internal_validate_ALL_PLATFORMS
  local __=$?
  if [[ $__ -ne 0 ]]; then
    return "$__"
  fi

  internal_declare_BOARD_MANAGER_PREFERENCES
  declare -rg ARDUINO_LIBRARY_NAME=fastled
  declare -rg SAMPLE_ROOT_DIR="${TRAVIS_BUILD_DIR}/examples"
  log_progress "SAMPLE_ROOT_DIR=${SAMPLE_ROOT_DIR}"

  log_progress "Starting Xvfb to make display available"
  # make display available for arduino CLI
  if [[ ${INSTALL_SH_SOURCED} -ne 0 ]]; then
    # sourced...
    /sbin/start-stop-daemon --start --quiet --pidfile /tmp/custom_xvfb_1.pid --make-pidfile --background --exec /usr/bin/Xvfb -- :1 -ac -screen 0 1280x1024x16
    sleep 3
    export DISPLAY=:1.0
  fi
  return 0
}
function internal_run_once_setup {
  local RETURN_VALUE=0
  
  log_section_header "Running one-time setup"
  
  log_progress "Installing Arduino IDE"
  internal_setup_arduino_ide
  RETURN_VALUE=$?
  if [[ $RETURN_VALUE -ne 0 ]]; then
    log_error "Failed to setup arduino ide ${RETURN_VALUE}"
    return ${RETURN_VALUE}
  fi

  log_progress "Installing teensyduino"
  internal_setup_teensyduino_headless
  RETURN_VALUE=$?
  if [[ $RETURN_VALUE -ne 0 ]]; then
    log_error "Failed to setup teensyduino ide ${?}"
    return ${RETURN_VALUE}
  fi
  
  log_progress "Linking Travis build directory as arduino library"
  internal_link_build_directory_as_arduino_library
  RETURN_VALUE=$?
  if [[ $RETURN_VALUE -ne 0 ]]; then
    log_error "Failed to link build directory as arduino library ${RETURN_VALUE}"
    return ${RETURN_VALUE}
  fi
  
  log_progress "Installing additional dependencies"
  internal_setup_install_dependencies
  RETURN_VALUE=$?
  if [[ $RETURN_VALUE -ne 0 ]]; then
    log_error "Failed to install arduino dependencies ${RETURN_VALUE}"
    return ${RETURN_VALUE}
  fi
  
  # continue adding more here...

  log_progress "run_once_setup completed successfully"
  return 0
}
# Gets the samples to be built for a given platform  -- env var SAMPLE_ROOT_DIR defines where to look....
# $1 := friendly platform key
# results will be put into (caller-defined) array 'samples'
function internal_build_get_samples_for_platform {

  local p_key
  p_key=${1:-} # set to empty string if no argument provided

  if [[ -z "${p_key}" ]]; then
    log_warning "'${FUNCNAME[0]}' first argument must be platform key"
    return 1
  fi
  if [[ ! -v ALL_PLATFORMS["$p_key"] ]]; then
    log_warning "'${FUNCNAME[0]}' platform key ($p_key) does not exist in ALL_PLATFORMS"
    return 2
  fi

  # grab all ino example sketches
  local -a examples=($(find "$SAMPLE_ROOT_DIR" -name "*.ino"))

  log_verbose "Checking build platform ${p_key} for compatible samples to build"

  # loop through example sketches
  for example in "${examples[@]}"; do

    local example_dir=$(dirname $example)   # full path to sketch
    local example_file=$(basename $example) # filename of sketch
    log_verbose "Checking sample ${example_dir} ... "

    if [[ -f "${example_dir}/.test.only."* ]]; then
      if [[ -f "${example_dir}/.test.only.${p_key}" ]]; then
        log_info "skipping ${example_dir} because (.test.only.*) exists without (.test.only.${p_key}) existing"
        continue
      fi
      log_verbose "found file (.test.only.${p_key}); continuing other checks"
    fi

    # ignore example if there is an all platform skip
    if [[ -f "${example_dir}/.test.skip" ]]; then
      log_info "skipping ${example_dir} because (.test.skip) exists"
      continue
    fi

    # ignore example if platform-specific skip file exists
    if [[ -f "${example_dir}/.test.skip.${p_key}" ]]; then
      log_info "skipping ${example_dir} because (.test.skip.${p_key}) exists"
      continue
    fi
    
    # for test purposes...
    #if [[ ! "$example_file" =~ ^[Zz]_ ]]; then
    #  log_error "TEST ONLY -- SHOULD NOT SHIP -- skipping ${example_dir} only testing simple tests"
    #  continue
    #fi

    log_verbose "sample ${example_dir} will be built for platform ${p_key}"
    samples+=("$example")
  done

  return 0
}
# modify env variable 'to_build' as associative array of platform to be built
# key := board build string
# val := platform friendly name
function internal_parse_build_platforms_parameters {
  if [[ -z ${1:-} ]]; then
    log_error "'${FUNCNAME[1]}' requires at least one argument"
    return 1
  fi
  # First, validate / converting parameters into array of platforms to build
  # #####################################################################

  # keys here are the platform string... this is done to dedupe (avoid running same board twice)

  log_section_header "Parsing: build_platform ${*}"

  # First argument can be 'all' to build all platforms that have a friendly name... (note string comparison)
  if [[ "${1}" == "all" ]]; then
    log_verbose "Detected first platform of 'all', so adding all friendly-named platforms"
    for p_key in "${!ALL_PLATFORMS[@]}"; do
      local p_val="${ALL_PLATFORMS[$p_key]}";
      if [[ -v to_build["$p_val"] ]]; then
        log_error "Duplicate build string? ${p_val} used for both ${to_build[$p_val]} and ${p_key}"
        return 3
      fi
      to_build["$p_val"]="${p_key}"
    done
    shift
  fi

  # add any remaining platforms
  while [[ ! -z "${1:-}" ]]; do
    if [[ ! -v ALL_PLATFORMS[$1] ]]; then
      log_error "INVALID platform requested: '${1}'"
      return 2
    fi
    local p_key="${1}"
    local p_val="${ALL_PLATFORMS[$p_key]}"
    if [[ -v to_build["$p_val"] ]]; then
      log_error "Duplicate build string? ${p_val} used for both ${to_build[$p_val]} and ${p_key}"
      return 3
    fi
    to_build["$p_val"]="${p_key}"
    shift
  done

  return 0
}

# pulled this out from function internal_build_platform to ease wrapping
# with travis-ci folds, which otherwise would require many branches to be edited
function internal_build_single_platform {
    local -a samples=()
    
    internal_build_get_samples_for_platform "$p_key"
    __=$?
    if [[ $__ -ne 0 ]]; then
      log_error "Unable to get list of samples to build '$__'"
      samples_failed["$p_key"]="Unable to get list of samples to build '$__'"
      return_value=$(( $return_value | 128 ))
      # TODO: add to overall log JSON
      return $__;
    fi

    if [[ ${#samples[@]} -eq 0 ]]; then
      log_warning "Skipping platform ${p_key} (${p_board}) due to zero applicable samples"
    else

      # (try to) ensure the board is installed
      internal_install_board "${p_key}"
      __=$?
      if [[ $__ -ne 0 ]]; then
        log_error "failure ensuring board is installed '$__'"
        samples_failed["$p_key"]="failure ensuring board is installed '$__'"
        return_value=$(( $return_value | 256 ))
        # TODO: add to overall log JSON
        return $__;
      fi
    
      # switch arduino to using the given platform
      log_action_start "Switching to platform ${p_key} (${p_board})"
      local platform_switch_output
      platform_switch_output=$(arduino --board $p_board --save-prefs 2>&1)
      __=$?
      if [[ $__ -ne 0 ]]; then
        log_action_failed "Failed with return code '$__':" "arduino --board ${p_board} --save-prefs 2>&1" "$platform_output"
        samples_failed["$p_key"]="Failed to switch to board '$__': $platform_output"
        return_value=$(( $return_value | 64 ))
        return $__;
      fi
      log_action_succeeded
    fi # if/else for ${#samples} -eq 0

    # now build each sample...
    # save error output in associative array with sample as key
    local sample_dir_chars=${#SAMPLE_ROOT_DIR}

    for q in "${samples[@]}"; do
      local q_dir=$(dirname $q)   # full path to sketch
      local q_file=$(basename $q) # filename of sketch
      local q_shortdir=${q_dir:(($sample_dir_chars + 1))}

      log_action_start "[$p_key] Building ${q_shortdir}/${q_file}"
      local build_output
      # for nrf52840, consider adding '--pref "compiler.warning_level=none"', as otherwise Nordic's SDK files emit warnings & errors

      build_output=$(arduino --board $p_board --verify $q --pref "build.verbose=true" 2>&1)
      __=$?
      if [[ "${__}" -ne 0 ]]; then
        log_action_failed
        log_build_error "$p_key # $q_shortdir" "$build_output"
        samples_failed["$p_key # $q_shortdir"]="$build_output"
      else
        log_action_succeeded
        samples_succeeded+=("$p_key # $q_shortdir")
      fi
    done # for q in samples...
  

}
function internal_build_platforms {
  local __
  local -A to_build=() # filled in by internal_parse_build_platforms_parameters

  # get list of platforms to be built
  internal_parse_build_platforms_parameters $*
  __=$?
  if [[ $__ -ne 0 ]]; then
    log_error "build_platforms parameter parsing error"
    return "$__"
  fi

  # Can now loop throught the associative array 'to_build'
  local return_value=0
  local -A samples_failed=()
  local -a samples_succeeded=()
  log_progress   "Starting build for ${#to_build[@]} platform(s):"
  for p_board in "${!to_build[@]}"; do
    local p_key="${to_build[$p_board]}"
    log_progress "                   ${p_key}"
  done

  for p_board in "${!to_build[@]}"; do
    local p_key="${to_build[$p_board]}"
    log_start_fold "Build [${p_key}]"
    internal_build_single_platform
    log_end_fold "Build [${p_key}]"
  done # for p_board in to_build

  printf "RESULTS: %s, ${VT_LGREEN}%s suceeded" "${#samples[@]}" "${#samples_succeeded[@]}"
  if [[ ${#samples_failed[@]} -eq 0 ]]; then
    printf " / 0 failures"
  else

    printf " / ${VT_LRED}%s failures"  "${#samples_failed[@]}"
    for succeeded_key in "${samples_succeeded[@]}"; do
      printf "${VT_LGREEN}PASSED: [%s]${VT_NORMAL}\n"  "$succeeded_key"
    done # for failure_key in samples_failed
    for failure_key in "${!samples_failed[@]}"; do
      log_build_error "[${failure_key}]" "${samples_failed[$failure_key]}\n"
      printf "${VT_LPURPLE}FAILED: [%s]${VT_NORMAL}\n" "$failure_key"
      return_value=$(( $return_value | 32 ))
    done # for failure_key in samples_failed

  fi
  printf "${VT_NORMAL}\n"
  return "$return_value"
}
# External entry point:
#    Called to build all examples for one or more platforms
# Examples:
#    build_platforms all
#    build_platforms uno
function build_platform {
  #log_start_fold "build_platform '${platforms}'"
  local -a platform_array=($platforms)
  internal_build_platforms "${platform_array[@]}"
  #log_end_fold "build_platform '${platforms}"
}


internal_script_config
__=$?
if [[ $__ -ne 0 ]]; then
  log_fatal "Failure in script config, exiting to abort attempts to build"
  $ret ${__}
fi

internal_run_once_setup
__=$?
if [[ $__ -ne 0 ]]; then
  log_fatal "Failure in run-once setup, exiting to abort attempts to build"
  $ret ${__}
fi

if [[ $INSTALL_SH_SOURCED -ne 1 ]]; then
  platforms="uno gemma zero due"  build_platform
  platforms="esp32 esp8266"       build_platform
  platforms="trinket m4"          build_platform
  platforms="stm32 nrf52840"      build_platform
fi

$ret ${__} 
