/* This file is part of the CARTA Image Viewer: https://github.com/CARTAvis/carta-backend
   Copyright 2018, 2019, 2020, 2021 Academia Sinica Institute of Astronomy and Astrophysics (ASIAA),
   Associated Universities, Inc. (AUI) and the Inter-University Institute for Data Intensive Astronomy (IDIA)
   SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "CommonTestUtilities.h"
#include "ImageData/FileLoader.h"
#include "Region/Region.h"
#include "Region/RegionHandler.h"
#include "src/Frame/Frame.h"

using namespace carta;

using ::testing::FloatNear;
using ::testing::Pointwise;

class LineSpatialProfileTest : public ::testing::Test {
public:
    static bool SetLineCut(carta::RegionHandler& region_handler, int file_id, int& region_id, const std::vector<float>& endpoints,
        casacore::CoordinateSystem* csys) {
        // Define RegionState for line region (region_id updated)
        std::vector<CARTA::Point> control_points;
        CARTA::Point point;
        point.set_x(endpoints[0]);
        point.set_y(endpoints[1]);
        control_points.push_back(point);
        point.set_x(endpoints[2]);
        point.set_y(endpoints[3]);
        control_points.push_back(point);
        RegionState region_state(file_id, CARTA::RegionType::LINE, control_points, 0.0);

        // Set region
        return region_handler.SetRegion(region_id, region_state, csys);
    }

    static bool GetLineProfiles(const std::string& image_path, const std::vector<float>& endpoints,
        const std::vector<CARTA::SetSpatialRequirements_SpatialConfig>& spatial_reqs,
        std::vector<CARTA::SpatialProfileData>& spatial_profiles) {
        std::shared_ptr<carta::FileLoader> loader(carta::FileLoader::GetLoader(image_path));
        std::shared_ptr<Frame> frame(new Frame(0, loader, "0"));
        carta::RegionHandler region_handler;

        // Set line region
        int file_id(0), region_id(-1);
        casacore::CoordinateSystem* csys(frame->CoordinateSystem());
        if (!SetLineCut(region_handler, file_id, region_id, endpoints, csys)) {
            return false;
        }

        // Get spatial profiles
        region_handler.SetSpatialRequirements(region_id, file_id, frame, spatial_reqs);
        return region_handler.FillSpatialProfileData(file_id, region_id, spatial_profiles);
    }

    static std::vector<float> ProfileValues(CARTA::SpatialProfile& profile) {
        std::string buffer = profile.raw_values_fp32();
        std::vector<float> values(buffer.size() / sizeof(float));
        memcpy(values.data(), buffer.data(), buffer.size());
        return values;
    }

    void SetUp() {
        setenv("HDF5_USE_FILE_LOCKING", "FALSE", 0);
    }
};

TEST_F(LineSpatialProfileTest, FitsLineProfile) {
    std::string image_path = FileFinder::FitsImagePath("noise_3d.fits");
    std::vector<float> endpoints = {0.0, 0.0, 9.0, 9.0};
    int width(3);
    std::vector<CARTA::SetSpatialRequirements_SpatialConfig> spatial_reqs = {Message::SpatialConfig("x", 0, 0, 0, width)};
    std::vector<CARTA::SpatialProfileData> spatial_profiles;

    bool ok = GetLineProfiles(image_path, endpoints, spatial_reqs, spatial_profiles);
    ASSERT_TRUE(ok);
    ASSERT_EQ(spatial_profiles.size(), 1);
    ASSERT_EQ(spatial_profiles[0].profiles_size(), 1);
}

TEST_F(LineSpatialProfileTest, Hdf5LineProfile) {
    std::string image_path = FileFinder::Hdf5ImagePath("noise_10px_10px.hdf5");
    std::vector<float> endpoints = {0.0, 0.0, 9.0, 9.0};
    int width(3);
    std::vector<CARTA::SetSpatialRequirements_SpatialConfig> spatial_reqs = {Message::SpatialConfig("x", 0, 0, 0, width)};
    std::vector<CARTA::SpatialProfileData> spatial_profiles;

    bool ok = GetLineProfiles(image_path, endpoints, spatial_reqs, spatial_profiles);
    ASSERT_TRUE(ok);
    ASSERT_EQ(spatial_profiles.size(), 1);
    ASSERT_EQ(spatial_profiles[0].profiles_size(), 1);
}

TEST_F(LineSpatialProfileTest, FitsHorizontalCutProfile) {
    std::string image_path = FileFinder::FitsImagePath("noise_3d.fits");
    std::vector<float> endpoints = {9.0, 5.0, 1.0, 5.0}; // Set line region at y=5
    int width(1);
    std::vector<CARTA::SetSpatialRequirements_SpatialConfig> spatial_reqs = {Message::SpatialConfig("x", 0, 0, 0, width)};
    std::vector<CARTA::SpatialProfileData> spatial_profiles;

    bool ok = GetLineProfiles(image_path, endpoints, spatial_reqs, spatial_profiles);
    ASSERT_TRUE(ok);
    ASSERT_EQ(spatial_profiles.size(), 1);
    ASSERT_EQ(spatial_profiles[0].profiles_size(), 1);

    // Profile data
    auto profile = spatial_profiles[0].profiles(0);
    std::vector<float> profile_data = ProfileValues(profile);
    EXPECT_EQ(profile_data.size(), 9);

    // Read image data slice for first channel
    FitsDataReader reader(image_path);
    auto image_data = reader.ReadRegion({1, 5, 0}, {10, 6, 1});

    // Profile data width=1 of horizontal line is same as slice
    CmpVectors(profile_data, image_data);
}

TEST_F(LineSpatialProfileTest, FitsVerticalCutProfile) {
    std::string image_path = FileFinder::FitsImagePath("noise_3d.fits");
    std::vector<float> endpoints = {5.0, 9.0, 5.0, 1.0}; // Set line region at x=5
    int width(1);
    std::vector<CARTA::SetSpatialRequirements_SpatialConfig> spatial_reqs = {Message::SpatialConfig("y", 0, 0, 0, width)};
    std::vector<CARTA::SpatialProfileData> spatial_profiles;

    bool ok = GetLineProfiles(image_path, endpoints, spatial_reqs, spatial_profiles);
    ASSERT_TRUE(ok);
    ASSERT_EQ(spatial_profiles.size(), 1);
    ASSERT_EQ(spatial_profiles[0].profiles_size(), 1);

    // Profile data
    auto profile = spatial_profiles[0].profiles(0);
    std::vector<float> profile_data = ProfileValues(profile);
    EXPECT_EQ(profile_data.size(), 9);

    // Read image data slice for first channel
    FitsDataReader reader(image_path);
    auto image_data = reader.ReadRegion({5, 1, 0}, {6, 10, 1});

    // Profile data width=1 of horizontal line is same as slice
    CmpVectors(profile_data, image_data);
}
