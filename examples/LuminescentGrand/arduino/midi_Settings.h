/*!
 *  @file       midi_Settings.h
 *  Project     Arduino MIDI Library
 *  @brief      MIDI Library for the Arduino - Settings
 *  @author     Francois Best
 *  @date       24/02/11
 *  @license    MIT - Copyright (c) 2015 Francois Best
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "midi_Defs.h"

BEGIN_MIDI_NAMESPACE

/*! \brief Default Settings for the MIDI Library.

 To change the default settings, don't edit them there, create a subclass and
 override the values in that subclass, then use the MIDI_CREATE_CUSTOM_INSTANCE
 macro to create your instance. The settings you don't override will keep their
 default value. Eg:
 \code{.cpp}
 struct MySettings : public midi::DefaultSettings
 {
    static const unsigned SysExMaxSize = 1024; // Accept SysEx messages up to 1024 bytes long.
 };

 MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial2, midi, MySettings);
 \endcode
 */
struct DefaultSettings
{
    /*! Running status enables short messages when sending multiple values
    of the same type and channel.\n
    Must be disabled to send USB MIDI messages to a computer
    Warning: does not work with some hardware, enable with caution.
    */
    static const bool UseRunningStatus = false;

    /*! NoteOn with 0 velocity should be handled as NoteOf.\n
    Set to true  to get NoteOff events when receiving null-velocity NoteOn messages.\n
    Set to false to get NoteOn  events when receiving null-velocity NoteOn messages.
    */
    static const bool HandleNullVelocityNoteOnAsNoteOff = true;

    /*! Setting this to true will make MIDI.read parse only one byte of data for each
    call when data is available. This can speed up your application if receiving
    a lot of traffic, but might induce MIDI Thru and treatment latency.
    */
    static const bool Use1ByteParsing = true;

    /*! Maximum size of SysEx receivable. Decrease to save RAM if you don't expect
    to receive SysEx, or adjust accordingly.
    */
    static const unsigned SysExMaxSize = 128;

    /*! Global switch to turn on/off sender ActiveSensing
    Set to true to send ActiveSensing
    Set to false will not send ActiveSensing message (will also save memory)
    */
    static const bool UseSenderActiveSensing = false;

    /*! Global switch to turn on/off receiver ActiveSensing
    Set to true to check for message timeouts (via ErrorCallback)
    Set to false will not check if chained device are still alive (if they use ActiveSensing) (will also save memory)
    */
    static const bool UseReceiverActiveSensing = false;

    /*! Active Sensing is intended to be sent
    repeatedly by the sender to tell the receiver that a connection is alive. Use
    of this message is optional. When initially received, the
    receiver will expect to receive another Active Sensing
    message each 300ms (max), and if it does not then it will
    assume that the connection has been terminated. At
    termination, the receiver will turn off all voices and return to
    normal (non- active sensing) operation.

    Typical value is 250 (ms) - an Active Sensing command is send every 250ms.
    (All Roland devices send Active Sensing every 250ms)

    Setting this field to 0 will disable sending MIDI active sensing.
    */
    static const uint16_t SenderActiveSensingPeriodicity = 0;
};

END_MIDI_NAMESPACE
