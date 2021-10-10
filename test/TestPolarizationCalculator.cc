/* This file is part of the CARTA Image Viewer: https://github.com/CARTAvis/carta-backend
   Copyright 2018, 2019, 2020, 2021 Academia Sinica Institute of Astronomy and Astrophysics (ASIAA),
   Associated Universities, Inc. (AUI) and the Inter-University Institute for Data Intensive Astronomy (IDIA)
   SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <gtest/gtest.h>

#include <casacore/images/Images/PagedImage.h>
#include <imageanalysis/ImageAnalysis/ImageMoments.h>

#include "CommonTestUtilities.h"
#include "ImageData/PolarizationCalculator.h"
#include "Logger/Logger.h"

static const string SAMPLE_IMAGE = "IRCp10216_sci.spw0.cube.IQUV.manual.pbcor.fits"; // shape: [256, 256, 480, 4]
static const int MAX_CHANNEL = 5;

using namespace carta;

class PolarizationCalculatorTest : public ::testing::Test, public FileFinder {
public:
    static void GetImageData(
        std::shared_ptr<const casacore::ImageInterface<casacore::Float>> image, int channel, int stokes, std::vector<float>& data) {
        // Get spectral and stokes indices
        casacore::CoordinateSystem coord_sys = image->coordinates();
        casacore::Vector<casacore::Int> linear_axes = coord_sys.linearAxesNumbers();
        int spectral_axis = coord_sys.spectralAxisNumber();
        int stokes_axis = coord_sys.polarizationAxisNumber();

        // Get a slicer
        casacore::IPosition start(image->shape().size());
        start = 0;
        casacore::IPosition end(image->shape());
        end -= 1;

        auto spectral_axis_size = image->shape()[spectral_axis];
        if ((spectral_axis >= 0) && (channel >= spectral_axis_size)) {
            spdlog::error("channel number {} is greater or equal than the spectral axis size {}", channel, spectral_axis_size);
            return;
        }

        auto stokes_axis_size = image->shape()[stokes_axis];
        if ((stokes_axis >= 0) && (stokes >= stokes_axis_size)) {
            spdlog::error("stokes number {} is greater or equal than the stokes axis size {}", stokes, stokes_axis_size);
            return;
        }

        if (spectral_axis >= 0) {
            start(spectral_axis) = channel;
            end(spectral_axis) = channel;
        }
        if (stokes_axis >= 0) {
            start(stokes_axis) = stokes;
            end(stokes_axis) = stokes;
        }

        // Get image data
        casacore::Slicer section(start, end, casacore::Slicer::endIsLast);
        data.resize(section.length().product());
        casacore::Array<float> tmp(section.length(), data.data(), casacore::StorageInitPolicy::SHARE);
        casacore::SubImage<float> subimage(*image, section);
        casacore::RO_MaskedLatticeIterator<float> lattice_iter(subimage);

        for (lattice_iter.reset(); !lattice_iter.atEnd(); ++lattice_iter) {
            casacore::Array<float> cursor_data = lattice_iter.cursor();
            casacore::IPosition cursor_shape(lattice_iter.cursorShape());
            casacore::IPosition cursor_position(lattice_iter.position());
            casacore::Slicer cursor_slicer(cursor_position, cursor_shape); // where to put the data
            tmp(cursor_slicer) = cursor_data;
        }
    }

    static void CheckPolarizationType(
        const std::shared_ptr<casacore::ImageInterface<float>>& image, casacore::Stokes::StokesTypes expected_stokes_type) {
        casacore::CoordinateSystem coord_sys = image->coordinates();
        EXPECT_TRUE(coord_sys.hasPolarizationCoordinate());

        if (coord_sys.hasPolarizationCoordinate()) {
            auto stokes_coord = coord_sys.stokesCoordinate();
            auto stokes_types = stokes_coord.stokes();
            EXPECT_EQ(stokes_types.size(), 1);
            for (auto stokes_type : stokes_types) {
                EXPECT_EQ(stokes_type, expected_stokes_type);
            }
        }
    }

    static void TestPolarizedIntensity(const std::shared_ptr<casacore::ImageInterface<float>>& image) {
        // Calculate polarized intensity
        carta::PolarizationCalculator polarization_calculator(image);
        auto resulting_image = polarization_calculator.ComputePolarizedIntensity();
        CheckPolarizationType(resulting_image, casacore::Stokes::StokesTypes::Plinear);

        // Get spectral axis size
        casacore::CoordinateSystem coord_sys = image->coordinates();
        int spectral_axis = coord_sys.spectralAxisNumber();
        int spectral_axis_size = image->shape()[spectral_axis];

        // Verify each pixel value from calculation results
        int stokes_q(1);
        int stokes_u(2);
        int stokes_pi(0);
        std::vector<float> data_q;
        std::vector<float> data_u;
        std::vector<float> data_results;

        for (int channel = 0; channel < spectral_axis_size; ++channel) {
            GetImageData(image, channel, stokes_q, data_q);
            GetImageData(image, channel, stokes_u, data_u);
            GetImageData(resulting_image, channel, stokes_pi, data_results);

            EXPECT_EQ(data_results.size(), data_q.size());
            EXPECT_EQ(data_results.size(), data_u.size());

            for (int i = 0; i < data_results.size(); ++i) {
                if (!isnan(data_results[i])) {
                    auto polarized_intensity = sqrt(pow(data_q[i], 2) + pow(data_u[i], 2));
                    EXPECT_FLOAT_EQ(data_results[i], polarized_intensity);
                }
            }
        }
    }

    static void TestPolarizedIntensityPerChannel(const std::shared_ptr<casacore::ImageInterface<float>>& image) {
        // Get spectral axis size
        casacore::CoordinateSystem coord_sys = image->coordinates();
        int spectral_axis = coord_sys.spectralAxisNumber();
        int spectral_axis_size = image->shape()[spectral_axis];

        // Verify each pixel value from calculation results
        int stokes_q(1);
        int stokes_u(2);
        int current_stokes(0);
        int current_channel(0);
        std::vector<float> data_q;
        std::vector<float> data_u;
        std::vector<float> data_results;
        int max_channel = (spectral_axis_size > MAX_CHANNEL) ? MAX_CHANNEL : spectral_axis_size;

        for (int channel = 0; channel < max_channel; ++channel) {
            // Calculate polarized intensity
            carta::PolarizationCalculator polarization_calculator(image, AxisRange(channel));
            auto resulting_image = polarization_calculator.ComputePolarizedIntensity();
            CheckPolarizationType(resulting_image, casacore::Stokes::StokesTypes::Plinear);

            GetImageData(image, channel, stokes_q, data_q);
            GetImageData(image, channel, stokes_u, data_u);
            GetImageData(resulting_image, current_channel, current_stokes, data_results);

            EXPECT_EQ(data_results.size(), data_q.size());
            EXPECT_EQ(data_results.size(), data_u.size());

            for (int i = 0; i < data_results.size(); ++i) {
                if (!isnan(data_results[i])) {
                    auto polarized_intensity = sqrt(pow(data_q[i], 2) + pow(data_u[i], 2));
                    EXPECT_FLOAT_EQ(data_results[i], polarized_intensity);
                }
            }
        }
    }

    static void TestFractionalPolarizedIntensity(const std::shared_ptr<casacore::ImageInterface<float>>& image) {
        // Calculate polarized intensity
        carta::PolarizationCalculator polarization_calculator(image);
        auto resulting_image = polarization_calculator.ComputeFractionalPolarizedIntensity();
        CheckPolarizationType(resulting_image, casacore::Stokes::StokesTypes::PFlinear);

        // Get spectral axis size
        casacore::CoordinateSystem coord_sys = image->coordinates();
        int spectral_axis = coord_sys.spectralAxisNumber();
        int spectral_axis_size = image->shape()[spectral_axis];

        // Verify each pixel value from calculation results
        int stokes_i(0);
        int stokes_q(1);
        int stokes_u(2);
        int stokes_fpi(0);
        std::vector<float> data_i;
        std::vector<float> data_q;
        std::vector<float> data_u;
        std::vector<float> data_results;

        for (int channel = 0; channel < spectral_axis_size; ++channel) {
            GetImageData(image, channel, stokes_i, data_i);
            GetImageData(image, channel, stokes_q, data_q);
            GetImageData(image, channel, stokes_u, data_u);
            GetImageData(resulting_image, channel, stokes_fpi, data_results);

            EXPECT_EQ(data_results.size(), data_i.size());
            EXPECT_EQ(data_results.size(), data_q.size());
            EXPECT_EQ(data_results.size(), data_u.size());

            for (int i = 0; i < data_results.size(); ++i) {
                if (!isnan(data_results[i])) {
                    auto polarized_intensity = sqrt(pow(data_q[i], 2) + pow(data_u[i], 2)) / data_i[i];
                    EXPECT_FLOAT_EQ(data_results[i], polarized_intensity);
                }
            }
        }
    }

    static void TestFractionalPolarizedIntensityPerChannel(const std::shared_ptr<casacore::ImageInterface<float>>& image) {
        // Get spectral axis size
        casacore::CoordinateSystem coord_sys = image->coordinates();
        int spectral_axis = coord_sys.spectralAxisNumber();
        int spectral_axis_size = image->shape()[spectral_axis];

        // Verify each pixel value from calculation results
        int stokes_i(0);
        int stokes_q(1);
        int stokes_u(2);
        int current_stokes(0);
        int current_channel(0);
        std::vector<float> data_i;
        std::vector<float> data_q;
        std::vector<float> data_u;
        std::vector<float> data_results;
        int max_channel = (spectral_axis_size > MAX_CHANNEL) ? MAX_CHANNEL : spectral_axis_size;

        for (int channel = 0; channel < max_channel; ++channel) {
            // Calculate polarized intensity
            carta::PolarizationCalculator polarization_calculator(image, AxisRange(channel));
            auto resulting_image = polarization_calculator.ComputeFractionalPolarizedIntensity();
            CheckPolarizationType(resulting_image, casacore::Stokes::StokesTypes::PFlinear);

            GetImageData(image, channel, stokes_i, data_i);
            GetImageData(image, channel, stokes_q, data_q);
            GetImageData(image, channel, stokes_u, data_u);
            GetImageData(resulting_image, current_channel, current_stokes, data_results);

            EXPECT_EQ(data_results.size(), data_i.size());
            EXPECT_EQ(data_results.size(), data_q.size());
            EXPECT_EQ(data_results.size(), data_u.size());

            for (int i = 0; i < data_results.size(); ++i) {
                if (!isnan(data_results[i])) {
                    auto polarized_intensity = sqrt(pow(data_q[i], 2) + pow(data_u[i], 2)) / data_i[i];
                    EXPECT_FLOAT_EQ(data_results[i], polarized_intensity);
                }
            }
        }
    }

    static void TestPolarizedAngle(const std::shared_ptr<casacore::ImageInterface<float>>& image, bool radiant) {
        carta::PolarizationCalculator polarization_calculator(image);
        // Calculate polarized angle
        auto resulting_image = polarization_calculator.ComputePolarizedAngle(radiant);
        CheckPolarizationType(resulting_image, casacore::Stokes::StokesTypes::Pangle);

        // Get spectral axis size
        casacore::CoordinateSystem coord_sys = image->coordinates();
        int spectral_axis = coord_sys.spectralAxisNumber();
        int spectral_axis_size = image->shape()[spectral_axis];

        // Verify each pixel value from calculation results
        int stokes_q(1);
        int stokes_u(2);
        int stokes_pa(0);
        std::vector<float> data_q;
        std::vector<float> data_u;
        std::vector<float> data_results;

        for (int channel = 0; channel < spectral_axis_size; ++channel) {
            GetImageData(image, channel, stokes_q, data_q);
            GetImageData(image, channel, stokes_u, data_u);
            GetImageData(resulting_image, channel, stokes_pa, data_results);

            EXPECT_EQ(data_results.size(), data_q.size());
            EXPECT_EQ(data_results.size(), data_u.size());

            for (int i = 0; i < data_results.size(); ++i) {
                if (!isnan(data_results[i])) {
                    auto polarized_angle = atan2(data_u[i], data_q[i]) / 2;
                    if (!radiant) {
                        polarized_angle = polarized_angle * 180 / C::pi; // as degree value
                    }
                    EXPECT_FLOAT_EQ(data_results[i], polarized_angle);
                }
            }
        }
    }

    static void TestPolarizedAnglePerChannel(const std::shared_ptr<casacore::ImageInterface<float>>& image, bool radiant) {
        // Get spectral axis size
        casacore::CoordinateSystem coord_sys = image->coordinates();
        int spectral_axis = coord_sys.spectralAxisNumber();
        int spectral_axis_size = image->shape()[spectral_axis];

        // Verify each pixel value from calculation results
        int stokes_q(1);
        int stokes_u(2);
        int current_stokes(0);
        int current_channel(0);
        std::vector<float> data_q;
        std::vector<float> data_u;
        std::vector<float> data_results;
        int max_channel = (spectral_axis_size > MAX_CHANNEL) ? MAX_CHANNEL : spectral_axis_size;

        for (int channel = 0; channel < max_channel; ++channel) {
            // Calculate polarized angle
            carta::PolarizationCalculator polarization_calculator(image, AxisRange(channel));
            auto resulting_image = polarization_calculator.ComputePolarizedAngle(radiant);
            CheckPolarizationType(resulting_image, casacore::Stokes::StokesTypes::Pangle);

            GetImageData(image, channel, stokes_q, data_q);
            GetImageData(image, channel, stokes_u, data_u);
            GetImageData(resulting_image, current_channel, current_stokes, data_results);

            EXPECT_EQ(data_results.size(), data_q.size());
            EXPECT_EQ(data_results.size(), data_u.size());

            for (int i = 0; i < data_results.size(); ++i) {
                if (!isnan(data_results[i])) {
                    auto polarized_angle = atan2(data_u[i], data_q[i]) / 2;
                    if (!radiant) {
                        polarized_angle = polarized_angle * 180 / C::pi; // as degree value
                    }
                    EXPECT_FLOAT_EQ(data_results[i], polarized_angle);
                }
            }
        }
    }

    static void TestPerformances(const std::shared_ptr<casacore::ImageInterface<float>>& image) {
        // Get spectral axis size
        casacore::CoordinateSystem coord_sys = image->coordinates();
        int spectral_axis = coord_sys.spectralAxisNumber();
        int spectral_axis_size = image->shape()[spectral_axis];
        int max_channel = (spectral_axis_size > MAX_CHANNEL) ? MAX_CHANNEL : spectral_axis_size;

        // Calculate polarized intensity per cube
        auto t_start_per_cube = std::chrono::high_resolution_clock::now();

        carta::PolarizationCalculator polarization_calculator(image, AxisRange(0, max_channel - 1));
        auto image_pi_per_cube = polarization_calculator.ComputePolarizedIntensity();
        auto image_fpi_per_cube = polarization_calculator.ComputeFractionalPolarizedIntensity();
        auto image_pa_per_cube = polarization_calculator.ComputePolarizedAngle(true);

        auto t_end_per_cube = std::chrono::high_resolution_clock::now();
        auto dt_per_cube = std::chrono::duration_cast<std::chrono::microseconds>(t_end_per_cube - t_start_per_cube).count();

        // Calculate polarized intensity per channel
        auto t_start_per_channel = std::chrono::high_resolution_clock::now();

        for (int channel = 0; channel < max_channel; ++channel) {
            carta::PolarizationCalculator polarization_calculator_per_channel(image, AxisRange(channel));
            auto image_pi_per_channel = polarization_calculator_per_channel.ComputePolarizedIntensity();
            auto image_fpi_per_channel = polarization_calculator_per_channel.ComputeFractionalPolarizedIntensity();
            auto image_pa_per_channel = polarization_calculator_per_channel.ComputePolarizedAngle(true);
        }

        auto t_end_per_channel = std::chrono::high_resolution_clock::now();
        auto dt_per_channel = std::chrono::duration_cast<std::chrono::microseconds>(t_end_per_channel - t_start_per_channel).count();

        EXPECT_LT(dt_per_cube, dt_per_channel);

        spdlog::info("Calculate polarized intensity per cube/channel spends: {:.3f}/{:.3f} ms", dt_per_cube * 1e-3, dt_per_channel * 1e-3);
    }

    static void TestConsistency(const std::shared_ptr<casacore::ImageInterface<float>>& image) {
        // Get spectral axis size
        casacore::CoordinateSystem coord_sys = image->coordinates();
        int spectral_axis = coord_sys.spectralAxisNumber();
        int spectral_axis_size = image->shape()[spectral_axis];
        int max_channel = (spectral_axis_size > MAX_CHANNEL) ? MAX_CHANNEL : spectral_axis_size;

        // Calculate polarized intensity per cube
        carta::PolarizationCalculator polarization_calculator(image, AxisRange(0, max_channel - 1));
        auto image_pi_per_cube = polarization_calculator.ComputePolarizedIntensity();
        auto image_fpi_per_cube = polarization_calculator.ComputeFractionalPolarizedIntensity();
        auto image_pa_per_cube = polarization_calculator.ComputePolarizedAngle(true);

        int current_stokes(0);
        int current_channel(0);
        std::vector<float> data_pi_per_cube;
        std::vector<float> data_fpi_per_cube;
        std::vector<float> data_pa_per_cube;
        std::vector<float> data_pi_per_channel;
        std::vector<float> data_fpi_per_channel;
        std::vector<float> data_pa_per_channel;

        // Calculate polarized intensity per channel
        for (int channel = 0; channel < max_channel; ++channel) {
            carta::PolarizationCalculator polarization_calculator_per_channel(image, AxisRange(channel));
            auto image_pi_per_channel = polarization_calculator_per_channel.ComputePolarizedIntensity();
            auto image_fpi_per_channel = polarization_calculator_per_channel.ComputeFractionalPolarizedIntensity();
            auto image_pa_per_channel = polarization_calculator_per_channel.ComputePolarizedAngle(true);

            GetImageData(image_pi_per_cube, channel, current_stokes, data_pi_per_cube);
            GetImageData(image_pi_per_channel, current_channel, current_stokes, data_pi_per_channel);

            EXPECT_EQ(data_pi_per_cube.size(), data_pi_per_channel.size());
            if (data_pi_per_cube.size() == data_pi_per_channel.size()) {
                for (int i = 0; i < data_pi_per_cube.size(); ++i) {
                    if (!isnan(data_pi_per_cube[i]) && !isnan(data_pi_per_channel[i])) {
                        EXPECT_FLOAT_EQ(data_pi_per_cube[i], data_pi_per_channel[i]);
                    }
                }
            }

            GetImageData(image_fpi_per_cube, channel, current_stokes, data_fpi_per_cube);
            GetImageData(image_fpi_per_channel, current_channel, current_stokes, data_fpi_per_channel);

            EXPECT_EQ(data_fpi_per_cube.size(), data_fpi_per_channel.size());
            if (data_fpi_per_cube.size() == data_fpi_per_channel.size()) {
                for (int i = 0; i < data_fpi_per_cube.size(); ++i) {
                    if (!isnan(data_fpi_per_cube[i]) && !isnan(data_fpi_per_channel[i])) {
                        EXPECT_FLOAT_EQ(data_fpi_per_cube[i], data_fpi_per_channel[i]);
                    }
                }
            }

            GetImageData(image_pa_per_cube, channel, current_stokes, data_pa_per_cube);
            GetImageData(image_pa_per_channel, current_channel, current_stokes, data_pa_per_channel);

            EXPECT_EQ(data_pa_per_cube.size(), data_pa_per_channel.size());
            if (data_pa_per_cube.size() == data_pa_per_channel.size()) {
                for (int i = 0; i < data_pa_per_cube.size(); ++i) {
                    if (!isnan(data_pa_per_cube[i]) && !isnan(data_pa_per_channel[i])) {
                        EXPECT_FLOAT_EQ(data_pa_per_cube[i], data_pa_per_channel[i]);
                    }
                }
            }
        }
    }
};

TEST_F(PolarizationCalculatorTest, TestPolarizedIntensity) {
    std::string file_path = FitsImagePath(SAMPLE_IMAGE);
    std::shared_ptr<casacore::ImageInterface<float>> image;

    if (OpenImage(image, file_path)) {
        TestPolarizedIntensity(image);
    } else {
        spdlog::warn("Fail to open the file {}! Ignore the test.", file_path);
    }
}

TEST_F(PolarizationCalculatorTest, TestPolarizedIntensityPerChannel) {
    std::string file_path = FitsImagePath(SAMPLE_IMAGE);
    std::shared_ptr<casacore::ImageInterface<float>> image;

    if (OpenImage(image, file_path)) {
        TestPolarizedIntensityPerChannel(image);
    } else {
        spdlog::warn("Fail to open the file {}! Ignore the test.", file_path);
    }
}

TEST_F(PolarizationCalculatorTest, TestFractionalPolarizedIntensity) {
    std::string file_path = FitsImagePath(SAMPLE_IMAGE);
    std::shared_ptr<casacore::ImageInterface<float>> image;

    if (OpenImage(image, file_path)) {
        TestFractionalPolarizedIntensity(image);
    } else {
        spdlog::warn("Fail to open the file {}! Ignore the test.", file_path);
    }
}

TEST_F(PolarizationCalculatorTest, TestFractionalPolarizedIntensityPerChannel) {
    std::string file_path = FitsImagePath(SAMPLE_IMAGE);
    std::shared_ptr<casacore::ImageInterface<float>> image;

    if (OpenImage(image, file_path)) {
        TestFractionalPolarizedIntensityPerChannel(image);
    } else {
        spdlog::warn("Fail to open the file {}! Ignore the test.", file_path);
    }
}

TEST_F(PolarizationCalculatorTest, TestPolarizedAngle) {
    std::string file_path = FitsImagePath(SAMPLE_IMAGE);
    std::shared_ptr<casacore::ImageInterface<float>> image;

    if (OpenImage(image, file_path)) {
        TestPolarizedAngle(image, true);
        TestPolarizedAngle(image, false);
    } else {
        spdlog::warn("Fail to open the file {}! Ignore the test.", file_path);
    }
}

TEST_F(PolarizationCalculatorTest, TestPolarizedAnglePerChannel) {
    std::string file_path = FitsImagePath(SAMPLE_IMAGE);
    std::shared_ptr<casacore::ImageInterface<float>> image;

    if (OpenImage(image, file_path)) {
        TestPolarizedAnglePerChannel(image, true);
        TestPolarizedAnglePerChannel(image, false);
    } else {
        spdlog::warn("Fail to open the file {}! Ignore the test.", file_path);
    }
}

TEST_F(PolarizationCalculatorTest, TestPerformances) {
    std::string file_path = FitsImagePath(SAMPLE_IMAGE);
    std::shared_ptr<casacore::ImageInterface<float>> image;

    if (OpenImage(image, file_path)) {
        TestPerformances(image);
    } else {
        spdlog::warn("Fail to open the file {}! Ignore the test.", file_path);
    }
}

TEST_F(PolarizationCalculatorTest, TestConsistency) {
    std::string file_path = FitsImagePath(SAMPLE_IMAGE);
    std::shared_ptr<casacore::ImageInterface<float>> image;

    if (OpenImage(image, file_path)) {
        TestConsistency(image);
    } else {
        spdlog::warn("Fail to open the file {}! Ignore the test.", file_path);
    }
}