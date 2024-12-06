/*!
 *  @file       .h
 *  Project     Arduino MIDI Library
 *  @brief      MIDI Library for the Arduino - Namespace declaration
 *  @version    4.0
 *  @author     Francois Best
 *  @date       24/02/11
 *  license     GPL Forty Seven Effects - 2011
 */

#include <inttypes.h>

#ifdef FSE_AVR
#include <ak47.h>
#endif

// -----------------------------------------------------------------------------
// Misc. options

// Running status enables short messages when sending multiple values
// of the same type and channel.
// Set to 0 if you have troubles controlling your hardware.
#define MIDI_USE_RUNNING_STATUS         1

// NoteOn with 0 velocity should be handled as NoteOf.
// Set to 1 to get NoteOff events when receiving null-velocity NoteOn messages.
// Set to 0 to get NoteOn  events when receiving null-velocity NoteOn messages.
#define MIDI_HANDLE_NULL_VELOCITY_NOTE_ON_AS_NOTE_OFF 1

// Setting this to 1 will make MIDI.read parse only one byte of data for each
// call when data is available. This can speed up your application if receiving
// a lot of traffic, but might induce MIDI Thru and treatment latency.
#define MIDI_USE_1BYTE_PARSING          1

#define MIDI_BAUDRATE                   31250
#define MIDI_SYSEX_ARRAY_SIZE           255     // Maximum size is 65535 bytes.


/*!
 *  @file       midi_Defs.h
 *  Project     Arduino MIDI Library
 *  @brief      MIDI Library for the Arduino - Definitions
 *  @version    4.0
 *  @author     Francois Best
 *  @date       24/02/11
 *  license     GPL Forty Seven Effects - 2011
 */


// -----------------------------------------------------------------------------

#define MIDI_CHANNEL_OMNI       0
#define MIDI_CHANNEL_OFF        17 // and over

#define MIDI_PITCHBEND_MIN      -8192
#define MIDI_PITCHBEND_MAX      8191


/*!
 *  @file       midi_Settings.h
 *  Project     Arduino MIDI Library
 *  @brief      MIDI Library for the Arduino - Settings
 *  @version    4.0
 *  @author     Francois Best
 *  @date       24/02/11
 *  license     GPL Forty Seven Effects - 2011
 */

// -----------------------------------------------------------------------------

// Here are a few settings you can change to customize
// the library for your own project. You can for example
// choose to compile only parts of it so you gain flash
// space and optimise the speed of your sketch.

// -----------------------------------------------------------------------------

// Compilation flags. Set them to 1 to build the associated feature
// (MIDI in, out, thru), or to 0 to disable the feature and save space.
// Note that thru can work only if input and output are enabled.

#define MIDI_BUILD_INPUT                1
#define MIDI_BUILD_OUTPUT               1
#define MIDI_BUILD_THRU                 1

#define MIDI_USE_CALLBACKS              1

// -----------------------------------------------------------------------------

// Create a MIDI object automatically on the port defined with MIDI_SERIAL_PORT.
#ifndef MIDI_AUTO_INSTANCIATE
#   ifdef ARDUINO
#       define MIDI_AUTO_INSTANCIATE    1
#   else
#       define MIDI_AUTO_INSTANCIATE    0   ///< @see MIDI_CREATE_INSTANCE
#   endif
#endif

// -----------------------------------------------------------------------------
// Default serial port configuration (if MIDI_AUTO_INSTANCIATE is set)

// Set the default port to use for MIDI.
#if MIDI_AUTO_INSTANCIATE
#   ifdef ARDUINO
#       ifdef USBCON
#           define MIDI_DEFAULT_SERIAL_PORT     Serial1 // For Leonardo
#       else
#           define MIDI_DEFAULT_SERIAL_PORT     Serial  // For other Arduinos
#       endif
#       define MIDI_DEFAULT_SERIAL_CLASS        HardwareSerial
#       include "Arduino.h"
#       include "HardwareSerial.h"
#   else
#       error Auto-instanciation disabled. Use MIDI_CREATE_INSTANCE macro.
#   endif
#endif


// -----------------------------------------------------------------------------
// Type definitions

typedef uint8_t byte;

typedef byte StatusByte;
typedef byte DataByte;
typedef byte Channel;
typedef byte FilterMode;

// -----------------------------------------------------------------------------

/*! Enumeration of MIDI types */
enum MidiType
{
    InvalidType           = 0x00,    ///< For notifying errors
    NoteOff               = 0x80,    ///< Note Off
    NoteOn                = 0x90,    ///< Note On
    AfterTouchPoly        = 0xA0,    ///< Polyphonic AfterTouch
    ControlChange         = 0xB0,    ///< Control Change / Channel Mode
    ProgramChange         = 0xC0,    ///< Program Change
    AfterTouchChannel     = 0xD0,    ///< Channel (monophonic) AfterTouch
    PitchBend             = 0xE0,    ///< Pitch Bend
    SystemExclusive       = 0xF0,    ///< System Exclusive
    TimeCodeQuarterFrame  = 0xF1,    ///< System Common - MIDI Time Code Quarter Frame
    SongPosition          = 0xF2,    ///< System Common - Song Position Pointer
    SongSelect            = 0xF3,    ///< System Common - Song Select
    TuneRequest           = 0xF6,    ///< System Common - Tune Request
    Clock                 = 0xF8,    ///< System Real Time - Timing Clock
    Start                 = 0xFA,    ///< System Real Time - Start
    Continue              = 0xFB,    ///< System Real Time - Continue
    Stop                  = 0xFC,    ///< System Real Time - Stop
    ActiveSensing         = 0xFE,    ///< System Real Time - Active Sensing
    SystemReset           = 0xFF,    ///< System Real Time - System Reset
};

// -----------------------------------------------------------------------------

/*! Enumeration of Thru filter modes */
enum MidiFilterMode
{
    Off                   = 0,  ///< Thru disabled (nothing passes through).
    Full                  = 1,  ///< Fully enabled Thru (every incoming message is sent back).
    SameChannel           = 2,  ///< Only the messages on the Input Channel will be sent back.
    DifferentChannel      = 3,  ///< All the messages but the ones on the Input Channel will be sent back.
};

// -----------------------------------------------------------------------------

/*! \brief Enumeration of Control Change command numbers.
 See the detailed controllers numbers & description here:
 http://www.somascape.org/midi/tech/spec.html#ctrlnums
 */
enum MidiControlChangeNumber
{
    // High resolution Continuous Controllers MSB (+32 for LSB) ----------------
    BankSelect                  = 0,
    ModulationWheel             = 1,
    BreathController            = 2,
    // CC3 undefined
    FootController              = 4,
    PortamentoTime              = 5,
    DataEntry                   = 6,
    ChannelVolume               = 7,
    Balance                     = 8,
    // CC9 undefined
    Pan                         = 10,
    ExpressionController        = 11,
    EffectControl1              = 12,
    EffectControl2              = 13,
    // CC14 undefined
    // CC15 undefined
    GeneralPurposeController1   = 16,
    GeneralPurposeController2   = 17,
    GeneralPurposeController3   = 18,
    GeneralPurposeController4   = 19,

    // Switches ----------------------------------------------------------------
    Sustain                     = 64,
    Portamento                  = 65,
    Sostenuto                   = 66,
    SoftPedal                   = 67,
    Legato                      = 68,
    Hold                        = 69,

