// Offline fault-injection runner. Compiles isolated copies, never edits
// sources.
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <sys/wait.h>

namespace {
std::string quote(const std::string& s) {
  std::string result = "'";
  for (char c : s) result += c == '\'' ? "'\\''" : std::string(1, c);
  return result + "'";
}
struct Fault {
  const char* name;
  const char* before;
  const char* after;
};
constexpr std::array faults{
    Fault{"modal_decay", "std::exp(log001 / (rate_ * seconds))",
          "std::exp(-log001 / (rate_ * seconds))"},
    Fault{"coupling", "z.x = couplingSin_ * x + couplingCos_ * z.x;",
          "z.x = couplingSin_ * x + 1.001 * couplingCos_ * z.x;"},
    Fault{"reset", "for (auto& m : b.modes) m.x = m.y = 0;",
          "for (auto& m : b.modes) m.x = m.x;"},
    Fault{"projection", "energy > 16 ? 4 / std::sqrt(energy) : 1", "1"},
    Fault{"input_finite", "if (!std::isfinite(drive))", "if (false)"},
    Fault{"midi_release", "if (e.type == EventType::NoteOn) release = false;",
          "if (false) release = false;"},
    Fault{"waveguide_decay", "std::exp(log001 / (root * seconds))", "1.01"},
    Fault{"delay_history", "if (n > available) return 0;",
          "if (false) return 0;"},
    Fault{"validation", "if (!validControls(c))", "if (false)"},
    Fault{"allocation", "if (!left || frames == 0) return;",
          "if (!left || frames == 0) return;\n  auto* p = ::operator "
          "new(16); ::operator delete(p);"}};
}  // namespace
int main(int argc, char** argv) {
  try {
    if (argc != 4)
      throw std::runtime_error(
          "usage: mutations SOURCE_DIRECTORY NEW_OUTPUT_DIRECTORY CXX");
    const std::filesystem::path source = std::filesystem::canonical(argv[1]);
    const std::filesystem::path output = std::filesystem::absolute(argv[2]);
    if (std::filesystem::exists(output))
      throw std::runtime_error(
          "mutation output already exists; choose a new directory");
    std::filesystem::create_directories(output);
    std::ifstream file(source / "core.cpp");
    if (!file) throw std::runtime_error("cannot read core.cpp");
    const std::string original{std::istreambuf_iterator<char>(file), {}};
    std::ofstream report(output / "results.csv");
    report.exceptions(std::ios::failbit | std::ios::badbit);
    report << "fault,compile_exit,test_exit,detected\n";
    for (const auto& fault : faults) {
      auto text = original;
      const auto position = text.find(fault.before);
      if (position == std::string::npos ||
          text.find(fault.before, position + 1) != std::string::npos)
        throw std::runtime_error(std::string("mutation anchor changed: ") +
                                 fault.name);
      text.replace(position, std::string(fault.before).size(), fault.after);
      const auto cpp = output / (std::string(fault.name) + ".cpp");
      const auto binary = output / fault.name;
      std::ofstream mutated(cpp);
      mutated.exceptions(std::ios::failbit | std::ios::badbit);
      mutated << text;
      mutated.close();
      const auto compile =
          quote(argv[3]) + " -std=c++20 -O2 -I " + quote(source.string()) +
          " " + quote(cpp.string()) + " " +
          quote((source / "tests.cpp").string()) + " -o " +
          quote(binary.string()) + " > " +
          quote(
              (output / (std::string(fault.name) + "-compile.log")).string()) +
          " 2>&1";
      if (std::system(compile.c_str()) != 0)
        throw std::runtime_error(std::string("mutation did not compile: ") +
                                 fault.name);
      const auto command =
          quote(binary.string()) + " > " +
          quote((output / (std::string(fault.name) + ".log")).string()) +
          " 2>&1";
      const int status = std::system(command.c_str());
      const bool detected =
          status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 1;
      report << fault.name << ",0,"
             << (status != -1 && WIFEXITED(status) ? WEXITSTATUS(status) : -1)
             << ',' << detected << '\n';
      if (!detected)
        throw std::runtime_error(std::string("test did not reject mutation: ") +
                                 fault.name);
      std::cout << fault.name << ": detected\n";
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
