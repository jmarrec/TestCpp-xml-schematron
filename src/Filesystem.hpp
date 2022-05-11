#ifndef UTILITIES_CORE_FILESYSTEM_HPP
#define UTILITIES_CORE_FILESYSTEM_HPP

#include <filesystem>
#include <string>

#if (defined(_WIN32) || defined(_WIN64))
#  include <locale>
#  include <codecvt>
#endif

namespace openstudio {
using path = std::filesystem::path;

namespace filesystem {

// types
using std::filesystem::path;

using std::filesystem::copy_options;
using std::filesystem::directory_entry;
using std::filesystem::filesystem_error;
using std::filesystem::recursive_directory_iterator;

// functions
using std::filesystem::canonical;
using std::filesystem::copy;
using std::filesystem::copy_file;
using std::filesystem::create_directories;
using std::filesystem::create_directory;
using std::filesystem::directory_iterator;
using std::filesystem::equivalent;
using std::filesystem::exists;
using std::filesystem::file_size;
using std::filesystem::is_directory;
using std::filesystem::is_empty;
using std::filesystem::is_regular_file;
using std::filesystem::is_symlink;
using std::filesystem::last_write_time;
using std::filesystem::read_symlink;
using std::filesystem::relative;
using std::filesystem::remove;
using std::filesystem::remove_all;
using std::filesystem::temp_directory_path;
using std::filesystem::weakly_canonical;

}  // namespace filesystem

/** path to UTF-8 encoding. */
inline std::string toString(const path& p) {
#if (defined(_WIN32) || defined(_WIN64))
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter;
  std::string s = converter.to_bytes(p.generic_wstring());
  return s;
#endif

  // cppcheck-suppress duplicateBreak
  return p.generic_string();
}

}  // namespace openstudio

#endif