    // Low resolution continuous controllers -----------------------------------
    SoundController1            = 70,   ///< Synth: Sound Variation   FX: Exciter On/Off
    SoundController2            = 71,   ///< Synth: Harmonic Content  FX: Compressor On/Off
    SoundController3            = 72,   ///< Synth: Release Time      FX: Distortion On/Off
    SoundController4            = 73,   ///< Synth: Attack Time       FX: EQ On/Off
    SoundController5            = 74,   ///< Synth: Brightness        FX: Expander On/Off
    SoundController6            = 75,   ///< Synth: Decay Time        FX: Reverb On/Off
    SoundController7            = 76,   ///< Synth: Vibrato Rate      FX: Delay On/Off
    SoundController8            = 77,   ///< Synth: Vibrato Depth     FX: Pitch Transpose On/Off
    SoundController9            = 78,   ///< Synth: Vibrato Delay     FX: Flange/Chorus On/Off
    SoundController10           = 79,   ///< Synth: Undefined         FX: Special Effects On/Off
    GeneralPurposeController5   = 80,
    GeneralPurposeController6   = 81,
    GeneralPurposeController7   = 82,
    GeneralPurposeController8   = 83,
    PortamentoControl           = 84,
    // CC85 to CC90 undefined
    Effects1                    = 91,   ///< Reverb send level
    Effects2                    = 92,   ///< Tremolo depth
    Effects3                    = 93,   ///< Chorus send level
    Effects4                    = 94,   ///< Celeste depth
    Effects5                    = 95,   ///< Phaser depth

    // Channel Mode messages ---------------------------------------------------
    AllSoundOff                 = 120,
    ResetAllControllers         = 121,
    LocalControl                = 122,
    AllNotesOff                 = 123,
    OmniModeOff                 = 124,
    OmniModeOn                  = 125,
    MonoModeOn                  = 126,
    PolyModeOn                  = 127
};


// -----------------------------------------------------------------------------

/*! The midimsg structure contains decoded data of a MIDI message
    read from the serial port with read()
 */
struct Message
{

    /*! The MIDI channel on which the message was recieved.
     \n Value goes from 1 to 16.
     */
    Channel channel;

    /*! The type of the message
     (see the MidiType enum for types reference)
     */
    MidiType type;

    /*! The first data byte.
     \n Value goes from 0 to 127.
     */
    DataByte data1;

    /*! The second data byte.
     If the message is only 2 bytes long, this one is null.
     \n Value goes from 0 to 127.
     */
    DataByte data2;

    /*! System Exclusive dedicated byte array.
     \n Array length is stocked on 16 bits,
     in data1 (LSB) and data2 (MSB)
     */
    DataByte sysexArray[MIDI_SYSEX_ARRAY_SIZE];

    /*! This boolean indicates if the message is valid or not.
     There is no channel consideration here,
     validity means the message respects the MIDI norm.
     */
    bool valid;

};

// -----------------------------------------------------------------------------

/*! \brief Create an instance of the library attached to a serial port.
 You can use HardwareSerial or SoftwareSerial for the serial port.
 */
#define MIDI_CREATE_INSTANCE(Type, SerialPort, Name)                            \
    MidiInterface<Type> Name((Type&)SerialPort);

/*! \brief Shortcut for MIDI Interface class with template argument.
 The class name for a MIDI object using the hardware UART would be
 midi::MidiInterface<HardwareSerial>, when the macro is MIDI_CLASS(HardwareSerial).
 */
#define MIDI_CLASS(Type)                                                        \
    midi::MidiInterface<Type>


// -----------------------------------------------------------------------------


/*!
 *  @file       MIDI.h
 *  Project     Arduino MIDI Library
 *  @brief      MIDI Library for the Arduino
 *  @version    4.0
 *  @author     Francois Best
 *  @date       24/02/11
 *  license     GPL Forty Seven Effects - 2011
 */


// -----------------------------------------------------------------------------


/*! \brief The main class for MIDI handling.
It is templated over the type of serial port to provide abstraction from
the hardware interface, meaning you can use HardwareSerial, SoftwareSerial
or ak47's Uart classes. The only requirement is that the class implements
the begin, read, write and available methods.
 */
template<class SerialPort>
class MidiInterface
{
public:
    MidiInterface(SerialPort& inSerial);
    ~MidiInterface();

public:
    void begin(Channel inChannel = 1);

    // -------------------------------------------------------------------------
    // MIDI Output

#if MIDI_BUILD_OUTPUT

public:
    inline void sendNoteOn(DataByte inNoteNumber,
                           DataByte inVelocity,
                           Channel inChannel);

    inline void sendNoteOff(DataByte inNoteNumber,
                            DataByte inVelocity,
                            Channel inChannel);

    inline void sendProgramChange(DataByte inProgramNumber,
                                  Channel inChannel);

    inline void sendControlChange(DataByte inControlNumber,
                                  DataByte inControlValue,
                                  Channel inChannel);

    inline void sendPitchBend(int inPitchValue,    Channel inChannel);
    inline void sendPitchBend(double inPitchValue, Channel inChannel);

    inline void sendPolyPressure(DataByte inNoteNumber,
                                 DataByte inPressure,
                                 Channel inChannel);

    inline void sendAfterTouch(DataByte inPressure,
                               Channel inChannel);

    inline void sendSysEx(unsigned inLength,
                          const byte* inArray,
                          bool inArrayContainsBoundaries = false);

    inline void sendTimeCodeQuarterFrame(DataByte inTypeNibble,
                                         DataByte inValuesNibble);
    inline void sendTimeCodeQuarterFrame(DataByte inData);

    inline void sendSongPosition(unsigned inBeats);
    inline void sendSongSelect(DataByte inSongNumber);
    inline void sendTuneRequest();
    inline void sendRealTime(MidiType inType);

public:
    void send(MidiType inType,
              DataByte inData1,
              DataByte inData2,
              Channel inChannel);

private:
    inline StatusByte getStatus(MidiType inType,
                                Channel inChannel) const;

#endif // MIDI_BUILD_OUTPUT

    // -------------------------------------------------------------------------
    // MIDI Input

#if MIDI_BUILD_INPUT

public:
    inline bool read();
    inline bool read(Channel inChannel);

public:
    inline MidiType getType() const;
    inline Channel  getChannel() const;
    inline DataByte getData1() const;
    inline DataByte getData2() const;
    inline const byte* getSysExArray() const;
    inline unsigned getSysExArrayLength() const;
    inline bool check() const;

public:
    inline Channel getInputChannel() const;
    inline void setInputChannel(Channel inChannel);

public:
    static inline MidiType getTypeFromStatusByte(byte inStatus);
	static inline bool isChannelMessage(MidiType inType);

private:
    bool parse();
    inline void handleNullVelocityNoteOnAsNoteOff() {

      #if MIDI_HANDLE_NULL_VELOCITY_NOTE_ON_AS_NOTE_OFF
      if (getType() == NoteOn && getData2() == 0)
      {
          mMessage.type = NoteOff;
      }
      #endif
    }
    inline bool inputFilter(Channel inChannel);
    inline void resetInput();

private:
    StatusByte  mRunningStatus_RX;
    Channel     mInputChannel;
    byte        mPendingMessage[3];
    unsigned    mPendingMessageExpectedLenght;
    unsigned    mPendingMessageIndex;
    Message     mMessage;

