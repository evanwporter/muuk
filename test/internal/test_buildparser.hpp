// #include "../../include/build/parser.hpp"
// #include "../../include/build/manager.hpp"
// #include "../../include/build/config.h"
// #include "../../include/muukfiler.hpp"
// #include "./mocks/mockfileop.hpp"

// #include <gtest/gtest.h>
// #include <gmock/gmock.h>

// #define SETUP_MUUK_FILER(mock_file_ops, content) \
//     mock_file_ops->write_file(content); \
//     muuk_filer = std::make_shared<MuukFiler>(mock_file_ops);

// #define CREATE_BUILDPARSER(parser, manager, filer, compiler, dir) \
//     BuildParser parser(manager, filer, compiler, dir, "debug"); \
//     parser.parse();

// using namespace ::testing;

// class BuildParserTest : public ::testing::Test {
// protected:
//     std::shared_ptr<BuildManager> build_manager;
//     std::shared_ptr<MuukFiler> muuk_filer;
//     muuk::Compiler compiler;
//     fs::path build_dir;
//     std::shared_ptr<MockFileOperations> mock_file_ops;

//     void SetUp() override {
//         mock_file_ops = std::make_shared<MockFileOperations>();
//         muuk_filer = std::make_shared<MuukFiler>(mock_file_ops);
//         build_manager = std::make_shared<BuildManager>();
//         compiler = muuk::Compiler::GCC;
//         build_dir = "test_build_dir";
//     }
// };

// TEST_F(BuildParserTest, ParseCompilationTargets) {
//     SETUP_MUUK_FILER(mock_file_ops, "[build.test_package]\nsources = [{path = 'src/test.cpp'}]\ncflags = ['-O2']\ninclude = ['include']\n");
//     CREATE_BUILDPARSER(parser, build_manager, muuk_filer, compiler, build_dir);

//     auto compilation_targets = build_manager->get_compilation_targets();
//     ASSERT_EQ(compilation_targets.size(), 1);
//     EXPECT_EQ(compilation_targets[0].inputs[0], util::file_system::to_linux_path((fs::current_path() / "src/test.cpp").string()));
//     EXPECT_EQ(compilation_targets[0].flags[0], "-O2");
// }

// TEST_F(BuildParserTest, ParseLibraries) {
//     SETUP_MUUK_FILER(mock_file_ops, "[library.test_lib]\nsources = [{path = 'src/test.cpp'}]\naflags = ['-static']\n");
//     CREATE_BUILDPARSER(parser, build_manager, muuk_filer, compiler, build_dir);

//     auto archive_targets = build_manager->get_archive_targets();
//     ASSERT_EQ(archive_targets.size(), 1);
//     EXPECT_EQ(archive_targets[0].inputs[0], "../../test_build_dir/test_lib/test.obj");
//     EXPECT_EQ(archive_targets[0].flags[0], "-static");
// }

// TEST_F(BuildParserTest, ParseExecutables) {
//     SETUP_MUUK_FILER(mock_file_ops, "[build.test_exe]\nsources = [{path = 'src/test.cpp'}]\nlflags = ['-lm']\nprofiles = ['debug']\n");
//     CREATE_BUILDPARSER(parser, build_manager, muuk_filer, compiler, build_dir);

//     auto link_targets = build_manager->get_link_targets();
//     ASSERT_EQ(link_targets.size(), 1);
//     EXPECT_EQ(link_targets[0].inputs[0], "../../test_build_dir/test_exe/test.obj");
//     EXPECT_EQ(link_targets[0].flags[0], "-lm");
// }

// TEST_F(BuildParserTest, ParseEmptyBuildFile) {
//     SETUP_MUUK_FILER(mock_file_ops, "");
//     CREATE_BUILDPARSER(parser, build_manager, muuk_filer, compiler, build_dir);

//     EXPECT_EQ(build_manager->get_compilation_targets().size(), 0);
//     EXPECT_EQ(build_manager->get_archive_targets().size(), 0);
//     EXPECT_EQ(build_manager->get_link_targets().size(), 0);
// }