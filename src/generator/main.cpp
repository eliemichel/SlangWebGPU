#include <argparse/argparse.hpp>

int main(int argc, char* argv[]) {
	argparse::ArgumentParser program("slang_webgpu_generator");

	program.add_argument("foo");

	program.parse_args(argc, argv);

	return 0;
}
