/*
 *   This file contains unit tests for functions in the FwdRefBeginDataset
 *   class.
 *
 *   Function tests included:
 *   - writeDataset
 *
 */

#include <cstdio>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "gtest_utilities.h"

#include "../../../subsetter/Configuration.h"
#include "../../../subsetter/FwdRefBeginDataset.h"
#include "../../../subsetter/IndexSelection.h"
#include "../../../subsetter/SubsetDataLayers.h"
#include "H5Cpp.h"

TEST(FwdRefBeginDatasetTest, writeDataset_selection_covers_whole_dataset)
{
    // A temporal-only request whose range covers every segment leaves the
    // IndexSelection restricted to the full extent with no explicit segments,
    // so IndexSelection::getSegments() returns an empty map. The index-begin
    // values must then be written through unchanged; previously the output
    // buffer was never filled and uninitialized memory was written.
    const std::string shortName = "ATL03";
    const std::string groupName = "/gt1l/geolocation/";
    const std::string beginName = "ph_index_beg";
    const std::string countName = "segment_ph_cnt";

    Configuration config(
        gtest_utilities::getFullPath("harmony_service/subsetter_config.json"));
    std::vector<std::string> variables;
    SubsetDataLayers subsetDataLayers(variables);

    H5::H5File in(gtest_utilities::getFullPath("tests/data/ATL03_gt1l.h5"),
                  H5F_ACC_RDONLY);
    H5::Group inGroup = in.openGroup(groupName);
    H5::DataSet inBegin = inGroup.openDataSet(beginName);
    H5::DataSet inCount = inGroup.openDataSet(countName);
    const size_t segmentCount = inBegin.getSpace().getSimpleExtentNpoints();
    ASSERT_GT(segmentCount, 0u);

    std::string outPath = (std::filesystem::temp_directory_path() /
                           "fwdrefbegin_whole_dataset_out.h5")
                              .string();
    H5::H5File out(outPath, H5F_ACC_TRUNC);
    H5::Group outGroup = out.createGroup("/gt1l").createGroup("geolocation");

    // writeDataset sizes its output from the count dataset already written
    // to the output group.
    std::vector<int64_t> counts(segmentCount);
    inCount.read(counts.data(), H5::PredType::NATIVE_INT64);
    hsize_t countDims[1] = {segmentCount};
    outGroup
        .createDataSet(
            countName, inCount.getDataType(), H5::DataSpace(1, countDims))
        .write(counts.data(), H5::PredType::NATIVE_INT64);

    // The selection a whole-range temporal subset produces: restricted to
    // the full extent, with no segments added.
    IndexSelection indexes(segmentCount);
    indexes.addRestriction(0, segmentCount);
    ASSERT_TRUE(indexes.getSegments().empty());

    FwdRefBeginDataset fwdRefBeginDataset(shortName, beginName, &config);
    fwdRefBeginDataset.writeDataset(
        outGroup, groupName, inBegin, &indexes, &subsetDataLayers);

    std::vector<int64_t> expected(segmentCount), actual(segmentCount);
    inBegin.read(expected.data(), H5::PredType::NATIVE_INT64);
    outGroup.openDataSet(beginName).read(actual.data(),
                                         H5::PredType::NATIVE_INT64);
    EXPECT_EQ(actual, expected);

    out.close();
    in.close();
    std::remove(outPath.c_str());
}