    // -------------------------------------------------------------------------
    // Input Callbacks

#if MIDI_USE_CALLBACKS

public:
    inline void setHandleNoteOff(void (*fptr)(byte channel, byte note, byte velocity));
    inline void setHandleNoteOn(void (*fptr)(byte channel, byte note, byte velocity));
    inline void setHandleAfterTouchPoly(void (*fptr)(byte channel, byte note, byte pressure));
    inline void setHandleControlChange(void (*fptr)(byte channel, byte number, byte value));
    inline void setHandleProgramChange(void (*fptr)(byte channel, byte number));
    inline void setHandleAfterTouchChannel(void (*fptr)(byte channel, byte pressure));
    inline void setHandlePitchBend(void (*fptr)(byte channel, int bend));
    inline void setHandleSystemExclusive(void (*fptr)(byte * array, byte size));
    inline void setHandleTimeCodeQuarterFrame(void (*fptr)(byte data));
    inline void setHandleSongPosition(void (*fptr)(unsigned beats));
    inline void setHandleSongSelect(void (*fptr)(byte songnumber));
    inline void setHandleTuneRequest(void (*fptr)(void));
    inline void setHandleClock(void (*fptr)(void));
    inline void setHandleStart(void (*fptr)(void));
    inline void setHandleContinue(void (*fptr)(void));
    inline void setHandleStop(void (*fptr)(void));
    inline void setHandleActiveSensing(void (*fptr)(void));
    inline void setHandleSystemReset(void (*fptr)(void));

    inline void disconnectCallbackFromType(MidiType inType);

private:
    void launchCallback();

    void (*mNoteOffCallback)(byte channel, byte note, byte velocity);
    void (*mNoteOnCallback)(byte channel, byte note, byte velocity);
    void (*mAfterTouchPolyCallback)(byte channel, byte note, byte velocity);
    void (*mControlChangeCallback)(byte channel, byte, byte);
    void (*mProgramChangeCallback)(byte channel, byte);
    void (*mAfterTouchChannelCallback)(byte channel, byte);
    void (*mPitchBendCallback)(byte channel, int);
    void (*mSystemExclusiveCallback)(byte * array, byte size);
    void (*mTimeCodeQuarterFrameCallback)(byte data);
    void (*mSongPositionCallback)(unsigned beats);
    void (*mSongSelectCallback)(byte songnumber);
    void (*mTuneRequestCallback)(void);
    void (*mClockCallback)(void);
    void (*mStartCallback)(void);
    void (*mContinueCallback)(void);
    void (*mStopCallback)(void);
    void (*mActiveSensingCallback)(void);
    void (*mSystemResetCallback)(void);

#endif // MIDI_USE_CALLBACKS
#endif // MIDI_BUILD_INPUT

    // -------------------------------------------------------------------------
    // MIDI Soft Thru

#if MIDI_BUILD_THRU

public:
    inline MidiFilterMode getFilterMode() const;
    inline bool getThruState() const;

    inline void turnThruOn(MidiFilterMode inThruFilterMode = Full);
    inline void turnThruOff();
    inline void setThruFilterMode(MidiFilterMode inThruFilterMode);


private:
    void thruFilter(byte inChannel);

private:
    bool            mThruActivated  : 1;
    MidiFilterMode  mThruFilterMode : 7;

#endif // MIDI_BUILD_THRU


#if MIDI_USE_RUNNING_STATUS
private:
    StatusByte mRunningStatus_TX;
#endif

private:
    SerialPort& mSerial;
};

// -----------------------------------------------------------------------------

unsigned encodeSysEx(const byte* inData,  byte* outSysEx, unsigned inLenght);
unsigned decodeSysEx(const byte* inSysEx, byte* outData,  unsigned inLenght);


// -----------------------------------------------------------------------------

#if MIDI_AUTO_INSTANCIATE && defined(ARDUINO)
    extern MidiInterface<MIDI_DEFAULT_SERIAL_CLASS> MIDI;
#endif

// -----------------------------------------------------------------------------

/*!
 *  @file       midi_Inline.hpp
 *  Project     Arduino MIDI Library
 *  @brief      MIDI Library for the Arduino - Inline implementations
 *  @version    4.0
 *  @author     Francois Best
 *  @date       24/02/11
 *  license     GPL Forty Seven Effects - 2011
 */

/// \brief Constructor for MidiInterface.
template<class SerialPort>
MidiInterface<SerialPort>::MidiInterface(SerialPort& inSerial)
    : mSerial(inSerial)
{
#if MIDI_BUILD_INPUT && MIDI_USE_CALLBACKS
    mNoteOffCallback                = 0;
    mNoteOnCallback                 = 0;
    mAfterTouchPolyCallback         = 0;
    mControlChangeCallback          = 0;
    mProgramChangeCallback          = 0;
    mAfterTouchChannelCallback      = 0;
    mPitchBendCallback              = 0;
    mSystemExclusiveCallback        = 0;
    mTimeCodeQuarterFrameCallback   = 0;
    mSongPositionCallback           = 0;
    mSongSelectCallback             = 0;
    mTuneRequestCallback            = 0;
    mClockCallback                  = 0;
    mStartCallback                  = 0;
    mContinueCallback               = 0;
    mStopCallback                   = 0;
    mActiveSensingCallback          = 0;
    mSystemResetCallback            = 0;
#endif
}

/*! \brief Destructor for MidiInterface.

 This is not really useful for the Arduino, as it is never called...
 */
template<class SerialPort>
MidiInterface<SerialPort>::~MidiInterface()
{
}

// -----------------------------------------------------------------------------

/*! \brief Call the begin method in the setup() function of the Arduino.

 All parameters are set to their default values:
 - Input channel set to 1 if no value is specified
 - Full thru mirroring
 */
template<class SerialPort>
void MidiInterface<SerialPort>::begin(Channel inChannel)
{
    // Initialise the Serial port
#if defined(ARDUINO)
    mSerial.begin(MIDI_BAUDRATE);
#elif defined(FSE_AVR)
    mSerial. template open<MIDI_BAUDRATE>();
#endif

#if MIDI_BUILD_OUTPUT && MIDI_USE_RUNNING_STATUS

    mRunningStatus_TX = InvalidType;

#endif // MIDI_BUILD_OUTPUT && MIDI_USE_RUNNING_STATUS


#if MIDI_BUILD_INPUT

    mInputChannel = inChannel;
    mRunningStatus_RX = InvalidType;
    mPendingMessageIndex = 0;
    mPendingMessageExpectedLenght = 0;

    mMessage.valid   = false;
    mMessage.type    = InvalidType;
    mMessage.channel = 0;
    mMessage.data1   = 0;
    mMessage.data2   = 0;

#endif // MIDI_BUILD_INPUT


#if (MIDI_BUILD_INPUT && MIDI_BUILD_OUTPUT && MIDI_BUILD_THRU) // Thru

    mThruFilterMode = Full;
    mThruActivated  = true;

#endif // Thru

}


// -----------------------------------------------------------------------------
//                                 Output
// -----------------------------------------------------------------------------

#if MIDI_BUILD_OUTPUT

/*! \addtogroup output
 @{
 */

/*! \brief Generate and send a MIDI message from the values given.
 \param inType    The message type (see type defines for reference)
 \param inData1   The first data byte.
 \param inData2   The second data byte (if the message contains only 1 data byte,
 set this one to 0).
 \param inChannel The output channel on which the message will be sent
 (values from 1 to 16). Note: you cannot send to OMNI.

 This is an internal method, use it only if you need to send raw data
 from your code, at your own risks.
 */
