/*!
 *  @file       midi_Message.h
 *  Project     Arduino MIDI Library
 *  @brief      MIDI Library for the Arduino - Message struct definition
 *  @author     Francois Best
 *  @date       11/06/14
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

#include "midi_Namespace.h"
#include "midi_Defs.h"
#include "fl/memfill.h"

BEGIN_MIDI_NAMESPACE

/*! The Message structure contains decoded data of a MIDI message
    read from the serial port with read()
 */
template<unsigned SysExMaxSize>
struct Message
{
    /*! Default constructor
     \n Initializes the attributes with their default values.
    */
    inline Message()
        : channel(0)
        , type(MIDI_NAMESPACE::InvalidType)
        , data1(0)
        , data2(0)
        , valid(false)
    {
        fl::memfill(sysexArray, 0, sSysExMaxSize * sizeof(DataByte));
    }

    /*! The maximum size for the System Exclusive array.
    */
    static const unsigned sSysExMaxSize = SysExMaxSize;

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
    DataByte sysexArray[sSysExMaxSize];

    /*! This boolean indicates if the message is valid or not.
     There is no channel consideration here,
     validity means the message respects the MIDI norm.
     */
    bool valid;

    /*! Total Length of the message.
     */
    unsigned length;
    
    inline unsigned getSysExSize() const
    {
        const unsigned size = unsigned(data2) << 8 | data1;
        return size > sSysExMaxSize ? sSysExMaxSize : size;
    }
};

END_MIDI_NAMESPACE
