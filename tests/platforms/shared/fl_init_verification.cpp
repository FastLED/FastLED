#include "platforms/shared/spi_hw_1.h"
#include "platforms/shared/spi_hw_2.h"
#include "platforms/shared/spi_hw_4.h"
#include "platforms/shared/spi_hw_8.h"
#include "platforms/shared/spi_hw_16.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

FL_TEST_CASE("FL_INIT verification - SPI stub instances are registered") {
    // Test that FL_INIT successfully registered all SPI stub instances
    // This verifies the static constructor mechanism works correctly

    FL_SUBCASE("SpiHw1 instances registered") {
        auto instances = fl::SpiHw1::getAll();
        FL_CHECK(instances.size() == 2);
        if (instances.size() == 2) {
            FL_CHECK(fl::string(instances[0]->getName()) == "MockSingle0");
            FL_CHECK(fl::string(instances[1]->getName()) == "MockSingle1");
        }
    }

    FL_SUBCASE("SpiHw2 instances registered") {
        auto instances = fl::SpiHw2::getAll();
        FL_CHECK(instances.size() == 2);
        if (instances.size() == 2) {
            FL_CHECK(fl::string(instances[0]->getName()) == "MockDual0");
            FL_CHECK(fl::string(instances[1]->getName()) == "MockDual1");
        }
    }

    FL_SUBCASE("SpiHw4 instances registered") {
        auto instances = fl::SpiHw4::getAll();
        FL_CHECK(instances.size() == 2);
        if (instances.size() == 2) {
            FL_CHECK(fl::string(instances[0]->getName()) == "MockQuad2");
            FL_CHECK(fl::string(instances[1]->getName()) == "MockQuad3");
        }
    }

    FL_SUBCASE("SpiHw8 instances registered") {
        auto instances = fl::SpiHw8::getAll();
        FL_CHECK(instances.size() == 2);
        if (instances.size() == 2) {
            FL_CHECK(fl::string(instances[0]->getName()) == "MockOctal2");
            FL_CHECK(fl::string(instances[1]->getName()) == "MockOctal3");
        }
    }

    FL_SUBCASE("SpiHw16 instances registered") {
        auto instances = fl::SpiHw16::getAll();
        FL_CHECK(instances.size() == 2);
        if (instances.size() == 2) {
            FL_CHECK(fl::string(instances[0]->getName()) == "MockHexadeca2");
            FL_CHECK(fl::string(instances[1]->getName()) == "MockHexadeca3");
        }
    }
}