template<class SerialPort>
void MidiInterface<SerialPort>::send(MidiType inType,
                                     DataByte inData1,
                                     DataByte inData2,
                                     Channel inChannel)
{
    // Then test if channel is valid
    if (inChannel >= MIDI_CHANNEL_OFF  ||
        inChannel == MIDI_CHANNEL_OMNI ||
        inType < NoteOff)
    {

#if MIDI_USE_RUNNING_STATUS
        mRunningStatus_TX = InvalidType;
#endif

        return; // Don't send anything
    }

    if (inType <= PitchBend)  // Channel messages
    {
        // Protection: remove MSBs on data
        inData1 &= 0x7f;
        inData2 &= 0x7f;

        const StatusByte status = getStatus(inType, inChannel);

#if MIDI_USE_RUNNING_STATUS
        // Check Running Status
        if (mRunningStatus_TX != status)
        {
            // New message, memorise and send header
            mRunningStatus_TX = status;
            mSerial.write(mRunningStatus_TX);
        }
#else
        // Don't care about running status, send the status byte.
        mSerial.write(status);
#endif

        // Then send data
        mSerial.write(inData1);
        if (inType != ProgramChange && inType != AfterTouchChannel)
            mSerial.write(inData2);

        return;
    }
    else if (inType >= TuneRequest && inType <= SystemReset)
        sendRealTime(inType); // System Real-time and 1 byte.
}

// -----------------------------------------------------------------------------

/*! \brief Send a Note On message
 \param inNoteNumber  Pitch value in the MIDI format (0 to 127).
 \param inVelocity    Note attack velocity (0 to 127). A NoteOn with 0 velocity
 is considered as a NoteOff.
 \param inChannel     The channel on which the message will be sent (1 to 16).

 Take a look at the values, names and frequencies of notes here:
 http://www.phys.unsw.edu.au/jw/notes.html
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendNoteOn(DataByte inNoteNumber,
                                           DataByte inVelocity,
                                           Channel inChannel)
{
    send(NoteOn, inNoteNumber, inVelocity, inChannel);
}

/*! \brief Send a Note Off message
 \param inNoteNumber  Pitch value in the MIDI format (0 to 127).
 \param inVelocity    Release velocity (0 to 127).
 \param inChannel     The channel on which the message will be sent (1 to 16).

 Note: you can send NoteOn with zero velocity to make a NoteOff, this is based
 on the Running Status principle, to avoid sending status messages and thus
 sending only NoteOn data. This method will always send a real NoteOff message.
 Take a look at the values, names and frequencies of notes here:
 http://www.phys.unsw.edu.au/jw/notes.html
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendNoteOff(DataByte inNoteNumber,
                                            DataByte inVelocity,
                                            Channel inChannel)
{
    send(NoteOff, inNoteNumber, inVelocity, inChannel);
}

/*! \brief Send a Program Change message
 \param inProgramNumber The Program to select (0 to 127).
 \param inChannel       The channel on which the message will be sent (1 to 16).
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendProgramChange(DataByte inProgramNumber,
                                                  Channel inChannel)
{
    send(ProgramChange, inProgramNumber, 0, inChannel);
}

/*! \brief Send a Control Change message
 \param inControlNumber The controller number (0 to 127).
 \param inControlValue  The value for the specified controller (0 to 127).
 \param inChannel       The channel on which the message will be sent (1 to 16).
 @see MidiControlChangeNumber
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendControlChange(DataByte inControlNumber,
                                                  DataByte inControlValue,
                                                  Channel inChannel)
{
    send(ControlChange, inControlNumber, inControlValue, inChannel);
}

/*! \brief Send a Polyphonic AfterTouch message (applies to a specified note)
 \param inNoteNumber  The note to apply AfterTouch to (0 to 127).
 \param inPressure    The amount of AfterTouch to apply (0 to 127).
 \param inChannel     The channel on which the message will be sent (1 to 16).
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendPolyPressure(DataByte inNoteNumber,
                                                 DataByte inPressure,
                                                 Channel inChannel)
{
    send(AfterTouchPoly, inNoteNumber, inPressure, inChannel);
}

/*! \brief Send a MonoPhonic AfterTouch message (applies to all notes)
 \param inPressure    The amount of AfterTouch to apply to all notes.
 \param inChannel     The channel on which the message will be sent (1 to 16).
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendAfterTouch(DataByte inPressure,
                                               Channel inChannel)
{
    send(AfterTouchChannel, inPressure, 0, inChannel);
}

/*! \brief Send a Pitch Bend message using a signed integer value.
 \param inPitchValue  The amount of bend to send (in a signed integer format),
 between MIDI_PITCHBEND_MIN and MIDI_PITCHBEND_MAX,
 center value is 0.
 \param inChannel     The channel on which the message will be sent (1 to 16).
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendPitchBend(int inPitchValue,
                                              Channel inChannel)
{
    const unsigned bend = inPitchValue - MIDI_PITCHBEND_MIN;
    send(PitchBend, (bend & 0x7f), (bend >> 7) & 0x7f, inChannel);
}


/*! \brief Send a Pitch Bend message using a floating point value.
 \param inPitchValue  The amount of bend to send (in a floating point format),
 between -1.0f (maximum downwards bend)
 and +1.0f (max upwards bend), center value is 0.0f.
 \param inChannel     The channel on which the message will be sent (1 to 16).
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendPitchBend(double inPitchValue,
                                              Channel inChannel)
{
    const int value = inPitchValue * MIDI_PITCHBEND_MAX;
    sendPitchBend(value, inChannel);
}

/*! \brief Generate and send a System Exclusive frame.
 \param inLength  The size of the array to send
 \param inArray   The byte array containing the data to send
 \param inArrayContainsBoundaries When set to 'true', 0xf0 & 0xf7 bytes
 (start & stop SysEx) will NOT be sent
 (and therefore must be included in the array).
 default value for ArrayContainsBoundaries is set to 'false' for compatibility
 with previous versions of the library.
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendSysEx(unsigned inLength,
                                          const byte* inArray,
                                          bool inArrayContainsBoundaries)
{
    if (inArrayContainsBoundaries == false)
    {
        mSerial.write(0xf0);

        for (unsigned i = 0; i < inLength; ++i)
            mSerial.write(inArray[i]);

        mSerial.write(0xf7);
    }
    else
    {
        for (unsigned i = 0; i < inLength; ++i)
            mSerial.write(inArray[i]);
    }

#if MIDI_USE_RUNNING_STATUS
    mRunningStatus_TX = InvalidType;
#endif
}

/*! \brief Send a Tune Request message.

 When a MIDI unit receives this message,
 it should tune its oscillators (if equipped with any).
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendTuneRequest()
{
    sendRealTime(TuneRequest);
}

/*! \brief Send a MIDI Time Code Quarter Frame.

 \param inTypeNibble      MTC type
 \param inValuesNibble    MTC data
 See MIDI Specification for more information.
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendTimeCodeQuarterFrame(DataByte inTypeNibble,
                                                         DataByte inValuesNibble)
{
    const byte data = (((inTypeNibble & 0x07) << 4) | (inValuesNibble & 0x0f));
    sendTimeCodeQuarterFrame(data);
}

/*! \brief Send a MIDI Time Code Quarter Frame.

 See MIDI Specification for more information.
 \param inData  if you want to encode directly the nibbles in your program,
                you can send the byte here.
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendTimeCodeQuarterFrame(DataByte inData)
{
    mSerial.write((byte)TimeCodeQuarterFrame);
    mSerial.write(inData);

#if MIDI_USE_RUNNING_STATUS
    mRunningStatus_TX = InvalidType;
#endif
}

/*! \brief Send a Song Position Pointer message.
 \param inBeats    The number of beats since the start of the song.
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendSongPosition(unsigned inBeats)
{
    mSerial.write((byte)SongPosition);
    mSerial.write(inBeats & 0x7f);
    mSerial.write((inBeats >> 7) & 0x7f);

#if MIDI_USE_RUNNING_STATUS
    mRunningStatus_TX = InvalidType;
#endif
}

/*! \brief Send a Song Select message */
template<class SerialPort>
void MidiInterface<SerialPort>::sendSongSelect(DataByte inSongNumber)
{
    mSerial.write((byte)SongSelect);
    mSerial.write(inSongNumber & 0x7f);

#if MIDI_USE_RUNNING_STATUS
    mRunningStatus_TX = InvalidType;
#endif
}

