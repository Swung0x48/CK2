#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include "CKAll.h"

namespace {

struct TestFileHeaderPart0 {
    char Signature[8];
    CKDWORD Crc;
    CKDWORD CKVersion;
    CKDWORD FileVersion;
    CKDWORD FileVersion2;
    CKDWORD FileWriteMode;
    CKDWORD Hdr1PackSize;
};

struct TestFileHeaderPart1 {
    CKDWORD DataPackSize;
    CKDWORD DataUnPackSize;
    CKDWORD ManagerCount;
    CKDWORD ObjectCount;
    CKDWORD MaxIDSaved;
    CKDWORD ProductVersion;
    CKDWORD ProductBuild;
    CKDWORD Hdr1UnPackSize;
};

template <typename T>
void AppendValue(std::vector<unsigned char> &buffer, const T &value) {
    const size_t oldSize = buffer.size();
    buffer.resize(oldSize + sizeof(T));
    std::memcpy(&buffer[oldSize], &value, sizeof(T));
}

std::vector<unsigned char> MakeFileWithMissingBehaviorDependency() {
    static_assert(sizeof(TestFileHeaderPart0) == 32, "Unexpected Virtools part-0 header layout");
    static_assert(sizeof(TestFileHeaderPart1) == 32, "Unexpected Virtools part-1 header layout");
    static_assert(sizeof(CKGUID) == 8, "Unexpected CKGUID layout");

    std::vector<unsigned char> headerPayload;
    const int pluginDependencyCount = 1;
    const int pluginCategory = CKPLUGIN_BEHAVIOR_DLL;
    const int guidCount = 1;
    const CKGUID missingGuid(0x4D4D4F41u, 0x55544831u);
    const int includedFilesPayloadSize = sizeof(int);
    const int includedFileCount = 0;

    AppendValue(headerPayload, pluginDependencyCount);
    AppendValue(headerPayload, pluginCategory);
    AppendValue(headerPayload, guidCount);
    AppendValue(headerPayload, missingGuid);
    AppendValue(headerPayload, includedFilesPayloadSize);
    AppendValue(headerPayload, includedFileCount);

    TestFileHeaderPart0 part0 = {};
    std::memcpy(part0.Signature, "Nemo Fi", sizeof(part0.Signature));
    part0.CKVersion = CKVERSION;
    part0.FileVersion = 8;
    part0.FileWriteMode = CKFILE_UNCOMPRESSED;
    part0.Hdr1PackSize = static_cast<CKDWORD>(headerPayload.size());

    TestFileHeaderPart1 part1 = {};
    part1.Hdr1UnPackSize = static_cast<CKDWORD>(headerPayload.size());

    CKDWORD crc = CKComputeDataCRC(reinterpret_cast<const char *>(&part0), sizeof(part0));
    crc = CKComputeDataCRC(reinterpret_cast<const char *>(&part1), sizeof(part1), crc);
    crc = CKComputeDataCRC(reinterpret_cast<const char *>(&headerPayload[0]),
                           static_cast<int>(headerPayload.size()), crc);
    part0.Crc = crc;

    std::vector<unsigned char> file;
    AppendValue(file, part0);
    AppendValue(file, part1);
    file.insert(file.end(), headerPayload.begin(), headerPayload.end());
    return file;
}

struct ScopedTestFiles {
    std::vector<std::filesystem::path> paths;

