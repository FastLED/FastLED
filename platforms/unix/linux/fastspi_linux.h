#ifndef __INC_FASTSPI_LINUX_H
#define __INC_FASTSPI_LINUX_H

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <stdexcept>
#include <system_error>

#define LINUX_SPIDEV_PATH    "/dev/spidev"

FASTLED_NAMESPACE_BEGIN

class Selectable;

template <uint8_t _SPI_BUS, uint8_t _SPI_CS, uint8_t _SPI_SPEED> class LinuxHardwareSPIOutput {
    Selectable *m_pSelect;
    uint8_t spiMode;
    uint8_t spiBitsPerWord;
    int spiFileDesc;
    uint32_t spiClock;
    uint8_t   *xmitBuffer;
    int xmitBufferSize;
    std::string spiDevice;

    // Hack for static methods
    static LinuxHardwareSPIOutput<_SPI_BUS, _SPI_CS, _SPI_SPEED> *spiBus;

    void throw_errno(const char *msg) {
        std::string throwMessage;
        throwMessage = msg;
        throwMessage += " [";
        throwMessage += spiDevice;
        throwMessage += "]";
        throw std::system_error(errno, std::system_category(), throwMessage);
    }

public:
    LinuxHardwareSPIOutput() {
        m_pSelect = NULL;
    }

    LinuxHardwareSPIOutput(Selectable *pSelect) {
        m_pSelect = pSelect;
    }

    // set the object representing the selectable
    void setSelect(Selectable *pSelect) {
        m_pSelect = pSelect;
    }

    // initialize the SPI subssytem
    void init() {
        spiClock = (int)_SPI_SPEED * 1000000L;
        spiMode = SPI_MODE_0;
        spiBitsPerWord = 8;

        spiDevice = LINUX_SPIDEV_PATH;
        spiDevice += std::to_string(_SPI_BUS);
        spiDevice += ".";
        spiDevice += std::to_string(_SPI_CS);

        if ((spiFileDesc = open(spiDevice.c_str(), O_RDWR)) < 0) {
            if (errno == ENOENT) {
                throw_errno("SPI bus does not exist. Is the kernel module loaded?");
            } else {
                throw_errno("Cannot open SPI bus.");
            }
        }

        if (ioctl(spiFileDesc, SPI_IOC_WR_MAX_SPEED_HZ, &spiClock) < 0) {
            throw_errno("Failed to set SPI speed");
        }

        if (ioctl(spiFileDesc, SPI_IOC_WR_MODE, &spiMode) < 0) {
            throw_errno("Failed to set SPI mode 0");
        }

        if (ioctl(spiFileDesc, SPI_IOC_WR_BITS_PER_WORD, &spiBitsPerWord) < 0) {
            throw_errno("Failed to set SPI word size");
        }

        if (spiBus == NULL)
          spiBus = this;
    }

    // latch the CS select
    void inline select() __attribute__((always_inline)) {
        if(m_pSelect != NULL) { m_pSelect->select(); }
    }


    // release the CS select
    void inline release() __attribute__((always_inline)) {
        if(m_pSelect != NULL) { m_pSelect->release(); }
    }

    // As of December 2016 the Linux SPI driver is still synchronous.
    // i.e. write() will return with the data completely sent over the SPI bus

    static void wait() __attribute__((always_inline)) {
        return;
    }

    // wait until all queued up data has been written
    void waitFully() __attribute__((always_inline)) {
        return;
    }

    uint8_t *getXmitBuffer(int wanted) {
        if (wanted > xmitBufferSize) {
            if (xmitBufferSize)
                delete xmitBuffer;
            xmitBuffer = new uint8_t[wanted];
            xmitBufferSize = wanted;
        }

        return xmitBuffer;
    }

    void sendToSPIBus(uint8_t *buffer, int len) {
        if (write(spiFileDesc, buffer, len) != len) {
            throw_errno("failed to write to SPI bus");
        }
    }

    // not the most efficient mechanism in the world - but should be enough for sm16716 and friends
    template <uint8_t BIT> inline static void writeBit(uint8_t b) {
        throw std::runtime_error("writeBit not implemented");
    }

    // write a byte out via SPI (returns immediately on writing register)
    static void writeByte(uint8_t b) __attribute__((always_inline)) {
      spiBus->sendToSPIBus(&b, 1);
    }
    // write a word out via SPI (returns immediately on writing register)
    static void writeWord(uint16_t w) __attribute__((always_inline)) {
      spiBus->sendToSPIBus((uint8_t *)&w, 2);
    }

    // A full cycle of writing a value for len bytes, including select, release, and waiting
    void writeBytesValue(uint8_t value, int len) {
        if (len == 0)
            return;

        uint8_t *buffer = getXmitBuffer(len);
        memset(buffer, value, len);
        select();
        sendToSPIBus(buffer, len);
        release();
    }

    //
    // TODO: This should really be an object method, not static.
    //
    static void writeBytesValueRaw(uint8_t value, int len){
      uint8_t *buf = spiBus->getXmitBuffer(len);
      memset(buf, value, len);
      spiBus->sendToSPIBus(buf, len);
    }

    // A full cycle of writing a raw block of data out, including select, release, and waiting
    template <class D> void writeBytes(uint8_t *data, int len) {
        select();
        uint8_t *adjusted;

        adjusted = getXmitBuffer(len);

        for (int i = 0; i < len; i++) {
            adjusted[i] = D::adjust(data[i]);
        }
        sendToSPIBus(adjusted, len);
        D::postBlock(len);
        release();
    }

    void writeBytes(uint8_t *data, int len) {
        sendToSPIBus(data, len);
    }

    template <uint8_t FLAGS, class D, EOrder RGB_ORDER>
    void writePixels(PixelController<RGB_ORDER> pixels) {
        int len = pixels.mLen, bufSize = len * 3;
        uint8_t *adjusted, *buf;

        adjusted = buf = getXmitBuffer(bufSize);

        if (FLAGS & FLAG_START_BIT)
            throw std::runtime_error("writePixels:FLAG_START_BIT flag not implemented");

        select();
        while(pixels.has(1)) {
            *buf++ = D::adjust(pixels.loadAndScale0());
            *buf++ = D::adjust(pixels.loadAndScale1());
            *buf++ = D::adjust(pixels.loadAndScale2());
            pixels.advanceData();
            pixels.stepDithering();
        }
        sendToSPIBus(adjusted, bufSize);
        D::postBlock(len);
        release();
    }
};

template <uint8_t _SPI_BUS, uint8_t _SPI_CS, uint8_t _SPI_SPEED> LinuxHardwareSPIOutput<_SPI_BUS, _SPI_CS, _SPI_SPEED> *LinuxHardwareSPIOutput<_SPI_BUS, _SPI_CS, _SPI_SPEED>::spiBus;

FASTLED_NAMESPACE_END;

#endif