/*! \brief Send a Real Time (one byte) message.

 \param inType    The available Real Time types are:
 Start, Stop, Continue, Clock, ActiveSensing and SystemReset.
 You can also send a Tune Request with this method.
 @see MidiType
 */
template<class SerialPort>
void MidiInterface<SerialPort>::sendRealTime(MidiType inType)
{
    switch (inType)
    {
        case TuneRequest: // Not really real-time, but one byte anyway.
        case Clock:
        case Start:
        case Stop:
        case Continue:
        case ActiveSensing:
        case SystemReset:
            mSerial.write((byte)inType);
            break;
        default:
            // Invalid Real Time marker
            break;
    }

    // Do not cancel Running Status for real-time messages as they can be
    // interleaved within any message. Though, TuneRequest can be sent here,
    // and as it is a System Common message, it must reset Running Status.
#if MIDI_USE_RUNNING_STATUS
    if (inType == TuneRequest) mRunningStatus_TX = InvalidType;
#endif
}

/*! @} */ // End of doc group MIDI Output

// -----------------------------------------------------------------------------

template<class SerialPort>
StatusByte MidiInterface<SerialPort>::getStatus(MidiType inType,
                                                Channel inChannel) const
{
    return ((byte)inType | ((inChannel - 1) & 0x0f));
}

#endif // MIDI_BUILD_OUTPUT


// -----------------------------------------------------------------------------
//                                  Input
// -----------------------------------------------------------------------------

#if MIDI_BUILD_INPUT

/*! \addtogroup input
 @{
*/

/*! \brief Read messages from the serial port using the main input channel.

 \return True if a valid message has been stored in the structure, false if not.
 A valid message is a message that matches the input channel. \n\n
 If the Thru is enabled and the message matches the filter,
 it is sent back on the MIDI output.
 @see see setInputChannel()
 */
template<class SerialPort>
inline bool MidiInterface<SerialPort>::read()
{
    return read(mInputChannel);
}

/*! \brief Read messages on a specified channel.
 */
template<class SerialPort>
inline bool MidiInterface<SerialPort>::read(Channel inChannel)
{
    if (inChannel >= MIDI_CHANNEL_OFF)
        return false; // MIDI Input disabled.

    if (parse())
    {
        handleNullVelocityNoteOnAsNoteOff();
        if (inputFilter(inChannel))
        {

#if (MIDI_BUILD_OUTPUT && MIDI_BUILD_THRU)
            thruFilter(inChannel);
#endif

#if MIDI_USE_CALLBACKS
            launchCallback();
#endif
            return true;
        }
    }

    return false;
}

// -----------------------------------------------------------------------------