    ~ScopedTestFiles() {
        std::error_code ignored;
        for (const std::filesystem::path &path : paths) {
            std::filesystem::remove(path, ignored);
        }
    }
};

struct FileLoadObserverCapture {
    CK_ID nestedInitiator = 0;
    int recordCount = 0;
};

void CaptureFileLoad(CKContext *,
                     CK_ID initiatorBehavior,
                     CKSTRING,
                     CKERROR,
                     const CKFileLoadObjectRecord *records,
                     int recordCount,
                     void *argument) {
    FileLoadObserverCapture *capture =
            static_cast<FileLoadObserverCapture *>(argument);
    if (!capture) {
        return;
    }
    if (initiatorBehavior != 0) {
        capture->nestedInitiator = initiatorBehavior;
    }
    capture->recordCount += records ? recordCount : 0;
}

struct NestedBehaviorLoadState {
    std::string fileName;
    CKERROR result = CKERR_INVALIDFILE;
    CKObjectArray loadedObjects;
};

NestedBehaviorLoadState *g_nestedBehaviorLoadState = nullptr;

int ExecuteNestedBehaviorLoad(const CKBehaviorContext &behaviorContext) {
    NestedBehaviorLoadState *state = g_nestedBehaviorLoadState;
    if (!state || !behaviorContext.Context) {
        return CKBR_GENERICERROR;
    }
    state->result = behaviorContext.Context->Load(
            const_cast<CKSTRING>(state->fileName.c_str()),
            &state->loadedObjects,
            CK_LOAD_DEFAULT);
    return state->result == CK_OK ? CKBR_OK : CKBR_GENERICERROR;
}

class CKAuthorityRuntimeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(CK_OK, CKStartUp());
    }

    static void TearDownTestSuite() {
        CKShutdown();
    }

    void SetUp() override {
        ASSERT_EQ(CK_OK, CKCreateContext(&context_, nullptr, 0, 0));
        ASSERT_NE(nullptr, context_);

        CKLevel *level = static_cast<CKLevel *>(context_->CreateObject(
            CKCID_LEVEL, "AuthorityRuntimeLevel", CK_OBJECTCREATION_DYNAMIC));
        ASSERT_NE(nullptr, level);
        context_->SetCurrentLevel(level);
        ASSERT_EQ(CK_OK, context_->Play());
    }

    void TearDown() override {
        if (context_) {
            CKCloseContext(context_);
            context_ = nullptr;
        }
    }

    CKContext *context_ = nullptr;
};

TEST_F(CKAuthorityRuntimeTest, ProcessFixedUsesSuppliedDeltaAndIgnoresWallClock) {
    CKTimeManager *timeManager = context_->GetTimeManager();
    ASSERT_NE(nullptr, timeManager);

    const float fixedDelta = 1000.0f / 66.0f;
    const CKDWORD initialTick = timeManager->GetMainTickCount();

    ASSERT_EQ(CK_OK, context_->ProcessFixed(fixedDelta));
    EXPECT_TRUE(timeManager->IsExternalClockEnabled());
    EXPECT_FLOAT_EQ(fixedDelta, timeManager->GetExternalClockDelta());
    EXPECT_FLOAT_EQ(fixedDelta, timeManager->GetLastDeltaTimeFree());
    EXPECT_FLOAT_EQ(fixedDelta, timeManager->GetLastDeltaTime());
    EXPECT_NEAR(fixedDelta, timeManager->GetAbsoluteTime(), 0.0001f);

    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    ASSERT_EQ(CK_OK, context_->ProcessFixed(fixedDelta));
    EXPECT_FLOAT_EQ(fixedDelta, timeManager->GetLastDeltaTimeFree());
    EXPECT_FLOAT_EQ(fixedDelta, timeManager->GetLastDeltaTime());
    EXPECT_NEAR(fixedDelta * 2.0f, timeManager->GetAbsoluteTime(), 0.0001f);
    EXPECT_EQ(initialTick + 2, timeManager->GetMainTickCount());
}