// Private method: MIDI parser
template<class SerialPort>
bool MidiInterface<SerialPort>::parse()
{
    const byte bytes_available = mSerial.available();

    if (bytes_available == 0) {
        // No data available.
        return false;
    }
    
    if ((mSerial.peek() == ActiveSensing) || (mSerial.peek() == Clock)) {
      // Consume status and keep going.
      mSerial.read();
      return parse();  // Keep going.
    }
    
    // Are we in the middle of another message?
    if ((mSerial.peek() >= NoteOff) && (mPendingMessageIndex > 0)) {
      // Then abort it and start over.
      resetInput();
    }
   

    // Parsing algorithm:
    // Get a byte from the serial buffer.
    // If there is no pending message to be recomposed, start a new one.
    //  - Find type and channel (if pertinent)
    //  - Look for other bytes in buffer, call parser recursively,
    //    until the message is assembled or the buffer is empty.
    // Else, add the extracted byte to the pending message, and check validity.
    // When the message is done, store it.

    const byte extracted = mSerial.read();

    if (mPendingMessageIndex == 0)
    {
        // Start a new pending message
        mPendingMessage[0] = extracted;

        // Check for running status first
        if (isChannelMessage(getTypeFromStatusByte(mRunningStatus_RX)))
        {
            // Only these types allow Running Status

            // If the status byte is not received, prepend it
            // to the pending message
            if (extracted < 0x80)
            {
                mPendingMessage[0]   = mRunningStatus_RX;
                mPendingMessage[1]   = extracted;
                mPendingMessageIndex = 1;
            }
            // Else: well, we received another status byte,
            // so the running status does not apply here.
            // It will be updated upon completion of this message.

            if (mPendingMessageIndex >= (mPendingMessageExpectedLenght - 1))
            {
                mMessage.type = getTypeFromStatusByte(mPendingMessage[0]);
                mMessage.channel = (mPendingMessage[0] & 0x0f) + 1;
                mMessage.data1 = mPendingMessage[1];

                // Save data2 only if applicable
                if (mPendingMessageExpectedLenght == 3)
                    mMessage.data2 = mPendingMessage[2];
                else
                    mMessage.data2 = 0;

                mPendingMessageIndex = 0;
                mPendingMessageExpectedLenght = 0;
                mMessage.valid = true;
                return true;
            }
        }

        switch (getTypeFromStatusByte(mPendingMessage[0]))
        {
                // 1 byte messages
            case Start:
            case Continue:
            case Stop:
            case Clock:
            case ActiveSensing:
            case SystemReset:
            case TuneRequest:
                // Handle the message type directly here.
                mMessage.type    = getTypeFromStatusByte(mPendingMessage[0]);
                mMessage.channel = 0;
                mMessage.data1   = 0;
                mMessage.data2   = 0;
                mMessage.valid   = true;

                // \fix Running Status broken when receiving Clock messages.
                // Do not reset all input attributes, Running Status must remain unchanged.
                //resetInput();

                // We still need to reset these
                mPendingMessageIndex = 0;
                mPendingMessageExpectedLenght = 0;

                return true;
                break;

                // 2 bytes messages
            case ProgramChange:
            case AfterTouchChannel:
            case TimeCodeQuarterFrame:
            case SongSelect:
                mPendingMessageExpectedLenght = 2;
                break;

                // 3 bytes messages
            case NoteOn:
            case NoteOff:
            case ControlChange:
            case PitchBend:
            case AfterTouchPoly:
            case SongPosition:
                mPendingMessageExpectedLenght = 3;
                break;

            case SystemExclusive:
                // The message can be any lenght
                // between 3 and MIDI_SYSEX_ARRAY_SIZE bytes
                mPendingMessageExpectedLenght = MIDI_SYSEX_ARRAY_SIZE;
                mRunningStatus_RX = InvalidType;
                mMessage.sysexArray[0] = SystemExclusive;
                break;

            case InvalidType:
            default:
                // This is obviously wrong. Let's get the hell out'a here.
                resetInput();
                return false;
                break;
        }

        // Then update the index of the pending message.
        mPendingMessageIndex++;

#if USE_1BYTE_PARSING
        // Message is not complete.
        return false;
#else
        // Call the parser recursively
        // to parse the rest of the message.
        return parse();
#endif

    }
    else
    {
        // First, test if this is a status byte
        if (extracted >= 0x80)
        {
            // Reception of status bytes in the middle of an uncompleted message
            // are allowed only for interleaved Real Time message or EOX
            switch (extracted)
            {
                case Clock:
                case Start:
                case Continue:
                case Stop:
                case ActiveSensing:
                case SystemReset:

                    // Here we will have to extract the one-byte message,
                    // pass it to the structure for being read outside
                    // the MIDI class, and recompose the message it was
                    // interleaved into. Oh, and without killing the running status..
                    // This is done by leaving the pending message as is,
                    // it will be completed on next calls.

                    mMessage.type    = (MidiType)extracted;
                    mMessage.data1   = 0;
                    mMessage.data2   = 0;
                    mMessage.channel = 0;
                    mMessage.valid   = true;
                    return true;

                    break;

                    // End of Exclusive
                case 0xf7:
                    if (mMessage.sysexArray[0] == SystemExclusive)
                    {
                        // Store the last byte (EOX)
                        mMessage.sysexArray[mPendingMessageIndex++] = 0xf7;
                        mMessage.type = SystemExclusive;

                        // Get length
                        mMessage.data1   = mPendingMessageIndex & 0xff;
                        mMessage.data2   = mPendingMessageIndex >> 8;
                        mMessage.channel = 0;
                        mMessage.valid   = true;

                        resetInput();
                        return true;
                    }
                    else
                    {
                        // Well well well.. error.
                        resetInput();
                        return false;
                    }

                    break;
                default:
                    break;
            }
        }

        // Add extracted data byte to pending message
        if (mPendingMessage[0] == SystemExclusive) {
            mMessage.sysexArray[mPendingMessageIndex] = extracted;
        } else {
            mPendingMessage[mPendingMessageIndex] = extracted;
        }

        // Now we are going to check if we have reached the end of the message
        if (mPendingMessageIndex >= (mPendingMessageExpectedLenght-1))
        {
            // "FML" case: fall down here with an overflown SysEx..
            // This means we received the last possible data byte that can fit
            // the buffer. If this happens, try increasing MIDI_SYSEX_ARRAY_SIZE.
            if (mPendingMessage[0] == SystemExclusive)
            {
                resetInput();
                return false;
            }

            mMessage.type = getTypeFromStatusByte(mPendingMessage[0]);

            if (isChannelMessage(mMessage.type))
                mMessage.channel = (mPendingMessage[0] & 0x0f) + 1;
            else
                mMessage.channel = 0;

            mMessage.data1 = mPendingMessage[1];

            // Save data2 only if applicable
            if (mPendingMessageExpectedLenght == 3)
                mMessage.data2 = mPendingMessage[2];
            else
                mMessage.data2 = 0;

            // Reset local variables
            mPendingMessageIndex = 0;
            mPendingMessageExpectedLenght = 0;

            mMessage.valid = true;

            // Activate running status (if enabled for the received type)
            switch (mMessage.type)
            {
                case NoteOff:
                case NoteOn:
                case AfterTouchPoly:
                case ControlChange:
                case ProgramChange:
                case AfterTouchChannel:
                case PitchBend:
                    // Running status enabled: store it from received message
                    mRunningStatus_RX = mPendingMessage[0];
                    break;

                default:
                    // No running status
                    mRunningStatus_RX = InvalidType;
                    break;
            }
            return true;
        }
        else
        {
            // Then update the index of the pending message.
            mPendingMessageIndex++;

#if USE_1BYTE_PARSING
            // Message is not complete.
            return false;
#else
            // Call the parser recursively
            // to parse the rest of the message.
            return parse();
#endif
        }
    }

    // What are our chances to fall here?
    return false;
}


// Private method: check if the received message is on the listened channel
template<class SerialPort>
inline bool MidiInterface<SerialPort>::inputFilter(Channel inChannel)
{
    // This method handles recognition of channel
    // (to know if the message is destinated to the Arduino)

    if (mMessage.type == InvalidType)
        return false;

    // First, check if the received message is Channel
    if (mMessage.type >= NoteOff && mMessage.type <= PitchBend)
    {
        // Then we need to know if we listen to it
        if ((mMessage.channel == mInputChannel) ||
            (mInputChannel == MIDI_CHANNEL_OMNI))
        {
            return true;
        }
        else
        {
            // We don't listen to this channel
            return false;
        }
    }
    else
    {
        // System messages are always received
        return true;
    }
}

// Private method: reset input attributes
template<class SerialPort>
inline void MidiInterface<SerialPort>::resetInput()
{
    mPendingMessageIndex = 0;
    mPendingMessageExpectedLenght = 0;
    mRunningStatus_RX = InvalidType;
}

// -----------------------------------------------------------------------------

/*! \brief Get the last received message's type

 Returns an enumerated type. @see MidiType
 */
template<class SerialPort>
MidiType MidiInterface<SerialPort>::getType() const
{
    return mMessage.type;
}

/*! \brief Get the channel of the message stored in the structure.

 \return Channel range is 1 to 16.
 For non-channel messages, this will return 0.
 */
template<class SerialPort>
Channel MidiInterface<SerialPort>::getChannel() const
{
    return mMessage.channel;
}

/*! \brief Get the first data byte of the last received message. */
template<class SerialPort>
DataByte MidiInterface<SerialPort>::getData1() const
{
    return mMessage.data1;
}

/*! \brief Get the second data byte of the last received message. */
template<class SerialPort>
DataByte MidiInterface<SerialPort>::getData2() const
{
    return mMessage.data2;
}

/*! \brief Get the System Exclusive byte array.

 @see getSysExArrayLength to get the array's length in bytes.
 */
template<class SerialPort>
const byte* MidiInterface<SerialPort>::getSysExArray() const
{
    return mMessage.sysexArray;
}

/*! \brief Get the lenght of the System Exclusive array.

 It is coded using data1 as LSB and data2 as MSB.
 \return The array's length, in bytes.
 */
template<class SerialPort>
unsigned MidiInterface<SerialPort>::getSysExArrayLength() const
{
    const unsigned size = ((unsigned)(mMessage.data2) << 8) | mMessage.data1;
    return (size > MIDI_SYSEX_ARRAY_SIZE) ? MIDI_SYSEX_ARRAY_SIZE : size;
}

/*! \brief Check if a valid message is stored in the structure. */
template<class SerialPort>
bool MidiInterface<SerialPort>::check() const
{
    return mMessage.valid;
}

// -----------------------------------------------------------------------------

template<class SerialPort>
Channel MidiInterface<SerialPort>::getInputChannel() const
{
    return mInputChannel;
}

/*! \brief Set the value for the input MIDI channel
 \param inChannel the channel value. Valid values are 1 to 16, MIDI_CHANNEL_OMNI
 if you want to listen to all channels, and MIDI_CHANNEL_OFF to disable input.
 */
template<class SerialPort>
void MidiInterface<SerialPort>::setInputChannel(Channel inChannel)
{
    mInputChannel = inChannel;
}

// -----------------------------------------------------------------------------

/*! \brief Extract an enumerated MIDI type from a status byte.

 This is a utility static method, used internally,
 made public so you can handle MidiTypes more easily.
 */
template<class SerialPort>
MidiType MidiInterface<SerialPort>::getTypeFromStatusByte(byte inStatus)
{
    if ((inStatus  < 0x80) ||
        (inStatus == 0xf4) ||
        (inStatus == 0xf5) ||
        (inStatus == 0xf9) ||
        (inStatus == 0xfD))
    {
        // Data bytes and undefined.
        return InvalidType;
    }
    if (inStatus < 0xf0)
    {
        // Channel message, remove channel nibble.
        return (MidiType)(inStatus & 0xf0);
    }

    return (MidiType)inStatus;
}

template<class SerialPort>
bool MidiInterface<SerialPort>::isChannelMessage(MidiType inType)
{
    return (inType == NoteOff           ||
            inType == NoteOn            ||
            inType == ControlChange     ||
            inType == AfterTouchPoly    ||
            inType == AfterTouchChannel ||
            inType == PitchBend         ||
            inType == ProgramChange);
}

// -----------------------------------------------------------------------------

#if MIDI_USE_CALLBACKS

/*! \addtogroup callbacks
 @{
 */