TEST_F(CKAuthorityRuntimeTest, ProcessFixedRejectsInvalidDeltasWithoutAdvancing) {
    CKTimeManager *timeManager = context_->GetTimeManager();
    ASSERT_NE(nullptr, timeManager);
    const CKDWORD initialTick = timeManager->GetMainTickCount();

    EXPECT_EQ(CKERR_INVALIDPARAMETER, context_->ProcessFixed(0.0f));
    EXPECT_EQ(CKERR_INVALIDPARAMETER, context_->ProcessFixed(-1.0f));
    EXPECT_EQ(CKERR_INVALIDPARAMETER,
              context_->ProcessFixed(std::numeric_limits<float>::infinity()));
    EXPECT_EQ(CKERR_INVALIDPARAMETER,
              context_->ProcessFixed(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_EQ(initialTick, timeManager->GetMainTickCount());
    EXPECT_FALSE(timeManager->IsExternalClockEnabled());
}

TEST_F(CKAuthorityRuntimeTest, LegacyLoadPolicyPreservesMissingPluginBehavior) {
    const std::vector<unsigned char> file = MakeFileWithMissingBehaviorDependency();
    CKObjectArray loadedObjects;

    EXPECT_EQ(CK_LOAD_POLICY_LEGACY, context_->GetLoadPolicy());
    EXPECT_EQ(CK_OK,
              context_->Load(static_cast<int>(file.size()),
                             const_cast<unsigned char *>(&file[0]),
                             &loadedObjects,
                             static_cast<CK_LOAD_FLAGS>(CK_LOAD_DEFAULT |
                                                        CK_LOAD_CHECKDEPENDENCIES)));
}

TEST_F(CKAuthorityRuntimeTest, StrictLoadPolicyRejectsTopLevelAndStagedCKFileLoads) {
    const std::vector<unsigned char> file = MakeFileWithMissingBehaviorDependency();
    CKObjectArray loadedObjects;

    ASSERT_EQ(CK_OK, context_->SetLoadPolicy(CK_LOAD_POLICY_STRICT));
    EXPECT_EQ(CK_LOAD_POLICY_STRICT, context_->GetLoadPolicy());

    EXPECT_EQ(CKERR_PLUGINSMISSING,
              context_->Load(static_cast<int>(file.size()),
                             const_cast<unsigned char *>(&file[0]),
                             &loadedObjects,
                             CK_LOAD_DEFAULT));

    CKFile *nestedFile = context_->CreateCKFile();
    ASSERT_NE(nullptr, nestedFile);
    EXPECT_EQ(CKERR_PLUGINSMISSING,
              nestedFile->OpenMemory(const_cast<unsigned char *>(&file[0]),
                                     static_cast<int>(file.size()),
                                     CK_LOAD_DEFAULT));
    EXPECT_EQ(CKERR_PLUGINSMISSING, nestedFile->LoadFileData(&loadedObjects));
    EXPECT_EQ(nullptr, nestedFile->m_Parser);
    EXPECT_EQ(nullptr, nestedFile->m_MappedFile);

    EXPECT_EQ(CKERR_PLUGINSMISSING,
              nestedFile->Load(const_cast<unsigned char *>(&file[0]),
                               static_cast<int>(file.size()),
                               &loadedObjects,
                               CK_LOAD_DEFAULT));
    EXPECT_EQ(nullptr, nestedFile->m_Parser);
    EXPECT_EQ(nullptr, nestedFile->m_MappedFile);

    unsigned char invalidFile[] = {'N', 'e', 'm', 'o'};
    EXPECT_EQ(CKERR_INVALIDFILE,
              nestedFile->OpenMemory(invalidFile,
                                     static_cast<int>(sizeof(invalidFile)),
                                     CK_LOAD_DEFAULT));
    EXPECT_EQ(CKERR_INVALIDFILE, nestedFile->LoadFileData(&loadedObjects));
    EXPECT_EQ(CK_OK, context_->DeleteCKFile(nestedFile));

    EXPECT_EQ(CKERR_INVALIDPARAMETER,
              context_->SetLoadPolicy(static_cast<CK_LOAD_POLICY>(99)));
    EXPECT_EQ(CK_LOAD_POLICY_STRICT, context_->GetLoadPolicy());
}

TEST_F(CKAuthorityRuntimeTest,
       FileLoadObserverReportsExecutingBehaviorForNestedLoad) {
    ASSERT_EQ(CK_OK, context_->SetLoadPolicy(CK_LOAD_POLICY_STRICT));

    const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
    ScopedTestFiles files;
    files.paths = {
            std::filesystem::temp_directory_path() /
                    ("ck2-observer-nested-" + std::to_string(stamp) + ".nmo")};
    const std::string nestedFile = files.paths[0].string();

    CKBehavior *sourceBehavior = static_cast<CKBehavior *>(
            context_->CreateObject(CKCID_BEHAVIOR,
                                   "SyntheticLoadInitiator"));
    CKGroup *nestedObject = static_cast<CKGroup *>(
            context_->CreateObject(CKCID_GROUP, "SyntheticNestedObject"));
    ASSERT_NE(nullptr, sourceBehavior);
    ASSERT_NE(nullptr, nestedObject);

    CKObjectArray nestedObjects;
    nestedObjects.InsertRear(nestedObject);
    ASSERT_EQ(CK_OK, context_->Save(
            const_cast<CKSTRING>(nestedFile.c_str()), &nestedObjects,
            CK_STATESAVE_ALL));
    context_->DestroyObject(nestedObject);

    FileLoadObserverCapture capture;
    context_->SetFileLoadObserver(CaptureFileLoad, &capture);
    sourceBehavior->UseFunction();
    sourceBehavior->SetFunction(ExecuteNestedBehaviorLoad);
    NestedBehaviorLoadState nestedState;
    nestedState.fileName = nestedFile;
    g_nestedBehaviorLoadState = &nestedState;
    sourceBehavior->Execute(1000.0f / 66.0f);
    g_nestedBehaviorLoadState = nullptr;
    context_->SetFileLoadObserver(nullptr, nullptr);

    EXPECT_EQ(CK_OK, nestedState.result);
    EXPECT_EQ(sourceBehavior->GetID(), capture.nestedInitiator);
    EXPECT_GT(capture.recordCount, 0);
}

} // namespace