template<class SerialPort> void MidiInterface<SerialPort>::setHandleNoteOff(void (*fptr)(byte channel, byte note, byte velocity))          { mNoteOffCallback              = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleNoteOn(void (*fptr)(byte channel, byte note, byte velocity))           { mNoteOnCallback               = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleAfterTouchPoly(void (*fptr)(byte channel, byte note, byte pressure))   { mAfterTouchPolyCallback       = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleControlChange(void (*fptr)(byte channel, byte number, byte value))     { mControlChangeCallback        = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleProgramChange(void (*fptr)(byte channel, byte number))                 { mProgramChangeCallback        = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleAfterTouchChannel(void (*fptr)(byte channel, byte pressure))           { mAfterTouchChannelCallback    = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandlePitchBend(void (*fptr)(byte channel, int bend))                        { mPitchBendCallback            = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleSystemExclusive(void (*fptr)(byte* array, byte size))                  { mSystemExclusiveCallback      = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleTimeCodeQuarterFrame(void (*fptr)(byte data))                          { mTimeCodeQuarterFrameCallback = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleSongPosition(void (*fptr)(unsigned beats))                             { mSongPositionCallback         = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleSongSelect(void (*fptr)(byte songnumber))                              { mSongSelectCallback           = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleTuneRequest(void (*fptr)(void))                                        { mTuneRequestCallback          = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleClock(void (*fptr)(void))                                              { mClockCallback                = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleStart(void (*fptr)(void))                                              { mStartCallback                = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleContinue(void (*fptr)(void))                                           { mContinueCallback             = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleStop(void (*fptr)(void))                                               { mStopCallback                 = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleActiveSensing(void (*fptr)(void))                                      { mActiveSensingCallback        = fptr; }
template<class SerialPort> void MidiInterface<SerialPort>::setHandleSystemReset(void (*fptr)(void))                                        { mSystemResetCallback          = fptr; }

/*! \brief Detach an external function from the given type.

 Use this method to cancel the effects of setHandle********.
 \param inType        The type of message to unbind.
 When a message of this type is received, no function will be called.
 */
template<class SerialPort>
void MidiInterface<SerialPort>::disconnectCallbackFromType(MidiType inType)
{
    switch (inType)
    {
        case NoteOff:               mNoteOffCallback                = 0; break;
        case NoteOn:                mNoteOnCallback                 = 0; break;
        case AfterTouchPoly:        mAfterTouchPolyCallback         = 0; break;
        case ControlChange:         mControlChangeCallback          = 0; break;
        case ProgramChange:         mProgramChangeCallback          = 0; break;
        case AfterTouchChannel:     mAfterTouchChannelCallback      = 0; break;
        case PitchBend:             mPitchBendCallback              = 0; break;
        case SystemExclusive:       mSystemExclusiveCallback        = 0; break;
        case TimeCodeQuarterFrame:  mTimeCodeQuarterFrameCallback   = 0; break;
        case SongPosition:          mSongPositionCallback           = 0; break;
        case SongSelect:            mSongSelectCallback             = 0; break;
        case TuneRequest:           mTuneRequestCallback            = 0; break;
        case Clock:                 mClockCallback                  = 0; break;
        case Start:                 mStartCallback                  = 0; break;
        case Continue:              mContinueCallback               = 0; break;
        case Stop:                  mStopCallback                   = 0; break;
        case ActiveSensing:         mActiveSensingCallback          = 0; break;
        case SystemReset:           mSystemResetCallback            = 0; break;
        default:
            break;
    }
}

/*! @} */ // End of doc group MIDI Callbacks

// Private - launch callback function based on received type.
template<class SerialPort>
void MidiInterface<SerialPort>::launchCallback()
{
    // The order is mixed to allow frequent messages to trigger their callback faster.
    switch (mMessage.type)
    {
            // Notes
        case NoteOff:               if (mNoteOffCallback != 0)               mNoteOffCallback(mMessage.channel, mMessage.data1, mMessage.data2);   break;
        case NoteOn:                if (mNoteOnCallback != 0)                mNoteOnCallback(mMessage.channel, mMessage.data1, mMessage.data2);    break;

            // Real-time messages
        case Clock:                 if (mClockCallback != 0)                 mClockCallback();           break;
        case Start:                 if (mStartCallback != 0)                 mStartCallback();           break;
        case Continue:              if (mContinueCallback != 0)              mContinueCallback();        break;
        case Stop:                  if (mStopCallback != 0)                  mStopCallback();            break;
        case ActiveSensing:         if (mActiveSensingCallback != 0)         mActiveSensingCallback();   break;

            // Continuous controllers
        case ControlChange:         if (mControlChangeCallback != 0)         mControlChangeCallback(mMessage.channel, mMessage.data1, mMessage.data2);    break;
        case PitchBend:             if (mPitchBendCallback != 0)             mPitchBendCallback(mMessage.channel, (int)((mMessage.data1 & 0x7f) | ((mMessage.data2 & 0x7f) << 7)) + MIDI_PITCHBEND_MIN); break; // TODO: check this
        case AfterTouchPoly:        if (mAfterTouchPolyCallback != 0)        mAfterTouchPolyCallback(mMessage.channel, mMessage.data1, mMessage.data2);    break;
        case AfterTouchChannel:     if (mAfterTouchChannelCallback != 0)     mAfterTouchChannelCallback(mMessage.channel, mMessage.data1);    break;

        case ProgramChange:         if (mProgramChangeCallback != 0)         mProgramChangeCallback(mMessage.channel, mMessage.data1);    break;
        case SystemExclusive:       if (mSystemExclusiveCallback != 0)       mSystemExclusiveCallback(mMessage.sysexArray, mMessage.data1);    break;

            // Occasional messages
        case TimeCodeQuarterFrame:  if (mTimeCodeQuarterFrameCallback != 0)  mTimeCodeQuarterFrameCallback(mMessage.data1);    break;
        case SongPosition:          if (mSongPositionCallback != 0)          mSongPositionCallback((mMessage.data1 & 0x7f) | ((mMessage.data2 & 0x7f) << 7));    break;
        case SongSelect:            if (mSongSelectCallback != 0)            mSongSelectCallback(mMessage.data1);    break;
        case TuneRequest:           if (mTuneRequestCallback != 0)           mTuneRequestCallback();    break;

        case SystemReset:           if (mSystemResetCallback != 0)           mSystemResetCallback();    break;
        case InvalidType:
        default:
            break;
    }
}

#endif // MIDI_USE_CALLBACKS

/*! @} */ // End of doc group MIDI Input

#endif // MIDI_BUILD_INPUT


// -----------------------------------------------------------------------------
//                                  Thru
// -----------------------------------------------------------------------------

#if (MIDI_BUILD_INPUT && MIDI_BUILD_OUTPUT && MIDI_BUILD_THRU)

/*! \addtogroup thru
 @{
 */

/*! \brief Set the filter for thru mirroring
 \param inThruFilterMode a filter mode

 @see MidiFilterMode
 */
template<class SerialPort>
void MidiInterface<SerialPort>::setThruFilterMode(MidiFilterMode inThruFilterMode)
{
    mThruFilterMode = inThruFilterMode;
    if (mThruFilterMode != Off)
        mThruActivated = true;
    else
        mThruActivated = false;
}

template<class SerialPort>
MidiFilterMode MidiInterface<SerialPort>::getFilterMode() const
{
    return mThruFilterMode;
}

template<class SerialPort>
bool MidiInterface<SerialPort>::getThruState() const
{
    return mThruActivated;
}

template<class SerialPort>
void MidiInterface<SerialPort>::turnThruOn(MidiFilterMode inThruFilterMode)
{
    mThruActivated = true;
    mThruFilterMode = inThruFilterMode;
}

template<class SerialPort>
void MidiInterface<SerialPort>::turnThruOff()
{
    mThruActivated = false;
    mThruFilterMode = Off;
}

/*! @} */ // End of doc group MIDI Thru

// This method is called upon reception of a message
// and takes care of Thru filtering and sending.
// - All system messages (System Exclusive, Common and Real Time) are passed
//   to output unless filter is set to Off.
// - Channel messages are passed to the output whether their channel
//   is matching the input channel and the filter setting
template<class SerialPort>
void MidiInterface<SerialPort>::thruFilter(Channel inChannel)
{
    // If the feature is disabled, don't do anything.
    if (!mThruActivated || (mThruFilterMode == Off))
        return;

    // First, check if the received message is Channel
    if (mMessage.type >= NoteOff && mMessage.type <= PitchBend)
    {
        const bool filter_condition = ((mMessage.channel == mInputChannel) ||
                                       (mInputChannel == MIDI_CHANNEL_OMNI));

        // Now let's pass it to the output
        switch (mThruFilterMode)
        {
            case Full:
                send(mMessage.type,
                     mMessage.data1,
                     mMessage.data2,
                     mMessage.channel);
                break;

            case SameChannel:
                if (filter_condition)
                {
                    send(mMessage.type,
                         mMessage.data1,
                         mMessage.data2,
                         mMessage.channel);
                }
                break;

            case DifferentChannel:
                if (!filter_condition)
                {
                    send(mMessage.type,
                         mMessage.data1,
                         mMessage.data2,
                         mMessage.channel);
                }
                break;

            case Off:
                // Do nothing.
                // Technically it's impossible to get there because
                // the case was already tested earlier.
                break;

            default:
                break;
        }
    }
    else
    {
        // Send the message to the output
        switch (mMessage.type)
        {
                // Real Time and 1 byte
            case Clock:
            case Start:
            case Stop:
            case Continue:
            case ActiveSensing:
            case SystemReset:
            case TuneRequest:
                sendRealTime(mMessage.type);
                break;

            case SystemExclusive:
                // Send SysEx (0xf0 and 0xf7 are included in the buffer)
                sendSysEx(getSysExArrayLength(), getSysExArray(), true);
                break;

            case SongSelect:
                sendSongSelect(mMessage.data1);
                break;

            case SongPosition:
                sendSongPosition(mMessage.data1 | ((unsigned)mMessage.data2 << 7));
                break;

            case TimeCodeQuarterFrame:
                sendTimeCodeQuarterFrame(mMessage.data1,mMessage.data2);
                break;
            default:
                break;
        }
    }
}

#endif // MIDI_BUILD_THRU

// -----------------------------------------------------------------------------


#if !(MIDI_BUILD_INPUT) && !(MIDI_BUILD_OUTPUT)
#   error To use MIDI, you need to enable at least input or output.
#endif

#if MIDI_BUILD_THRU && !(MIDI_BUILD_OUTPUT)
#   error For thru to work, you need to enable output.
#endif
#if MIDI_BUILD_THRU && !(MIDI_BUILD_INPUT)
#   error For thru to work, you need to enable input.
#endif

// -----------------------------------------------------------------------------

#if MIDI_AUTO_INSTANCIATE && defined(ARDUINO)
    MIDI_CREATE_INSTANCE(MIDI_DEFAULT_SERIAL_CLASS,
                         MIDI_DEFAULT_SERIAL_PORT,
                         MIDI);
#endif

// -----------------------------------------------------------------------------


/*! \brief Encode System Exclusive messages.
 SysEx messages are encoded to guarantee transmission of data bytes higher than
 127 without breaking the MIDI protocol. Use this static method to convert the
 data you want to send.
 \param inData The data to encode.
 \param outSysEx The output buffer where to store the encoded message.
 \param inLength The lenght of the input buffer.
 \return The lenght of the encoded output buffer.
 @see decodeSysEx
 */
unsigned encodeSysEx(const byte* inData, byte* outSysEx, unsigned inLength)
{
    unsigned outLength  = 0;     // Num bytes in output array.
    byte count          = 0;     // Num 7bytes in a block.
    outSysEx[0]         = 0;

    for (unsigned i = 0; i < inLength; ++i)
    {
        const byte data = inData[i];
        const byte msb  = data >> 7;
        const byte body = data & 0x7f;

        outSysEx[0] |= (msb << count);
        outSysEx[1 + count] = body;

        if (count++ == 6)
        {
            outSysEx   += 8;
            outLength  += 8;
            outSysEx[0] = 0;
            count       = 0;
        }
    }
    return outLength + count + (count != 0 ? 1 : 0);
}

/*! \brief Decode System Exclusive messages.
 SysEx messages are encoded to guarantee transmission of data bytes higher than
 127 without breaking the MIDI protocol. Use this static method to reassemble
 your received message.
 \param inSysEx The SysEx data received from MIDI in.
 \param outData    The output buffer where to store the decrypted message.
 \param inLength The lenght of the input buffer.
 \return The lenght of the output buffer.
 @see encodeSysEx @see getSysExArrayLength
 */
unsigned decodeSysEx(const byte* inSysEx, byte* outData, unsigned inLength)
{
    unsigned count  = 0;
    byte msbStorage = 0;

    for (unsigned i = 0; i < inLength; ++i)
    {
        if ((i % 8) == 0)
        {
            msbStorage = inSysEx[i];
        }
        else
        {
            outData[count++] = inSysEx[i] | ((msbStorage & 1) << 7);
            msbStorage >>= 1;
        }
    }
    return count;
}

